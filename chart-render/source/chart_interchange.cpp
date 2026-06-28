// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart_interchange.hpp"

#include <sstream>

namespace ocpn::render {
namespace {

std::string StableKeyPart(const std::string& value,
                          const char* fallback) {
  return value.empty() ? std::string(fallback) : value;
}

}  // namespace

const char* ToString(InterchangeContainerKind kind) {
  switch (kind) {
    case InterchangeContainerKind::kMbtiles:
      return "mbtiles";
    case InterchangeContainerKind::kPmtiles:
      return "pmtiles";
  }
  return "unknown";
}

const char* ToString(InterchangeIntent intent) {
  switch (intent) {
    case InterchangeIntent::kSourceInput:
      return "source_input";
    case InterchangeIntent::kDebugArtifact:
      return "debug_artifact";
    case InterchangeIntent::kFixtureCorpus:
      return "fixture_corpus";
    case InterchangeIntent::kRendererHotPath:
      return "renderer_hot_path";
  }
  return "unknown";
}

InterchangeClassification ClassifyInterchangePackage(
    const InterchangePackage& package, InterchangeIntent intent) {
  InterchangeClassification classification;
  classification.intent = intent;

  switch (intent) {
    case InterchangeIntent::kSourceInput:
      classification.allowed = true;
      classification.reason =
          "Interchange package may feed chart-source normalization.";
      break;
    case InterchangeIntent::kDebugArtifact:
      classification.allowed = true;
      classification.reason =
          "Interchange package may be retained for provenance and debugging.";
      break;
    case InterchangeIntent::kFixtureCorpus:
      classification.allowed = true;
      classification.reason =
          "Interchange package may carry regression fixtures before decoding.";
      break;
    case InterchangeIntent::kRendererHotPath:
      classification.allowed = false;
      classification.reason =
          "Renderer backends consume normalized scene and GPU resources, not "
          "container-specific tile streams.";
      break;
  }

  classification.renderer_contract = false;
  classification.requires_chart_source_normalization = true;
  classification.cache_repeated_container_decode =
      package.contains_raster_payload || package.contains_vector_payload;
  return classification;
}

DebugArtifact MakeInterchangeDebugArtifact(const InterchangePackage& package) {
  DebugArtifact artifact;
  artifact.artifact_id =
      StableKeyPart(package.package_id, "interchange-package");
  artifact.kind = DebugArtifactKind::kInterchangePackage;
  artifact.source_id = package.source_id;
  artifact.content_hash = package.content_hash;
  artifact.media_type = std::string("application/vnd.opencpn.") +
                        ToString(package.container);
  artifact.producer = "chart_interchange";
  artifact.metadata["container"] = ToString(package.container);
  artifact.metadata["tile_format"] = package.tile_format;
  artifact.metadata["renderer_contract"] = "false";
  artifact.metadata["requires_chart_source_normalization"] = "true";
  return artifact;
}

InterchangeCachePlan BuildInterchangeCachePlan(
    const InterchangePackage& package, const RenderView& view) {
  const std::string package_key =
      StableKeyPart(package.content_hash, package.package_id.c_str());
  const std::string view_key = StableKeyPart(view.view_id, "view");

  std::ostringstream scene;
  scene << "chart-source:" << ToString(package.container) << ":"
        << package_key << ":" << view_key;

  std::ostringstream gpu;
  gpu << "gpu-resource:" << ToString(package.container) << ":" << package_key
      << ":" << view.pixel_size.width << "x" << view.pixel_size.height;

  InterchangeCachePlan plan;
  plan.normalized_scene_cache_key = scene.str();
  plan.gpu_resource_cache_key = gpu.str();
  plan.renderer_reads_container_directly = false;
  plan.keep_decode_off_hot_path = true;
  return plan;
}

}  // namespace ocpn::render
