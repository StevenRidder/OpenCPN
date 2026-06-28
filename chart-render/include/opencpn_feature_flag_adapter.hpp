// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "nautical_render_model.hpp"
#include "render_backend.hpp"

#include <string>
#include <vector>

namespace ocpn::render {

enum class OpenCpnRendererRoute {
  kLegacyCanvas,
  kSharedRenderer,
  kUnavailable
};

struct OpenCpnFeatureFlags {
  bool shared_renderer_enabled = false;
  bool allow_legacy_fallback = true;
  std::string shared_renderer_flag_name = "OCPN_USE_SHARED_RENDERER";
};

struct OpenCpnCanvasLifecycle {
  std::string canvas_id;
  bool wx_canvas_owned_by_opencpn = true;
  bool swapchain_owned_by_opencpn = true;
};

struct OpenCpnAdapterInput {
  OpenCpnCanvasLifecycle lifecycle;
  RenderView render_view;
  DisplayState display_state;
  RenderTarget render_target;
};

struct OpenCpnFeatureFlagDecision {
  OpenCpnRendererRoute route = OpenCpnRendererRoute::kLegacyCanvas;
  bool fallback_available = true;
  std::string reason_code;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(OpenCpnRendererRoute route);

// Decides whether an OpenCPN chart canvas should stay on the legacy canvas path
// or hand a validated neutral render model to the shared renderer seam.
//
// The adapter deliberately does not choose chart sources, S-52/S-101 rules,
// quilting, cache policy, scheduler policy, or backend-specific VSG behavior.
// OpenCPN remains the owner of wx canvas and swapchain lifetime.
OpenCpnFeatureFlagDecision ChooseOpenCpnRenderer(
    const OpenCpnFeatureFlags& flags,
    const OpenCpnAdapterInput& input,
    const NauticalRenderModel* model);

}  // namespace ocpn::render
