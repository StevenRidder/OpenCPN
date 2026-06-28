// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart_source.hpp"

#include <cstdint>
#include <string>

namespace ocpn::render {

enum class InterchangeContainerKind {
  kMbtiles,
  kPmtiles
};

enum class InterchangeIntent {
  kSourceInput,
  kDebugArtifact,
  kFixtureCorpus,
  kRendererHotPath
};

struct InterchangePackage {
  InterchangeContainerKind container = InterchangeContainerKind::kMbtiles;
  std::string package_id;
  std::string source_id;
  std::string content_hash;
  std::string tile_format;
  std::uint32_t min_zoom = 0;
  std::uint32_t max_zoom = 0;
  GeoBounds geographic_bbox;
  bool contains_raster_payload = false;
  bool contains_vector_payload = false;
};

struct InterchangeClassification {
  InterchangeIntent intent = InterchangeIntent::kSourceInput;
  bool allowed = false;
  bool renderer_contract = false;
  bool requires_chart_source_normalization = true;
  bool cache_repeated_container_decode = true;
  std::string reason;
};

struct InterchangeCachePlan {
  std::string normalized_scene_cache_key;
  std::string gpu_resource_cache_key;
  bool renderer_reads_container_directly = false;
  bool keep_decode_off_hot_path = true;
};

const char* ToString(InterchangeContainerKind kind);
const char* ToString(InterchangeIntent intent);

InterchangeClassification ClassifyInterchangePackage(
    const InterchangePackage& package, InterchangeIntent intent);

DebugArtifact MakeInterchangeDebugArtifact(const InterchangePackage& package);

InterchangeCachePlan BuildInterchangeCachePlan(
    const InterchangePackage& package, const RenderView& view);

}  // namespace ocpn::render
