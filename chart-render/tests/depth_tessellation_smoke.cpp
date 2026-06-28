// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_tessellation.hpp"

#include <iostream>

int main() {
  ocpn::render::RenderView view;
  view.view_id = "depth-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.pixel_size = {256, 256};

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::RenderScene scene =
      ocpn::render::depth::BuildDepthTessellationFixture(view, display);
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::depth::ValidateDepthTessellationScene(scene,
                                                           &diagnostics)) {
    std::cerr << "Depth tessellation smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  return 0;
}
