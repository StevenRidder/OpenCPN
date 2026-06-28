// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kNauticalRenderModelSchemaVersion = 1;

enum class NauticalPrimitiveType {
  kAreaFill,
  kLineStroke,
  kSymbolInstance,
  kTextLabel,
  kSounding,
  kRasterPatch,
  kContourLine,
  kClipBoundary
};

struct SourceTraceHandle {
  std::vector<std::string> provenance_refs;
  std::vector<std::string> conversion_trace_refs;
  std::string source_chart_id;
  std::string source_object_id;
  std::string source_object_class;
  std::string presentation_rule_id;
};

struct NauticalLodHint {
  double min_scale_denom = 0.0;
  double max_scale_denom = 0.0;
  double overzoom = 1.0;
  std::string display_category;
  std::string priority;
  bool visible = true;
};

struct NauticalCacheKey {
  std::string scene_key;
  std::string primitive_key;
  std::string resource_key;
};

struct ChartCoverageMetadata {
  std::string source_id;
  GeoBounds image_bounds;
  GeoBounds chart_bounds;
  GeoBounds visible_bounds;
  GeoBounds collar_bounds;
  std::string no_data_policy;
  std::string collar_policy;
  std::string boundary_policy;
  std::string quilt_policy;
  int quilt_rank = 0;
  bool allow_visible_outside_chart_bounds = false;
};

struct BackendHandoffContract {
  std::string backend_contract = "draw_only";
  std::string semantic_owner = "presentation_compiler";
  std::string cache_owner = "runtime_gpu_asset_cache";
  CoordinateSpace coordinate_space = CoordinateSpace::kTarget;
  std::vector<std::string> accepted_backend_targets;
};

struct NauticalPrimitive {
  std::string primitive_id;
  NauticalPrimitiveType type = NauticalPrimitiveType::kAreaFill;
  std::string role;
  std::vector<Geometry> geometries;
  std::string fill_ref;
  std::string pattern_ref;
  std::string line_style_ref;
  std::string symbol_ref;
  std::string font_ref;
  std::string texture_ref;
  std::string text;
  double depth_m = 0.0;
  double width_px = 0.0;
  double opacity = 1.0;
  double rotation_deg = 0.0;
  double scale = 1.0;
  Point2 position;
  std::string anchor;
  SourceTraceHandle trace;
  NauticalLodHint lod;
  NauticalCacheKey cache_key;
  ChartCoverageMetadata coverage;
  BackendHandoffContract handoff;
  std::map<std::string, std::string> metadata;
};

struct NauticalLayer {
  std::string layer_id;
  std::string source_group_id;
  std::string presentation_layer;
  int draw_order = 0;
  int quilt_rank = 0;
  std::vector<NauticalPrimitive> primitives;
};

struct NauticalRenderModel {
  std::uint32_t schema_version = kNauticalRenderModelSchemaVersion;
  std::string model_id;
  std::string source_epoch;
  RenderView render_view;
  DisplayState display_state;
  ResourceTable resource_table;
  std::vector<NauticalLayer> layers;
  std::vector<ProvenanceRecord> provenance_table;
  std::vector<Diagnostic> diagnostics;
  std::map<std::string, std::string> metadata;
};

const char* ToString(NauticalPrimitiveType type);

NauticalRenderModel BuildNauticalRenderModel(const RenderScene& scene);

bool ValidateNauticalRenderModel(const NauticalRenderModel& model,
                                 std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
