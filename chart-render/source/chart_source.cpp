// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart_source.hpp"

#include <cmath>
#include <set>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Reject the chart-source product before S-52 command conversion.";
  return diagnostic;
}

Diagnostic Error(std::string code, std::string message) {
  return MakeDiagnostic(DiagnosticSeverity::kError, std::move(code),
                        std::move(message));
}

Diagnostic Warning(std::string code, std::string message) {
  return MakeDiagnostic(DiagnosticSeverity::kWarning, std::move(code),
                        std::move(message));
}

bool IsFinite(double value) {
  return std::isfinite(value);
}

bool ValidBounds(const GeoBounds& bounds) {
  return IsFinite(bounds.west) && IsFinite(bounds.south) &&
         IsFinite(bounds.east) && IsFinite(bounds.north) &&
         bounds.west < bounds.east && bounds.south < bounds.north;
}

bool ContainsBounds(const GeoBounds& outer, const GeoBounds& inner) {
  return outer.west <= inner.west && outer.south <= inner.south &&
         outer.east >= inner.east && outer.north >= inner.north;
}

}  // namespace

const char* ToString(ChartSourceKind kind) {
  switch (kind) {
    case ChartSourceKind::kS57Cell:
      return "s57_cell";
    case ChartSourceKind::kSencCell:
      return "senc_cell";
    case ChartSourceKind::kRasterChart:
      return "raster_chart";
    case ChartSourceKind::kMbtilesPackage:
      return "mbtiles_package";
    case ChartSourceKind::kPmtilesPackage:
      return "pmtiles_package";
    case ChartSourceKind::kS101Dataset:
      return "s101_dataset";
    case ChartSourceKind::kDebugFixture:
      return "debug_fixture";
  }
  return "unknown";
}

const char* ToString(RasterSheetKind kind) {
  switch (kind) {
    case RasterSheetKind::kChartImage:
      return "chart_image";
    case RasterSheetKind::kCollarMask:
      return "collar_mask";
    case RasterSheetKind::kNoDataMask:
      return "no_data_mask";
    case RasterSheetKind::kCoverageMask:
      return "coverage_mask";
    case RasterSheetKind::kDebugOverlay:
      return "debug_overlay";
  }
  return "unknown";
}

const char* ToString(DebugArtifactKind kind) {
  switch (kind) {
    case DebugArtifactKind::kSourceMetadata:
      return "source_metadata";
    case DebugArtifactKind::kFeatureDump:
      return "feature_dump";
    case DebugArtifactKind::kGeometryTrace:
      return "geometry_trace";
    case DebugArtifactKind::kRuleTrace:
      return "rule_trace";
    case DebugArtifactKind::kRasterFootprint:
      return "raster_footprint";
    case DebugArtifactKind::kInterchangePackage:
      return "interchange_package";
  }
  return "unknown";
}

bool ValidateChartSourceProduct(const ChartSourceProduct& product,
                                std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (product.schema_version != kChartSourceSchemaVersion) {
    out.push_back(Error("chart_source_schema_version",
                        "Unsupported chart-source product schema version."));
    ok = false;
  }
  if (product.product_id.empty()) {
    out.push_back(Error("chart_source_product_id",
                        "Chart-source product is missing product_id."));
    ok = false;
  }

  std::set<std::string> source_ids;
  for (const ChartSourceRef& source : product.sources) {
    if (source.source_id.empty()) {
      out.push_back(Error("chart_source_source_id",
                          "Chart-source input is missing source_id."));
      ok = false;
      continue;
    }
    source_ids.insert(source.source_id);
  }

  for (const NormalizedChartObject& object : product.objects) {
    if (object.object_id.empty()) {
      out.push_back(Error("chart_source_object_id",
                          "Normalized chart object is missing object_id."));
      ok = false;
    }
    if (object.provenance_refs.empty()) {
      out.push_back(Error("chart_source_object_provenance",
                          "Normalized chart object has no provenance_refs."));
      ok = false;
    }
  }

  for (const RasterSheet& sheet : product.raster_sheets) {
    if (sheet.sheet_id.empty()) {
      out.push_back(Error("chart_source_raster_sheet_id",
                          "Raster sheet is missing sheet_id."));
      ok = false;
    }
    if (!sheet.source_id.empty() && source_ids.count(sheet.source_id) == 0) {
      out.push_back(Error("chart_source_raster_source",
                          "Raster sheet references an unknown source_id."));
      ok = false;
    }
    if (!ValidBounds(sheet.geographic_bbox) || !ValidBounds(sheet.chart_bounds) ||
        !ValidBounds(sheet.visible_bounds) || !ValidBounds(sheet.collar_bounds)) {
      out.push_back(Error("chart_source_raster_bounds",
                          "Raster sheet is missing valid geographic, chart, "
                          "visible, or collar bounds."));
      ok = false;
    }
    if (sheet.pixel_size.width == 0 || sheet.pixel_size.height == 0) {
      out.push_back(Error("chart_source_raster_pixels",
                          "Raster sheet is missing pixel_size."));
      ok = false;
    }
    if (sheet.no_data_policy.empty() || sheet.collar_policy.empty() ||
        sheet.boundary_policy.empty() || sheet.quilt_policy.empty()) {
      out.push_back(Error("chart_source_raster_policy",
                          "Raster sheet is missing no-data, collar, boundary, "
                          "or quilt policy."));
      ok = false;
    }
    if (sheet.allow_visible_outside_chart_bounds &&
        ContainsBounds(sheet.chart_bounds, sheet.visible_bounds)) {
      out.push_back(Warning(
          "chart_source_raster_visible_bounds",
          "Raster sheet allows visible data outside chart bounds, but "
          "visible_bounds are contained by chart_bounds."));
    }
  }

  for (const DebugArtifact& artifact : product.debug_artifacts) {
    if (artifact.artifact_id.empty()) {
      out.push_back(Error("chart_source_debug_artifact_id",
                          "Debug artifact is missing artifact_id."));
      ok = false;
    }
  }

  return ok;
}

}  // namespace ocpn::render
