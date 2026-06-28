// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_debug_app.hpp"

#include <iostream>

namespace {

bool HasTransform(const ocpn::render::chart1::ObjectInspection& inspection,
                  const std::string& expected) {
  for (const std::string& step : inspection.projection_transform) {
    if (step == expected) {
      return true;
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

void PrintObjects(
    const std::vector<ocpn::render::chart1::ObjectInspection>& objects) {
  for (const ocpn::render::chart1::ObjectInspection& object : objects) {
    std::cerr << object.case_id << " source=" << object.source_feature_id
              << " primitive=" << object.backend_primitive_id
              << " rule=" << object.s52_rule_id
              << " layer=" << object.layer_id << "\n";
  }
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "chart-1-debug-smoke";
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

  ocpn::render::chart1::DebugReport report =
      ocpn::render::chart1::BuildDebugReport(view, display);

  if (!report.ok || report.objects.size() != 3 || report.layers.empty()) {
    std::cerr << "Chart 1 debug app report is incomplete\n";
    PrintDiagnostics(report.diagnostics);
    return 1;
  }

  const ocpn::render::chart1::ObjectInspection* area =
      ocpn::render::chart1::FindObjectBySourceFeatureId(report, "DEPARE.1");
  const ocpn::render::chart1::ObjectInspection* contour =
      ocpn::render::chart1::FindObjectByBackendPrimitiveId(
          report, "prim-DEPCNT.1-safety_contour");
  const ocpn::render::chart1::ObjectInspection* buoy =
      ocpn::render::chart1::FindObjectBySourceFeatureId(report, "BOYLAT.1");

  if (!area || !contour || !buoy) {
    std::cerr << "Missing point, line, or area inspection row\n";
    PrintObjects(report.objects);
    return 1;
  }

  if (area->source_chart_id != "chart-1-fixture" ||
      area->source_object_class != "DEPARE" ||
      area->s52_rule_id != "s52:DEPARE:depth_area" ||
      area->normalized_geometry.geometry_id != "geom-depth-area" ||
      area->original_geometry.source_geometry_hash !=
          "source:geom-depth-area" ||
      area->cache.tile_cache_key.empty() ||
      !HasTransform(*area, "projection:web_mercator_tile")) {
    std::cerr << "Area source-to-render trace is incomplete\n";
    return 1;
  }

  if (contour->source_object_class != "DEPCNT" ||
      contour->backend_primitive_id != "prim-DEPCNT.1-safety_contour" ||
      contour->s52_rule_id != "s52:DEPCNT:safety_contour" ||
      contour->presentation_layer != "line" ||
      contour->normalized_geometry.geometry_id != "geom-depth-contour") {
    std::cerr << "Contour layer or primitive trace is incomplete\n";
    return 1;
  }

  if (buoy->source_object_class != "BOYLAT" ||
      buoy->backend_primitive_id != "prim-BOYLAT.1-navaid_symbol" ||
      buoy->s52_rule_id != "s52:BOYLAT:navaid_symbol" ||
      buoy->normalized_geometry.geometry_id != "geom-buoy" ||
      buoy->display_category != "standard" ||
      buoy->final_gpu_asset_id.find("gpu:vulkan-scenegraph-placeholder") !=
          0 ||
      buoy->final_web_asset_id.find("web:vulkan-scenegraph-placeholder") !=
          0) {
    std::cerr << "Buoy GPU/web asset trace is incomplete\n";
    return 1;
  }

  if (ocpn::render::chart1::FindObjectsInLayer(report, "s52-areas").size() !=
          1 ||
      ocpn::render::chart1::FindObjectsInLayer(report, "s52-lines").size() !=
          1 ||
      ocpn::render::chart1::FindObjectsInLayer(report, "s52-symbols").size() !=
          1) {
    std::cerr << "Layer inspection did not expose the compiled Chart 1 layers\n";
    return 1;
  }

  return 0;
}
