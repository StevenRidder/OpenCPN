// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "draw_backend_contract.hpp"
#include "source_to_render_inspection.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kHelmWebgpuArtifactConsumerSchemaVersion = 1;

enum class HelmWebgpuClientTarget {
  kWebGpu,
  kWebGlMapLibre,
  kServerRaster
};

enum class HelmWebgpuArtifactFamily {
  kCompiledPrimitivePacket,
  kInspectionPacket,
  kRasterFallbackTile,
  kOfflinePack,
  kEnvironmentalFieldPacket,
  kEnvironmentalLegend,
  kHelmOverlayRegistryAsset,
  kHelmUiRegistryAsset
};

enum class HelmWebgpuEnvironmentalFieldKind {
  kScalarTexture,
  kVectorUvTexture,
  kParticleVectorField,
  kLegend
};

enum class HelmWebgpuEnvironmentalProductFamily {
  kAdvisoryOpenMeteo,
  kAdvisoryOpenMarine,
  kS100S412,
  kS100S413,
  kS100S414
};

struct HelmWebgpuRegistryAsset {
  std::string asset_id;
  std::string registry_id;
  std::string semantic_tier = "tier2_helm_overlay";
  std::string semantic_owner = "helm_overlay_registry";
  std::string asset_role;
  std::vector<std::string> provenance_refs;
  bool inspectable = true;
};

struct HelmWebgpuEnvironmentalTextureComponent {
  std::string component_role;
  std::string texture_id;
  std::string unit;
  std::string sample_encoding = "float32";
  std::string no_data_mask_id;
  std::string cache_key;
  std::vector<std::string> provenance_refs;
  bool values_are_normalized = true;
};

struct HelmWebgpuEnvironmentalTimeSlice {
  std::string reference_time;
  std::string valid_time_start;
  std::string valid_time_end;
  std::string interpolation_mode = "source-declared-linear";
  std::string lod_parent_packet_id;
  std::vector<HelmWebgpuEnvironmentalTextureComponent> components;
};

struct HelmWebgpuEnvironmentalFieldPacket {
  std::string packet_id;
  HelmWebgpuEnvironmentalFieldKind field_kind =
      HelmWebgpuEnvironmentalFieldKind::kScalarTexture;
  HelmWebgpuEnvironmentalProductFamily product_family =
      HelmWebgpuEnvironmentalProductFamily::kAdvisoryOpenMeteo;
  std::string product_id;
  std::string variable_name;
  std::string semantic_tier = "tier2_helm_overlay";
  std::string semantic_owner = "helm_environment_registry";
  std::string source_authority;
  std::string source_epoch;
  std::string coverage_id;
  std::string cache_namespace = "helm-environment-fields";
  std::string provenance_handle;
  std::string inspection_trace_id;
  std::string legend_asset_id;
  std::string fallback_raster_route_id = "webgpu-to-server_raster";
  std::string fallback_reason;
  std::vector<std::string> advisory_constraints;
  std::vector<HelmWebgpuEnvironmentalTimeSlice> time_slices;
};

struct HelmWebgpuConsumerOptions {
  std::string contract_id;
  std::string client_id = "helm-browser-client";
  std::string packet_schema = "helm-webgpu-artifact-consumer-v1";
  std::string route_prefix = "/chart";
  HelmWebgpuClientTarget primary_target = HelmWebgpuClientTarget::kWebGpu;
  bool include_raster_fallback = true;
  bool include_offline_pack = true;
  std::vector<HelmWebgpuRegistryAsset> registry_assets;
  std::vector<HelmWebgpuEnvironmentalFieldPacket> environmental_fields;
};

struct HelmWebgpuArtifactSlice {
  std::string artifact_id;
  HelmWebgpuArtifactFamily family =
      HelmWebgpuArtifactFamily::kCompiledPrimitivePacket;
  HelmWebgpuClientTarget target = HelmWebgpuClientTarget::kWebGpu;
  std::string packet_schema;
  std::string semantic_tier = "tier1_official_chart";
  std::string semantic_owner = "presentation_compiler";
  std::string source_standard;
  std::string registry_id;
  std::string input_model_id;
  std::string input_model_epoch;
  std::string artifact_manifest_id;
  std::string inspection_report_id;
  std::string cache_epoch;
  std::string invalidation_epoch;
  std::vector<std::string> primitive_ids;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> query_ids;
  std::uint64_t byte_estimate = 0;
  bool browser_may_compose = true;
  bool browser_may_decide_chart_semantics = false;
  bool requires_inspection = true;
};

struct HelmWebgpuFallbackRoute {
  HelmWebgpuClientTarget from = HelmWebgpuClientTarget::kWebGpu;
  HelmWebgpuClientTarget to = HelmWebgpuClientTarget::kWebGlMapLibre;
  std::string route_id;
  std::string reason;
  bool semantic_preserving = true;
  bool visible_diagnostic = true;
  bool uses_server_authority = true;
};

struct HelmWebgpuInspectionHook {
  std::string hook_id;
  std::string semantic_tier;
  std::string semantic_owner;
  std::string source_chart_id;
  std::string source_object_id;
  std::string presentation_rule_id;
  std::string primitive_id;
  std::string artifact_id;
  std::string object_query_id;
  std::string pixel_query_id;
  std::string final_web_asset_id;
  std::string wrong_location_owner;
  std::string wrong_symbol_owner;
};

struct HelmWebgpuConsumerContract {
  std::uint32_t schema_version =
      kHelmWebgpuArtifactConsumerSchemaVersion;
  std::string contract_id;
  std::string input_contract = "backend-neutral-nautical-render-model";
  std::string artifact_contract = "machine-local-gpu-artifacts";
  std::string inspection_contract = "source-to-render-inspection";
  std::string client_owner = "helm_browser_client";
  std::string semantic_owner = "presentation_compiler";
  std::string cache_owner = "runtime_gpu_artifact_cache";
  std::string backend_owner = "draw_backend";
  std::string scheduler_owner = "adapter_scheduler";
  HelmWebgpuClientTarget primary_target = HelmWebgpuClientTarget::kWebGpu;
  std::string client_id;
  std::string route_prefix;
  std::string input_model_id;
  std::string input_model_epoch;
  std::string artifact_manifest_id;
  std::string draw_backend_contract_id;
  std::string inspection_report_id;
  std::vector<HelmWebgpuArtifactSlice> artifacts;
  std::vector<HelmWebgpuEnvironmentalFieldPacket> environmental_fields;
  std::vector<HelmWebgpuFallbackRoute> fallbacks;
  std::vector<HelmWebgpuInspectionHook> inspection_hooks;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(HelmWebgpuClientTarget target);
const char* ToString(HelmWebgpuArtifactFamily family);
const char* ToString(HelmWebgpuEnvironmentalFieldKind kind);
const char* ToString(HelmWebgpuEnvironmentalProductFamily family);

std::vector<HelmWebgpuEnvironmentalFieldPacket>
BuildHelmWebgpuEnvironmentalFieldExamples(std::string source_epoch);

HelmWebgpuConsumerContract BuildHelmWebgpuConsumerContract(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& draw_contract,
    const SourceToRenderInspectionReport& inspection,
    const HelmWebgpuConsumerOptions& options = {});

bool ValidateHelmWebgpuConsumerContract(
    const HelmWebgpuConsumerContract& contract,
    std::vector<Diagnostic>* diagnostics);

bool ValidateHelmWebgpuConsumerHandoff(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& draw_contract,
    const SourceToRenderInspectionReport& inspection,
    const HelmWebgpuConsumerContract& contract,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
