// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "draw_backend_contract.hpp"
#include "performance_budget_contract.hpp"
#include "portable_nautical_package.hpp"
#include "s52/s52_command_builder.hpp"
#include "viewport_tile_scheduler.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

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

const ocpn::render::PerformanceBudgetTarget* FindTarget(
    const ocpn::render::ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    ocpn::render::PerformanceMetric metric) {
  for (const ocpn::render::PerformanceBudgetTarget& target : budget.targets) {
    if (target.profile_id == profile_id && target.metric == metric) {
      return &target;
    }
  }
  return nullptr;
}

ocpn::render::PerformanceMeasurement Measure(
    std::string profile_id, ocpn::render::PerformanceMetric metric,
    double value, std::string evidence_id) {
  ocpn::render::PerformanceMeasurement measurement;
  measurement.profile_id = std::move(profile_id);
  measurement.metric = metric;
  measurement.value = value;
  measurement.unit = ocpn::render::ExpectedUnit(metric);
  measurement.evidence_id = std::move(evidence_id);
  return measurement;
}

std::vector<ocpn::render::PerformanceMeasurement> FixtureMeasurements(
    const ocpn::render::ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    std::uint64_t resident_bytes,
    std::uint64_t disk_bytes) {
  std::vector<ocpn::render::PerformanceMeasurement> measurements;
  auto add_scaled = [&](ocpn::render::PerformanceMetric metric, double scale) {
    const ocpn::render::PerformanceBudgetTarget* target =
        FindTarget(budget, profile_id, metric);
    if (target) {
      measurements.push_back(Measure(profile_id, metric,
                                     target->max_value * scale,
                                     "fixture:" + profile_id));
    }
  };
  add_scaled(ocpn::render::PerformanceMetric::kPackageLoadMs, 0.50);
  add_scaled(ocpn::render::PerformanceMetric::kPresentationCompileMs, 0.50);
  add_scaled(ocpn::render::PerformanceMetric::kGpuArtifactCompileMs, 0.50);
  add_scaled(ocpn::render::PerformanceMetric::kGpuArtifactCacheHitMs, 0.40);
  add_scaled(ocpn::render::PerformanceMetric::kBackendRenderMs, 0.60);
  add_scaled(ocpn::render::PerformanceMetric::kActivePowerWatts, 0.70);
  add_scaled(ocpn::render::PerformanceMetric::kIdlePowerWatts, 0.60);
  add_scaled(ocpn::render::PerformanceMetric::kViewportScheduleMs, 0.50);
  measurements.push_back(
      Measure(profile_id, ocpn::render::PerformanceMetric::kResidentMemoryBytes,
              static_cast<double>(resident_bytes), "fixture:memory"));
  measurements.push_back(
      Measure(profile_id, ocpn::render::PerformanceMetric::kDiskCacheBytes,
              static_cast<double>(disk_bytes), "fixture:disk-cache"));
  return measurements;
}

ocpn::render::ViewportTileSchedulerInput SchedulerInput(
    const ocpn::render::RenderView& view) {
  ocpn::render::ViewportTileSchedulerInput input;
  input.render_view = view;
  input.center_tile = {8, 125, 90};
  input.fractional_zoom = 0.6;
  input.epoch.chart_epoch = "format2-fixture";
  input.epoch.presentation_epoch = "s52-fixture";
  input.epoch.display_epoch = "day-standard";
  input.epoch.scheduler_epoch = "perf1-budget";
  input.epoch.source_group_id = "perf1";
  return input;
}

}  // namespace

int main() {
  const ocpn::render::ProductionPerformanceBudget budget =
      ocpn::render::BuildFirstProductionPerformanceBudget();
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::ValidatePerformanceBudget(budget, &diagnostics)) {
    std::cerr << "First production performance budget failed validation\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  const ocpn::render::PortableNauticalPackage package =
      ocpn::render::BuildPortablePackageFixture();
  const std::string encoded_package =
      ocpn::render::WritePortableNauticalPackage(package);
  if (encoded_package.empty()) {
    std::cerr << "Portable package fixture did not produce bytes\n";
    return 1;
  }

  ocpn::render::RenderView view;
  view.view_id = "perf-1-budget-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {512, 512};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::s52::S52CommandBuilder builder;
  const ocpn::render::RenderScene scene =
      builder.BuildFixtureScene(view, display);
  ocpn::render::NauticalRenderModel model =
      ocpn::render::BuildNauticalRenderModel(scene);
  for (ocpn::render::NauticalLayer& layer : model.layers) {
    for (ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      primitive.metadata["semantic_tier"] = "tier1_official_chart";
      primitive.metadata["source_standard"] = "s52-compatible";
    }
  }

  ocpn::render::GpuArtifactCacheOptions cache_options;
  cache_options.backend_target = "vsg";
  cache_options.device_profile = "vulkan-proof-device";
  cache_options.material_profile = "neutral-material-v1";
  cache_options.cache_namespace = "opencpn-perf-budget";
  cache_options.memory_budget_bytes = 32ULL * 1024ULL * 1024ULL;
  const ocpn::render::GpuArtifactCacheManifest artifacts =
      ocpn::render::BuildGpuArtifactCacheManifest(model, cache_options);

  ocpn::render::DrawBackendCapabilities vsg;
  vsg.backend_id = "vsg-native-proof";
  vsg.target = ocpn::render::DrawBackendTarget::kVsgVulkan;
  vsg.device_profile = cache_options.device_profile;
  vsg.material_profile = cache_options.material_profile;
  vsg.supports_swapchain = true;
  vsg.supports_readback = true;
  const ocpn::render::DrawBackendContract backend_contract =
      ocpn::render::BuildDrawBackendContract(model, artifacts, vsg);
  if (!ocpn::render::ValidateDrawBackendHandoff(model, artifacts,
                                                backend_contract,
                                                &diagnostics)) {
    std::cerr << "Draw backend contract fixture failed validation\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ViewportTileSchedulerPolicy scheduler_policy;
  scheduler_policy.max_prefetch_tiles = 16;
  const ocpn::render::ViewportTilePlan tile_plan =
      ocpn::render::BuildViewportTilePlan(SchedulerInput(view),
                                          scheduler_policy);
  if (!ocpn::render::ValidateViewportTilePlan(tile_plan, &diagnostics)) {
    std::cerr << "Viewport scheduler fixture failed validation\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  const std::uint64_t resident_bytes =
      artifacts.stats.estimated_bytes + encoded_package.size();
  const std::uint64_t disk_bytes =
      artifacts.stats.estimated_bytes + encoded_package.size() +
      static_cast<std::uint64_t>(tile_plan.requests.size()) * 256U;

  const ocpn::render::PerformanceBudgetEvaluation desktop =
      ocpn::render::EvaluatePerformanceBudget(
          budget, "desktop_reference",
          FixtureMeasurements(budget, "desktop_reference", resident_bytes,
                              disk_bytes));
  if (!desktop.ok) {
    std::cerr << "Desktop reference budget fixture failed\n";
    PrintDiagnostics(desktop.diagnostics);
    return 1;
  }

  const ocpn::render::PerformanceBudgetEvaluation boat =
      ocpn::render::EvaluatePerformanceBudget(
          budget, "boat_low_power",
          FixtureMeasurements(budget, "boat_low_power", resident_bytes,
                              disk_bytes));
  if (!boat.ok) {
    std::cerr << "Boat low-power budget fixture failed\n";
    PrintDiagnostics(boat.diagnostics);
    return 1;
  }

  ocpn::render::ProductionPerformanceBudget invalid = budget;
  invalid.targets.erase(
      std::remove_if(invalid.targets.begin(), invalid.targets.end(),
                     [](const ocpn::render::PerformanceBudgetTarget& target) {
                       return target.profile_id == "boat_low_power" &&
                              target.metric ==
                                  ocpn::render::PerformanceMetric::
                                      kGpuArtifactCacheHitMs;
                     }),
      invalid.targets.end());
  diagnostics.clear();
  if (ocpn::render::ValidatePerformanceBudget(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "performance_budget_required_metric")) {
    std::cerr << "Budget accepted a missing required cache-hit target\n";
    return 1;
  }

  std::vector<ocpn::render::PerformanceMeasurement> over_power =
      FixtureMeasurements(budget, "boat_low_power", resident_bytes, disk_bytes);
  for (ocpn::render::PerformanceMeasurement& measurement : over_power) {
    if (measurement.metric ==
        ocpn::render::PerformanceMetric::kActivePowerWatts) {
      measurement.value = 99.0;
    }
  }
  const ocpn::render::PerformanceBudgetEvaluation power_eval =
      ocpn::render::EvaluatePerformanceBudget(budget, "boat_low_power",
                                              over_power);
  if (power_eval.ok ||
      !HasDiagnostic(power_eval.diagnostics, "performance_budget_exceeded")) {
    std::cerr << "Budget accepted boat-class active power overrun\n";
    return 1;
  }

  std::vector<ocpn::render::PerformanceMeasurement> missing_render =
      FixtureMeasurements(budget, "desktop_reference", resident_bytes,
                          disk_bytes);
  missing_render.erase(
      std::remove_if(missing_render.begin(), missing_render.end(),
                     [](const ocpn::render::PerformanceMeasurement& sample) {
                       return sample.metric ==
                              ocpn::render::PerformanceMetric::kBackendRenderMs;
                     }),
      missing_render.end());
  const ocpn::render::PerformanceBudgetEvaluation missing_eval =
      ocpn::render::EvaluatePerformanceBudget(budget, "desktop_reference",
                                              missing_render);
  if (missing_eval.ok ||
      !HasDiagnostic(missing_eval.diagnostics,
                     "performance_budget_missing_measurement")) {
    std::cerr << "Budget accepted missing render-time measurement\n";
    return 1;
  }

  std::cout << "ok performance-budget: " << budget.targets.size()
            << " targets, package_bytes=" << encoded_package.size()
            << " gpu_bytes=" << artifacts.stats.estimated_bytes
            << " scheduled_tiles=" << tile_plan.requests.size() << "\n";
  return 0;
}
