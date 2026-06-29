// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s57_portable_package_converter.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Inspect the S-57 converter fixture before package acceptance.";
  return diagnostic;
}

GeoBounds Bounds(double west, double south, double east, double north) {
  GeoBounds bounds;
  bounds.west = west;
  bounds.south = south;
  bounds.east = east;
  bounds.north = north;
  return bounds;
}

Geometry AreaGeometry(std::string id, GeoBounds bounds) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kGeographic;
  geometry.rings.push_back({{bounds.west, bounds.south},
                            {bounds.east, bounds.south},
                            {bounds.east, bounds.north},
                            {bounds.west, bounds.north},
                            {bounds.west, bounds.south}});
  return geometry;
}

Geometry LineGeometry(std::string id) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kGeographic;
  geometry.points = {{-81.842, 24.446}, {-81.812, 24.461},
                     {-81.771, 24.486}};
  return geometry;
}

Geometry PointGeometry(std::string id, double lon, double lat) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kGeographic;
  geometry.points.push_back({lon, lat});
  return geometry;
}

ChartAttribute Attr(std::string acronym, std::string value,
                    std::string display_value = {}) {
  ChartAttribute attr;
  attr.acronym = std::move(acronym);
  attr.value = std::move(value);
  attr.display_value = display_value.empty() ? attr.value
                                             : std::move(display_value);
  return attr;
}

std::uint64_t Fnv1a(const std::string& input) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : input) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string HashString(const std::string& input) {
  std::ostringstream out;
  out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0')
      << Fnv1a(input);
  return out.str();
}

std::string SourceId(const S57FixtureCell& cell) {
  return "s57:" + cell.dataset_name;
}

std::string PackageFeatureId(const S57FixtureCell& cell,
                             const S57FixtureFeature& feature) {
  return cell.dataset_name + ":" + feature.source_feature_id;
}

std::string ProvenanceId(const S57FixtureCell& cell,
                         const S57FixtureFeature& feature) {
  return "prov:s57:" + cell.dataset_name + ":" + feature.source_feature_id;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

bool HasDiagnostic(const ChartSourceProduct& product,
                   const std::string& code) {
  return std::any_of(product.diagnostics.begin(), product.diagnostics.end(),
                     [&](const Diagnostic& diagnostic) {
                       return diagnostic.code == code;
                     });
}

const NormalizedChartObject* FindObject(const ChartSourceProduct& product,
                                        const std::string& object_id) {
  for (const NormalizedChartObject& object : product.objects) {
    if (object.object_id == object_id) {
      return &object;
    }
  }
  return nullptr;
}

const ProvenanceRecord* FindProvenance(const ChartSourceProduct& product,
                                       const std::string& provenance_id) {
  for (const ProvenanceRecord& provenance : product.provenance_table) {
    if (provenance.provenance_id == provenance_id) {
      return &provenance;
    }
  }
  return nullptr;
}

bool HasAttr(const NormalizedChartObject& object, const std::string& acronym,
             const std::string& value) {
  return std::any_of(object.attributes.begin(), object.attributes.end(),
                     [&](const ChartAttribute& attr) {
                       return attr.acronym == acronym && attr.value == value;
                     });
}

}  // namespace

S57FixtureCell BuildS57ConverterFixtureCell() {
  S57FixtureCell cell;
  cell.dataset_name = "US5CONVERT2";
  cell.edition = "4";
  cell.update = "2";
  cell.producer_code = "US00";
  cell.product_identifier = "INT.IHO.S-57.fixture";
  cell.product_edition = "3.1";
  cell.dataset_reference_date = "2026-06-30";
  cell.source_epoch = "producer:US00/dataset:US5CONVERT2/edition:4/update:2";
  cell.content_hash = HashString("US5CONVERT2|edition=4|update=2|fixture");
  cell.native_projection = "WGS84";
  cell.native_scale_denom = 20000.0;
  cell.geographic_bbox = Bounds(-81.86, 24.42, -81.74, 24.53);
  cell.metadata["distribution_class"] = "redistributable_synthetic";
  cell.metadata["fixture_scope"] = "bounded_s57_vertical_slice";
  cell.metadata["update_chain"] = "base:4,update:1,update:2";

  S57FixtureFeature depth_area;
  depth_area.source_feature_id = "DEPARE.1001";
  depth_area.object_class = "DEPARE";
  depth_area.geometry_kind = NormalizedGeometryKind::kArea;
  depth_area.geometry = AreaGeometry("s57-geom-depare-1001",
                                     Bounds(-81.846, 24.435, -81.754, 24.515));
  depth_area.attributes = {Attr("DRVAL1", "0", "0 m"),
                           Attr("DRVAL2", "4", "4 m"),
                           Attr("QUASOU", "6", "surveyed")};
  depth_area.min_scale_denom = 10000.0;
  depth_area.max_scale_denom = 90000.0;
  depth_area.source_priority = 10;
  depth_area.source_geometry_hash =
      HashString("DEPARE.1001|area|-81.846,24.435,-81.754,24.515");
  depth_area.metadata["s57.scamin"] = "60000";
  depth_area.metadata["s57.update_action"] = "modified_by_update_2";
  cell.features.push_back(std::move(depth_area));

  S57FixtureFeature contour;
  contour.source_feature_id = "DEPCNT.2001";
  contour.object_class = "DEPCNT";
  contour.geometry_kind = NormalizedGeometryKind::kLine;
  contour.geometry = LineGeometry("s57-geom-depcnt-2001");
  contour.attributes = {Attr("VALDCO", "10", "10 m")};
  contour.min_scale_denom = 5000.0;
  contour.max_scale_denom = 80000.0;
  contour.source_priority = 15;
  contour.source_geometry_hash = HashString("DEPCNT.2001|line|10m");
  contour.metadata["s57.scamin"] = "40000";
  contour.metadata["s57.update_action"] = "base";
  cell.features.push_back(std::move(contour));

  S57FixtureFeature buoy;
  buoy.source_feature_id = "BOYLAT.3001";
  buoy.object_class = "BOYLAT";
  buoy.geometry_kind = NormalizedGeometryKind::kPoint;
  buoy.geometry = PointGeometry("s57-geom-boylat-3001", -81.801, 24.472);
  buoy.attributes = {Attr("BOYSHP", "1", "conical"),
                     Attr("CATLAM", "1", "port hand"),
                     Attr("OBJNAM", "Convert 2 fixture buoy")};
  buoy.min_scale_denom = 5000.0;
  buoy.max_scale_denom = 40000.0;
  buoy.source_priority = 20;
  buoy.source_geometry_hash = HashString("BOYLAT.3001|point|-81.801,24.472");
  buoy.metadata["s57.scamin"] = "22000";
  buoy.metadata["s57.update_action"] = "inserted_by_update_1";
  cell.features.push_back(std::move(buoy));

  S57FixtureFeature unsupported;
  unsupported.source_feature_id = "LIGHTS.9001";
  unsupported.object_class = "LIGHTS";
  unsupported.geometry_kind = NormalizedGeometryKind::kPoint;
  unsupported.geometry =
      PointGeometry("s57-geom-lights-9001", -81.792, 24.481);
  unsupported.attributes = {Attr("COLOUR", "3", "red"),
                            Attr("LITCHR", "2", "flashing")};
  unsupported.min_scale_denom = 5000.0;
  unsupported.max_scale_denom = 40000.0;
  unsupported.source_priority = 30;
  unsupported.source_geometry_hash =
      HashString("LIGHTS.9001|point|-81.792,24.481");
  unsupported.supported = false;
  unsupported.unsupported_reason =
      "Light sectors are deferred in the first S-57 converter slice.";
  unsupported.metadata["s57.update_action"] = "base";
  cell.features.push_back(std::move(unsupported));

  return cell;
}

S57ConverterIdentity S57PortablePackageConverter::Identity() const {
  return {};
}

PortableNauticalPackage S57PortablePackageConverter::Convert(
    const S57FixtureCell& cell) const {
  const S57ConverterIdentity identity = Identity();
  PortableNauticalPackage package;
  package.manifest.package_id = SourceId(cell) + ":package";
  package.manifest.profile = "s57-s52-poc";
  package.manifest.producer = "opencpn-chart-render";
  package.manifest.producer_version = "convert-2";
  package.manifest.converter_id = identity.converter_id;
  package.manifest.converter_version = identity.converter_version;
  package.manifest.created_at = "2026-06-30T00:00:00Z";
  package.manifest.source_epoch = cell.source_epoch;
  package.manifest.content_hash = HashString(cell.content_hash + "|portable");
  package.manifest.fixture_package = true;
  package.manifest.metadata["converter_module"] = identity.converter_id;
  package.manifest.metadata["converter_version"] = identity.converter_version;
  package.manifest.metadata["package_boundary"] = "portable_chart_truth";
  package.manifest.metadata["s100.product_identifier"] =
      cell.product_identifier;
  package.manifest.metadata["s100.product_edition"] = cell.product_edition;

  ChartSourceProduct& product = package.product;
  product.product_id = SourceId(cell) + ":normalized";
  product.metadata["converter_module"] = identity.converter_id;
  product.metadata["source_epoch"] = cell.source_epoch;
  product.metadata["source_schema"] = "s57-fixture";
  product.metadata["update_chain"] = MetadataValue(cell.metadata, "update_chain");

  ChartSourceRef source;
  source.source_id = SourceId(cell);
  source.kind = ChartSourceKind::kS57Cell;
  source.role = ChartSourceRole::kPrimary;
  source.native_name = cell.dataset_name;
  source.edition = cell.edition;
  source.update = cell.update;
  source.content_hash = cell.content_hash;
  source.native_projection = cell.native_projection;
  source.native_scale_denom = cell.native_scale_denom;
  source.geographic_bbox = cell.geographic_bbox;
  source.metadata = cell.metadata;
  source.metadata["s57.dataset_name"] = cell.dataset_name;
  source.metadata["s57.dataset_edition"] = cell.edition;
  source.metadata["s57.dataset_update"] = cell.update;
  source.metadata["s57.dataset_reference_date"] =
      cell.dataset_reference_date;
  source.metadata["s57.producer_code"] = cell.producer_code;
  source.metadata["s57.product_identifier"] = cell.product_identifier;
  source.metadata["s57.product_edition"] = cell.product_edition;
  product.sources.push_back(std::move(source));

  for (const S57FixtureFeature& feature : cell.features) {
    const std::string provenance_id = ProvenanceId(cell, feature);

    ProvenanceRecord provenance;
    provenance.provenance_id = provenance_id;
    provenance.source_chart_id = SourceId(cell);
    provenance.source_chart_edition = cell.edition;
    provenance.source_update = cell.update;
    provenance.source_object_id = feature.source_feature_id;
    provenance.source_object_class = feature.object_class;
    provenance.source_geometry_hash = feature.source_geometry_hash;
    provenance.generated_geometry_hash =
        HashString(feature.geometry.geometry_id + "|" +
                   feature.source_geometry_hash);
    provenance.conversion_stage = "s57_fixture_to_portable_package";
    provenance.transform_chain = {
        "s57:decode_feature", "s57:apply_update_chain",
        "s57:normalize_wgs84", "portable:write_records"};
    product.provenance_table.push_back(std::move(provenance));

    if (!feature.supported) {
      product.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kWarning, "s57.unsupported_object_class",
          feature.object_class + " " + feature.source_feature_id + ": " +
              feature.unsupported_reason,
          {provenance_id}));
      continue;
    }

    NormalizedChartObject object;
    object.object_id = PackageFeatureId(cell, feature);
    object.object_class = feature.object_class;
    object.geometry_kind = feature.geometry_kind;
    object.geometry = feature.geometry;
    object.attributes = feature.attributes;
    object.min_scale_denom = feature.min_scale_denom;
    object.max_scale_denom = feature.max_scale_denom;
    object.source_priority = feature.source_priority;
    object.provenance_refs.push_back(provenance_id);
    object.metadata = feature.metadata;
    object.metadata["s57.source_feature_id"] = feature.source_feature_id;
    object.metadata["s57.object_class"] = feature.object_class;
    object.metadata["s57.source_geometry_hash"] =
        feature.source_geometry_hash;
    product.objects.push_back(std::move(object));
  }

  PortableCoverageRecord coverage;
  coverage.coverage_id = "coverage:" + SourceId(cell);
  coverage.source_id = SourceId(cell);
  coverage.geometry = AreaGeometry("coverage-geom:" + SourceId(cell),
                                   cell.geographic_bbox);
  coverage.min_scale_denom = 5000.0;
  coverage.max_scale_denom = 90000.0;
  coverage.priority = 10;
  coverage.chart_family = "S-57";
  coverage.boundary_policy = "cell_coverage_polygon";
  coverage.update_epoch = cell.source_epoch;
  for (const ProvenanceRecord& provenance : product.provenance_table) {
    coverage.provenance_refs.push_back(provenance.provenance_id);
  }
  coverage.metadata["coverage_role"] = "primary_cell";
  coverage.metadata["s57.dataset_name"] = cell.dataset_name;
  coverage.metadata["trace_handle"] = "s57:" + cell.dataset_name +
                                      ":edition:" + cell.edition +
                                      ":update:" + cell.update;
  package.coverage.push_back(std::move(coverage));

  package.checksums = ComputePortablePackageChecksums(package);
  return package;
}

bool ValidateS57ConverterFixturePackage(
    const PortableNauticalPackage& package,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidatePortableNauticalPackage(package, &out);
  if (package.manifest.converter_id !=
          "synthetic-s57-portable-converter" ||
      package.manifest.converter_version != "convert-2" ||
      package.manifest.profile != "s57-s52-poc") {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_identity",
                                 "Package lost converter identity or profile."));
    ok = false;
  }
  if (package.product.sources.size() != 1 ||
      package.product.sources.front().kind != ChartSourceKind::kS57Cell ||
      package.product.sources.front().source_id != "s57:US5CONVERT2" ||
      package.product.sources.front().edition != "4" ||
      package.product.sources.front().update != "2" ||
      package.product.sources.front().content_hash.empty() ||
      MetadataValue(package.product.sources.front().metadata,
                    "s57.producer_code") != "US00") {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_source_identity",
                                 "Package lost S-57 source identity, edition, "
                                 "update, hash, or producer metadata."));
    ok = false;
  }

  const NormalizedChartObject* depth_area =
      FindObject(package.product, "US5CONVERT2:DEPARE.1001");
  if (!depth_area || depth_area->object_class != "DEPARE" ||
      depth_area->geometry_kind != NormalizedGeometryKind::kArea ||
      depth_area->geometry.rings.empty() ||
      !HasAttr(*depth_area, "DRVAL2", "4") ||
      MetadataValue(depth_area->metadata, "s57.source_feature_id") !=
          "DEPARE.1001" ||
      MetadataValue(depth_area->metadata, "s57.source_geometry_hash").empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_feature_preservation",
                                 "Depth-area feature lost source id, "
                                 "attributes, geometry, or geometry hash."));
    ok = false;
  }

  const NormalizedChartObject* buoy =
      FindObject(package.product, "US5CONVERT2:BOYLAT.3001");
  if (!buoy || buoy->geometry_kind != NormalizedGeometryKind::kPoint ||
      buoy->geometry.points.empty() || !HasAttr(*buoy, "CATLAM", "1")) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_point_preservation",
                                 "Buoy feature lost point geometry or "
                                 "attributes."));
    ok = false;
  }

  const ProvenanceRecord* provenance =
      FindProvenance(package.product, "prov:s57:US5CONVERT2:DEPARE.1001");
  if (!provenance || provenance->source_chart_id != "s57:US5CONVERT2" ||
      provenance->source_object_id != "DEPARE.1001" ||
      provenance->source_geometry_hash.empty() ||
      provenance->generated_geometry_hash.empty() ||
      provenance->transform_chain.size() < 4) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_provenance",
                                 "Package lost S-57 provenance chain."));
    ok = false;
  }

  if (package.coverage.size() != 1 ||
      package.coverage.front().source_id != "s57:US5CONVERT2" ||
      package.coverage.front().geometry.rings.empty() ||
      package.coverage.front().provenance_refs.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_coverage",
                                 "Package lost source coverage polygon or "
                                 "coverage provenance."));
    ok = false;
  }

  if (!HasDiagnostic(package.product, "s57.unsupported_object_class")) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_unsupported_diagnostic",
                                 "Unsupported fixture feature was not reported "
                                 "as a diagnostic."));
    ok = false;
  }

  const PortablePackageChecksums expected =
      ComputePortablePackageChecksums(package);
  if (expected.package_hash != package.checksums.package_hash) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "s57_converter_checksum",
                                 "Package checksum changed after conversion."));
    ok = false;
  }

  return ok;
}

}  // namespace ocpn::render
