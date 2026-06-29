// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "portable_nautical_package.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
namespace {

constexpr const char* kPackageMagic = "portable_nautical_package";

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Reject the portable nautical package before presentation compilation.";
  return diagnostic;
}

Diagnostic Error(std::string code, std::string message,
                 std::vector<std::string> provenance_refs = {}) {
  return MakeDiagnostic(DiagnosticSeverity::kError, std::move(code),
                        std::move(message), std::move(provenance_refs));
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

bool IsFinite(double value) {
  return std::isfinite(value);
}

bool ValidBounds(const GeoBounds& bounds) {
  return IsFinite(bounds.west) && IsFinite(bounds.south) &&
         IsFinite(bounds.east) && IsFinite(bounds.north) &&
         bounds.west < bounds.east && bounds.south < bounds.north;
}

GeoBounds EmptyBounds() {
  GeoBounds bounds;
  bounds.west = std::numeric_limits<double>::max();
  bounds.south = std::numeric_limits<double>::max();
  bounds.east = std::numeric_limits<double>::lowest();
  bounds.north = std::numeric_limits<double>::lowest();
  return bounds;
}

bool IsEmptyBounds(const GeoBounds& bounds) {
  return bounds.west > bounds.east || bounds.south > bounds.north;
}

void IncludePoint(GeoBounds* bounds, const Point2& point) {
  bounds->west = std::min(bounds->west, point.x);
  bounds->south = std::min(bounds->south, point.y);
  bounds->east = std::max(bounds->east, point.x);
  bounds->north = std::max(bounds->north, point.y);
}

GeoBounds GeometryBounds(const Geometry& geometry) {
  GeoBounds bounds = EmptyBounds();
  for (const Point2& point : geometry.points) {
    IncludePoint(&bounds, point);
  }
  for (const auto& ring : geometry.rings) {
    for (const Point2& point : ring) {
      IncludePoint(&bounds, point);
    }
  }
  return IsEmptyBounds(bounds) ? GeoBounds{} : bounds;
}

bool HasGeometryCoordinates(const Geometry& geometry) {
  if (!geometry.points.empty()) {
    return true;
  }
  return std::any_of(geometry.rings.begin(), geometry.rings.end(),
                     [](const std::vector<Point2>& ring) {
                       return !ring.empty();
                     });
}

std::string DoubleString(double value) {
  std::ostringstream out;
  out << std::setprecision(17) << value;
  return out.str();
}

std::string IntString(int value) {
  return std::to_string(value);
}

std::string UIntString(std::uint32_t value) {
  return std::to_string(value);
}

std::string BoolString(bool value) {
  return value ? "true" : "false";
}

std::string HexByte(unsigned char value) {
  const char* digits = "0123456789ABCDEF";
  std::string encoded;
  encoded.push_back('%');
  encoded.push_back(digits[(value >> 4) & 0x0F]);
  encoded.push_back(digits[value & 0x0F]);
  return encoded;
}

std::string Escape(const std::string& value) {
  std::string escaped;
  for (unsigned char ch : value) {
    if (ch == '%' || ch == '|' || ch == '\n' || ch == '\r') {
      escaped += HexByte(ch);
    } else {
      escaped.push_back(static_cast<char>(ch));
    }
  }
  return escaped;
}

int HexValue(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + ch - 'A';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + ch - 'a';
  }
  return -1;
}

bool Unescape(const std::string& encoded, std::string* value) {
  std::string decoded;
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] != '%') {
      decoded.push_back(encoded[i]);
      continue;
    }
    if (i + 2 >= encoded.size()) {
      return false;
    }
    const int hi = HexValue(encoded[i + 1]);
    const int lo = HexValue(encoded[i + 2]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    decoded.push_back(static_cast<char>((hi << 4) | lo));
    i += 2;
  }
  *value = std::move(decoded);
  return true;
}

bool SplitLine(const std::string& line, std::vector<std::string>* fields) {
  fields->clear();
  std::string field;
  for (char ch : line) {
    if (ch == '|') {
      std::string decoded;
      if (!Unescape(field, &decoded)) {
        return false;
      }
      fields->push_back(std::move(decoded));
      field.clear();
    } else {
      field.push_back(ch);
    }
  }
  std::string decoded;
  if (!Unescape(field, &decoded)) {
    return false;
  }
  fields->push_back(std::move(decoded));
  return true;
}

std::string Line(std::initializer_list<std::string> fields) {
  std::string line;
  bool first = true;
  for (const std::string& field : fields) {
    if (!first) {
      line.push_back('|');
    }
    first = false;
    line += Escape(field);
  }
  return line;
}

std::string JoinLines(const std::vector<std::string>& lines) {
  std::ostringstream out;
  for (const std::string& line : lines) {
    out << line << '\n';
  }
  return out.str();
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

std::string HashLines(const std::vector<std::string>& lines) {
  return HashString(JoinLines(lines));
}

const char* ToString(ChartSourceRole role) {
  switch (role) {
    case ChartSourceRole::kPrimary:
      return "primary";
    case ChartSourceRole::kUpdate:
      return "update";
    case ChartSourceRole::kOverview:
      return "overview";
    case ChartSourceRole::kOverlay:
      return "overlay";
    case ChartSourceRole::kDebug:
      return "debug";
  }
  return "unknown";
}

const char* ToString(NormalizedGeometryKind kind) {
  switch (kind) {
    case NormalizedGeometryKind::kUnknown:
      return "unknown";
    case NormalizedGeometryKind::kPoint:
      return "point";
    case NormalizedGeometryKind::kLine:
      return "line";
    case NormalizedGeometryKind::kArea:
      return "area";
    case NormalizedGeometryKind::kMultiPoint:
      return "multi_point";
    case NormalizedGeometryKind::kRasterSheetReference:
      return "raster_sheet_reference";
  }
  return "unknown";
}

const char* ToString(CoordinateSpace space) {
  switch (space) {
    case CoordinateSpace::kGeographic:
      return "geographic";
    case CoordinateSpace::kProjected:
      return "projected";
    case CoordinateSpace::kTarget:
      return "target";
    case CoordinateSpace::kGlyph:
      return "glyph";
    case CoordinateSpace::kRaster:
      return "raster";
  }
  return "unknown";
}

const char* ToString(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::kInfo:
      return "info";
    case DiagnosticSeverity::kWarning:
      return "warning";
    case DiagnosticSeverity::kError:
      return "error";
  }
  return "unknown";
}

bool ParseBool(const std::string& value, bool* out) {
  if (value == "true") {
    *out = true;
    return true;
  }
  if (value == "false") {
    *out = false;
    return true;
  }
  return false;
}

bool ParseUInt(const std::string& value, std::uint32_t* out) {
  if (value.empty()) {
    return false;
  }
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return false;
  }
  *out = static_cast<std::uint32_t>(parsed);
  return true;
}

bool ParseInt(const std::string& value, int* out) {
  if (value.empty()) {
    return false;
  }
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return false;
  }
  *out = static_cast<int>(parsed);
  return true;
}

bool ParseDouble(const std::string& value, double* out) {
  if (value.empty()) {
    return false;
  }
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0') {
    return false;
  }
  *out = parsed;
  return true;
}

bool ParseChartSourceKind(const std::string& value, ChartSourceKind* kind) {
  if (value == "s57_cell") {
    *kind = ChartSourceKind::kS57Cell;
  } else if (value == "senc_cell") {
    *kind = ChartSourceKind::kSencCell;
  } else if (value == "raster_chart") {
    *kind = ChartSourceKind::kRasterChart;
  } else if (value == "mbtiles_package") {
    *kind = ChartSourceKind::kMbtilesPackage;
  } else if (value == "pmtiles_package") {
    *kind = ChartSourceKind::kPmtilesPackage;
  } else if (value == "s101_dataset") {
    *kind = ChartSourceKind::kS101Dataset;
  } else if (value == "debug_fixture") {
    *kind = ChartSourceKind::kDebugFixture;
  } else {
    return false;
  }
  return true;
}

bool ParseChartSourceRole(const std::string& value, ChartSourceRole* role) {
  if (value == "primary") {
    *role = ChartSourceRole::kPrimary;
  } else if (value == "update") {
    *role = ChartSourceRole::kUpdate;
  } else if (value == "overview") {
    *role = ChartSourceRole::kOverview;
  } else if (value == "overlay") {
    *role = ChartSourceRole::kOverlay;
  } else if (value == "debug") {
    *role = ChartSourceRole::kDebug;
  } else {
    return false;
  }
  return true;
}

bool ParseGeometryKind(const std::string& value,
                       NormalizedGeometryKind* kind) {
  if (value == "unknown") {
    *kind = NormalizedGeometryKind::kUnknown;
  } else if (value == "point") {
    *kind = NormalizedGeometryKind::kPoint;
  } else if (value == "line") {
    *kind = NormalizedGeometryKind::kLine;
  } else if (value == "area") {
    *kind = NormalizedGeometryKind::kArea;
  } else if (value == "multi_point") {
    *kind = NormalizedGeometryKind::kMultiPoint;
  } else if (value == "raster_sheet_reference") {
    *kind = NormalizedGeometryKind::kRasterSheetReference;
  } else {
    return false;
  }
  return true;
}

bool ParseRasterSheetKind(const std::string& value, RasterSheetKind* kind) {
  if (value == "chart_image") {
    *kind = RasterSheetKind::kChartImage;
  } else if (value == "collar_mask") {
    *kind = RasterSheetKind::kCollarMask;
  } else if (value == "no_data_mask") {
    *kind = RasterSheetKind::kNoDataMask;
  } else if (value == "coverage_mask") {
    *kind = RasterSheetKind::kCoverageMask;
  } else if (value == "debug_overlay") {
    *kind = RasterSheetKind::kDebugOverlay;
  } else {
    return false;
  }
  return true;
}

bool ParseDebugArtifactKind(const std::string& value,
                            DebugArtifactKind* kind) {
  if (value == "source_metadata") {
    *kind = DebugArtifactKind::kSourceMetadata;
  } else if (value == "feature_dump") {
    *kind = DebugArtifactKind::kFeatureDump;
  } else if (value == "geometry_trace") {
    *kind = DebugArtifactKind::kGeometryTrace;
  } else if (value == "rule_trace") {
    *kind = DebugArtifactKind::kRuleTrace;
  } else if (value == "raster_footprint") {
    *kind = DebugArtifactKind::kRasterFootprint;
  } else if (value == "interchange_package") {
    *kind = DebugArtifactKind::kInterchangePackage;
  } else {
    return false;
  }
  return true;
}

bool ParseCoordinateSpace(const std::string& value, CoordinateSpace* space) {
  if (value == "geographic") {
    *space = CoordinateSpace::kGeographic;
  } else if (value == "projected") {
    *space = CoordinateSpace::kProjected;
  } else if (value == "target") {
    *space = CoordinateSpace::kTarget;
  } else if (value == "glyph") {
    *space = CoordinateSpace::kGlyph;
  } else if (value == "raster") {
    *space = CoordinateSpace::kRaster;
  } else {
    return false;
  }
  return true;
}

bool ParseSeverity(const std::string& value, DiagnosticSeverity* severity) {
  if (value == "info") {
    *severity = DiagnosticSeverity::kInfo;
  } else if (value == "warning") {
    *severity = DiagnosticSeverity::kWarning;
  } else if (value == "error") {
    *severity = DiagnosticSeverity::kError;
  } else {
    return false;
  }
  return true;
}

std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool ContainsForbiddenPortableField(const std::string& value) {
  const std::string lowered = LowerAscii(value);
  for (const char* forbidden :
       {"backend", "gpu", "vulkan", "vsg", "webgpu", "metal", "opengl",
        "shader", "device_memory", "buffer_layout", "texture_atlas",
        "wx_canvas", "helm_http", "etag", "scheduler"}) {
    if (lowered.find(forbidden) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void CheckMetadataBoundary(const std::map<std::string, std::string>& metadata,
                           const std::string& owner, bool* ok,
                           std::vector<Diagnostic>* out) {
  for (const auto& entry : metadata) {
    if (ContainsForbiddenPortableField(entry.first) ||
        ContainsForbiddenPortableField(entry.second)) {
      out->push_back(Error("portable_package_backend_field",
                           "Portable package metadata leaks backend/GPU field "
                           "on " +
                               owner + ": " + entry.first));
      *ok = false;
    }
  }
}

void AppendMapLines(const std::string& tag, const std::string& id,
                    const std::map<std::string, std::string>& metadata,
                    std::vector<std::string>* lines) {
  for (const auto& entry : metadata) {
    lines->push_back(Line({tag, id, entry.first, entry.second}));
  }
}

void AppendStringVectorLines(const std::string& tag, const std::string& id,
                             const std::vector<std::string>& values,
                             std::vector<std::string>* lines) {
  for (const std::string& value : values) {
    lines->push_back(Line({tag, id, value}));
  }
}

void AppendGeometryLines(const std::string& point_tag,
                         const std::string& ring_tag,
                         const std::string& owner_id,
                         const Geometry& geometry,
                         std::vector<std::string>* lines) {
  for (const Point2& point : geometry.points) {
    lines->push_back(Line({point_tag, owner_id, DoubleString(point.x),
                           DoubleString(point.y)}));
  }
  for (std::size_t ring_index = 0; ring_index < geometry.rings.size();
       ++ring_index) {
    for (const Point2& point : geometry.rings[ring_index]) {
      lines->push_back(Line({ring_tag, owner_id,
                             UIntString(static_cast<std::uint32_t>(ring_index)),
                             DoubleString(point.x), DoubleString(point.y)}));
    }
  }
}

void AppendCoreLines(const PortableNauticalPackage& package,
                     std::vector<std::string>* lines) {
  lines->push_back(Line({kPackageMagic,
                         UIntString(kPortableNauticalPackageSchemaVersion)}));

  const PortablePackageManifest& manifest = package.manifest;
  lines->push_back(Line({"manifest", UIntString(manifest.schema_version),
                         manifest.package_id, manifest.profile,
                         manifest.producer, manifest.producer_version,
                         manifest.converter_id, manifest.converter_version,
                         manifest.created_at, manifest.source_epoch,
                         manifest.content_hash,
                         manifest.coordinate_reference, manifest.units,
                         BoolString(manifest.fixture_package)}));
  AppendMapLines("manifest_meta", manifest.package_id, manifest.metadata,
                 lines);

  const ChartSourceProduct& product = package.product;
  lines->push_back(Line({"product", UIntString(product.schema_version),
                         product.product_id}));
  AppendMapLines("product_meta", product.product_id, product.metadata, lines);

  for (const ChartSourceRef& source : product.sources) {
    lines->push_back(Line({"source", source.source_id, ToString(source.kind),
                           ToString(source.role), source.native_name,
                           source.edition, source.update,
                           source.content_hash, source.native_projection,
                           DoubleString(source.native_scale_denom),
                           DoubleString(source.geographic_bbox.west),
                           DoubleString(source.geographic_bbox.south),
                           DoubleString(source.geographic_bbox.east),
                           DoubleString(source.geographic_bbox.north)}));
    AppendMapLines("source_meta", source.source_id, source.metadata, lines);
  }

  for (const NormalizedChartObject& object : product.objects) {
    lines->push_back(Line({"feature", object.object_id, object.object_class,
                           ToString(object.geometry_kind),
                           object.geometry.geometry_id,
                           ToString(object.geometry.coordinate_space),
                           DoubleString(object.min_scale_denom),
                           DoubleString(object.max_scale_denom),
                           IntString(object.source_priority)}));
    AppendGeometryLines("geometry_point", "geometry_ring_point",
                        object.object_id, object.geometry, lines);
    for (const ChartAttribute& attr : object.attributes) {
      lines->push_back(Line({"feature_attr", object.object_id, attr.acronym,
                             attr.value, attr.display_value}));
    }
    AppendStringVectorLines("feature_prov", object.object_id,
                            object.provenance_refs, lines);
    AppendMapLines("feature_meta", object.object_id, object.metadata, lines);
  }

  for (const RasterSheet& sheet : product.raster_sheets) {
    lines->push_back(Line(
        {"raster", sheet.sheet_id, ToString(sheet.kind), sheet.source_id,
         DoubleString(sheet.geographic_bbox.west),
         DoubleString(sheet.geographic_bbox.south),
         DoubleString(sheet.geographic_bbox.east),
         DoubleString(sheet.geographic_bbox.north),
         DoubleString(sheet.chart_bounds.west),
         DoubleString(sheet.chart_bounds.south),
         DoubleString(sheet.chart_bounds.east),
         DoubleString(sheet.chart_bounds.north),
         DoubleString(sheet.visible_bounds.west),
         DoubleString(sheet.visible_bounds.south),
         DoubleString(sheet.visible_bounds.east),
         DoubleString(sheet.visible_bounds.north),
         DoubleString(sheet.collar_bounds.west),
         DoubleString(sheet.collar_bounds.south),
         DoubleString(sheet.collar_bounds.east),
         DoubleString(sheet.collar_bounds.north),
         UIntString(sheet.pixel_size.width), UIntString(sheet.pixel_size.height),
         ToString(sheet.coordinate_space), sheet.content_hash,
         sheet.color_model, sheet.no_data_policy, sheet.collar_policy,
         sheet.boundary_policy, sheet.quilt_policy, IntString(sheet.quilt_rank),
         BoolString(sheet.allow_visible_outside_chart_bounds)}));
    AppendStringVectorLines("raster_prov", sheet.sheet_id,
                            sheet.provenance_refs, lines);
    AppendMapLines("raster_meta", sheet.sheet_id, sheet.metadata, lines);
  }

  for (const PortableCoverageRecord& coverage : package.coverage) {
    lines->push_back(Line({"coverage", coverage.coverage_id,
                           coverage.source_id, coverage.geometry.geometry_id,
                           ToString(coverage.geometry.coordinate_space),
                           DoubleString(coverage.min_scale_denom),
                           DoubleString(coverage.max_scale_denom),
                           IntString(coverage.priority),
                           coverage.chart_family, coverage.boundary_policy,
                           coverage.update_epoch}));
    AppendGeometryLines("coverage_point", "coverage_ring_point",
                        coverage.coverage_id, coverage.geometry, lines);
    AppendStringVectorLines("coverage_prov", coverage.coverage_id,
                            coverage.provenance_refs, lines);
    AppendMapLines("coverage_meta", coverage.coverage_id, coverage.metadata,
                   lines);
  }

  for (const DebugArtifact& artifact : product.debug_artifacts) {
    lines->push_back(Line({"debug_artifact", artifact.artifact_id,
                           ToString(artifact.kind), artifact.source_id,
                           artifact.content_hash, artifact.media_type,
                           artifact.producer}));
    AppendStringVectorLines("debug_artifact_prov", artifact.artifact_id,
                            artifact.provenance_refs, lines);
    AppendMapLines("debug_artifact_meta", artifact.artifact_id,
                   artifact.metadata, lines);
  }

  for (const ProvenanceRecord& provenance : product.provenance_table) {
    lines->push_back(Line({"provenance", provenance.provenance_id,
                           provenance.source_chart_id,
                           provenance.source_chart_edition,
                           provenance.source_update,
                           provenance.source_object_id,
                           provenance.source_object_class,
                           provenance.source_geometry_hash,
                           provenance.generated_geometry_hash,
                           provenance.target_geometry_hash,
                           provenance.s52_rule_id,
                           provenance.render_command_id,
                           provenance.conversion_stage,
                           provenance.quilt_decision_id}));
    AppendStringVectorLines("provenance_step", provenance.provenance_id,
                            provenance.transform_chain, lines);
    AppendStringVectorLines("provenance_warning", provenance.provenance_id,
                            provenance.warnings, lines);
  }

  for (const Diagnostic& diagnostic : product.diagnostics) {
    lines->push_back(Line({"diagnostic", ToString(diagnostic.severity),
                           diagnostic.code, diagnostic.message,
                           diagnostic.suggested_action}));
    AppendStringVectorLines("diagnostic_prov", diagnostic.code,
                            diagnostic.provenance_refs, lines);
  }
}

std::vector<std::string> FilterLines(const std::vector<std::string>& lines,
                                     const std::set<std::string>& tags) {
  std::vector<std::string> filtered;
  for (const std::string& line : lines) {
    const std::size_t separator = line.find('|');
    const std::string tag =
        separator == std::string::npos ? line : line.substr(0, separator);
    if (tags.count(tag) != 0) {
      filtered.push_back(line);
    }
  }
  return filtered;
}

bool RequireFieldCount(const std::vector<std::string>& fields,
                       std::size_t expected, const std::string& line,
                       std::vector<Diagnostic>* out) {
  if (fields.size() == expected) {
    return true;
  }
  out->push_back(Error("portable_package_parse",
                       "Malformed portable package line: " + line));
  return false;
}

template <typename T>
T* FindById(std::vector<T>* records, const std::string& id,
            const std::map<std::string, std::size_t>& index) {
  const auto it = index.find(id);
  if (it == index.end()) {
    return nullptr;
  }
  return &(*records)[it->second];
}

bool ParseBounds(const std::vector<std::string>& fields, std::size_t start,
                 GeoBounds* bounds) {
  return ParseDouble(fields[start], &bounds->west) &&
         ParseDouble(fields[start + 1], &bounds->south) &&
         ParseDouble(fields[start + 2], &bounds->east) &&
         ParseDouble(fields[start + 3], &bounds->north);
}

}  // namespace

PortableNauticalPackage BuildPortablePackageFixture() {
  PortableNauticalPackage package;
  package.manifest.package_id = "format-2-portable-package-fixture";
  package.manifest.profile = "s57-s52-poc";
  package.manifest.producer = "opencpn-chart-render";
  package.manifest.producer_version = "format-2";
  package.manifest.converter_id = "synthetic-s57-portable-converter";
  package.manifest.converter_version = "1";
  package.manifest.created_at = "2026-06-30T00:00:00Z";
  package.manifest.source_epoch = "edition-1-update-0";
  package.manifest.content_hash = "fnv1a64:synthetic-source-format-2";
  package.manifest.fixture_package = true;
  package.manifest.metadata["distribution_class"] =
      "redistributable_synthetic";
  package.manifest.metadata["package_boundary"] = "portable_chart_truth";
  package.manifest.metadata["presentation_profile"] = "s52";

  ChartSourceProduct& product = package.product;
  product.product_id = "format-2-normalized-source-product";
  product.metadata["package_id"] = package.manifest.package_id;
  product.metadata["source_epoch"] = package.manifest.source_epoch;
  product.metadata["source_schema"] = "portable-nautical-package-v1";

  const GeoBounds cell_bounds = Bounds(-81.86, 24.42, -81.74, 24.53);
  ChartSourceRef source;
  source.source_id = "synthetic-noaa-format2-cell";
  source.kind = ChartSourceKind::kS57Cell;
  source.role = ChartSourceRole::kPrimary;
  source.native_name = "US5FORMAT2 synthetic NOAA-style fixture";
  source.edition = "1";
  source.update = "0";
  source.content_hash = "fnv1a64:us5format2-synthetic-base";
  source.native_projection = "WGS84";
  source.native_scale_denom = 20000.0;
  source.geographic_bbox = cell_bounds;
  source.metadata["license_class"] = "redistributable_synthetic";
  source.metadata["source_family"] = "S-57";
  source.metadata["update_chain"] = "base:0";
  product.sources.push_back(std::move(source));

  auto add_object = [&](NormalizedChartObject object,
                        ProvenanceRecord provenance) {
    product.provenance_table.push_back(std::move(provenance));
    product.objects.push_back(std::move(object));
  };

  NormalizedChartObject depth_area;
  depth_area.object_id = "US5FORMAT2:DEPARE.1001";
  depth_area.object_class = "DEPARE";
  depth_area.geometry_kind = NormalizedGeometryKind::kArea;
  depth_area.geometry = AreaGeometry("geom-depare-1001",
                                     Bounds(-81.845, 24.435, -81.755, 24.515));
  depth_area.attributes = {Attr("DRVAL1", "0", "0 m"),
                           Attr("DRVAL2", "4", "4 m"),
                           Attr("QUASOU", "6", "surveyed")};
  depth_area.min_scale_denom = 10000.0;
  depth_area.max_scale_denom = 90000.0;
  depth_area.source_priority = 10;
  depth_area.provenance_refs.push_back("prov-depare-1001");
  depth_area.metadata["native_feature_id"] = "DEPARE.1001";
  depth_area.metadata["scamin"] = "60000";

  ProvenanceRecord depth_provenance;
  depth_provenance.provenance_id = "prov-depare-1001";
  depth_provenance.source_chart_id = "synthetic-noaa-format2-cell";
  depth_provenance.source_chart_edition = "1";
  depth_provenance.source_update = "0";
  depth_provenance.source_object_id = "DEPARE.1001";
  depth_provenance.source_object_class = "DEPARE";
  depth_provenance.source_geometry_hash = "fnv1a64:source-depare-1001";
  depth_provenance.generated_geometry_hash = "fnv1a64:geom-depare-1001";
  depth_provenance.conversion_stage = "portable_fixture_normalization";
  depth_provenance.transform_chain = {
      "s57:feature_decode", "s57:wgs84_coordinates",
      "portable:normalized_geometry"};
  add_object(std::move(depth_area), std::move(depth_provenance));

  NormalizedChartObject buoy;
  buoy.object_id = "US5FORMAT2:BOYLAT.2001";
  buoy.object_class = "BOYLAT";
  buoy.geometry_kind = NormalizedGeometryKind::kPoint;
  buoy.geometry = PointGeometry("geom-boylat-2001", -81.802, 24.472);
  buoy.attributes = {Attr("BOYSHP", "1", "conical"),
                     Attr("CATLAM", "1", "port hand"),
                     Attr("OBJNAM", "Format 2 fixture buoy")};
  buoy.min_scale_denom = 5000.0;
  buoy.max_scale_denom = 40000.0;
  buoy.source_priority = 20;
  buoy.provenance_refs.push_back("prov-boylat-2001");
  buoy.metadata["native_feature_id"] = "BOYLAT.2001";
  buoy.metadata["display_category"] = "standard";

  ProvenanceRecord buoy_provenance;
  buoy_provenance.provenance_id = "prov-boylat-2001";
  buoy_provenance.source_chart_id = "synthetic-noaa-format2-cell";
  buoy_provenance.source_chart_edition = "1";
  buoy_provenance.source_update = "0";
  buoy_provenance.source_object_id = "BOYLAT.2001";
  buoy_provenance.source_object_class = "BOYLAT";
  buoy_provenance.source_geometry_hash = "fnv1a64:source-boylat-2001";
  buoy_provenance.generated_geometry_hash = "fnv1a64:geom-boylat-2001";
  buoy_provenance.conversion_stage = "portable_fixture_normalization";
  buoy_provenance.transform_chain = {
      "s57:feature_decode", "s57:wgs84_coordinates",
      "portable:normalized_geometry"};
  add_object(std::move(buoy), std::move(buoy_provenance));

  PortableCoverageRecord coverage;
  coverage.coverage_id = "coverage-format2-cell";
  coverage.source_id = "synthetic-noaa-format2-cell";
  coverage.geometry = AreaGeometry("geom-coverage-format2-cell", cell_bounds);
  coverage.min_scale_denom = 5000.0;
  coverage.max_scale_denom = 90000.0;
  coverage.priority = 10;
  coverage.chart_family = "S-57";
  coverage.boundary_policy = "clip_to_cell_coverage";
  coverage.update_epoch = package.manifest.source_epoch;
  coverage.provenance_refs = {"prov-depare-1001", "prov-boylat-2001"};
  coverage.metadata["coverage_role"] = "primary_cell";
  coverage.metadata["scale_band"] = "harbor";
  package.coverage.push_back(std::move(coverage));

  package.checksums = ComputePortablePackageChecksums(package);
  return package;
}

PortablePackageChecksums ComputePortablePackageChecksums(
    const PortableNauticalPackage& package) {
  std::vector<std::string> lines;
  AppendCoreLines(package, &lines);

  PortablePackageChecksums checksums;
  checksums.record_hashes["manifest"] =
      HashLines(FilterLines(lines, {kPackageMagic, "manifest",
                                    "manifest_meta", "product",
                                    "product_meta"}));
  checksums.record_hashes["sources"] =
      HashLines(FilterLines(lines, {"source", "source_meta"}));
  checksums.record_hashes["features"] = HashLines(FilterLines(
      lines, {"feature", "feature_attr", "feature_prov", "feature_meta",
              "geometry_point", "geometry_ring_point"}));
  checksums.record_hashes["rasters"] =
      HashLines(FilterLines(lines, {"raster", "raster_prov", "raster_meta"}));
  checksums.record_hashes["coverage"] = HashLines(FilterLines(
      lines, {"coverage", "coverage_point", "coverage_ring_point",
              "coverage_prov", "coverage_meta"}));
  checksums.record_hashes["debug_artifacts"] = HashLines(FilterLines(
      lines, {"debug_artifact", "debug_artifact_prov",
              "debug_artifact_meta"}));
  checksums.record_hashes["provenance"] = HashLines(FilterLines(
      lines, {"provenance", "provenance_step", "provenance_warning"}));
  checksums.record_hashes["diagnostics"] =
      HashLines(FilterLines(lines, {"diagnostic", "diagnostic_prov"}));
  checksums.package_hash = HashLines(lines);
  return checksums;
}

std::string WritePortableNauticalPackage(
    const PortableNauticalPackage& package) {
  std::vector<std::string> lines;
  AppendCoreLines(package, &lines);

  const PortablePackageChecksums checksums =
      ComputePortablePackageChecksums(package);
  for (const auto& entry : checksums.record_hashes) {
    lines.push_back(Line({"checksum", entry.first, entry.second}));
  }
  lines.push_back(Line({"package_checksum", checksums.package_hash}));
  return JoinLines(lines);
}

bool ReadPortableNauticalPackage(const std::string& encoded,
                                 PortableNauticalPackage* package,
                                 std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;
  if (!package) {
    out.push_back(Error("portable_package_parse",
                        "ReadPortableNauticalPackage requires output storage."));
    return false;
  }

  PortableNauticalPackage parsed;
  std::map<std::string, std::size_t> source_index;
  std::map<std::string, std::size_t> object_index;
  std::map<std::string, std::size_t> raster_index;
  std::map<std::string, std::size_t> coverage_index;
  std::map<std::string, std::size_t> debug_index;
  std::map<std::string, std::size_t> provenance_index;
  std::map<std::string, std::size_t> diagnostic_index;

  bool ok = true;
  std::istringstream input(encoded);
  std::string raw_line;
  while (std::getline(input, raw_line)) {
    if (raw_line.empty()) {
      continue;
    }
    std::vector<std::string> fields;
    if (!SplitLine(raw_line, &fields) || fields.empty()) {
      out.push_back(Error("portable_package_parse",
                          "Unable to decode portable package line."));
      ok = false;
      continue;
    }

    const std::string& tag = fields[0];
    if (tag == kPackageMagic) {
      if (!RequireFieldCount(fields, 2, raw_line, &out)) {
        ok = false;
        continue;
      }
      std::uint32_t version = 0;
      if (!ParseUInt(fields[1], &version) ||
          version != kPortableNauticalPackageSchemaVersion) {
        out.push_back(Error("portable_package_schema",
                            "Unsupported portable package stream version."));
        ok = false;
      }
    } else if (tag == "manifest") {
      if (!RequireFieldCount(fields, 14, raw_line, &out)) {
        ok = false;
        continue;
      }
      bool fixture = false;
      std::uint32_t version = 0;
      if (!ParseUInt(fields[1], &version) || !ParseBool(fields[13], &fixture)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse manifest scalar fields."));
        ok = false;
        continue;
      }
      parsed.manifest.schema_version = version;
      parsed.manifest.package_id = fields[2];
      parsed.manifest.profile = fields[3];
      parsed.manifest.producer = fields[4];
      parsed.manifest.producer_version = fields[5];
      parsed.manifest.converter_id = fields[6];
      parsed.manifest.converter_version = fields[7];
      parsed.manifest.created_at = fields[8];
      parsed.manifest.source_epoch = fields[9];
      parsed.manifest.content_hash = fields[10];
      parsed.manifest.coordinate_reference = fields[11];
      parsed.manifest.units = fields[12];
      parsed.manifest.fixture_package = fixture;
    } else if (tag == "manifest_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      parsed.manifest.metadata[fields[2]] = fields[3];
    } else if (tag == "product") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      if (!ParseUInt(fields[1], &parsed.product.schema_version)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse product schema version."));
        ok = false;
        continue;
      }
      parsed.product.product_id = fields[2];
    } else if (tag == "product_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      parsed.product.metadata[fields[2]] = fields[3];
    } else if (tag == "source") {
      if (!RequireFieldCount(fields, 14, raw_line, &out)) {
        ok = false;
        continue;
      }
      ChartSourceRef source;
      if (!ParseChartSourceKind(fields[2], &source.kind) ||
          !ParseChartSourceRole(fields[3], &source.role) ||
          !ParseDouble(fields[9], &source.native_scale_denom) ||
          !ParseBounds(fields, 10, &source.geographic_bbox)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse source record."));
        ok = false;
        continue;
      }
      source.source_id = fields[1];
      source.native_name = fields[4];
      source.edition = fields[5];
      source.update = fields[6];
      source.content_hash = fields[7];
      source.native_projection = fields[8];
      source_index[source.source_id] = parsed.product.sources.size();
      parsed.product.sources.push_back(std::move(source));
    } else if (tag == "source_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      ChartSourceRef* source =
          FindById(&parsed.product.sources, fields[1], source_index);
      if (!source) {
        out.push_back(Error("portable_package_parse",
                            "source_meta references unknown source."));
        ok = false;
        continue;
      }
      source->metadata[fields[2]] = fields[3];
    } else if (tag == "feature") {
      if (!RequireFieldCount(fields, 9, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject object;
      if (!ParseGeometryKind(fields[3], &object.geometry_kind) ||
          !ParseCoordinateSpace(fields[5], &object.geometry.coordinate_space) ||
          !ParseDouble(fields[6], &object.min_scale_denom) ||
          !ParseDouble(fields[7], &object.max_scale_denom) ||
          !ParseInt(fields[8], &object.source_priority)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse feature record."));
        ok = false;
        continue;
      }
      object.object_id = fields[1];
      object.object_class = fields[2];
      object.geometry.geometry_id = fields[4];
      object_index[object.object_id] = parsed.product.objects.size();
      parsed.product.objects.push_back(std::move(object));
    } else if (tag == "geometry_point") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject* object =
          FindById(&parsed.product.objects, fields[1], object_index);
      Point2 point;
      if (!object || !ParseDouble(fields[2], &point.x) ||
          !ParseDouble(fields[3], &point.y)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse geometry point."));
        ok = false;
        continue;
      }
      object->geometry.points.push_back(point);
    } else if (tag == "geometry_ring_point") {
      if (!RequireFieldCount(fields, 5, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject* object =
          FindById(&parsed.product.objects, fields[1], object_index);
      std::uint32_t ring_index = 0;
      Point2 point;
      if (!object || !ParseUInt(fields[2], &ring_index) ||
          !ParseDouble(fields[3], &point.x) ||
          !ParseDouble(fields[4], &point.y)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse geometry ring point."));
        ok = false;
        continue;
      }
      if (object->geometry.rings.size() <= ring_index) {
        object->geometry.rings.resize(ring_index + 1);
      }
      object->geometry.rings[ring_index].push_back(point);
    } else if (tag == "feature_attr") {
      if (!RequireFieldCount(fields, 5, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject* object =
          FindById(&parsed.product.objects, fields[1], object_index);
      if (!object) {
        out.push_back(Error("portable_package_parse",
                            "feature_attr references unknown feature."));
        ok = false;
        continue;
      }
      object->attributes.push_back(Attr(fields[2], fields[3], fields[4]));
    } else if (tag == "feature_prov") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject* object =
          FindById(&parsed.product.objects, fields[1], object_index);
      if (!object) {
        out.push_back(Error("portable_package_parse",
                            "feature_prov references unknown feature."));
        ok = false;
        continue;
      }
      object->provenance_refs.push_back(fields[2]);
    } else if (tag == "feature_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      NormalizedChartObject* object =
          FindById(&parsed.product.objects, fields[1], object_index);
      if (!object) {
        out.push_back(Error("portable_package_parse",
                            "feature_meta references unknown feature."));
        ok = false;
        continue;
      }
      object->metadata[fields[2]] = fields[3];
    } else if (tag == "raster") {
      if (!RequireFieldCount(fields, 31, raw_line, &out)) {
        ok = false;
        continue;
      }
      RasterSheet sheet;
      if (!ParseRasterSheetKind(fields[2], &sheet.kind) ||
          !ParseBounds(fields, 4, &sheet.geographic_bbox) ||
          !ParseBounds(fields, 8, &sheet.chart_bounds) ||
          !ParseBounds(fields, 12, &sheet.visible_bounds) ||
          !ParseBounds(fields, 16, &sheet.collar_bounds) ||
          !ParseUInt(fields[20], &sheet.pixel_size.width) ||
          !ParseUInt(fields[21], &sheet.pixel_size.height) ||
          !ParseCoordinateSpace(fields[22], &sheet.coordinate_space) ||
          !ParseInt(fields[29], &sheet.quilt_rank) ||
          !ParseBool(fields[30], &sheet.allow_visible_outside_chart_bounds)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse raster sheet."));
        ok = false;
        continue;
      }
      sheet.sheet_id = fields[1];
      sheet.source_id = fields[3];
      sheet.content_hash = fields[23];
      sheet.color_model = fields[24];
      sheet.no_data_policy = fields[25];
      sheet.collar_policy = fields[26];
      sheet.boundary_policy = fields[27];
      sheet.quilt_policy = fields[28];
      raster_index[sheet.sheet_id] = parsed.product.raster_sheets.size();
      parsed.product.raster_sheets.push_back(std::move(sheet));
    } else if (tag == "raster_prov") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      RasterSheet* sheet =
          FindById(&parsed.product.raster_sheets, fields[1], raster_index);
      if (!sheet) {
        out.push_back(Error("portable_package_parse",
                            "raster_prov references unknown raster sheet."));
        ok = false;
        continue;
      }
      sheet->provenance_refs.push_back(fields[2]);
    } else if (tag == "raster_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      RasterSheet* sheet =
          FindById(&parsed.product.raster_sheets, fields[1], raster_index);
      if (!sheet) {
        out.push_back(Error("portable_package_parse",
                            "raster_meta references unknown raster sheet."));
        ok = false;
        continue;
      }
      sheet->metadata[fields[2]] = fields[3];
    } else if (tag == "coverage") {
      if (!RequireFieldCount(fields, 11, raw_line, &out)) {
        ok = false;
        continue;
      }
      PortableCoverageRecord coverage;
      if (!ParseCoordinateSpace(fields[4], &coverage.geometry.coordinate_space) ||
          !ParseDouble(fields[5], &coverage.min_scale_denom) ||
          !ParseDouble(fields[6], &coverage.max_scale_denom) ||
          !ParseInt(fields[7], &coverage.priority)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse coverage record."));
        ok = false;
        continue;
      }
      coverage.coverage_id = fields[1];
      coverage.source_id = fields[2];
      coverage.geometry.geometry_id = fields[3];
      coverage.chart_family = fields[8];
      coverage.boundary_policy = fields[9];
      coverage.update_epoch = fields[10];
      coverage_index[coverage.coverage_id] = parsed.coverage.size();
      parsed.coverage.push_back(std::move(coverage));
    } else if (tag == "coverage_point") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      PortableCoverageRecord* coverage =
          FindById(&parsed.coverage, fields[1], coverage_index);
      Point2 point;
      if (!coverage || !ParseDouble(fields[2], &point.x) ||
          !ParseDouble(fields[3], &point.y)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse coverage point."));
        ok = false;
        continue;
      }
      coverage->geometry.points.push_back(point);
    } else if (tag == "coverage_ring_point") {
      if (!RequireFieldCount(fields, 5, raw_line, &out)) {
        ok = false;
        continue;
      }
      PortableCoverageRecord* coverage =
          FindById(&parsed.coverage, fields[1], coverage_index);
      std::uint32_t ring_index = 0;
      Point2 point;
      if (!coverage || !ParseUInt(fields[2], &ring_index) ||
          !ParseDouble(fields[3], &point.x) ||
          !ParseDouble(fields[4], &point.y)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse coverage ring point."));
        ok = false;
        continue;
      }
      if (coverage->geometry.rings.size() <= ring_index) {
        coverage->geometry.rings.resize(ring_index + 1);
      }
      coverage->geometry.rings[ring_index].push_back(point);
    } else if (tag == "coverage_prov") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      PortableCoverageRecord* coverage =
          FindById(&parsed.coverage, fields[1], coverage_index);
      if (!coverage) {
        out.push_back(Error("portable_package_parse",
                            "coverage_prov references unknown coverage."));
        ok = false;
        continue;
      }
      coverage->provenance_refs.push_back(fields[2]);
    } else if (tag == "coverage_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      PortableCoverageRecord* coverage =
          FindById(&parsed.coverage, fields[1], coverage_index);
      if (!coverage) {
        out.push_back(Error("portable_package_parse",
                            "coverage_meta references unknown coverage."));
        ok = false;
        continue;
      }
      coverage->metadata[fields[2]] = fields[3];
    } else if (tag == "debug_artifact") {
      if (!RequireFieldCount(fields, 7, raw_line, &out)) {
        ok = false;
        continue;
      }
      DebugArtifact artifact;
      if (!ParseDebugArtifactKind(fields[2], &artifact.kind)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse debug artifact kind."));
        ok = false;
        continue;
      }
      artifact.artifact_id = fields[1];
      artifact.source_id = fields[3];
      artifact.content_hash = fields[4];
      artifact.media_type = fields[5];
      artifact.producer = fields[6];
      debug_index[artifact.artifact_id] = parsed.product.debug_artifacts.size();
      parsed.product.debug_artifacts.push_back(std::move(artifact));
    } else if (tag == "debug_artifact_prov") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      DebugArtifact* artifact = FindById(&parsed.product.debug_artifacts,
                                         fields[1], debug_index);
      if (!artifact) {
        out.push_back(Error(
            "portable_package_parse",
            "debug_artifact_prov references unknown debug artifact."));
        ok = false;
        continue;
      }
      artifact->provenance_refs.push_back(fields[2]);
    } else if (tag == "debug_artifact_meta") {
      if (!RequireFieldCount(fields, 4, raw_line, &out)) {
        ok = false;
        continue;
      }
      DebugArtifact* artifact = FindById(&parsed.product.debug_artifacts,
                                         fields[1], debug_index);
      if (!artifact) {
        out.push_back(Error(
            "portable_package_parse",
            "debug_artifact_meta references unknown debug artifact."));
        ok = false;
        continue;
      }
      artifact->metadata[fields[2]] = fields[3];
    } else if (tag == "provenance") {
      if (!RequireFieldCount(fields, 14, raw_line, &out)) {
        ok = false;
        continue;
      }
      ProvenanceRecord provenance;
      provenance.provenance_id = fields[1];
      provenance.source_chart_id = fields[2];
      provenance.source_chart_edition = fields[3];
      provenance.source_update = fields[4];
      provenance.source_object_id = fields[5];
      provenance.source_object_class = fields[6];
      provenance.source_geometry_hash = fields[7];
      provenance.generated_geometry_hash = fields[8];
      provenance.target_geometry_hash = fields[9];
      provenance.s52_rule_id = fields[10];
      provenance.render_command_id = fields[11];
      provenance.conversion_stage = fields[12];
      provenance.quilt_decision_id = fields[13];
      provenance_index[provenance.provenance_id] =
          parsed.product.provenance_table.size();
      parsed.product.provenance_table.push_back(std::move(provenance));
    } else if (tag == "provenance_step") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      ProvenanceRecord* provenance = FindById(
          &parsed.product.provenance_table, fields[1], provenance_index);
      if (!provenance) {
        out.push_back(Error("portable_package_parse",
                            "provenance_step references unknown provenance."));
        ok = false;
        continue;
      }
      provenance->transform_chain.push_back(fields[2]);
    } else if (tag == "provenance_warning") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      ProvenanceRecord* provenance = FindById(
          &parsed.product.provenance_table, fields[1], provenance_index);
      if (!provenance) {
        out.push_back(Error(
            "portable_package_parse",
            "provenance_warning references unknown provenance."));
        ok = false;
        continue;
      }
      provenance->warnings.push_back(fields[2]);
    } else if (tag == "diagnostic") {
      if (!RequireFieldCount(fields, 5, raw_line, &out)) {
        ok = false;
        continue;
      }
      Diagnostic diagnostic;
      if (!ParseSeverity(fields[1], &diagnostic.severity)) {
        out.push_back(Error("portable_package_parse",
                            "Unable to parse diagnostic severity."));
        ok = false;
        continue;
      }
      diagnostic.code = fields[2];
      diagnostic.message = fields[3];
      diagnostic.suggested_action = fields[4];
      diagnostic_index[diagnostic.code] = parsed.product.diagnostics.size();
      parsed.product.diagnostics.push_back(std::move(diagnostic));
    } else if (tag == "diagnostic_prov") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      Diagnostic* diagnostic =
          FindById(&parsed.product.diagnostics, fields[1], diagnostic_index);
      if (!diagnostic) {
        out.push_back(Error("portable_package_parse",
                            "diagnostic_prov references unknown diagnostic."));
        ok = false;
        continue;
      }
      diagnostic->provenance_refs.push_back(fields[2]);
    } else if (tag == "checksum") {
      if (!RequireFieldCount(fields, 3, raw_line, &out)) {
        ok = false;
        continue;
      }
      parsed.checksums.record_hashes[fields[1]] = fields[2];
    } else if (tag == "package_checksum") {
      if (!RequireFieldCount(fields, 2, raw_line, &out)) {
        ok = false;
        continue;
      }
      parsed.checksums.package_hash = fields[1];
    } else {
      out.push_back(Error("portable_package_parse",
                          "Unknown portable package record: " + tag));
      ok = false;
    }
  }

  if (ok) {
    *package = std::move(parsed);
  }
  return ok;
}

bool ValidatePortableNauticalPackage(
    const PortableNauticalPackage& package,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateChartSourceProduct(package.product, &out);
  if (package.manifest.schema_version !=
      kPortableNauticalPackageSchemaVersion) {
    out.push_back(Error("portable_package_schema",
                        "Unsupported portable package schema version."));
    ok = false;
  }
  if (package.manifest.package_id.empty() || package.manifest.profile.empty() ||
      package.manifest.converter_id.empty() ||
      package.manifest.converter_version.empty() ||
      package.manifest.source_epoch.empty() ||
      package.manifest.content_hash.empty()) {
    out.push_back(Error("portable_package_manifest",
                        "Portable package manifest is missing identity, "
                        "profile, converter, source epoch, or content hash."));
    ok = false;
  }
  if (package.manifest.coordinate_reference.empty() ||
      package.manifest.units.empty()) {
    out.push_back(Error("portable_package_manifest_coordinates",
                        "Portable package must declare coordinate reference "
                        "and units."));
    ok = false;
  }
  if (package.product.sources.empty() || package.product.objects.empty() ||
      package.product.provenance_table.empty() || package.coverage.empty()) {
    out.push_back(Error("portable_package_records",
                        "Portable package must include sources, features, "
                        "coverage, and provenance."));
    ok = false;
  }

  CheckMetadataBoundary(package.manifest.metadata, "manifest", &ok, &out);
  CheckMetadataBoundary(package.product.metadata, "product", &ok, &out);

  std::set<std::string> source_ids;
  for (const ChartSourceRef& source : package.product.sources) {
    source_ids.insert(source.source_id);
    if (source.source_id.empty() || source.native_name.empty() ||
        source.edition.empty() || source.content_hash.empty() ||
        source.native_projection.empty() ||
        source.native_scale_denom <= 0.0 ||
        !ValidBounds(source.geographic_bbox)) {
      out.push_back(Error("portable_package_source_identity",
                          "Source record is missing identity, hash, scale, "
                          "projection, or geographic bounds."));
      ok = false;
    }
    CheckMetadataBoundary(source.metadata, "source:" + source.source_id, &ok,
                          &out);
  }

  std::set<std::string> provenance_ids;
  for (const ProvenanceRecord& provenance : package.product.provenance_table) {
    provenance_ids.insert(provenance.provenance_id);
    if (provenance.provenance_id.empty() ||
        provenance.source_chart_id.empty() ||
        provenance.source_object_id.empty() ||
        provenance.source_object_class.empty() ||
        provenance.source_geometry_hash.empty() ||
        provenance.generated_geometry_hash.empty() ||
        provenance.conversion_stage.empty() ||
        provenance.transform_chain.empty()) {
      out.push_back(Error("portable_package_provenance",
                          "Provenance record is missing source object, "
                          "geometry hash, converter stage, or transform chain.",
                          {provenance.provenance_id}));
      ok = false;
    }
    if (!provenance.source_chart_id.empty() &&
        source_ids.count(provenance.source_chart_id) == 0) {
      out.push_back(Error("portable_package_provenance_source",
                          "Provenance references an unknown source chart.",
                          {provenance.provenance_id}));
      ok = false;
    }
  }

  for (const NormalizedChartObject& object : package.product.objects) {
    if (object.object_id.empty() || object.object_class.empty() ||
        object.geometry_kind == NormalizedGeometryKind::kUnknown ||
        object.geometry.geometry_id.empty() ||
        !HasGeometryCoordinates(object.geometry) || object.attributes.empty() ||
        object.min_scale_denom <= 0.0 || object.max_scale_denom <= 0.0) {
      out.push_back(Error("portable_package_feature_identity",
                          "Feature is missing source id, class, normalized "
                          "geometry, attributes, or scale metadata.",
                          object.provenance_refs));
      ok = false;
    }
    if (object.min_scale_denom > object.max_scale_denom) {
      out.push_back(Error("portable_package_feature_scale",
                          "Feature scale range is inverted.",
                          object.provenance_refs));
      ok = false;
    }
    const GeoBounds bounds = GeometryBounds(object.geometry);
    if (!ValidBounds(bounds) &&
        object.geometry_kind != NormalizedGeometryKind::kPoint) {
      out.push_back(Error("portable_package_feature_bounds",
                          "Feature geometry bounds are invalid.",
                          object.provenance_refs));
      ok = false;
    }
    for (const ChartAttribute& attr : object.attributes) {
      if (attr.acronym.empty() || attr.value.empty()) {
        out.push_back(Error("portable_package_feature_attribute",
                            "Feature attribute is missing acronym or value.",
                            object.provenance_refs));
        ok = false;
      }
    }
    for (const std::string& provenance_ref : object.provenance_refs) {
      if (provenance_ids.count(provenance_ref) == 0) {
        out.push_back(Error("portable_package_feature_provenance",
                            "Feature references unknown provenance.",
                            {provenance_ref}));
        ok = false;
      }
    }
    CheckMetadataBoundary(object.metadata, "feature:" + object.object_id, &ok,
                          &out);
  }

  for (const RasterSheet& sheet : package.product.raster_sheets) {
    CheckMetadataBoundary(sheet.metadata, "raster:" + sheet.sheet_id, &ok,
                          &out);
  }
  for (const DebugArtifact& artifact : package.product.debug_artifacts) {
    CheckMetadataBoundary(artifact.metadata,
                          "debug_artifact:" + artifact.artifact_id, &ok,
                          &out);
  }

  for (const PortableCoverageRecord& coverage : package.coverage) {
    if (coverage.coverage_id.empty() || coverage.source_id.empty() ||
        source_ids.count(coverage.source_id) == 0 ||
        coverage.geometry.geometry_id.empty() ||
        !HasGeometryCoordinates(coverage.geometry) ||
        coverage.min_scale_denom <= 0.0 ||
        coverage.max_scale_denom <= 0.0 ||
        coverage.min_scale_denom > coverage.max_scale_denom ||
        coverage.chart_family.empty() || coverage.boundary_policy.empty() ||
        coverage.update_epoch.empty() || coverage.provenance_refs.empty()) {
      out.push_back(Error("portable_package_coverage",
                          "Coverage is missing source, normalized geometry, "
                          "scale range, policy, epoch, or provenance.",
                          coverage.provenance_refs));
      ok = false;
    }
    const GeoBounds bounds = GeometryBounds(coverage.geometry);
    if (!ValidBounds(bounds)) {
      out.push_back(Error("portable_package_coverage_bounds",
                          "Coverage geometry bounds are invalid.",
                          coverage.provenance_refs));
      ok = false;
    }
    for (const std::string& provenance_ref : coverage.provenance_refs) {
      if (provenance_ids.count(provenance_ref) == 0) {
        out.push_back(Error("portable_package_coverage_provenance",
                            "Coverage references unknown provenance.",
                            {provenance_ref}));
        ok = false;
      }
    }
    CheckMetadataBoundary(coverage.metadata,
                          "coverage:" + coverage.coverage_id, &ok, &out);
  }

  const PortablePackageChecksums expected =
      ComputePortablePackageChecksums(package);
  if (package.checksums.package_hash.empty()) {
    out.push_back(Error("portable_package_checksum",
                        "Portable package is missing package checksum."));
    ok = false;
  } else if (package.checksums.package_hash != expected.package_hash) {
    out.push_back(Error("portable_package_checksum",
                        "Portable package checksum does not match records."));
    ok = false;
  }
  for (const auto& entry : expected.record_hashes) {
    const auto found = package.checksums.record_hashes.find(entry.first);
    if (found == package.checksums.record_hashes.end() ||
        found->second != entry.second) {
      out.push_back(Error("portable_package_record_checksum",
                          "Portable package record checksum mismatch for " +
                              entry.first + "."));
      ok = false;
    }
  }

  return ok;
}

bool RoundTripPortablePackageFixture(PortableNauticalPackage* round_tripped,
                                     std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  const PortableNauticalPackage fixture = BuildPortablePackageFixture();
  std::vector<Diagnostic> validation;
  if (!ValidatePortableNauticalPackage(fixture, &validation)) {
    out.insert(out.end(), validation.begin(), validation.end());
    return false;
  }

  const std::string encoded = WritePortableNauticalPackage(fixture);
  PortableNauticalPackage decoded;
  if (!ReadPortableNauticalPackage(encoded, &decoded, &out)) {
    return false;
  }
  validation.clear();
  if (!ValidatePortableNauticalPackage(decoded, &validation)) {
    out.insert(out.end(), validation.begin(), validation.end());
    return false;
  }

  const std::string encoded_again = WritePortableNauticalPackage(decoded);
  if (encoded != encoded_again) {
    out.push_back(Error("portable_package_roundtrip",
                        "Portable package round trip is not byte-stable."));
    return false;
  }

  if (round_tripped) {
    *round_tripped = std::move(decoded);
  }
  return true;
}

}  // namespace ocpn::render
