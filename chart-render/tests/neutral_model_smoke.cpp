// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"
#include "depth_tessellation.hpp"
#include "nautical_render_model.hpp"
#include "s52/s52_command_builder.hpp"
#include "vsg/vsg_backend.hpp"

#include <iostream>
#include <set>

namespace {

void AddTypes(const ocpn::render::NauticalRenderModel& model,
              std::set<ocpn::render::NauticalPrimitiveType>* types) {
  for (const ocpn::render::NauticalLayer& layer : model.layers) {
    for (const ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      types->insert(primitive.type);
    }
  }
}

bool Validate(const ocpn::render::NauticalRenderModel& model) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateNauticalRenderModel(model, &diagnostics)) {
    return true;
  }
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "neutral-model-smoke";
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
  const ocpn::render::RenderScene fixture_scene =
      builder.BuildFixtureScene(view, display);
  const ocpn::render::NauticalRenderModel fixture_model =
      ocpn::render::BuildNauticalRenderModel(fixture_scene);
  if (!Validate(fixture_model)) {
    return 1;
  }

  const ocpn::render::RenderScene depth_scene =
      ocpn::render::depth::BuildDepthTessellationFixture(view, display);
  const ocpn::render::NauticalRenderModel depth_model =
      ocpn::render::BuildNauticalRenderModel(depth_scene);
  if (!Validate(depth_model)) {
    return 1;
  }

  const ocpn::render::ChartSourceProduct quilting_product =
      ocpn::render::depth::BuildRasterQuiltingFixtureProduct();
  const ocpn::render::RenderScene quilting_scene =
      builder.BuildSceneFromChartSource(quilting_product, view, display);
  const ocpn::render::NauticalRenderModel quilting_model =
      ocpn::render::BuildNauticalRenderModel(quilting_scene);
  if (!Validate(quilting_model)) {
    return 1;
  }

  std::set<ocpn::render::NauticalPrimitiveType> types;
  AddTypes(fixture_model, &types);
  AddTypes(depth_model, &types);
  AddTypes(quilting_model, &types);

  for (const ocpn::render::NauticalPrimitiveType required :
       {ocpn::render::NauticalPrimitiveType::kAreaFill,
        ocpn::render::NauticalPrimitiveType::kLineStroke,
        ocpn::render::NauticalPrimitiveType::kSymbolInstance,
        ocpn::render::NauticalPrimitiveType::kTextLabel,
        ocpn::render::NauticalPrimitiveType::kSounding,
        ocpn::render::NauticalPrimitiveType::kRasterPatch,
        ocpn::render::NauticalPrimitiveType::kContourLine}) {
    if (types.count(required) == 0) {
      std::cerr << "Missing neutral primitive type "
                << ocpn::render::ToString(required) << "\n";
      return 1;
    }
  }

  ocpn::render::vsg::VsgBackend backend;
  ocpn::render::RenderTarget target;
  target.kind = ocpn::render::RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.target_id = "neutral-model-smoke";
  const ocpn::render::RenderResult result =
      backend.RenderModel(quilting_model, target);
  if (result.pixels.rgba8.size() != 256U * 256U * 4U ||
      result.diagnostics.empty()) {
    std::cerr << "VSG placeholder did not consume neutral model boundary\n";
    return 1;
  }

  return 0;
}
