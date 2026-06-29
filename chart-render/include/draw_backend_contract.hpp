// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "gpu_artifact_cache_contract.hpp"
#include "nautical_render_model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kDrawBackendContractSchemaVersion = 1;

enum class DrawBackendTarget {
  kVsgVulkan,
  kWebGpu,
  kWebGlMapLibre,
  kServerRaster
};

struct DrawBackendCapabilities {
  std::string backend_id = "vsg";
  DrawBackendTarget target = DrawBackendTarget::kVsgVulkan;
  std::string device_profile = "generic-gpu";
  std::string material_profile = "neutral-material-v1";
  bool accepts_neutral_model = false;
  bool accepts_gpu_artifacts = true;
  bool supports_swapchain = false;
  bool supports_offscreen = true;
  bool supports_readback = true;
  bool supports_overlay_composition = false;
  std::vector<std::string> fallback_backend_ids;
};

struct DrawInputTierHandle {
  std::string semantic_tier = "tier1_official_chart";
  std::string semantic_owner = "presentation_compiler";
  std::string source_standard;
  std::string registry_id;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> primitive_ids;
};

struct DrawBackendContract {
  std::uint32_t schema_version = kDrawBackendContractSchemaVersion;
  std::string contract_id;
  std::string input_contract = "backend-neutral-nautical-render-model";
  std::string artifact_contract = "machine-local-gpu-artifacts";
  std::string backend_owner = "draw_backend";
  std::string semantic_owner = "presentation_compiler";
  std::string cache_owner = "runtime_gpu_artifact_cache";
  std::string scheduler_owner = "adapter_scheduler";
  std::string visual_tier_policy = "preserve_source_tier_semantics";
  std::string input_model_id;
  std::string input_model_epoch;
  std::string artifact_manifest_id;
  DrawBackendCapabilities capabilities;
  std::vector<DrawInputTierHandle> input_tiers;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(DrawBackendTarget target);

DrawBackendContract BuildDrawBackendContract(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendCapabilities& capabilities = DrawBackendCapabilities{});

bool ValidateDrawBackendContract(
    const DrawBackendContract& contract,
    std::vector<Diagnostic>* diagnostics);

bool ValidateDrawBackendHandoff(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& contract,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
