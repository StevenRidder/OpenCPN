// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "portable_nautical_package.hpp"

#include <iostream>
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

const ocpn::render::ProvenanceRecord* FindProvenance(
    const ocpn::render::ChartSourceProduct& product,
    const std::string& provenance_id) {
  for (const ocpn::render::ProvenanceRecord& provenance :
       product.provenance_table) {
    if (provenance.provenance_id == provenance_id) {
      return &provenance;
    }
  }
  return nullptr;
}

bool HasAttribute(const ocpn::render::NormalizedChartObject& object,
                  const std::string& acronym, const std::string& value) {
  for (const ocpn::render::ChartAttribute& attr : object.attributes) {
    if (attr.acronym == acronym && attr.value == value &&
        !attr.display_value.empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  ocpn::render::PortableNauticalPackage round_tripped;
  if (!ocpn::render::RoundTripPortablePackageFixture(&round_tripped,
                                                     &diagnostics)) {
    std::cerr << "Portable package fixture round trip failed\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  if (round_tripped.manifest.package_id !=
          "format-2-portable-package-fixture" ||
      round_tripped.product.sources.size() != 1 ||
      round_tripped.product.objects.size() != 2 ||
      round_tripped.coverage.size() != 1 ||
      round_tripped.checksums.package_hash.empty() ||
      round_tripped.checksums.record_hashes.count("features") == 0 ||
      round_tripped.checksums.record_hashes.count("coverage") == 0) {
    std::cerr << "Round-tripped package lost manifest, record, or checksum "
                 "identity\n";
    return 1;
  }

  const ocpn::render::ChartSourceRef& source =
      round_tripped.product.sources.front();
  if (source.source_id != "synthetic-noaa-format2-cell" ||
      source.content_hash.empty() || source.native_scale_denom != 20000.0 ||
      source.metadata.at("license_class") != "redistributable_synthetic") {
    std::cerr << "Round-tripped package lost source identity, scale, checksum, "
                 "or distribution metadata\n";
    return 1;
  }

  const ocpn::render::NormalizedChartObject* depth_area =
      FindObject(round_tripped.product, "US5FORMAT2:DEPARE.1001");
  if (!depth_area || depth_area->object_class != "DEPARE" ||
      depth_area->geometry.geometry_id != "geom-depare-1001" ||
      depth_area->geometry.rings.empty() ||
      depth_area->min_scale_denom != 10000.0 ||
      depth_area->max_scale_denom != 90000.0 ||
      !HasAttribute(*depth_area, "DRVAL2", "4") ||
      depth_area->metadata.at("native_feature_id") != "DEPARE.1001") {
    std::cerr << "Round-tripped package lost normalized feature geometry, "
                 "attributes, scale, or native feature id\n";
    return 1;
  }

  const ocpn::render::ProvenanceRecord* provenance =
      FindProvenance(round_tripped.product, "prov-depare-1001");
  if (!provenance ||
      provenance->source_chart_id != "synthetic-noaa-format2-cell" ||
      provenance->source_object_id != "DEPARE.1001" ||
      provenance->source_geometry_hash.empty() ||
      provenance->generated_geometry_hash.empty() ||
      provenance->transform_chain.size() < 3) {
    std::cerr << "Round-tripped package lost provenance chain or geometry "
                 "hashes\n";
    return 1;
  }

  const ocpn::render::PortableCoverageRecord& coverage =
      round_tripped.coverage.front();
  if (coverage.coverage_id != "coverage-format2-cell" ||
      coverage.source_id != "synthetic-noaa-format2-cell" ||
      coverage.geometry.rings.empty() ||
      coverage.min_scale_denom != 5000.0 ||
      coverage.max_scale_denom != 90000.0 ||
      coverage.metadata.at("coverage_role") != "primary_cell") {
    std::cerr << "Round-tripped package lost coverage geometry or scale "
                 "metadata\n";
    return 1;
  }

  const std::string encoded =
      ocpn::render::WritePortableNauticalPackage(round_tripped);
  ocpn::render::PortableNauticalPackage decoded;
  diagnostics.clear();
  if (!ocpn::render::ReadPortableNauticalPackage(encoded, &decoded,
                                                 &diagnostics)) {
    std::cerr << "Portable package parser rejected the writer output\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }
  if (encoded != ocpn::render::WritePortableNauticalPackage(decoded)) {
    std::cerr << "Portable package writer/parser are not canonical\n";
    return 1;
  }

  ocpn::render::PortableNauticalPackage boundary_leak =
      ocpn::render::BuildPortablePackageFixture();
  boundary_leak.product.metadata["gpu_cache_key"] = "do-not-store";
  boundary_leak.checksums =
      ocpn::render::ComputePortablePackageChecksums(boundary_leak);
  diagnostics.clear();
  if (ocpn::render::ValidatePortableNauticalPackage(boundary_leak,
                                                    &diagnostics)) {
    std::cerr << "Portable package validator allowed backend/GPU metadata\n";
    return 1;
  }

  bool saw_backend_field = false;
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == "portable_package_backend_field") {
      saw_backend_field = true;
    }
  }
  if (!saw_backend_field) {
    std::cerr << "Portable package validator did not identify backend/GPU "
                 "metadata leakage\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  return 0;
}
