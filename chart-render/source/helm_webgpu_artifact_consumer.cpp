// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "helm_webgpu_artifact_consumer.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Keep Helm WebGPU as an artifact consumer: the server-side C++ "
      "pipeline owns chart semantics, while the browser composes packets, "
      "overlays, UI assets, and explicit fallbacks with inspection handles.";
  return diagnostic;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool StartsWith(const std::string& value, const char* prefix) {
  const std::string prefix_text(prefix);
  return value.size() >= prefix_text.size() &&
         value.compare(0, prefix_text.size(), prefix_text) == 0;
}

bool Contains(const std::string& value, const char* token) {
  return ToLower(value).find(token) != std::string::npos;
}

bool ContainsChartStandard(const std::string& value) {
  const std::string lower = ToLower(value);
  return lower.find("s52") != std::string::npos ||
         lower.find("s-52") != std::string::npos ||
         lower.find("s101") != std::string::npos ||
         lower.find("s-101") != std::string::npos;
}

bool ContainsBrowserSemanticPolicy(const std::string& value) {
  const std::string lower = ToLower(value);
  for (const char* token :
       {"s52-rule", "s-52-rule", "s101-rule", "s-101-rule",
        "display-category", "display_category", "scamin",
        "safety-contour", "safety_contour", "chart-source",
        "chart_source", "quilting", "presentation-rule"}) {
    if (lower.find(token) != std::string::npos) return true;
  }
  return false;
}

bool ContainsBackendInternalName(const std::string& value) {
  const std::string lower = ToLower(value);
  return lower.find("vsg") != std::string::npos ||
         lower.find("opencpn") != std::string::npos ||
         lower.find("wx") != std::string::npos;
}

bool IsTier1(const std::string& tier) {
  return StartsWith(tier, "tier1");
}

bool IsTier2(const std::string& tier) {
  return StartsWith(tier, "tier2");
}

bool IsTier3(const std::string& tier) {
  return StartsWith(tier, "tier3");
}

bool IsHelmTier(const std::string& tier) {
  return IsTier2(tier) || IsTier3(tier);
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::kError;
                     });
}

void AddUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) return;
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string FirstSourceStandard(const GpuArtifactCacheManifest& manifest) {
  for (const GpuArtifactRecord& artifact : manifest.artifacts) {
    if (IsTier1(artifact.tier.semantic_tier) &&
        !artifact.tier.source_standard.empty()) {
      return artifact.tier.source_standard;
    }
  }
  return {};
}

std::vector<std::string> OfficialPrimitiveIds(
    const GpuArtifactCacheManifest& manifest) {
  std::vector<std::string> primitive_ids;
  for (const GpuArtifactRecord& artifact : manifest.artifacts) {
    if (!IsTier1(artifact.tier.semantic_tier)) continue;
    for (const std::string& primitive_id : artifact.tier.primitive_ids) {
      AddUnique(&primitive_ids, primitive_id);
    }
  }
  return primitive_ids;
}

std::vector<std::string> OfficialProvenanceRefs(
    const GpuArtifactCacheManifest& manifest) {
  std::vector<std::string> provenance_refs;
  for (const GpuArtifactRecord& artifact : manifest.artifacts) {
    if (!IsTier1(artifact.tier.semantic_tier)) continue;
    for (const std::string& provenance_ref : artifact.tier.provenance_refs) {
      AddUnique(&provenance_refs, provenance_ref);
    }
  }
  return provenance_refs;
}

std::vector<std::string> QueryIds(
    const SourceToRenderInspectionReport& inspection) {
  std::vector<std::string> ids;
  for (const SourceToRenderInspectionRow& row : inspection.rows) {
    AddUnique(&ids, row.query.object_query_id);
    AddUnique(&ids, row.query.pixel_query_id);
    AddUnique(&ids, row.query.hit_test_index_id);
  }
  return ids;
}

std::string ArtifactId(const char* prefix, const NauticalRenderModel& model,
                       const GpuArtifactCacheManifest& artifacts) {
  return std::string(prefix) + ":" + model.model_id + ":" +
         model.source_epoch + ":" + artifacts.manifest_id;
}

HelmWebgpuArtifactSlice OfficialSlice(
    HelmWebgpuArtifactFamily family,
    HelmWebgpuClientTarget target,
    const char* id_prefix,
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const SourceToRenderInspectionReport& inspection,
    const HelmWebgpuConsumerOptions& options) {
  HelmWebgpuArtifactSlice slice;
  slice.artifact_id = ArtifactId(id_prefix, model, artifacts);
  slice.family = family;
  slice.target = target;
  slice.packet_schema = options.packet_schema;
  slice.semantic_tier = "tier1_official_chart";
  slice.semantic_owner = "presentation_compiler";
  slice.source_standard = FirstSourceStandard(artifacts);
  slice.input_model_id = model.model_id;
  slice.input_model_epoch = model.source_epoch;
  slice.artifact_manifest_id = artifacts.manifest_id;
  slice.inspection_report_id = inspection.report_id;
  slice.cache_epoch = model.source_epoch;
  slice.invalidation_epoch = artifacts.options.invalidation_epoch;
  slice.primitive_ids = OfficialPrimitiveIds(artifacts);
  slice.provenance_refs = OfficialProvenanceRefs(artifacts);
  slice.query_ids = QueryIds(inspection);
  slice.byte_estimate = artifacts.stats.estimated_bytes;
  slice.requires_inspection = true;
  return slice;
}

HelmWebgpuArtifactSlice RegistrySlice(
    const HelmWebgpuRegistryAsset& asset,
    const NauticalRenderModel& model,
    const HelmWebgpuConsumerOptions& options) {
  HelmWebgpuArtifactSlice slice;
  slice.artifact_id = asset.asset_id;
  slice.family = IsTier3(asset.semantic_tier)
                     ? HelmWebgpuArtifactFamily::kHelmUiRegistryAsset
                     : HelmWebgpuArtifactFamily::kHelmOverlayRegistryAsset;
  slice.target = HelmWebgpuClientTarget::kWebGpu;
  slice.packet_schema = options.packet_schema;
  slice.semantic_tier = asset.semantic_tier;
  slice.semantic_owner = asset.semantic_owner;
  slice.registry_id = asset.registry_id;
  slice.input_model_id = model.model_id;
  slice.input_model_epoch = model.source_epoch;
  slice.provenance_refs = asset.provenance_refs;
  slice.query_ids = {"registry-query:" + asset.registry_id + ":" +
                     asset.asset_id};
  slice.requires_inspection = asset.inspectable;
  return slice;
}

HelmWebgpuArtifactSlice EnvironmentalSlice(
    const HelmWebgpuEnvironmentalFieldPacket& packet,
    HelmWebgpuArtifactFamily family, const NauticalRenderModel& model,
    const HelmWebgpuConsumerOptions& options) {
  HelmWebgpuArtifactSlice slice;
  slice.artifact_id = family == HelmWebgpuArtifactFamily::kEnvironmentalLegend
                          ? packet.legend_asset_id
                          : packet.packet_id;
  slice.family = family;
  slice.target = HelmWebgpuClientTarget::kWebGpu;
  slice.packet_schema = options.packet_schema + ":environment-field";
  slice.semantic_tier = packet.semantic_tier;
  slice.semantic_owner = packet.semantic_owner;
  slice.registry_id = "environment:" + packet.product_id + ":" +
                      packet.coverage_id;
  slice.input_model_id = model.model_id;
  slice.input_model_epoch = model.source_epoch;
  slice.inspection_report_id = packet.inspection_trace_id;
  slice.cache_epoch = packet.source_epoch;
  slice.invalidation_epoch = packet.source_epoch;
  slice.provenance_refs = {packet.provenance_handle};
  slice.query_ids = {packet.inspection_trace_id};
  slice.requires_inspection = true;
  return slice;
}

HelmWebgpuFallbackRoute Fallback(HelmWebgpuClientTarget to,
                                 std::string reason) {
  HelmWebgpuFallbackRoute route;
  route.from = HelmWebgpuClientTarget::kWebGpu;
  route.to = to;
  route.route_id = std::string("webgpu-to-") + ToString(to);
  route.reason = std::move(reason);
  route.semantic_preserving = true;
  route.visible_diagnostic = true;
  route.uses_server_authority = true;
  return route;
}

std::vector<HelmWebgpuInspectionHook> HooksFromInspection(
    const SourceToRenderInspectionReport& inspection) {
  std::vector<HelmWebgpuInspectionHook> hooks;
  for (const SourceToRenderInspectionRow& row : inspection.rows) {
    HelmWebgpuInspectionHook hook;
    hook.hook_id = "helm-webgpu:" + row.row_id;
    hook.semantic_tier = row.tier.semantic_tier;
    hook.semantic_owner = row.tier.semantic_owner;
    hook.source_chart_id = row.source.source_chart_id;
    hook.source_object_id = row.source.source_object_id;
    hook.presentation_rule_id = row.presentation.presentation_rule_id;
    hook.primitive_id = row.presentation.primitive_id;
    hook.artifact_id = row.artifacts.empty()
                           ? row.backend.final_web_asset_id
                           : row.artifacts.front().artifact_id;
    hook.object_query_id = row.query.object_query_id;
    hook.pixel_query_id = row.query.pixel_query_id;
    hook.final_web_asset_id = row.backend.final_web_asset_id;
    hook.wrong_location_owner = row.tier.wrong_location_owner;
    hook.wrong_symbol_owner = row.tier.wrong_symbol_owner;
    hooks.push_back(std::move(hook));
  }
  return hooks;
}

bool HasTargetFallback(const HelmWebgpuConsumerContract& contract,
                       HelmWebgpuClientTarget to) {
  return std::any_of(
      contract.fallbacks.begin(), contract.fallbacks.end(),
      [&](const HelmWebgpuFallbackRoute& route) {
        return route.from == HelmWebgpuClientTarget::kWebGpu &&
               route.to == to && route.semantic_preserving &&
               route.visible_diagnostic && route.uses_server_authority;
      });
}

bool HasComponentRole(const HelmWebgpuEnvironmentalTimeSlice& slice,
                      const char* role) {
  return std::any_of(
      slice.components.begin(), slice.components.end(),
      [&](const HelmWebgpuEnvironmentalTextureComponent& component) {
        return component.component_role == role;
      });
}

bool HasFallbackRouteId(const HelmWebgpuConsumerContract& contract,
                        const std::string& route_id) {
  return std::any_of(contract.fallbacks.begin(), contract.fallbacks.end(),
                     [&](const HelmWebgpuFallbackRoute& route) {
                       return route.route_id == route_id &&
                              route.semantic_preserving &&
                              route.visible_diagnostic &&
                              route.uses_server_authority;
                     });
}

bool HasInspectionHookForPrimitive(
    const HelmWebgpuConsumerContract& contract,
    const std::string& primitive_id) {
  return std::any_of(
      contract.inspection_hooks.begin(), contract.inspection_hooks.end(),
      [&](const HelmWebgpuInspectionHook& hook) {
        return hook.primitive_id == primitive_id &&
               !hook.object_query_id.empty() &&
               !hook.pixel_query_id.empty() &&
               !hook.final_web_asset_id.empty();
      });
}

bool ValidateEnvironmentalField(
    const HelmWebgpuEnvironmentalFieldPacket& packet,
    const HelmWebgpuConsumerContract& contract,
    std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  const std::vector<std::string> provenance =
      packet.provenance_handle.empty()
          ? std::vector<std::string>{}
          : std::vector<std::string>{packet.provenance_handle};

  if (packet.packet_id.empty() || packet.product_id.empty() ||
      packet.variable_name.empty() || packet.source_authority.empty() ||
      packet.source_epoch.empty() || packet.coverage_id.empty() ||
      packet.cache_namespace.empty() || packet.provenance_handle.empty() ||
      packet.inspection_trace_id.empty() ||
      packet.fallback_raster_route_id.empty() ||
      packet.fallback_reason.empty() || packet.time_slices.empty()) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_env_identity",
        "Environmental field packet is missing identity, source, cache, "
        "inspection, fallback, or time-slice metadata.",
        provenance));
    ok = false;
  }
  if (!IsHelmTier(packet.semantic_tier) ||
      !Contains(packet.semantic_owner, "helm") ||
      packet.semantic_owner == "presentation_compiler") {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_env_tier",
        "Environmental products are Helm overlay packets, not Tier 1 chart "
        "truth or presentation compiler output.",
        provenance));
    ok = false;
  }
  if (ContainsBrowserSemanticPolicy(packet.packet_id) ||
      ContainsBrowserSemanticPolicy(packet.product_id) ||
      ContainsBackendInternalName(packet.packet_id) ||
      ContainsBackendInternalName(packet.cache_namespace)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_env_boundary",
        "Environmental packet identity must not leak chart semantic policy or "
        "VSG/OpenCPN backend internals into Helm browser code.",
        provenance));
    ok = false;
  }
  if (!HasFallbackRouteId(contract, packet.fallback_raster_route_id)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_env_fallback",
        "Environmental field packet must reference a visible "
        "semantic-preserving fallback route.",
        provenance));
    ok = false;
  }

  for (const HelmWebgpuEnvironmentalTimeSlice& slice :
       packet.time_slices) {
    if (slice.reference_time.empty() || slice.valid_time_start.empty() ||
        slice.valid_time_end.empty() || slice.interpolation_mode.empty() ||
        slice.lod_parent_packet_id.empty() || slice.components.empty()) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_env_time_slice",
          "Environmental time slice must carry reference/valid time, "
          "interpolation, LOD parent fallback, and texture components.",
          provenance));
      ok = false;
    }
    if (packet.field_kind == HelmWebgpuEnvironmentalFieldKind::kScalarTexture &&
        !HasComponentRole(slice, "scalar")) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_env_scalar_component",
          "Scalar environmental field packet is missing a scalar texture "
          "component.",
          provenance));
      ok = false;
    }
    if ((packet.field_kind ==
             HelmWebgpuEnvironmentalFieldKind::kVectorUvTexture ||
         packet.field_kind ==
             HelmWebgpuEnvironmentalFieldKind::kParticleVectorField) &&
        (!HasComponentRole(slice, "u") || !HasComponentRole(slice, "v"))) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_env_vector_components",
          "Vector environmental field packet must carry both u and v texture "
          "components.",
          provenance));
      ok = false;
    }
    for (const HelmWebgpuEnvironmentalTextureComponent& component :
         slice.components) {
      if (component.component_role.empty() || component.texture_id.empty() ||
          component.unit.empty() || component.sample_encoding.empty() ||
          component.no_data_mask_id.empty() || component.cache_key.empty() ||
          component.provenance_refs.empty() ||
          !component.values_are_normalized) {
        diagnostics->push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "helm_webgpu_env_texture_component",
            "Environmental texture component must carry role, texture, unit, "
            "sample encoding, no-data mask, cache key, normalized values, "
            "and provenance refs.",
            component.provenance_refs));
        ok = false;
      }
      if (ContainsBackendInternalName(component.texture_id) ||
          ContainsBackendInternalName(component.cache_key)) {
        diagnostics->push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "helm_webgpu_env_boundary",
            "Environmental texture component must use neutral cache handles, "
            "not VSG/OpenCPN backend internals.",
            component.provenance_refs));
        ok = false;
      }
    }
  }
  return ok;
}

bool ValidateArtifact(const HelmWebgpuArtifactSlice& artifact,
                      std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  if (artifact.artifact_id.empty() || artifact.packet_schema.empty() ||
      artifact.input_model_id.empty()) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_artifact_identity",
        "Helm WebGPU artifact slice is missing identity or schema.",
        artifact.provenance_refs));
    ok = false;
  }
  if (!artifact.browser_may_compose ||
      artifact.browser_may_decide_chart_semantics) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_semantic_authority",
        "Helm browser artifacts may compose but must not decide chart "
        "semantics.",
        artifact.provenance_refs));
    ok = false;
  }
  if (ContainsBrowserSemanticPolicy(artifact.artifact_id) ||
      ContainsBrowserSemanticPolicy(artifact.packet_schema) ||
      ContainsBrowserSemanticPolicy(artifact.registry_id)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_policy_leak",
        "Helm browser artifact identity contains chart semantic policy words.",
        artifact.provenance_refs));
    ok = false;
  }

  if (IsTier1(artifact.semantic_tier)) {
    if (artifact.semantic_owner != "presentation_compiler" ||
        artifact.source_standard.empty() ||
        (artifact.primitive_ids.empty() && artifact.query_ids.empty())) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_tier1_owner",
          "Tier 1 official chart artifacts must come from the presentation "
          "pipeline and carry source standard plus primitive or query handles.",
          artifact.provenance_refs));
      ok = false;
    }
    if (!artifact.registry_id.empty()) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_tier_masquerade",
          "Tier 1 official chart artifacts must not be represented as Helm "
          "registry assets.",
          artifact.provenance_refs));
      ok = false;
    }
  } else if (IsHelmTier(artifact.semantic_tier)) {
    if (artifact.registry_id.empty() ||
        !Contains(artifact.semantic_owner, "helm") ||
        artifact.semantic_owner == "presentation_compiler" ||
        ContainsChartStandard(artifact.source_standard)) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_tier_masquerade",
          "Tier 2/3 Helm overlay and UI assets need Helm registry ownership "
          "and must not masquerade as S-52/S-101 chart truth.",
          artifact.provenance_refs));
      ok = false;
    }
  } else {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_unknown_tier",
        "Helm WebGPU artifact has an unknown visual tier.",
        artifact.provenance_refs));
    ok = false;
  }
  return ok;
}

}  // namespace

const char* ToString(HelmWebgpuClientTarget target) {
  switch (target) {
    case HelmWebgpuClientTarget::kWebGpu:
      return "webgpu";
    case HelmWebgpuClientTarget::kWebGlMapLibre:
      return "webgl_maplibre";
    case HelmWebgpuClientTarget::kServerRaster:
      return "server_raster";
  }
  return "unknown";
}

const char* ToString(HelmWebgpuArtifactFamily family) {
  switch (family) {
    case HelmWebgpuArtifactFamily::kCompiledPrimitivePacket:
      return "compiled_primitive_packet";
    case HelmWebgpuArtifactFamily::kInspectionPacket:
      return "inspection_packet";
    case HelmWebgpuArtifactFamily::kRasterFallbackTile:
      return "raster_fallback_tile";
    case HelmWebgpuArtifactFamily::kOfflinePack:
      return "offline_pack";
    case HelmWebgpuArtifactFamily::kEnvironmentalFieldPacket:
      return "environmental_field_packet";
    case HelmWebgpuArtifactFamily::kEnvironmentalLegend:
      return "environmental_legend";
    case HelmWebgpuArtifactFamily::kHelmOverlayRegistryAsset:
      return "helm_overlay_registry_asset";
    case HelmWebgpuArtifactFamily::kHelmUiRegistryAsset:
      return "helm_ui_registry_asset";
  }
  return "unknown";
}

const char* ToString(HelmWebgpuEnvironmentalFieldKind kind) {
  switch (kind) {
    case HelmWebgpuEnvironmentalFieldKind::kScalarTexture:
      return "scalar_texture";
    case HelmWebgpuEnvironmentalFieldKind::kVectorUvTexture:
      return "vector_uv_texture";
    case HelmWebgpuEnvironmentalFieldKind::kParticleVectorField:
      return "particle_vector_field";
    case HelmWebgpuEnvironmentalFieldKind::kLegend:
      return "legend";
  }
  return "unknown";
}

const char* ToString(HelmWebgpuEnvironmentalProductFamily family) {
  switch (family) {
    case HelmWebgpuEnvironmentalProductFamily::kAdvisoryOpenMeteo:
      return "advisory_open_meteo";
    case HelmWebgpuEnvironmentalProductFamily::kAdvisoryOpenMarine:
      return "advisory_open_marine";
    case HelmWebgpuEnvironmentalProductFamily::kS100S412:
      return "s100_s412";
    case HelmWebgpuEnvironmentalProductFamily::kS100S413:
      return "s100_s413";
    case HelmWebgpuEnvironmentalProductFamily::kS100S414:
      return "s100_s414";
  }
  return "unknown";
}

std::vector<HelmWebgpuEnvironmentalFieldPacket>
BuildHelmWebgpuEnvironmentalFieldExamples(std::string source_epoch) {
  auto component = [&](std::string role, std::string packet_id,
                       std::string unit) {
    HelmWebgpuEnvironmentalTextureComponent result;
    result.component_role = std::move(role);
    result.texture_id = "env-texture:" + packet_id + ":" +
                        result.component_role + ":" + source_epoch;
    result.unit = std::move(unit);
    result.no_data_mask_id = "env-mask:" + packet_id + ":" + source_epoch;
    result.cache_key = "env-cache:" + packet_id + ":" +
                       result.component_role + ":" + source_epoch;
    result.provenance_refs = {"prov:" + packet_id + ":" + source_epoch};
    return result;
  };

  auto time_slice = [&](const std::string& packet_id,
                        std::vector<HelmWebgpuEnvironmentalTextureComponent>
                            components) {
    HelmWebgpuEnvironmentalTimeSlice slice;
    slice.reference_time = "2026-03-17T00:00:00Z";
    slice.valid_time_start = "2026-03-17T00:00:00Z";
    slice.valid_time_end = "2026-03-17T06:00:00Z";
    slice.interpolation_mode = "source-declared-linear";
    slice.lod_parent_packet_id = "env-parent:" + packet_id + ":z5";
    slice.components = std::move(components);
    return slice;
  };

  auto packet = [&](std::string id,
                    HelmWebgpuEnvironmentalFieldKind kind,
                    HelmWebgpuEnvironmentalProductFamily family,
                    std::string product_id, std::string variable_name,
                    std::string authority) {
    HelmWebgpuEnvironmentalFieldPacket result;
    result.packet_id = id;
    result.field_kind = kind;
    result.product_family = family;
    result.product_id = std::move(product_id);
    result.variable_name = std::move(variable_name);
    result.source_authority = std::move(authority);
    result.source_epoch = source_epoch;
    result.coverage_id = "coverage:" + result.product_id;
    result.provenance_handle = "prov:" + result.product_id + ":" +
                               source_epoch;
    result.inspection_trace_id = "inspect:" + id;
    result.legend_asset_id = "legend:" + id;
    result.fallback_reason =
        "Use server-authoritative raster overlay when WebGPU field textures "
        "or time interpolation are unavailable.";
    return result;
  };

  std::vector<HelmWebgpuEnvironmentalFieldPacket> packets;

  HelmWebgpuEnvironmentalFieldPacket wind = packet(
      "env:open-meteo:wind10m",
      HelmWebgpuEnvironmentalFieldKind::kVectorUvTexture,
      HelmWebgpuEnvironmentalProductFamily::kAdvisoryOpenMeteo,
      "open-meteo-wind10m", "wind_10m", "Open-Meteo advisory model");
  wind.advisory_constraints = {
      "advisory_forecast_not_official_navigation_warning"};
  wind.time_slices.push_back(time_slice(
      wind.packet_id,
      {component("u", wind.packet_id, "m/s"),
       component("v", wind.packet_id, "m/s")}));
  packets.push_back(std::move(wind));

  HelmWebgpuEnvironmentalFieldPacket wave = packet(
      "env:open-marine:wave-height",
      HelmWebgpuEnvironmentalFieldKind::kScalarTexture,
      HelmWebgpuEnvironmentalProductFamily::kAdvisoryOpenMarine,
      "open-marine-wave-height", "significant_wave_height",
      "Open-Marine advisory model");
  wave.advisory_constraints = {
      "advisory_forecast_not_official_s100_product"};
  wave.time_slices.push_back(time_slice(
      wave.packet_id, {component("scalar", wave.packet_id, "m")}));
  packets.push_back(std::move(wave));

  HelmWebgpuEnvironmentalFieldPacket warning = packet(
      "env:s100:s412:weather-warning",
      HelmWebgpuEnvironmentalFieldKind::kScalarTexture,
      HelmWebgpuEnvironmentalProductFamily::kS100S412,
      "s100-s412-weather-warning", "weather_warning_area",
      "official S-100 producing authority");
  warning.time_slices.push_back(time_slice(
      warning.packet_id, {component("scalar", warning.packet_id, "category")}));
  packets.push_back(std::move(warning));

  HelmWebgpuEnvironmentalFieldPacket conditions = packet(
      "env:s100:s413:metocean-conditions",
      HelmWebgpuEnvironmentalFieldKind::kVectorUvTexture,
      HelmWebgpuEnvironmentalProductFamily::kS100S413,
      "s100-s413-metocean-conditions", "surface_current",
      "official S-100 producing authority");
  conditions.time_slices.push_back(time_slice(
      conditions.packet_id,
      {component("u", conditions.packet_id, "m/s"),
       component("v", conditions.packet_id, "m/s")}));
  packets.push_back(std::move(conditions));

  HelmWebgpuEnvironmentalFieldPacket observations = packet(
      "env:s100:s414:weather-observation",
      HelmWebgpuEnvironmentalFieldKind::kScalarTexture,
      HelmWebgpuEnvironmentalProductFamily::kS100S414,
      "s100-s414-weather-observation", "observed_air_pressure",
      "official S-100 producing authority");
  observations.time_slices.push_back(time_slice(
      observations.packet_id, {component("scalar", observations.packet_id, "hPa")}));
  packets.push_back(std::move(observations));

  return packets;
}

HelmWebgpuConsumerContract BuildHelmWebgpuConsumerContract(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& draw_contract,
    const SourceToRenderInspectionReport& inspection,
    const HelmWebgpuConsumerOptions& options) {
  HelmWebgpuConsumerContract contract;
  contract.primary_target = options.primary_target;
  contract.client_id = options.client_id;
  contract.route_prefix = options.route_prefix;
  contract.input_model_id = model.model_id;
  contract.input_model_epoch = model.source_epoch;
  contract.artifact_manifest_id = artifacts.manifest_id;
  contract.draw_backend_contract_id = draw_contract.contract_id;
  contract.inspection_report_id = inspection.report_id;

  if (options.contract_id.empty()) {
    std::ostringstream id;
    id << options.client_id << ":" << ToString(options.primary_target) << ":"
       << model.model_id << ":" << model.source_epoch << ":"
       << artifacts.manifest_id;
    contract.contract_id = id.str();
  } else {
    contract.contract_id = options.contract_id;
  }

  contract.artifacts.push_back(OfficialSlice(
      HelmWebgpuArtifactFamily::kCompiledPrimitivePacket,
      HelmWebgpuClientTarget::kWebGpu, "compiled-primitives", model,
      artifacts, inspection, options));
  contract.artifacts.push_back(OfficialSlice(
      HelmWebgpuArtifactFamily::kInspectionPacket,
      HelmWebgpuClientTarget::kWebGpu, "inspection-packet", model,
      artifacts, inspection, options));
  if (options.include_raster_fallback) {
    contract.artifacts.push_back(OfficialSlice(
        HelmWebgpuArtifactFamily::kRasterFallbackTile,
        HelmWebgpuClientTarget::kServerRaster, "raster-fallback", model,
        artifacts, inspection, options));
  }
  if (options.include_offline_pack) {
    contract.artifacts.push_back(OfficialSlice(
        HelmWebgpuArtifactFamily::kOfflinePack,
        HelmWebgpuClientTarget::kWebGpu, "offline-pack", model, artifacts,
        inspection, options));
  }
  contract.environmental_fields = options.environmental_fields;
  for (const HelmWebgpuEnvironmentalFieldPacket& field :
       contract.environmental_fields) {
    contract.artifacts.push_back(EnvironmentalSlice(
        field, HelmWebgpuArtifactFamily::kEnvironmentalFieldPacket, model,
        options));
    if (!field.legend_asset_id.empty()) {
      contract.artifacts.push_back(EnvironmentalSlice(
          field, HelmWebgpuArtifactFamily::kEnvironmentalLegend, model,
          options));
    }
  }
  for (const HelmWebgpuRegistryAsset& asset : options.registry_assets) {
    contract.artifacts.push_back(RegistrySlice(asset, model, options));
  }

  contract.fallbacks.push_back(Fallback(
      HelmWebgpuClientTarget::kWebGlMapLibre,
      "WebGPU unavailable, disabled, or below required feature profile."));
  contract.fallbacks.push_back(Fallback(
      HelmWebgpuClientTarget::kServerRaster,
      "Verification path, unsupported client, or semantic-preserving safety "
      "fallback requested."));
  contract.inspection_hooks = HooksFromInspection(inspection);

  contract.diagnostics = artifacts.diagnostics;
  contract.diagnostics.insert(contract.diagnostics.end(),
                              draw_contract.diagnostics.begin(),
                              draw_contract.diagnostics.end());
  contract.diagnostics.insert(contract.diagnostics.end(),
                              inspection.diagnostics.begin(),
                              inspection.diagnostics.end());
  return contract;
}

bool ValidateHelmWebgpuConsumerContract(
    const HelmWebgpuConsumerContract& contract,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (contract.schema_version != kHelmWebgpuArtifactConsumerSchemaVersion ||
      contract.contract_id.empty() || contract.client_id.empty() ||
      contract.route_prefix.empty() || contract.input_model_id.empty() ||
      contract.input_model_epoch.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_identity",
        "Helm WebGPU consumer contract has invalid identity."));
    ok = false;
  }
  if (contract.input_contract != "backend-neutral-nautical-render-model" ||
      contract.artifact_contract != "machine-local-gpu-artifacts" ||
      contract.inspection_contract != "source-to-render-inspection" ||
      contract.client_owner != "helm_browser_client" ||
      contract.semantic_owner != "presentation_compiler" ||
      contract.cache_owner != "runtime_gpu_artifact_cache" ||
      contract.backend_owner != "draw_backend" ||
      contract.scheduler_owner != "adapter_scheduler") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_boundary",
        "Helm WebGPU consumer must preserve server semantic, cache, backend, "
        "scheduler, and inspection ownership."));
    ok = false;
  }
  if (contract.primary_target != HelmWebgpuClientTarget::kWebGpu) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_primary_target",
        "HELMWEBGPU-1 requires WebGPU as the primary Helm client target."));
    ok = false;
  }
  if (Contains(contract.client_id, "vsg") || Contains(contract.route_prefix, "vsg")) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_vsg_internals",
        "Helm WebGPU consumer contract must not expose VSG internals."));
    ok = false;
  }
  if (contract.artifacts.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_artifacts_empty",
        "Helm WebGPU consumer contract has no artifact slices."));
    ok = false;
  }

  bool saw_tier1 = false;
  bool saw_helm_registry = false;
  bool saw_compiled_packet = false;
  bool saw_inspection_packet = false;
  bool saw_raster_fallback = false;
  for (const HelmWebgpuArtifactSlice& artifact : contract.artifacts) {
    ok = ValidateArtifact(artifact, &out) && ok;
    saw_tier1 = saw_tier1 || IsTier1(artifact.semantic_tier);
    saw_helm_registry =
        saw_helm_registry || IsHelmTier(artifact.semantic_tier);
    saw_compiled_packet =
        saw_compiled_packet ||
        artifact.family == HelmWebgpuArtifactFamily::kCompiledPrimitivePacket;
    saw_inspection_packet =
        saw_inspection_packet ||
        artifact.family == HelmWebgpuArtifactFamily::kInspectionPacket;
    saw_raster_fallback =
        saw_raster_fallback ||
        artifact.family == HelmWebgpuArtifactFamily::kRasterFallbackTile;
    if (IsTier1(artifact.semantic_tier) && artifact.requires_inspection) {
      for (const std::string& primitive_id : artifact.primitive_ids) {
        if (!HasInspectionHookForPrimitive(contract, primitive_id)) {
          out.push_back(MakeDiagnostic(
              DiagnosticSeverity::kError, "helm_webgpu_inspection_hook",
              "Tier 1 browser artifact is missing source-to-render query "
              "hooks for a primitive.",
              artifact.provenance_refs));
          ok = false;
          break;
        }
      }
    }
  }
  if (!saw_tier1 || !saw_helm_registry) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_tier_coverage",
        "HELMWEBGPU-1 must distinguish Tier 1 official chart artifacts from "
        "Tier 2/3 Helm overlay or UI registry assets."));
    ok = false;
  }
  if (!saw_compiled_packet || !saw_inspection_packet || !saw_raster_fallback) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_packet_coverage",
        "Helm WebGPU consumer contract must include compiled primitive, "
        "inspection, and server-raster fallback artifact families."));
    ok = false;
  }
  if (!HasTargetFallback(contract, HelmWebgpuClientTarget::kWebGlMapLibre) ||
      !HasTargetFallback(contract, HelmWebgpuClientTarget::kServerRaster)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fallback",
        "Helm WebGPU consumer contract must define visible semantic-preserving "
        "WebGL/MapLibre and server-raster fallbacks."));
    ok = false;
  }
  for (const HelmWebgpuFallbackRoute& fallback : contract.fallbacks) {
    if (fallback.route_id.empty() || fallback.from != HelmWebgpuClientTarget::kWebGpu ||
        !fallback.semantic_preserving || !fallback.visible_diagnostic ||
        !fallback.uses_server_authority) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_fallback",
          "Helm WebGPU fallback route is not visible, semantic-preserving, "
          "and backed by server authority."));
      ok = false;
    }
  }
  for (const HelmWebgpuEnvironmentalFieldPacket& field :
       contract.environmental_fields) {
    ok = ValidateEnvironmentalField(field, contract, &out) && ok;
  }
  if (contract.inspection_hooks.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_inspection_hook",
        "Helm WebGPU consumer contract has no object/pixel inspection hooks."));
    ok = false;
  }
  for (const HelmWebgpuInspectionHook& hook : contract.inspection_hooks) {
    if (hook.hook_id.empty() || hook.primitive_id.empty() ||
        hook.artifact_id.empty() || hook.object_query_id.empty() ||
        hook.pixel_query_id.empty() || hook.final_web_asset_id.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_inspection_hook",
          "Helm WebGPU inspection hook is missing primitive, artifact, query, "
          "or final web asset handles."));
      ok = false;
    }
    if (IsTier1(hook.semantic_tier)) {
      if (hook.semantic_owner != "presentation_compiler" ||
          hook.source_chart_id.empty() || hook.source_object_id.empty() ||
          hook.presentation_rule_id.empty() ||
          Contains(hook.wrong_location_owner, "helm")) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "helm_webgpu_tier1_inspection",
            "Tier 1 WebGPU inspection hooks must route official chart truth "
            "back to source/converter/presentation ownership."));
        ok = false;
      }
    } else if (IsHelmTier(hook.semantic_tier)) {
      if (!Contains(hook.wrong_location_owner, "helm") ||
          !Contains(hook.wrong_symbol_owner, "helm")) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "helm_webgpu_overlay_inspection",
            "Tier 2/3 WebGPU inspection hooks must route overlay/UI issues "
            "to Helm ownership."));
        ok = false;
      }
    }
  }
  return ok;
}

bool ValidateHelmWebgpuConsumerHandoff(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& draw_contract,
    const SourceToRenderInspectionReport& inspection,
    const HelmWebgpuConsumerContract& contract,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateHelmWebgpuConsumerContract(contract, &out);
  ok = ValidateNauticalRenderModel(model, &out) && ok;
  ok = ValidateGpuArtifactCacheManifest(artifacts, &out) && ok;
  ok = ValidateDrawBackendHandoff(model, artifacts, draw_contract, &out) && ok;
  ok = ValidateSourceToRenderInspectionReport(inspection, &out) && ok;

  if (draw_contract.capabilities.target != DrawBackendTarget::kWebGpu ||
      artifacts.options.backend_target != "webgpu") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_backend_target",
        "Helm artifact consumer handoff must use the WebGPU draw-backend "
        "target, not VSG/OpenCPN backend internals."));
    ok = false;
  }
  if (!draw_contract.capabilities.supports_overlay_composition) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_overlay_composition",
        "Helm WebGPU target must declare overlay/UI composition support."));
    ok = false;
  }
  if (contract.input_model_id != model.model_id ||
      contract.input_model_epoch != model.source_epoch ||
      contract.artifact_manifest_id != artifacts.manifest_id ||
      contract.draw_backend_contract_id != draw_contract.contract_id ||
      contract.inspection_report_id != inspection.report_id) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_input_mismatch",
        "Helm WebGPU consumer contract does not match the model, artifacts, "
        "backend contract, or inspection report."));
    ok = false;
  }
  if (!inspection.ok || HasError(contract.diagnostics)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_upstream_diagnostics",
        "Helm WebGPU consumer cannot hide upstream inspection or artifact "
        "errors behind a green client contract."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render
