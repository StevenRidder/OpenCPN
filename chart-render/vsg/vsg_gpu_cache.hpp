// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "nautical_render_model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render::vsg {

inline constexpr std::uint32_t kVsgGpuCacheSchemaVersion = 1;

enum class VsgGpuAssetKind {
  kVertexBuffer,
  kIndexBuffer,
  kInstanceBuffer,
  kTexture,
  kAtlas,
  kUniformBlock
};

enum class VsgGpuResidency {
  kFrameLocal,
  kSceneLocal,
  kMachineLocal
};

struct VsgGpuCacheOptions {
  std::string device_profile = "vsg-proof-device";
  std::string shader_profile = "neutral-model-v1";
  std::string cache_namespace = "opencpn-vsg";
  bool include_frame_local_assets = true;
};

struct VsgGpuCacheKey {
  std::string namespace_id;
  std::string device_profile;
  std::string shader_profile;
  std::string model_key;
  std::string asset_key;
  std::string content_key;
};

struct VsgGpuAsset {
  std::string asset_id;
  VsgGpuAssetKind kind = VsgGpuAssetKind::kVertexBuffer;
  VsgGpuResidency residency = VsgGpuResidency::kSceneLocal;
  VsgGpuCacheKey cache_key;
  std::string primitive_type;
  std::string resource_id;
  std::string resource_type;
  std::string usage;
  std::vector<std::string> primitive_ids;
  std::vector<std::string> provenance_refs;
  std::uint64_t byte_size = 0;
  std::uint32_t vertices = 0;
  std::uint32_t indices = 0;
  std::uint32_t instances = 0;
  bool descriptor_ready = false;
};

struct VsgGpuCacheStats {
  std::uint32_t texture_assets = 0;
  std::uint32_t vector_buffer_assets = 0;
  std::uint32_t instance_buffer_assets = 0;
  std::uint32_t uniform_assets = 0;
  std::uint64_t estimated_bytes = 0;
};

struct VsgGpuCacheManifest {
  std::uint32_t schema_version = kVsgGpuCacheSchemaVersion;
  std::string manifest_id;
  std::string input_model_id;
  std::string input_model_epoch;
  std::string cache_scope = "machine-local-vsg-gpu-assets";
  std::string input_contract = "backend-neutral-nautical-render-model";
  std::string semantic_owner = "presentation_compiler";
  std::string cache_owner = "vsg_runtime_gpu_asset_cache";
  VsgGpuCacheOptions options;
  std::vector<VsgGpuAsset> assets;
  VsgGpuCacheStats stats;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(VsgGpuAssetKind kind);
const char* ToString(VsgGpuResidency residency);

VsgGpuCacheManifest BuildVsgGpuCacheManifest(
    const NauticalRenderModel& model,
    const VsgGpuCacheOptions& options = VsgGpuCacheOptions{});

bool ValidateVsgGpuCacheManifest(const VsgGpuCacheManifest& manifest,
                                 std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::vsg
