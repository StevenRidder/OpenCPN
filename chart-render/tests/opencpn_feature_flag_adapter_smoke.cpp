// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "opencpn_feature_flag_adapter.hpp"
#include "s52/s52_command_builder.hpp"

#include <iostream>
#include <string>

namespace {

bool ExpectRoute(const char* case_name,
                 const ocpn::render::OpenCpnFeatureFlagDecision& decision,
                 ocpn::render::OpenCpnRendererRoute expected_route,
                 const std::string& expected_reason) {
  if (decision.route != expected_route ||
      decision.reason_code != expected_reason ||
      decision.diagnostics.empty()) {
    std::cerr << case_name << " failed: route="
              << ocpn::render::ToString(decision.route)
              << " reason=" << decision.reason_code
              << " diagnostics=" << decision.diagnostics.size() << "\n";
    return false;
  }
  return true;
}

ocpn::render::RenderView BuildView() {
  ocpn::render::RenderView view;
  view.view_id = "opencpn-adapt-smoke";
  view.projection = ocpn::render::Projection::kMercator;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {512, 512};
  view.device_pixel_ratio = 2.0;
  return view;
}

ocpn::render::DisplayState BuildDisplay() {
  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  return display;
}

ocpn::render::OpenCpnAdapterInput BuildInput(
    const ocpn::render::RenderView& view,
    const ocpn::render::DisplayState& display) {
  ocpn::render::OpenCpnAdapterInput input;
  input.lifecycle.canvas_id = "chart-canvas-1";
  input.lifecycle.wx_canvas_owned_by_opencpn = true;
  input.lifecycle.swapchain_owned_by_opencpn = true;
  input.render_view = view;
  input.display_state = display;
  input.render_target.kind = ocpn::render::RenderTargetKind::kSwapchain;
  input.render_target.pixel_size = view.pixel_size;
  input.render_target.device_pixel_ratio = view.device_pixel_ratio;
  input.render_target.target_id = "chart-canvas-1-swapchain";
  return input;
}

}  // namespace

int main() {
  const ocpn::render::RenderView view = BuildView();
  const ocpn::render::DisplayState display = BuildDisplay();

  ocpn::render::s52::S52CommandBuilder builder;
  const ocpn::render::RenderScene scene =
      builder.BuildFixtureScene(view, display);
  const ocpn::render::NauticalRenderModel model =
      ocpn::render::BuildNauticalRenderModel(scene);

  ocpn::render::OpenCpnFeatureFlags flags;
  flags.shared_renderer_enabled = true;
  flags.allow_legacy_fallback = true;

  const ocpn::render::OpenCpnAdapterInput input = BuildInput(view, display);
  const ocpn::render::OpenCpnFeatureFlagDecision shared_decision =
      ocpn::render::ChooseOpenCpnRenderer(flags, input, &model);
  if (!ExpectRoute("valid neutral model", shared_decision,
                   ocpn::render::OpenCpnRendererRoute::kSharedRenderer,
                   "neutral_model_valid")) {
    return 1;
  }

  ocpn::render::OpenCpnFeatureFlags disabled_flags = flags;
  disabled_flags.shared_renderer_enabled = false;
  const ocpn::render::OpenCpnFeatureFlagDecision disabled_decision =
      ocpn::render::ChooseOpenCpnRenderer(disabled_flags, input, &model);
  if (!ExpectRoute("disabled feature flag", disabled_decision,
                   ocpn::render::OpenCpnRendererRoute::kLegacyCanvas,
                   "feature_flag_disabled")) {
    return 1;
  }

  ocpn::render::OpenCpnAdapterInput lifecycle_input = input;
  lifecycle_input.lifecycle.swapchain_owned_by_opencpn = false;
  const ocpn::render::OpenCpnFeatureFlagDecision lifecycle_decision =
      ocpn::render::ChooseOpenCpnRenderer(flags, lifecycle_input, &model);
  if (!ExpectRoute("lifecycle guard", lifecycle_decision,
                   ocpn::render::OpenCpnRendererRoute::kLegacyCanvas,
                   "opencpn_lifecycle_not_owned")) {
    return 1;
  }

  const ocpn::render::OpenCpnFeatureFlagDecision missing_model_decision =
      ocpn::render::ChooseOpenCpnRenderer(flags, input, nullptr);
  if (!ExpectRoute("missing neutral model", missing_model_decision,
                   ocpn::render::OpenCpnRendererRoute::kLegacyCanvas,
                   "missing_neutral_model")) {
    return 1;
  }

  ocpn::render::OpenCpnFeatureFlags strict_flags = flags;
  strict_flags.allow_legacy_fallback = false;
  const ocpn::render::OpenCpnFeatureFlagDecision strict_decision =
      ocpn::render::ChooseOpenCpnRenderer(strict_flags, input, nullptr);
  if (!ExpectRoute("strict missing neutral model", strict_decision,
                   ocpn::render::OpenCpnRendererRoute::kUnavailable,
                   "missing_neutral_model")) {
    return 1;
  }

  ocpn::render::NauticalRenderModel invalid_model = model;
  invalid_model.metadata.erase("backend_contract");
  const ocpn::render::OpenCpnFeatureFlagDecision invalid_decision =
      ocpn::render::ChooseOpenCpnRenderer(flags, input, &invalid_model);
  if (!ExpectRoute("invalid neutral model", invalid_decision,
                   ocpn::render::OpenCpnRendererRoute::kLegacyCanvas,
                   "invalid_neutral_model")) {
    return 1;
  }

  return 0;
}
