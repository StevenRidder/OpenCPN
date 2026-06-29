// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s57_portable_package_converter.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

void PrintDiagnostics(const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

const ocpn::render::NormalizedChartObject* FindObject(
    const ocpn::render::ChartSourceProduct& product,
    const std::string& object_id) {
  for (const ocpn::render::NormalizedChartObject& object : product.objects) {
    if (object.object_id == object_id) {
      return &object;
    }
  }
  return nullptr;
}

bool HasDiagnostic(const ocpn::render::ChartSourceProduct& product,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : product.diagnostics) {
    if (diagnostic.code == code && !diagnostic.provenance_refs.empty()) {
      return true;
    }
  }
  return false;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

}  // namespace

int main() {
  const ocpn::render::S57FixtureCell cell =
      ocpn::render::BuildS57ConverterFixtureCell();
  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(cell);

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::ValidateS57ConverterFixturePackage(package,
                                                        &diagnostics)) {
    std::cerr << "S-57 converter fixture package failed validation\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  const std::string encoded =
      ocpn::render::WritePortableNauticalPackage(package);
  ocpn::render::PortableNauticalPackage decoded;
  diagnostics.clear();
  if (!ocpn::render::ReadPortableNauticalPackage(encoded, &decoded,
                                                 &diagnostics)) {
    std::cerr << "S-57 package writer produced unreadable output\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }
  diagnostics.clear();
  if (!ocpn::render::ValidateS57ConverterFixturePackage(decoded,
                                                        &diagnostics)) {
    std::cerr << "Round-tripped S-57 converter package failed validation\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }
  if (encoded != ocpn::render::WritePortableNauticalPackage(decoded)) {
    std::cerr << "S-57 converter package is not byte-stable after read/write\n";
    return 1;
  }

  if (decoded.manifest.converter_id !=
          "synthetic-s57-portable-converter" ||
      decoded.manifest.converter_version != "convert-2" ||
      decoded.manifest.source_epoch !=
          "producer:US00/dataset:US5CONVERT2/edition:4/update:2" ||
      decoded.checksums.package_hash.empty() ||
      decoded.checksums.record_hashes.count("features") == 0 ||
      decoded.checksums.record_hashes.count("diagnostics") == 0) {
    std::cerr << "S-57 converter package lost manifest or checksum identity\n";
    return 1;
  }

  const ocpn::render::ChartSourceRef& source =
      decoded.product.sources.front();
  if (source.kind != ocpn::render::ChartSourceKind::kS57Cell ||
      source.native_name != "US5CONVERT2" || source.edition != "4" ||
      source.update != "2" || source.content_hash.empty() ||
      source.native_scale_denom != 20000.0 ||
      MetadataValue(source.metadata, "update_chain") !=
          "base:4,update:1,update:2" ||
      MetadataValue(source.metadata, "s57.producer_code") != "US00") {
    std::cerr << "S-57 source identity/update metadata was not preserved\n";
    return 1;
  }

  const ocpn::render::NormalizedChartObject* depth_area =
      FindObject(decoded.product, "US5CONVERT2:DEPARE.1001");
  if (!depth_area || depth_area->geometry.rings.empty() ||
      MetadataValue(depth_area->metadata, "s57.source_feature_id") !=
          "DEPARE.1001" ||
      MetadataValue(depth_area->metadata, "s57.update_action") !=
          "modified_by_update_2" ||
      MetadataValue(depth_area->metadata, "s57.source_geometry_hash").empty()) {
    std::cerr << "S-57 DEPARE source feature id, update action, or geometry "
                 "hash was not preserved\n";
    return 1;
  }

  const ocpn::render::NormalizedChartObject* unsupported =
      FindObject(decoded.product, "US5CONVERT2:LIGHTS.9001");
  if (unsupported != nullptr) {
    std::cerr << "Unsupported LIGHTS feature was emitted as a normalized "
                 "feature instead of a diagnostic\n";
    return 1;
  }
  if (!HasDiagnostic(decoded.product, "s57.unsupported_object_class")) {
    std::cerr << "Unsupported LIGHTS feature diagnostic is missing or "
                 "untraceable\n";
    return 1;
  }

  if (decoded.coverage.size() != 1 ||
      decoded.coverage.front().source_id != "s57:US5CONVERT2" ||
      decoded.coverage.front().geometry.rings.empty() ||
      decoded.coverage.front().provenance_refs.size() !=
          decoded.product.provenance_table.size()) {
    std::cerr << "S-57 coverage polygon or trace handles were not preserved\n";
    return 1;
  }

  ocpn::render::PortableNauticalPackage boundary_leak = package;
  boundary_leak.product.sources.front().metadata["gpu_buffer"] = "forbidden";
  boundary_leak.checksums =
      ocpn::render::ComputePortablePackageChecksums(boundary_leak);
  diagnostics.clear();
  if (ocpn::render::ValidatePortableNauticalPackage(boundary_leak,
                                                    &diagnostics)) {
    std::cerr << "Portable package accepted a backend/GPU leak from converter "
                 "metadata\n";
    return 1;
  }

  return 0;
}
