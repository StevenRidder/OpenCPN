// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart_source.hpp"

#include <set>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic Error(std::string code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Reject the chart-source product before S-52 command conversion.";
  return diagnostic;
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
