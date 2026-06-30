// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "helm_webgpu_artifact_consumer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kHelmWebgpuBrowserFixtureSchemaVersion = 1;

struct HelmWebgpuBrowserFeatureProfile {
  std::string profile_id = "helm-browser-webgpu-reference";
  bool webgpu_available = true;
  bool webgpu_device_lost = false;
  bool supports_storage_buffers = true;
  bool supports_texture_arrays = true;
  bool webgl_maplibre_available = true;
  bool server_raster_available = true;
  bool offline_cache_available = true;
};

struct HelmWebgpuComposedArtifact {
  std::string artifact_id;
  HelmWebgpuArtifactFamily family =
      HelmWebgpuArtifactFamily::kCompiledPrimitivePacket;
  HelmWebgpuClientTarget selected_target = HelmWebgpuClientTarget::kWebGpu;
  std::string fallback_route_id;
  std::string semantic_tier;
  std::string semantic_owner;
  std::string source_standard;
  std::string registry_id;
  std::string composition_role;
  std::string browser_decision_scope = "compose_only";
  std::vector<std::string> provenance_refs;
  std::vector<std::string> query_ids;
  bool chart_semantics_server_authoritative = false;
  bool browser_may_decide_chart_semantics = false;
  bool inspection_required = true;
};

struct HelmWebgpuSafetyInspectionTrace {
  std::string trace_id;
  std::string source_chart_id;
  std::string source_object_id;
  std::string presentation_rule_id;
  std::string primitive_id;
  std::string artifact_id;
  std::string final_web_asset_id;
  std::string object_query_id;
  std::string pixel_query_id;
  std::string visibility_state = "server_declared_rendered";
  bool safety_relevant = true;
  bool server_declared_hidden_or_simplified = false;
  bool browser_may_warn_or_query = true;
  bool browser_may_decide_safety_semantics = false;
};

struct HelmWebgpuBrowserConsumerFixture {
  std::uint32_t schema_version = kHelmWebgpuBrowserFixtureSchemaVersion;
  std::string fixture_id = "helm-webgpu-browser-consumer-fixture";
  std::string contract_id;
  std::string feature_profile_id;
  HelmWebgpuClientTarget selected_target = HelmWebgpuClientTarget::kWebGpu;
  std::string fallback_route_id;
  std::string fallback_reason;
  bool webgpu_path_active = true;
  bool server_raster_authority_preserved = true;
  std::vector<HelmWebgpuComposedArtifact> composed_artifacts;
  std::vector<HelmWebgpuSafetyInspectionTrace> safety_traces;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

HelmWebgpuBrowserConsumerFixture BuildHelmWebgpuBrowserConsumerFixture(
    const HelmWebgpuConsumerContract& contract,
    const HelmWebgpuBrowserFeatureProfile& features = {});

bool ValidateHelmWebgpuBrowserConsumerFixture(
    const HelmWebgpuConsumerContract& contract,
    const HelmWebgpuBrowserFeatureProfile& features,
    const HelmWebgpuBrowserConsumerFixture& fixture,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
