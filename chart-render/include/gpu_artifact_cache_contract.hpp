// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "nautical_render_model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kGpuArtifactCacheSchemaVersion = 1;

enum class GpuArtifactKind {
  kVertexBuffer,
  kIndexBuffer,
  kUniformBlock,
  kRasterTexture,
  kTextureAtlas,
  kGlyphAtlas,
  kLinePattern,
  kMaterialPipeline,
  kViewportTileEntry
};

enum class GpuArtifactResidency {
  kFrameLocal,
  kViewportLocal,
  kSceneLocal,
  kMachineLocal
};

struct GpuArtifactCacheOptions {
  std::string backend_target = "vsg";
  std::string device_profile = "generic-gpu";
  std::string material_profile = "neutral-model-v1";
  std::string cache_namespace = "opencpn-gpu-artifacts";
  std::string invalidation_epoch;
  std::uint64_t memory_budget_bytes = 64ULL * 1024ULL * 1024ULL;
  bool include_viewport_tile_entries = true;
};

struct GpuArtifactCacheKey {
  std::string namespace_id;
  std::string backend_target;
  std::string device_profile;
  std::string material_profile;
  std::string model_key;
  std::string artifact_key;
  std::string content_key;
  std::string invalidation_epoch;
};

struct GpuArtifactTierHandle {
  std::string semantic_tier = "tier1_official_chart";
  std::string semantic_owner = "presentation_compiler";
  std::string source_standard;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> primitive_ids;
};

struct GpuArtifactRecord {
  std::string artifact_id;
  GpuArtifactKind kind = GpuArtifactKind::kVertexBuffer;
  GpuArtifactResidency residency = GpuArtifactResidency::kSceneLocal;
  GpuArtifactCacheKey cache_key;
  GpuArtifactTierHandle tier;
  std::string usage;
  std::string primitive_type;
  std::string resource_id;
  std::string resource_type;
  std::string material_key;
  std::string pipeline_key;
  std::string invalidation_domain;
  std::uint64_t byte_size = 0;
  std::uint32_t vertices = 0;
  std::uint32_t indices = 0;
  std::uint32_t instances = 0;
  std::uint32_t memory_priority = 100;
  bool rebuildable = true;
  bool device_specific = true;
  bool compressed = false;
};

struct GpuArtifactCacheStats {
  std::uint32_t buffer_artifacts = 0;
  std::uint32_t texture_artifacts = 0;
  std::uint32_t atlas_artifacts = 0;
  std::uint32_t material_artifacts = 0;
  std::uint32_t viewport_artifacts = 0;
  std::uint64_t estimated_bytes = 0;
  std::uint64_t memory_budget_bytes = 0;
  bool over_budget = false;
};

struct GpuArtifactCacheManifest {
  std::uint32_t schema_version = kGpuArtifactCacheSchemaVersion;
  std::string manifest_id;
  std::string input_contract = "backend-neutral-nautical-render-model";
  std::string input_model_id;
  std::string input_model_epoch;
  std::string cache_scope = "machine-local-gpu-artifacts";
  std::string cache_owner = "runtime_gpu_artifact_cache";
  std::string semantic_owner = "presentation_compiler";
  GpuArtifactCacheOptions options;
  GpuArtifactCacheStats stats;
  std::vector<GpuArtifactRecord> artifacts;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(GpuArtifactKind kind);
const char* ToString(GpuArtifactResidency residency);

GpuArtifactCacheManifest BuildGpuArtifactCacheManifest(
    const NauticalRenderModel& model,
    const GpuArtifactCacheOptions& options = GpuArtifactCacheOptions{});

bool ValidateGpuArtifactCacheManifest(
    const GpuArtifactCacheManifest& manifest,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
