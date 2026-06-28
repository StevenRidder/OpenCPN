// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "viewport_tile_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
namespace {

struct TileOrder {
  bool operator()(TileId lhs, TileId rhs) const {
    if (lhs.z != rhs.z) return lhs.z < rhs.z;
    if (lhs.y != rhs.y) return lhs.y < rhs.y;
    return lhs.x < rhs.x;
  }
};

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::string suggested_action) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action = std::move(suggested_action);
  return diagnostic;
}

std::uint32_t HalfSpan(std::uint32_t pixels, std::uint32_t tile_size_px) {
  if (tile_size_px == 0) return 0;
  const double half_pixels = static_cast<double>(pixels) / 2.0;
  return static_cast<std::uint32_t>(
      std::max(1.0, std::ceil(half_pixels / tile_size_px)));
}

std::uint32_t OverscanExtraTiles(const RenderView& view,
                                 const ViewportTileSchedulerPolicy& policy) {
  if (policy.tile_size_px == 0) return policy.overscan_margin_tiles;
  const std::uint32_t pixel_extra = static_cast<std::uint32_t>(
      std::ceil(static_cast<double>(view.overscan_px) / policy.tile_size_px));
  return pixel_extra + policy.overscan_margin_tiles;
}

std::string TileKey(TileId tile) {
  std::ostringstream out;
  out << tile.z << "/" << tile.x << "/" << tile.y;
  return out.str();
}

std::string PlanId(const ViewportTileSchedulerInput& input,
                   const ViewportTileSchedulerPolicy& policy) {
  std::ostringstream out;
  out << policy.policy_id << ":" << input.render_view.view_id << ":"
      << TileKey(input.center_tile) << ":" << input.epoch.chart_epoch << ":"
      << input.epoch.presentation_epoch << ":"
      << input.epoch.display_epoch << ":"
      << input.epoch.scheduler_epoch;
  return out.str();
}

ViewportTileCacheKey MakeCacheKey(const ViewportTileSchedulerInput& input,
                                  const ViewportTileSchedulerPolicy& policy,
                                  TileId tile, TileRequestRole role) {
  ViewportTileCacheKey key;
  key.chart_epoch = input.epoch.chart_epoch;
  key.presentation_epoch = input.epoch.presentation_epoch;
  key.display_epoch = input.epoch.display_epoch;
  key.scheduler_epoch = input.epoch.scheduler_epoch;
  key.policy_id = policy.policy_id;
  key.source_group_id = input.epoch.source_group_id;
  key.tile = tile;
  key.role = role;
  return key;
}

int PriorityFor(TileId center, TileId tile, TileRequestRole role) {
  const int distance = std::abs(tile.x - center.x) + std::abs(tile.y - center.y);
  switch (role) {
    case TileRequestRole::kVisible:
      return distance;
    case TileRequestRole::kOverscan:
      return 100 + distance;
    case TileRequestRole::kPrefetch:
      return 200 + distance;
    case TileRequestRole::kZoomBlend:
      return 300 + distance;
  }
  return 999;
}

ViewportTileRequest MakeRequest(const ViewportTileSchedulerInput& input,
                                const ViewportTileSchedulerPolicy& policy,
                                TileId tile, TileRequestRole role,
                                const char* reason) {
  ViewportTileRequest request;
  request.tile = tile;
  request.role = role;
  request.priority = PriorityFor(input.center_tile, tile, role);
  request.reason = reason;
  request.cache_key = MakeCacheKey(input, policy, tile, role);
  return request;
}

bool InRange(TileId tile, TileId center, std::uint32_t half_x,
             std::uint32_t half_y) {
  return std::abs(tile.x - center.x) <= static_cast<int>(half_x) &&
         std::abs(tile.y - center.y) <= static_cast<int>(half_y);
}

void AddRangeRequests(ViewportTilePlan* plan, TileRequestRole role,
                      std::uint32_t half_x, std::uint32_t half_y,
                      std::uint32_t inner_half_x,
                      std::uint32_t inner_half_y,
                      std::uint32_t max_requests, const char* reason) {
  std::uint32_t added = 0;
  const TileId center = plan->input.center_tile;
  for (int y = center.y - static_cast<int>(half_y);
       y <= center.y + static_cast<int>(half_y); ++y) {
    for (int x = center.x - static_cast<int>(half_x);
         x <= center.x + static_cast<int>(half_x); ++x) {
      TileId tile{center.z, x, y};
      if (tile.x < 0 || tile.y < 0) continue;
      if (role != TileRequestRole::kVisible &&
          InRange(tile, center, inner_half_x, inner_half_y)) {
        continue;
      }
      if (max_requests != 0 && added >= max_requests) {
        plan->diagnostics.push_back(MakeDiagnostic(
            DiagnosticSeverity::kWarning, "viewport_scheduler_prefetch_limit",
            "Viewport prefetch ring was truncated by max_prefetch_tiles.",
            "Increase the adapter scheduler prefetch budget if panning "
            "requires a wider warm cache."));
        return;
      }
      plan->requests.push_back(
          MakeRequest(plan->input, plan->policy, tile, role, reason));
      ++added;
    }
  }
}

std::vector<TileId> VisibleTiles(const ViewportTilePlan& plan) {
  std::vector<TileId> tiles;
  for (const ViewportTileRequest& request : plan.requests) {
    if (request.role == TileRequestRole::kVisible) {
      tiles.push_back(request.tile);
    }
  }
  return tiles;
}

double ClampedBlendFraction(double fraction) {
  if (fraction < 0.0) return 0.0;
  if (fraction > 1.0) return 1.0;
  return fraction;
}

void AddZoomBlend(ViewportTilePlan* plan) {
  const ViewportTileSchedulerPolicy& policy = plan->policy;
  if (!policy.enable_adjacent_zoom_blend) return;

  const double fraction = ClampedBlendFraction(plan->input.fractional_zoom);
  if (fraction < policy.zoom_blend_min_fraction ||
      fraction > policy.zoom_blend_max_fraction) {
    return;
  }

  const std::vector<TileId> visible_tiles = VisibleTiles(*plan);
  std::set<TileId, TileOrder> scheduled;
  const bool lower_zoom = fraction < 0.5;
  plan->zoom_blend.choice = lower_zoom
                                ? AdjacentZoomChoice::kLowerZoomParent
                                : AdjacentZoomChoice::kHigherZoomChildren;
  plan->zoom_blend.base_zoom_opacity = lower_zoom ? fraction : 1.0 - fraction;
  plan->zoom_blend.adjacent_zoom_opacity =
      1.0 - plan->zoom_blend.base_zoom_opacity;

  for (TileId tile : visible_tiles) {
    if (lower_zoom) {
      if (tile.z == 0) continue;
      TileId parent{tile.z - 1, tile.x / 2, tile.y / 2};
      if (scheduled.insert(parent).second) {
        ViewportTileRequest request =
            MakeRequest(plan->input, plan->policy, parent,
                        TileRequestRole::kZoomBlend,
                        "adjacent lower zoom parent for fractional zoom blend");
        plan->zoom_blend.requests.push_back(request);
        plan->requests.push_back(std::move(request));
      }
      continue;
    }

    for (int dy = 0; dy < 2; ++dy) {
      for (int dx = 0; dx < 2; ++dx) {
        TileId child{tile.z + 1, tile.x * 2 + dx, tile.y * 2 + dy};
        if (scheduled.insert(child).second) {
          ViewportTileRequest request =
              MakeRequest(plan->input, plan->policy, child,
                          TileRequestRole::kZoomBlend,
                          "adjacent higher zoom child for fractional zoom blend");
          plan->zoom_blend.requests.push_back(request);
          plan->requests.push_back(std::move(request));
        }
      }
    }
  }
}

bool EmptyEpoch(const TileCacheEpoch& epoch) {
  return epoch.chart_epoch.empty() || epoch.presentation_epoch.empty() ||
         epoch.display_epoch.empty() || epoch.scheduler_epoch.empty();
}

}  // namespace

const char* ToString(TileRequestRole role) {
  switch (role) {
    case TileRequestRole::kVisible:
      return "visible";
    case TileRequestRole::kOverscan:
      return "overscan";
    case TileRequestRole::kPrefetch:
      return "prefetch";
    case TileRequestRole::kZoomBlend:
      return "zoom_blend";
  }
  return "unknown";
}

const char* ToString(AdjacentZoomChoice choice) {
  switch (choice) {
    case AdjacentZoomChoice::kNone:
      return "none";
    case AdjacentZoomChoice::kLowerZoomParent:
      return "lower_zoom_parent";
    case AdjacentZoomChoice::kHigherZoomChildren:
      return "higher_zoom_children";
  }
  return "unknown";
}

ViewportTilePlan BuildViewportTilePlan(
    const ViewportTileSchedulerInput& input,
    const ViewportTileSchedulerPolicy& policy) {
  ViewportTilePlan plan;
  plan.policy = policy;
  plan.input = input;
  plan.plan_id = PlanId(input, policy);
  plan.visible_half_span_x =
      HalfSpan(input.render_view.pixel_size.width, policy.tile_size_px);
  plan.visible_half_span_y =
      HalfSpan(input.render_view.pixel_size.height, policy.tile_size_px);
  plan.overscan_extra_tiles = OverscanExtraTiles(input.render_view, policy);

  AddRangeRequests(&plan, TileRequestRole::kVisible, plan.visible_half_span_x,
                   plan.visible_half_span_y, 0, 0, 0,
                   "visible viewport coverage");

  const std::uint32_t overscan_half_x =
      plan.visible_half_span_x + plan.overscan_extra_tiles;
  const std::uint32_t overscan_half_y =
      plan.visible_half_span_y + plan.overscan_extra_tiles;
  AddRangeRequests(&plan, TileRequestRole::kOverscan, overscan_half_x,
                   overscan_half_y, plan.visible_half_span_x,
                   plan.visible_half_span_y, 0,
                   "overscan margin for pan continuity");

  const std::uint32_t prefetch_half_x =
      overscan_half_x + policy.prefetch_margin_tiles;
  const std::uint32_t prefetch_half_y =
      overscan_half_y + policy.prefetch_margin_tiles;
  AddRangeRequests(&plan, TileRequestRole::kPrefetch, prefetch_half_x,
                   prefetch_half_y, overscan_half_x, overscan_half_y,
                   policy.max_prefetch_tiles,
                   "prefetch margin for adjacent viewport cache warmup");

  AddZoomBlend(&plan);

  std::stable_sort(plan.requests.begin(), plan.requests.end(),
                   [](const ViewportTileRequest& lhs,
                      const ViewportTileRequest& rhs) {
                     return lhs.priority < rhs.priority;
                   });
  return plan;
}

bool ValidateViewportTilePlan(const ViewportTilePlan& plan,
                              std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (plan.schema_version != kViewportTileSchedulerSchemaVersion ||
      plan.plan_id.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "viewport_scheduler_identity",
        "Viewport tile scheduler plan has invalid identity.",
        "Build the plan through BuildViewportTilePlan before use."));
    ok = false;
  }
  if (plan.policy_owner != "adapter_scheduler" ||
      plan.backend_contract != "no_backend_semantics" ||
      plan.semantic_owner != "presentation_compiler") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "viewport_scheduler_boundary",
        "Viewport scheduler plan leaked ownership across the backend boundary.",
        "Keep overscan, prefetch, cache invalidation, and zoom blending in the "
        "adapter scheduler, before VSG/OpenGL/Metal/WebGPU backend handoff."));
    ok = false;
  }
  if (plan.policy.tile_size_px == 0) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "viewport_scheduler_tile_size",
        "Viewport scheduler tile size must be non-zero.",
        "Configure a concrete tile size for cache and prefetch planning."));
    ok = false;
  }
  if (EmptyEpoch(plan.input.epoch)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "viewport_scheduler_epoch",
        "Viewport scheduler cache keys are missing chart, presentation, "
        "display, or scheduler epochs.",
        "Include cache invalidation epochs from chart data, presentation "
        "rules, display settings, and scheduler policy."));
    ok = false;
  }

  bool saw_visible = false;
  std::set<std::string> request_keys;
  for (const ViewportTileRequest& request : plan.requests) {
    if (request.tile.z < 0 || request.tile.x < 0 || request.tile.y < 0) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "viewport_scheduler_tile_id",
          "Viewport scheduler emitted an invalid tile id.",
          "Clamp or wrap tile coordinates before issuing backend-independent "
          "tile requests."));
      ok = false;
    }
    if (request.role == TileRequestRole::kVisible) {
      saw_visible = true;
    }
    if (CacheKeyString(request.cache_key).empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "viewport_scheduler_cache_key",
          "Viewport scheduler emitted an empty cache key.",
          "Build cache keys from chart, presentation, display, scheduler, "
          "policy, role, and tile identity."));
      ok = false;
    }
    const std::string request_key = CacheKeyString(request.cache_key) + ":" +
                                    ToString(request.role) + ":" +
                                    request.reason;
    if (!request_keys.insert(request_key).second) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "viewport_scheduler_duplicate_request",
          "Viewport scheduler emitted a duplicate tile request.",
          "Deduplicate adjacent zoom and cache warmup requests before "
          "dispatch."));
      ok = false;
    }
  }

  if (!saw_visible) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "viewport_scheduler_visible",
        "Viewport scheduler plan does not cover the visible viewport.",
        "Always schedule visible viewport tiles before overscan or prefetch."));
    ok = false;
  }
  return ok;
}

std::string CacheKeyString(const ViewportTileCacheKey& cache_key) {
  if (cache_key.namespace_id.empty() || cache_key.chart_epoch.empty() ||
      cache_key.presentation_epoch.empty() ||
      cache_key.display_epoch.empty() ||
      cache_key.scheduler_epoch.empty() || cache_key.policy_id.empty()) {
    return {};
  }
  std::ostringstream out;
  out << cache_key.namespace_id << ":" << cache_key.chart_epoch << ":"
      << cache_key.presentation_epoch << ":" << cache_key.display_epoch << ":"
      << cache_key.scheduler_epoch << ":" << cache_key.policy_id << ":"
      << cache_key.source_group_id << ":" << TileKey(cache_key.tile);
  return out.str();
}

}  // namespace ocpn::render
