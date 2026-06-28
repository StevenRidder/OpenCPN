// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "opencpn_feature_flag_adapter.hpp"

#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeAdapterDiagnostic(DiagnosticSeverity severity, std::string code,
                                 std::string message,
                                 std::string suggested_action) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action = std::move(suggested_action);
  return diagnostic;
}

OpenCpnFeatureFlagDecision FallbackDecision(const OpenCpnFeatureFlags& flags,
                                            std::string reason_code,
                                            Diagnostic diagnostic) {
  OpenCpnFeatureFlagDecision decision;
  decision.route = flags.allow_legacy_fallback
                       ? OpenCpnRendererRoute::kLegacyCanvas
                       : OpenCpnRendererRoute::kUnavailable;
  decision.fallback_available = flags.allow_legacy_fallback;
  decision.reason_code = std::move(reason_code);
  decision.diagnostics.push_back(std::move(diagnostic));
  return decision;
}

bool SamePixelSize(PixelSize lhs, PixelSize rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height;
}

}  // namespace

const char* ToString(OpenCpnRendererRoute route) {
  switch (route) {
    case OpenCpnRendererRoute::kLegacyCanvas:
      return "legacy_canvas";
    case OpenCpnRendererRoute::kSharedRenderer:
      return "shared_renderer";
    case OpenCpnRendererRoute::kUnavailable:
      return "unavailable";
  }
  return "unknown";
}

OpenCpnFeatureFlagDecision ChooseOpenCpnRenderer(
    const OpenCpnFeatureFlags& flags,
    const OpenCpnAdapterInput& input,
    const NauticalRenderModel* model) {
  if (!flags.shared_renderer_enabled) {
    return FallbackDecision(
        flags, "feature_flag_disabled",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kInfo, "opencpn_adapter.flag_disabled",
            "OpenCPN shared renderer feature flag is disabled.",
            "Keep using the existing OpenCPN chart canvas until the feature "
            "flag is enabled for this canvas."));
  }

  if (!input.lifecycle.wx_canvas_owned_by_opencpn ||
      !input.lifecycle.swapchain_owned_by_opencpn) {
    return FallbackDecision(
        flags, "opencpn_lifecycle_not_owned",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kError, "opencpn_adapter.lifecycle",
            "Shared renderer path requires OpenCPN-owned wx canvas and "
            "swapchain lifetime.",
            "Bind the shared renderer to the existing OpenCPN canvas/swapchain "
            "lifecycle instead of creating an independent backend owner."));
  }

  if (input.render_target.kind != RenderTargetKind::kSwapchain) {
    return FallbackDecision(
        flags, "target_not_swapchain",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kWarning, "opencpn_adapter.target_kind",
            "OpenCPN interactive adapter expected a swapchain render target.",
            "Use the Helm/offscreen adapter for tile targets and keep this "
            "feature flag scoped to the OpenCPN interactive canvas."));
  }

  if (input.lifecycle.canvas_id.empty() ||
      input.render_target.target_id.empty()) {
    return FallbackDecision(
        flags, "missing_canvas_identity",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kWarning, "opencpn_adapter.canvas_identity",
            "OpenCPN canvas identity is missing from the adapter input.",
            "Pass the existing canvas id and swapchain target id through the "
            "adapter before enabling the shared renderer path."));
  }

  if (model == nullptr) {
    return FallbackDecision(
        flags, "missing_neutral_model",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kError, "opencpn_adapter.missing_model",
            "Shared renderer path requires a neutral nautical render model.",
            "Compile S-52/S-101 presentation output to the neutral render model "
            "before handing work to a renderer backend."));
  }

  std::vector<Diagnostic> validation_diagnostics;
  if (!ValidateNauticalRenderModel(*model, &validation_diagnostics)) {
    OpenCpnFeatureFlagDecision decision = FallbackDecision(
        flags, "invalid_neutral_model",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kError, "opencpn_adapter.invalid_model",
            "Neutral nautical render model failed validation.",
            "Keep chart semantics in the presentation compiler and hand only "
            "validated neutral primitives to the renderer backend."));
    decision.diagnostics.insert(decision.diagnostics.end(),
                                validation_diagnostics.begin(),
                                validation_diagnostics.end());
    return decision;
  }

  if (!SamePixelSize(model->render_view.pixel_size,
                     input.render_target.pixel_size)) {
    return FallbackDecision(
        flags, "target_size_mismatch",
        MakeAdapterDiagnostic(
            DiagnosticSeverity::kWarning, "opencpn_adapter.target_size",
            "Neutral render model pixel size does not match the canvas target.",
            "Rebuild the neutral render model from the current OpenCPN canvas "
            "viewport before handing it to the shared renderer."));
  }

  OpenCpnFeatureFlagDecision decision;
  decision.route = OpenCpnRendererRoute::kSharedRenderer;
  decision.fallback_available = flags.allow_legacy_fallback;
  decision.reason_code = "neutral_model_valid";
  decision.diagnostics.push_back(MakeAdapterDiagnostic(
      DiagnosticSeverity::kInfo, "opencpn_adapter.shared_renderer",
      "OpenCPN canvas may use the shared renderer with the neutral model.",
      "Renderer backends must replay neutral primitives only; chart-source, "
      "presentation, quilting, cache, and scheduler semantics stay before the "
      "backend handoff."));
  return decision;
}

}  // namespace ocpn::render
