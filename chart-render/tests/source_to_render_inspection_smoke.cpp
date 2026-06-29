// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_debug_app.hpp"
#include "depth_quilting.hpp"
#include "gpu_artifact_cache_contract.hpp"
#include "s52/s52_command_builder.hpp"
#include "source_to_render_inspection.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

bool HasProjectionStep(
    const ocpn::render::SourceToRenderInspectionRow& row,
    const std::string& step) {
  for (const std::string& actual : row.converter.projection_transform) {
    if (actual == step) return true;
  }
  return false;
}

bool HasArtifactKind(
    const ocpn::render::SourceToRenderInspectionReport& report,
    const std::string& kind) {
  for (const ocpn::render::SourceToRenderInspectionRow& row : report.rows) {
    for (const ocpn::render::InspectionArtifactHandle& artifact :
         row.artifacts) {
      if (artifact.artifact_kind == kind) return true;
    }
  }
  return false;
}

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "source-to-render-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.82, 24.45, -81.78, 24.49};
  view.center = {-81.80, 24.47};
  view.scale_denom = 12000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.display_category = ocpn::render::DisplayCategory::kStandard;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  const ocpn::render::chart1::DebugReport debug_report =
      ocpn::render::chart1::BuildDebugReport(view, display);
  if (!debug_report.ok || !debug_report.source_to_render.ok) {
    std::cerr << "Chart 1 debug report did not expose a valid source-to-render "
                 "inspection contract\n";
    PrintDiagnostics(debug_report.diagnostics);
    PrintDiagnostics(debug_report.source_to_render.diagnostics);
    return 1;
  }

  const ocpn::render::SourceToRenderInspectionReport& report =
      debug_report.source_to_render;
  if (report.rows.size() != debug_report.objects.size() ||
      report.cache_manifest_id.empty() || report.scene_artifacts.empty()) {
    std::cerr << "Inspection report did not retain model/cache scene coverage\n";
    return 1;
  }

  const ocpn::render::SourceToRenderInspectionRow* area =
      ocpn::render::FindInspectionsBySourceObjectId(report, "DEPARE.1")
          .empty()
          ? nullptr
          : ocpn::render::FindInspectionsBySourceObjectId(report, "DEPARE.1")
                .front();
  const ocpn::render::SourceToRenderInspectionRow* buoy =
      ocpn::render::FindInspectionByPrimitiveId(
          report, "prim-BOYLAT.1-navaid_symbol");
  if (!area || !buoy) {
    std::cerr << "Inspection lookup could not find area or buoy rows\n";
    return 1;
  }

  if (area->source.source_chart_id != "chart-1-fixture" ||
      area->source.source_object_class != "DEPARE" ||
      area->converter.source_product_id != "chart-1-debug-source" ||
      area->converter.normalized_feature_id != "DEPARE.1" ||
      area->converter.normalized_geometry.geometry_id != "geom-depth-area" ||
      !HasProjectionStep(*area, "projection:web_mercator_tile") ||
      area->presentation.presentation_rule_id != "s52:DEPARE:depth_area" ||
      area->presentation.presentation_layer != "area" ||
      area->cache.tile_cache_key.empty() ||
      area->artifacts.empty() ||
      area->backend.final_draw_item_id.empty() ||
      area->backend.final_gpu_asset_id.empty() ||
      area->query.object_query_id.empty() ||
      area->query.pixel_query_id.empty()) {
    std::cerr << "Area source-to-render inspection row is incomplete\n";
    return 1;
  }

  if (buoy->tier.semantic_tier != "tier1_official_chart" ||
      buoy->tier.semantic_owner != "presentation_compiler" ||
      buoy->tier.source_standard != "s52-compatible" ||
      buoy->tier.wrong_symbol_owner !=
          "converter_presentation_or_backend" ||
      buoy->backend.backend_contract != "draw_only" ||
      buoy->backend.backend_name != "vulkan-scenegraph-placeholder") {
    std::cerr << "Tier 1 provenance or backend handoff owner is wrong\n";
    return 1;
  }

  ocpn::render::SourceToRenderInspectionReport invalid = report;
  invalid.rows.front().tier.semantic_owner = "backend";
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateSourceToRenderInspectionReport(invalid,
                                                           &diagnostics) ||
      !HasDiagnostic(diagnostics, "source_to_render_semantic_owner")) {
    std::cerr << "Inspection contract accepted backend-owned chart semantics\n";
    return 1;
  }

  invalid = report;
  invalid.rows.front().artifacts.clear();
  diagnostics.clear();
  if (ocpn::render::ValidateSourceToRenderInspectionReport(invalid,
                                                           &diagnostics) ||
      !HasDiagnostic(diagnostics, "source_to_render_missing_artifact")) {
    std::cerr << "Inspection contract accepted a row without cache artifacts\n";
    return 1;
  }

  invalid = report;
  invalid.rows.front().tier.semantic_tier = "tier2_helm_overlay";
  invalid.rows.front().tier.wrong_symbol_owner = "presentation_compiler";
  diagnostics.clear();
  if (ocpn::render::ValidateSourceToRenderInspectionReport(invalid,
                                                           &diagnostics) ||
      !HasDiagnostic(diagnostics, "source_to_render_tier_owner")) {
    std::cerr << "Inspection contract accepted wrong owner for Tier 2 overlay\n";
    return 1;
  }

  ocpn::render::s52::S52CommandBuilder builder;
  const ocpn::render::ChartSourceProduct quilting_product =
      ocpn::render::depth::BuildRasterQuiltingFixtureProduct();
  const ocpn::render::RenderScene quilting_scene =
      builder.BuildSceneFromChartSource(quilting_product, view, display);
  const ocpn::render::NauticalRenderModel quilting_model =
      ocpn::render::BuildNauticalRenderModel(quilting_scene);
  const ocpn::render::GpuArtifactCacheManifest quilting_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(quilting_model);

  ocpn::render::SourceToRenderInspectionOptions options;
  options.backend_name = "vulkan-scenegraph-placeholder";
  options.target = ocpn::render::chart1::Chart1DebugTarget(view.pixel_size);
  const ocpn::render::SourceToRenderInspectionReport quilting_report =
      ocpn::render::BuildSourceToRenderInspectionReport(
          quilting_model, quilting_manifest, options);
  if (!quilting_report.ok || !HasArtifactKind(quilting_report, "raster_texture")) {
    std::cerr << "Raster quilting inspection did not retain raster source/cache "
                 "trace\n";
    PrintDiagnostics(quilting_report.diagnostics);
    return 1;
  }

  std::cout << "ok source-to-render-inspection: " << report.rows.size()
            << " chart rows, " << report.scene_artifacts.size()
            << " scene artifacts, " << quilting_report.rows.size()
            << " raster rows\n";
  return 0;
}
