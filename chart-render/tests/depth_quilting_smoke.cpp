// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"
#include "s52/s52_command_builder.hpp"

#include <iostream>

int main() {
  ocpn::render::ChartSourceProduct product =
      ocpn::render::depth::BuildRasterQuiltingFixtureProduct();

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::depth::ValidateRasterQuiltingProduct(product,
                                                          &diagnostics)) {
    std::cerr << "Raster quilting product smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  ocpn::render::RenderView view;
  view.view_id = "depth-quilting-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::s52::S52CommandBuilder builder;
  ocpn::render::RenderScene scene =
      builder.BuildSceneFromChartSource(product, view, display);

  diagnostics.clear();
  if (!ocpn::render::depth::ValidateRasterQuiltingScene(scene,
                                                        &diagnostics)) {
    std::cerr << "Raster quilting scene smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  return 0;
}
