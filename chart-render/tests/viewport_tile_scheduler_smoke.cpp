// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "viewport_tile_scheduler.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

ocpn::render::ViewportTileSchedulerInput BuildInput(double fractional_zoom) {
  ocpn::render::ViewportTileSchedulerInput input;
  input.render_view.view_id = "viewport-scheduler-smoke";
  input.render_view.projection = ocpn::render::Projection::kWebMercatorTile;
  input.render_view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  input.render_view.center = {-81.80, 24.47};
  input.render_view.scale_denom = 20000.0;
  input.render_view.pixel_size = {512, 512};
  input.render_view.overscan_px = 0;
  input.center_tile = {8, 125, 90};
  input.fractional_zoom = fractional_zoom;
  input.epoch.chart_epoch = "chart-catalog-a";
  input.epoch.presentation_epoch = "s52-rules-a";
  input.epoch.display_epoch = "day-standard-metric";
  input.epoch.scheduler_epoch = "scheduler-policy-a";
  input.epoch.source_group_id = "chart-group-1";
  return input;
}

std::size_t CountRole(const ocpn::render::ViewportTilePlan& plan,
                      ocpn::render::TileRequestRole role) {
  std::size_t count = 0;
  for (const ocpn::render::ViewportTileRequest& request : plan.requests) {
    if (request.role == role) ++count;
  }
  return count;
}

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

const ocpn::render::ViewportTileRequest* FindRequest(
    const ocpn::render::ViewportTilePlan& plan,
    ocpn::render::TileId tile,
    ocpn::render::TileRequestRole role) {
  for (const ocpn::render::ViewportTileRequest& request : plan.requests) {
    if (request.tile.z == tile.z && request.tile.x == tile.x &&
        request.tile.y == tile.y && request.role == role) {
      return &request;
    }
  }
  return nullptr;
}

bool ValidatePlan(const ocpn::render::ViewportTilePlan& plan,
                  const char* label) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateViewportTilePlan(plan, &diagnostics)) {
    return true;
  }
  std::cerr << label << " failed validation\n";
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

bool NearlyEqual(double lhs, double rhs) {
  return std::fabs(lhs - rhs) < 0.0001;
}

}  // namespace

int main() {
  ocpn::render::ViewportTileSchedulerPolicy policy;
  policy.overscan_margin_tiles = 1;
  policy.prefetch_margin_tiles = 1;
  policy.max_prefetch_tiles = 64;

  const ocpn::render::ViewportTileSchedulerInput input = BuildInput(0.6);
  const ocpn::render::ViewportTilePlan plan =
      ocpn::render::BuildViewportTilePlan(input, policy);
  if (!ValidatePlan(plan, "high zoom blend")) return 1;

  if (CountRole(plan, ocpn::render::TileRequestRole::kVisible) != 9 ||
      CountRole(plan, ocpn::render::TileRequestRole::kOverscan) != 16 ||
      CountRole(plan, ocpn::render::TileRequestRole::kPrefetch) != 24 ||
      CountRole(plan, ocpn::render::TileRequestRole::kZoomBlend) != 36) {
    std::cerr << "Unexpected visible/overscan/prefetch/blend request counts\n";
    return 1;
  }

  if (plan.zoom_blend.choice !=
          ocpn::render::AdjacentZoomChoice::kHigherZoomChildren ||
      !NearlyEqual(plan.zoom_blend.base_zoom_opacity, 0.4) ||
      !NearlyEqual(plan.zoom_blend.adjacent_zoom_opacity, 0.6)) {
    std::cerr << "Higher-zoom blend policy was not selected correctly\n";
    return 1;
  }

  const ocpn::render::ViewportTilePlan repeat =
      ocpn::render::BuildViewportTilePlan(input, policy);
  if (plan.requests.size() != repeat.requests.size() ||
      ocpn::render::CacheKeyString(plan.requests.front().cache_key) !=
          ocpn::render::CacheKeyString(repeat.requests.front().cache_key) ||
      ocpn::render::CacheKeyString(plan.requests.back().cache_key) !=
          ocpn::render::CacheKeyString(repeat.requests.back().cache_key)) {
    std::cerr << "Viewport scheduler cache keys are not deterministic\n";
    return 1;
  }

  ocpn::render::ViewportTileSchedulerInput changed_display = input;
  changed_display.epoch.display_epoch = "night-standard-metric";
  const ocpn::render::ViewportTilePlan changed_plan =
      ocpn::render::BuildViewportTilePlan(changed_display, policy);
  if (ocpn::render::CacheKeyString(plan.requests.front().cache_key) ==
      ocpn::render::CacheKeyString(changed_plan.requests.front().cache_key)) {
    std::cerr << "Display epoch did not invalidate viewport tile cache keys\n";
    return 1;
  }

  ocpn::render::TileId warmed_tile{8, 127, 92};
  const ocpn::render::ViewportTileRequest* overscan_request =
      FindRequest(plan, warmed_tile, ocpn::render::TileRequestRole::kOverscan);
  ocpn::render::ViewportTileSchedulerInput panned_input = input;
  panned_input.center_tile = {8, 126, 91};
  const ocpn::render::ViewportTilePlan panned_plan =
      ocpn::render::BuildViewportTilePlan(panned_input, policy);
  if (!ValidatePlan(panned_plan, "panned viewport")) return 1;
  const ocpn::render::ViewportTileRequest* visible_request =
      FindRequest(panned_plan, warmed_tile,
                  ocpn::render::TileRequestRole::kVisible);
  if (overscan_request == nullptr || visible_request == nullptr ||
      ocpn::render::CacheKeyString(overscan_request->cache_key) !=
          ocpn::render::CacheKeyString(visible_request->cache_key)) {
    std::cerr << "Overscan cache key was not reused when tile became visible\n";
    return 1;
  }

  const ocpn::render::ViewportTilePlan lower_blend =
      ocpn::render::BuildViewportTilePlan(BuildInput(0.35), policy);
  if (!ValidatePlan(lower_blend, "low zoom blend")) return 1;
  if (lower_blend.zoom_blend.choice !=
          ocpn::render::AdjacentZoomChoice::kLowerZoomParent ||
      CountRole(lower_blend, ocpn::render::TileRequestRole::kZoomBlend) != 4) {
    std::cerr << "Lower-zoom parent blend policy was not selected correctly\n";
    return 1;
  }

  ocpn::render::ViewportTileSchedulerPolicy limited_policy = policy;
  limited_policy.max_prefetch_tiles = 5;
  const ocpn::render::ViewportTilePlan limited_plan =
      ocpn::render::BuildViewportTilePlan(input, limited_policy);
  if (!ValidatePlan(limited_plan, "limited prefetch")) return 1;
  if (CountRole(limited_plan, ocpn::render::TileRequestRole::kPrefetch) != 5 ||
      !HasDiagnostic(limited_plan.diagnostics,
                     "viewport_scheduler_prefetch_limit")) {
    std::cerr << "Prefetch limit did not truncate with a scheduler diagnostic\n";
    return 1;
  }

  ocpn::render::ViewportTileSchedulerPolicy disabled_blend_policy = policy;
  disabled_blend_policy.enable_adjacent_zoom_blend = false;
  const ocpn::render::ViewportTilePlan disabled_blend_plan =
      ocpn::render::BuildViewportTilePlan(input, disabled_blend_policy);
  if (!ValidatePlan(disabled_blend_plan, "disabled blend")) return 1;
  if (disabled_blend_plan.zoom_blend.choice !=
          ocpn::render::AdjacentZoomChoice::kNone ||
      CountRole(disabled_blend_plan,
                ocpn::render::TileRequestRole::kZoomBlend) != 0) {
    std::cerr << "Disabled blend policy still scheduled blend requests\n";
    return 1;
  }

  ocpn::render::ViewportTilePlan invalid_boundary = plan;
  invalid_boundary.policy_owner = "backend";
  std::vector<ocpn::render::Diagnostic> invalid_diagnostics;
  if (ocpn::render::ValidateViewportTilePlan(invalid_boundary,
                                             &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "viewport_scheduler_boundary")) {
    std::cerr << "Viewport scheduler accepted backend-owned policy\n";
    return 1;
  }

  ocpn::render::ViewportTileSchedulerInput missing_epoch = input;
  missing_epoch.epoch.presentation_epoch.clear();
  const ocpn::render::ViewportTilePlan missing_epoch_plan =
      ocpn::render::BuildViewportTilePlan(missing_epoch, policy);
  std::vector<ocpn::render::Diagnostic> epoch_diagnostics;
  if (ocpn::render::ValidateViewportTilePlan(missing_epoch_plan,
                                             &epoch_diagnostics) ||
      !HasDiagnostic(epoch_diagnostics, "viewport_scheduler_epoch")) {
    std::cerr << "Viewport scheduler accepted missing cache epoch metadata\n";
    return 1;
  }

  std::cout << "ok viewport scheduler: " << plan.requests.size()
            << " requests, blend="
            << ocpn::render::ToString(plan.zoom_blend.choice) << "\n";
  return 0;
}
