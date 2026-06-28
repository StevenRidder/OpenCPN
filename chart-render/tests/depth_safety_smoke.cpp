// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_safety.hpp"

#include <iostream>

int main() {
  ocpn::render::RenderView view;
  view.view_id = "depth-safety-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.scale_denom = 12000.0;
  view.pixel_size = {256, 256};

  ocpn::render::DisplayState display;
  display.shallow_contour_m = 2.0;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  display.deep_contour_m = 20.0;

  ocpn::render::RenderScene scene =
      ocpn::render::depth::BuildSafetyDepthFixture(view, display);
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::depth::ValidateSafetyDepthScene(scene, &diagnostics)) {
    std::cerr << "Depth safety smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  return 0;
}
