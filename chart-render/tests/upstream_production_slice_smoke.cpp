// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "opencpn_feature_flag_adapter.hpp"
#include "performance_budget_contract.hpp"
#include "production_golden_corpus.hpp"
#include "production_performance_fixture.hpp"
#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kPackageHash = "fnv1a64:e2c66b175a87d42b";
constexpr const char* kImageHash = "009410097424697d";

bool HasError(const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.severity == ocpn::render::DiagnosticSeverity::kError) {
      return true;
    }
  }
  return false;
}

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

ocpn::render::RenderView BuildProductionView() {
  ocpn::render::RenderView view;
  view.view_id = "upstream1-production-slice";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 5000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;
  return view;
}

ocpn::render::DisplayState BuildProductionDisplay() {
  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  return display;
}

ocpn::render::OpenCpnAdapterInput BuildAdapterInput(
    const ocpn::render::RenderView& view,
    const ocpn::render::DisplayState& display) {
  ocpn::render::OpenCpnAdapterInput input;
  input.lifecycle.canvas_id = "upstream1-chart-canvas";
  input.lifecycle.wx_canvas_owned_by_opencpn = true;
  input.lifecycle.swapchain_owned_by_opencpn = true;
  input.render_view = view;
  input.display_state = display;
  input.render_target.kind = ocpn::render::RenderTargetKind::kSwapchain;
  input.render_target.pixel_size = view.pixel_size;
  input.render_target.device_pixel_ratio = view.device_pixel_ratio;
  input.render_target.target_id = "upstream1-chart-canvas-swapchain";
  return input;
}

bool ValidateFeatureFlagPath() {
  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(ocpn::render::BuildS57ConverterFixtureCell());

  const ocpn::render::RenderView view = BuildProductionView();
  const ocpn::render::DisplayState display = BuildProductionDisplay();
  const ocpn::render::NauticalRenderModel model =
      ocpn::render::s52::CompileS52PackagePresentation(package, view, display);

  std::vector<ocpn::render::Diagnostic> model_diagnostics;
  if (!ocpn::render::ValidateNauticalRenderModel(model, &model_diagnostics)) {
    std::cerr << "UPSTREAM-1 model failed validation before feature flag\n";
    PrintDiagnostics(model_diagnostics);
    return false;
  }

  ocpn::render::OpenCpnFeatureFlags flags;
  flags.shared_renderer_enabled = true;
  flags.allow_legacy_fallback = true;
  flags.shared_renderer_flag_name = "OCPN_USE_SHARED_RENDERER";

  const ocpn::render::OpenCpnAdapterInput input =
      BuildAdapterInput(view, display);
  const ocpn::render::OpenCpnFeatureFlagDecision shared =
      ocpn::render::ChooseOpenCpnRenderer(flags, input, &model);
  if (shared.route != ocpn::render::OpenCpnRendererRoute::kSharedRenderer ||
      shared.reason_code != "neutral_model_valid") {
    std::cerr << "UPSTREAM-1 did not select shared renderer for valid model\n";
    return false;
  }

  ocpn::render::OpenCpnFeatureFlags disabled = flags;
  disabled.shared_renderer_enabled = false;
  const ocpn::render::OpenCpnFeatureFlagDecision fallback =
      ocpn::render::ChooseOpenCpnRenderer(disabled, input, &model);
  if (fallback.route != ocpn::render::OpenCpnRendererRoute::kLegacyCanvas ||
      fallback.reason_code != "feature_flag_disabled") {
    std::cerr << "UPSTREAM-1 did not keep legacy renderer as disabled default\n";
    return false;
  }

  ocpn::render::OpenCpnAdapterInput invalid_lifecycle = input;
  invalid_lifecycle.lifecycle.swapchain_owned_by_opencpn = false;
  const ocpn::render::OpenCpnFeatureFlagDecision lifecycle =
      ocpn::render::ChooseOpenCpnRenderer(flags, invalid_lifecycle, &model);
  if (lifecycle.route != ocpn::render::OpenCpnRendererRoute::kLegacyCanvas ||
      lifecycle.reason_code != "opencpn_lifecycle_not_owned") {
    std::cerr << "UPSTREAM-1 feature flag ignored OpenCPN lifecycle ownership\n";
    return false;
  }

  return true;
}

bool ValidateGoldenSlice() {
  const ocpn::render::ProductionGoldenCorpusExpected expected =
      ocpn::render::BuildDefaultProductionGoldenCorpusExpected();
  const ocpn::render::ProductionGoldenCorpusSnapshot snapshot =
      ocpn::render::BuildProductionGoldenCorpusSnapshot(expected);

  if (!snapshot.ok || HasError(snapshot.diagnostics)) {
    std::cerr << "UPSTREAM-1 golden slice failed\n";
    PrintDiagnostics(snapshot.diagnostics);
    return false;
  }
  if (snapshot.package_hash != kPackageHash ||
      snapshot.golden_image_hash != kImageHash ||
      snapshot.primitives.size() != 3U ||
      snapshot.cache_manifest.artifacts.size() != 13U ||
      snapshot.inspection_report.rows.size() != 3U ||
      snapshot.golden_image_bytes != 256U * 256U * 4U ||
      snapshot.known_limitations.empty()) {
    std::cerr << "UPSTREAM-1 golden slice no longer matches the bounded "
                 "production fixture\n";
    return false;
  }

  bool saw_buoy = false;
  for (const ocpn::render::SourceToRenderInspectionRow& row :
       snapshot.inspection_report.rows) {
    if (row.source.source_object_id == "BOYLAT.3001" &&
        row.presentation.presentation_rule_id == "s52:BOYLAT:navaid_symbol" &&
        row.backend.backend_contract == "draw_only" &&
        row.query.rendered_pixel_hash == kImageHash &&
        row.query.sampled_rendered_pixel) {
      saw_buoy = true;
    }
  }
  if (!saw_buoy) {
    std::cerr << "UPSTREAM-1 inspection trace lost the BOYLAT source-to-pixel "
                 "row\n";
    return false;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot semantic_drift = snapshot;
  semantic_drift.primitives.front().presentation_rule_id =
      "s52:DEPARE:backend_owned";
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          semantic_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_primitive_hash")) {
    std::cerr << "UPSTREAM-1 golden gate accepted semantic drift\n";
    PrintDiagnostics(diagnostics);
    return false;
  }

  return true;
}

bool ValidatePerformanceSlice() {
  const ocpn::render::ProductionPerformanceBudget budget =
      ocpn::render::BuildFirstProductionPerformanceBudget();
  const ocpn::render::ProductionFixturePerformanceEvidence evidence =
      ocpn::render::MeasureProductionFixturePerformance(8);

  if (!evidence.ok || HasError(evidence.diagnostics)) {
    std::cerr << "UPSTREAM-1 performance slice failed\n";
    PrintDiagnostics(evidence.diagnostics);
    return false;
  }
  if (evidence.package_hash != kPackageHash ||
      evidence.golden_image_hash != kImageHash ||
      evidence.primitive_count != 3U ||
      evidence.gpu_artifact_count != 13U ||
      evidence.inspection_row_count != 3U ||
      !evidence.package_round_trip_stable ||
      !evidence.gpu_cache_repeat_stable ||
      !evidence.render_repeat_stable ||
      !HasDiagnostic(evidence.diagnostics,
                     "production_performance_power_unavailable")) {
    std::cerr << "UPSTREAM-1 performance evidence no longer measures the "
                 "production slice honestly\n";
    return false;
  }

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::ValidateProductionFixturePerformanceEvidence(
          budget, evidence, &diagnostics)) {
    std::cerr << "UPSTREAM-1 performance budget rejected current evidence\n";
    PrintDiagnostics(diagnostics);
    return false;
  }

  return true;
}

}  // namespace

int main() {
  if (!ValidateFeatureFlagPath()) return 1;
  if (!ValidateGoldenSlice()) return 1;
  if (!ValidatePerformanceSlice()) return 1;

  std::cout << "ok upstream-production-slice: package=" << kPackageHash
            << " image_hash=" << kImageHash
            << " primitives=3 artifacts=13 inspection_rows=3"
            << " legacy_default=feature_flag_disabled\n";
  return 0;
}

