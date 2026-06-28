// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart1_conformance.hpp"
#include "nautical_render_model.hpp"
#include "render_backend.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ocpn::render::chart1 {

struct GeometryInspection {
  std::string geometry_id;
  CoordinateSpace coordinate_space = CoordinateSpace::kTarget;
  std::size_t point_count = 0;
  std::size_t ring_count = 0;
  std::string source_geometry_hash;
  std::string generated_geometry_hash;
  std::string target_geometry_hash;
};

struct ScaleInspection {
  double min_scale_denom = 0.0;
  double max_scale_denom = 0.0;
  double view_scale_denom = 0.0;
  double overzoom = 1.0;
};

struct CacheInspection {
  std::string scene_key;
  std::string primitive_key;
  std::string resource_key;
  std::string tile_cache_key;
};

struct ObjectInspection {
  std::string case_id;
  std::string command_id;
  std::string layer_id;
  std::string presentation_layer;
  std::string source_chart_id;
  std::string source_feature_id;
  std::string source_object_class;
  std::string s52_rule_id;
  std::string display_category;
  std::vector<std::string> projection_transform;
  GeometryInspection original_geometry;
  GeometryInspection normalized_geometry;
  ScaleInspection scale;
  CacheInspection cache;
  std::string backend_primitive_id;
  std::string final_gpu_asset_id;
  std::string final_web_asset_id;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> conversion_trace_refs;
};

struct LayerInspection {
  std::string layer_id;
  std::string presentation_layer;
  int draw_order = 0;
  std::vector<std::string> backend_primitive_ids;
};

struct DebugReport {
  std::string report_id = "chart-1-debug-report";
  ConformanceScene conformance;
  NauticalRenderModel model;
  RenderResult backend_result;
  std::vector<ObjectInspection> objects;
  std::vector<LayerInspection> layers;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

RenderTarget Chart1DebugTarget(PixelSize pixel_size);

DebugReport BuildDebugReport(RenderView view, DisplayState display);

DebugReport BuildDebugReport(RenderView view, DisplayState display,
                             RenderTarget target);

const ObjectInspection* FindObjectBySourceFeatureId(
    const DebugReport& report, const std::string& source_feature_id);

const ObjectInspection* FindObjectByBackendPrimitiveId(
    const DebugReport& report, const std::string& backend_primitive_id);

std::vector<const ObjectInspection*> FindObjectsInLayer(
    const DebugReport& report, const std::string& layer_id);

}  // namespace ocpn::render::chart1
