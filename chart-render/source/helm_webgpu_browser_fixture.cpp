// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "helm_webgpu_browser_fixture.hpp"

#include <algorithm>
#include <cctype>
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
      "Keep the Helm browser fixture as a consumer of server-produced chart "
      "artifacts. Feature detection may choose WebGPU or a visible fallback, "
      "but chart portrayal, safety semantics, and source authority stay in "
      "the C++ presentation/inspection contracts.";
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

bool WebGpuUsable(const HelmWebgpuBrowserFeatureProfile& features) {
  return features.webgpu_available && !features.webgpu_device_lost &&
         features.supports_storage_buffers && features.supports_texture_arrays;
}

const HelmWebgpuFallbackRoute* FindFallback(
    const HelmWebgpuConsumerContract& contract,
    HelmWebgpuClientTarget target) {
  for (const HelmWebgpuFallbackRoute& fallback : contract.fallbacks) {
    if (fallback.to == target && fallback.semantic_preserving &&
        fallback.visible_diagnostic && fallback.uses_server_authority) {
      return &fallback;
    }
  }
  return nullptr;
}

HelmWebgpuClientTarget SelectTarget(
    const HelmWebgpuConsumerContract& contract,
    const HelmWebgpuBrowserFeatureProfile& features,
    const HelmWebgpuFallbackRoute** selected_fallback) {
  *selected_fallback = nullptr;
  if (WebGpuUsable(features)) return HelmWebgpuClientTarget::kWebGpu;
  if (features.webgl_maplibre_available) {
    *selected_fallback =
        FindFallback(contract, HelmWebgpuClientTarget::kWebGlMapLibre);
    if (*selected_fallback != nullptr) {
      return HelmWebgpuClientTarget::kWebGlMapLibre;
    }
  }
  if (features.server_raster_available) {
    *selected_fallback =
        FindFallback(contract, HelmWebgpuClientTarget::kServerRaster);
    if (*selected_fallback != nullptr) {
      return HelmWebgpuClientTarget::kServerRaster;
    }
  }
  return HelmWebgpuClientTarget::kServerRaster;
}

std::string CompositionRole(const HelmWebgpuArtifactSlice& artifact) {
  if (IsTier1(artifact.semantic_tier)) return "official_chart_artifact";
  switch (artifact.family) {
    case HelmWebgpuArtifactFamily::kEnvironmentalFieldPacket:
    case HelmWebgpuArtifactFamily::kEnvironmentalLegend:
      return "helm_environment_overlay";
    case HelmWebgpuArtifactFamily::kHelmOverlayRegistryAsset:
      return "helm_overlay_asset";
    case HelmWebgpuArtifactFamily::kHelmUiRegistryAsset:
      return "helm_ui_asset";
    default:
      return "helm_registry_or_overlay_asset";
  }
}

HelmWebgpuComposedArtifact ComposeArtifact(
    const HelmWebgpuArtifactSlice& artifact,
    HelmWebgpuClientTarget selected_target,
    const HelmWebgpuFallbackRoute* fallback) {
  HelmWebgpuComposedArtifact composed;
  composed.artifact_id = artifact.artifact_id;
  composed.family = artifact.family;
  composed.selected_target = selected_target;
  composed.fallback_route_id = fallback == nullptr ? std::string{} :
                                                   fallback->route_id;
  composed.semantic_tier = artifact.semantic_tier;
  composed.semantic_owner = artifact.semantic_owner;
  composed.source_standard = artifact.source_standard;
  composed.registry_id = artifact.registry_id;
  composed.composition_role = CompositionRole(artifact);
  composed.provenance_refs = artifact.provenance_refs;
  composed.query_ids = artifact.query_ids;
  composed.chart_semantics_server_authoritative =
      IsTier1(artifact.semantic_tier);
  composed.browser_decision_scope =
      composed.chart_semantics_server_authoritative
          ? "compose_server_chart_artifact_only"
          : "compose_helm_registry_asset_or_overlay";
  composed.browser_may_decide_chart_semantics = false;
  composed.inspection_required = artifact.requires_inspection;
  return composed;
}

HelmWebgpuSafetyInspectionTrace SafetyTraceFromHook(
    const HelmWebgpuInspectionHook& hook, bool hidden_or_simplified) {
  HelmWebgpuSafetyInspectionTrace trace;
  trace.trace_id = hidden_or_simplified ? "safety-hidden:" + hook.hook_id
                                        : "safety-rendered:" + hook.hook_id;
  trace.source_chart_id = hook.source_chart_id;
  trace.source_object_id = hook.source_object_id;
  trace.presentation_rule_id = hook.presentation_rule_id;
  trace.primitive_id = hook.primitive_id;
  trace.artifact_id = hook.artifact_id;
  trace.final_web_asset_id = hook.final_web_asset_id;
  trace.object_query_id = hook.object_query_id;
  trace.pixel_query_id = hook.pixel_query_id;
  trace.server_declared_hidden_or_simplified = hidden_or_simplified;
  trace.visibility_state = hidden_or_simplified
                               ? "server_declared_hidden_or_simplified_low_zoom"
                               : "server_declared_rendered";
  return trace;
}

bool ValidateComposedArtifact(const HelmWebgpuComposedArtifact& artifact,
                              std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  if (artifact.artifact_id.empty() || artifact.semantic_tier.empty() ||
      artifact.semantic_owner.empty() || artifact.composition_role.empty()) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_item_identity",
        "Helm WebGPU browser fixture item is missing identity or ownership.",
        artifact.provenance_refs));
    ok = false;
  }
  if (artifact.browser_may_decide_chart_semantics) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_semantic_authority",
        "The browser fixture must not decide S-52/S-101/S-100 chart "
        "semantics.",
        artifact.provenance_refs));
    ok = false;
  }
  if (IsTier1(artifact.semantic_tier)) {
    if (artifact.semantic_owner != "presentation_compiler" ||
        artifact.source_standard.empty() ||
        !artifact.chart_semantics_server_authoritative ||
        artifact.browser_decision_scope !=
            "compose_server_chart_artifact_only" ||
        (artifact.query_ids.empty() && artifact.provenance_refs.empty()) ||
        !artifact.registry_id.empty()) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_fixture_tier1",
          "Tier 1 chart items in the browser fixture must be consumed "
          "server artifacts with inspection/provenance handles.",
          artifact.provenance_refs));
      ok = false;
    }
  } else if (IsHelmTier(artifact.semantic_tier)) {
    if (!Contains(artifact.semantic_owner, "helm") ||
        artifact.semantic_owner == "presentation_compiler" ||
        (artifact.registry_id.empty() &&
         artifact.family !=
             HelmWebgpuArtifactFamily::kEnvironmentalFieldPacket &&
         artifact.family != HelmWebgpuArtifactFamily::kEnvironmentalLegend)) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_fixture_helm_tier",
          "Tier 2/3 items in the browser fixture must remain Helm-owned "
          "overlay, environmental, or UI assets.",
          artifact.provenance_refs));
      ok = false;
    }
  } else {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_unknown_tier",
        "Browser fixture item has an unknown visual tier.",
        artifact.provenance_refs));
    ok = false;
  }
  return ok;
}

bool ValidateSafetyTrace(const HelmWebgpuSafetyInspectionTrace& trace,
                         std::vector<Diagnostic>* diagnostics) {
  if (trace.trace_id.empty() || trace.source_object_id.empty() ||
      trace.presentation_rule_id.empty() || trace.artifact_id.empty() ||
      trace.final_web_asset_id.empty() || trace.object_query_id.empty() ||
      trace.pixel_query_id.empty() || trace.visibility_state.empty() ||
      !trace.safety_relevant || !trace.browser_may_warn_or_query ||
      trace.browser_may_decide_safety_semantics) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_safety_trace",
        "Safety-relevant chart traces must keep source/object, "
        "presentation, artifact, pixel/query, and server-declared "
        "visibility handles without browser-owned safety semantics."));
    return false;
  }
  return true;
}

}  // namespace

HelmWebgpuBrowserConsumerFixture BuildHelmWebgpuBrowserConsumerFixture(
    const HelmWebgpuConsumerContract& contract,
    const HelmWebgpuBrowserFeatureProfile& features) {
  HelmWebgpuBrowserConsumerFixture fixture;
  fixture.contract_id = contract.contract_id;
  fixture.feature_profile_id = features.profile_id;

  const HelmWebgpuFallbackRoute* fallback = nullptr;
  fixture.selected_target = SelectTarget(contract, features, &fallback);
  fixture.webgpu_path_active = fixture.selected_target ==
                               HelmWebgpuClientTarget::kWebGpu;
  if (fallback != nullptr) {
    fixture.fallback_route_id = fallback->route_id;
    fixture.fallback_reason = fallback->reason;
  }

  fixture.composed_artifacts.reserve(contract.artifacts.size());
  for (const HelmWebgpuArtifactSlice& artifact : contract.artifacts) {
    fixture.composed_artifacts.push_back(
        ComposeArtifact(artifact, fixture.selected_target, fallback));
  }

  bool added_hidden_or_simplified_trace = false;
  for (const HelmWebgpuInspectionHook& hook : contract.inspection_hooks) {
    if (!IsTier1(hook.semantic_tier)) continue;
    fixture.safety_traces.push_back(SafetyTraceFromHook(hook, false));
    if (!added_hidden_or_simplified_trace) {
      fixture.safety_traces.push_back(SafetyTraceFromHook(hook, true));
      added_hidden_or_simplified_trace = true;
    }
  }

  std::vector<Diagnostic> diagnostics;
  fixture.ok = ValidateHelmWebgpuBrowserConsumerFixture(
      contract, features, fixture, &diagnostics);
  fixture.diagnostics = std::move(diagnostics);
  return fixture;
}

bool ValidateHelmWebgpuBrowserConsumerFixture(
    const HelmWebgpuConsumerContract& contract,
    const HelmWebgpuBrowserFeatureProfile& features,
    const HelmWebgpuBrowserConsumerFixture& fixture,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  ok = ValidateHelmWebgpuConsumerContract(contract, &out) && ok;
  if (fixture.schema_version != kHelmWebgpuBrowserFixtureSchemaVersion ||
      fixture.fixture_id.empty() || fixture.contract_id != contract.contract_id ||
      fixture.feature_profile_id != features.profile_id) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_identity",
        "Helm WebGPU browser fixture identity does not match the consumer "
        "contract or feature profile."));
    ok = false;
  }

  const bool webgpu_usable = WebGpuUsable(features);
  if (fixture.selected_target == HelmWebgpuClientTarget::kWebGpu &&
      (!webgpu_usable || !fixture.webgpu_path_active ||
       !fixture.fallback_route_id.empty())) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_feature_detection",
        "Browser fixture selected WebGPU without the required WebGPU feature "
        "profile, or carried a fallback while WebGPU is active."));
    ok = false;
  }
  if (fixture.selected_target != HelmWebgpuClientTarget::kWebGpu) {
    const HelmWebgpuFallbackRoute* fallback =
        FindFallback(contract, fixture.selected_target);
    if (webgpu_usable || fixture.webgpu_path_active ||
        fixture.fallback_route_id.empty() || fallback == nullptr ||
        fallback->route_id != fixture.fallback_route_id) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "helm_webgpu_fixture_fallback",
          "Browser fixture fallback must be feature-detected, visible, "
          "semantic-preserving, and declared by the consumer contract."));
      ok = false;
    }
  }
  if (fixture.selected_target == HelmWebgpuClientTarget::kWebGlMapLibre &&
      !features.webgl_maplibre_available) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_feature_detection",
        "Browser fixture selected WebGL/MapLibre when that fallback is not "
        "available."));
    ok = false;
  }
  if (fixture.selected_target == HelmWebgpuClientTarget::kServerRaster &&
      !features.server_raster_available) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_feature_detection",
        "Browser fixture selected server-raster fallback when it is not "
        "available."));
    ok = false;
  }
  if (!fixture.server_raster_authority_preserved) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_server_authority",
        "Browser fixture must preserve server authority for chart pixels and "
        "fallback rasters."));
    ok = false;
  }

  bool saw_tier1 = false;
  bool saw_tier2 = false;
  bool saw_tier3 = false;
  bool saw_official_chart = false;
  bool saw_overlay_or_environment = false;
  bool saw_ui = false;
  for (const HelmWebgpuComposedArtifact& item :
       fixture.composed_artifacts) {
    ok = ValidateComposedArtifact(item, &out) && ok;
    saw_tier1 = saw_tier1 || IsTier1(item.semantic_tier);
    saw_tier2 = saw_tier2 || IsTier2(item.semantic_tier);
    saw_tier3 = saw_tier3 || IsTier3(item.semantic_tier);
    saw_official_chart =
        saw_official_chart || item.composition_role == "official_chart_artifact";
    saw_overlay_or_environment =
        saw_overlay_or_environment ||
        item.composition_role == "helm_overlay_asset" ||
        item.composition_role == "helm_environment_overlay";
    saw_ui = saw_ui || item.composition_role == "helm_ui_asset";
  }
  if (fixture.composed_artifacts.empty() || !saw_tier1 || !saw_tier2 ||
      !saw_tier3 || !saw_official_chart || !saw_overlay_or_environment ||
      !saw_ui) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_tier_coverage",
        "HELMWEBGPU-2 fixture must compose Tier 1 official chart artifacts "
        "with representative Tier 2 overlays/environment and Tier 3 UI "
        "assets."));
    ok = false;
  }

  bool saw_hidden_or_simplified = false;
  for (const HelmWebgpuSafetyInspectionTrace& trace : fixture.safety_traces) {
    ok = ValidateSafetyTrace(trace, &out) && ok;
    saw_hidden_or_simplified =
        saw_hidden_or_simplified ||
        trace.server_declared_hidden_or_simplified;
  }
  if (fixture.safety_traces.empty() || !saw_hidden_or_simplified) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_safety_trace",
        "HELMWEBGPU-2 fixture must include safety-relevant query handles, "
        "including a server-declared hidden/simplified low-zoom trace."));
    ok = false;
  }

  if (HasError(contract.diagnostics)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "helm_webgpu_fixture_upstream_diagnostics",
        "Browser fixture cannot hide upstream artifact or inspection errors."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render
