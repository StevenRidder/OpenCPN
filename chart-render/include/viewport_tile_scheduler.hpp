// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kViewportTileSchedulerSchemaVersion = 1;

enum class TileRequestRole {
  kVisible,
  kOverscan,
  kPrefetch,
  kZoomBlend
};

enum class AdjacentZoomChoice {
  kNone,
  kLowerZoomParent,
  kHigherZoomChildren
};

struct TileId {
  int z = 0;
  int x = 0;
  int y = 0;
};

struct TileCacheEpoch {
  std::string chart_epoch;
  std::string presentation_epoch;
  std::string display_epoch;
  std::string scheduler_epoch = "viewport-scheduler-v1";
  std::string source_group_id;
};

struct ViewportTileSchedulerPolicy {
  std::string policy_id = "adapter-viewport-cache-v1";
  std::uint32_t tile_size_px = 256;
  std::uint32_t overscan_margin_tiles = 1;
  std::uint32_t prefetch_margin_tiles = 1;
  std::uint32_t max_prefetch_tiles = 64;
  bool enable_adjacent_zoom_blend = true;
  double zoom_blend_min_fraction = 0.25;
  double zoom_blend_max_fraction = 0.75;
};

struct ViewportTileSchedulerInput {
  RenderView render_view;
  TileId center_tile;
  double fractional_zoom = 0.0;
  TileCacheEpoch epoch;
};

struct ViewportTileCacheKey {
  std::string namespace_id = "viewport-tile-cache";
  std::string chart_epoch;
  std::string presentation_epoch;
  std::string display_epoch;
  std::string scheduler_epoch;
  std::string policy_id;
  std::string source_group_id;
  TileId tile;
  TileRequestRole role = TileRequestRole::kVisible;
};

struct ViewportTileRequest {
  TileId tile;
  TileRequestRole role = TileRequestRole::kVisible;
  int priority = 0;
  std::string reason;
  ViewportTileCacheKey cache_key;
};

struct ZoomBlendPlan {
  AdjacentZoomChoice choice = AdjacentZoomChoice::kNone;
  double base_zoom_opacity = 1.0;
  double adjacent_zoom_opacity = 0.0;
  std::vector<ViewportTileRequest> requests;
};

struct ViewportTilePlan {
  std::uint32_t schema_version = kViewportTileSchedulerSchemaVersion;
  std::string plan_id;
  std::string policy_owner = "adapter_scheduler";
  std::string semantic_owner = "presentation_compiler";
  std::string backend_contract = "no_backend_semantics";
  std::string cache_owner = "adapter_viewport_cache";
  ViewportTileSchedulerPolicy policy;
  ViewportTileSchedulerInput input;
  std::uint32_t visible_half_span_x = 0;
  std::uint32_t visible_half_span_y = 0;
  std::uint32_t overscan_extra_tiles = 0;
  std::vector<ViewportTileRequest> requests;
  ZoomBlendPlan zoom_blend;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(TileRequestRole role);
const char* ToString(AdjacentZoomChoice choice);

ViewportTilePlan BuildViewportTilePlan(
    const ViewportTileSchedulerInput& input,
    const ViewportTileSchedulerPolicy& policy = ViewportTileSchedulerPolicy{});

bool ValidateViewportTilePlan(const ViewportTilePlan& plan,
                              std::vector<Diagnostic>* diagnostics);

std::string CacheKeyString(const ViewportTileCacheKey& cache_key);

}  // namespace ocpn::render
