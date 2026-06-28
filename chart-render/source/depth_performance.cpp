// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_performance.hpp"

#include "depth_safety.hpp"
#include "vsg/vsg_backend.hpp"

#include <chrono>
#include <cstddef>
#include <utility>

namespace ocpn::render::depth {
namespace {

Diagnostic Error(std::string code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Inspect the depth performance smoke before trusting timing numbers.";
  return diagnostic;
}

std::uint64_t CountCommands(const RenderScene& scene) {
  std::uint64_t count = 0;
  for (const CommandGroup& group : scene.command_groups) {
    count += group.commands.size();
  }
  return count;
}

RenderView ViewFromOptions(const DepthPerformanceOptions& options) {
  RenderView view;
  view.view_id = "depth-performance-smoke";
  view.projection = Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.82, 24.45, -81.78, 24.49};
  view.center = {-81.80, 24.47};
  view.scale_denom = options.scale_denom;
  view.pixel_size = options.tile_size;
  view.overscan_px = 16;
  return view;
}

DisplayState DisplayForSmoke() {
  DisplayState display;
  display.shallow_contour_m = 2.0;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  display.deep_contour_m = 20.0;
  return display;
}

}  // namespace

DepthPerformanceSample RunDepthPerformanceSmoke(
    const DepthPerformanceOptions& options) {
  DepthPerformanceSample sample;
  sample.iterations = options.iterations;

  vsg::VsgBackend backend;
  RenderTarget target;
  target.kind = RenderTargetKind::kOffscreen;
  target.pixel_size = options.tile_size;
  target.target_id = "depth-performance-smoke";

  const auto start = std::chrono::steady_clock::now();
  for (std::uint32_t i = 0; i < options.iterations; ++i) {
    RenderScene scene =
        BuildSafetyDepthFixture(ViewFromOptions(options), DisplayForSmoke());
    std::vector<Diagnostic> validation_diagnostics;
    ValidateSafetyDepthScene(scene, &validation_diagnostics);
    sample.diagnostic_count += validation_diagnostics.size();
    sample.command_count += CountCommands(scene);

    RenderResult result = backend.Render(scene, target);
    sample.pixel_bytes += result.pixels.rgba8.size();
    sample.diagnostic_count += result.diagnostics.size();
    if (result.ok) {
      ++sample.backend_ok_count;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  sample.elapsed_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  return sample;
}

bool ValidateDepthPerformanceSample(const DepthPerformanceOptions& options,
                                    const DepthPerformanceSample& sample,
                                    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (sample.iterations != options.iterations || sample.iterations == 0) {
    out.push_back(
        Error("depth_perf_iterations", "Depth smoke iteration count is invalid."));
    ok = false;
  }
  if (sample.command_count == 0) {
    out.push_back(
        Error("depth_perf_commands", "Depth smoke emitted no render commands."));
    ok = false;
  }
  const std::uint64_t expected_pixel_bytes =
      static_cast<std::uint64_t>(options.tile_size.width) *
      static_cast<std::uint64_t>(options.tile_size.height) * 4 *
      options.iterations;
  if (sample.pixel_bytes < expected_pixel_bytes) {
    out.push_back(Error("depth_perf_pixel_bytes",
                        "Depth smoke did not exercise a full RGBA tile buffer."));
    ok = false;
  }
  if (sample.elapsed_ms < 0.0) {
    out.push_back(
        Error("depth_perf_elapsed", "Depth smoke elapsed time is invalid."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render::depth
