// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_conformance.hpp"

#include <iostream>

int main() {
  ocpn::render::RenderView view;
  view.view_id = "chart-1-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.82, 24.45, -81.78, 24.49};
  view.center = {-81.80, 24.47};
  view.scale_denom = 12000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::chart1::ConformanceScene scene =
      ocpn::render::chart1::BuildConformanceScene(view, display);
  if (!scene.ok || scene.case_results.size() != 3) {
    std::cerr << "Chart 1 conformance smoke failed with "
              << scene.diagnostics.size() << " diagnostics\n";
    for (const ocpn::render::Diagnostic& diagnostic : scene.diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  return 0;
}
