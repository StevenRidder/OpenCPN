// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "draw_backend_contract.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <tuple>
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
      "Keep source conversion, S-52/S-101 presentation, quilting, scheduling, "
      "and tier semantics before backend handoff; draw backends consume neutral "
      "models or machine-local artifacts only.";
  return diagnostic;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool IsBackendOwner(const std::string& value) {
  const std::string lower = ToLower(value);
  return lower == "backend" || lower == "draw_backend" ||
         lower == "renderer_backend";
}

bool ContainsChartStandard(const std::string& value) {
  const std::string lower = ToLower(value);
  return lower.find("s52") != std::string::npos ||
         lower.find("s-52") != std::string::npos ||
         lower.find("s101") != std::string::npos ||
         lower.find("s-101") != std::string::npos;
}

bool ContainsPolicyLeak(const std::string& value) {
  const std::string lower = ToLower(value);
  for (const char* token :
       {"chart-source", "chart_source", "mbtiles", "pmtiles", "quilting",
        "scheduler", "prefetch", "display-category", "display_category",
        "scamin", "safety-contour", "safety_contour"}) {
    if (lower.find(token) != std::string::npos) return true;
  }
  return false;
}

std::string TargetKey(DrawBackendTarget target) {
  switch (target) {
    case DrawBackendTarget::kVsgVulkan:
      return "vsg";
    case DrawBackendTarget::kWebGpu:
      return "webgpu";
    case DrawBackendTarget::kWebGlMapLibre:
      return "webgl_maplibre";
    case DrawBackendTarget::kServerRaster:
      return "server_raster";
  }
  return "unknown";
}

void AddUniqueTier(std::vector<DrawInputTierHandle>* tiers,
                   const GpuArtifactTierHandle& source) {
  if (source.semantic_tier.empty() && source.semantic_owner.empty()) return;
  if (source.source_standard.empty() && source.provenance_refs.empty() &&
      source.primitive_ids.empty()) {
    return;
  }
  const auto key = std::make_tuple(source.semantic_tier, source.semantic_owner,
                                   source.source_standard);
  for (DrawInputTierHandle& tier : *tiers) {
    if (std::make_tuple(tier.semantic_tier, tier.semantic_owner,
                        tier.source_standard) == key) {
      for (const std::string& ref : source.provenance_refs) {
        if (!ref.empty() &&
            std::find(tier.provenance_refs.begin(), tier.provenance_refs.end(),
                      ref) == tier.provenance_refs.end()) {
          tier.provenance_refs.push_back(ref);
        }
      }
      for (const std::string& primitive_id : source.primitive_ids) {
        if (!primitive_id.empty() &&
            std::find(tier.primitive_ids.begin(), tier.primitive_ids.end(),
                      primitive_id) == tier.primitive_ids.end()) {
          tier.primitive_ids.push_back(primitive_id);
        }
      }
      return;
    }
  }

  DrawInputTierHandle tier;
  tier.semantic_tier = source.semantic_tier;
  tier.semantic_owner = source.semantic_owner;
  tier.source_standard = source.source_standard;
  tier.provenance_refs = source.provenance_refs;
  tier.primitive_ids = source.primitive_ids;
  tiers->push_back(std::move(tier));
}

bool TierLooksOfficial(const DrawInputTierHandle& tier) {
  return tier.semantic_tier == "tier1_official_chart";
}

bool TierLooksOverlay(const DrawInputTierHandle& tier) {
  return tier.semantic_tier.find("tier2") == 0 ||
         tier.semantic_tier.find("tier3") == 0;
}

bool ValidateTier(const DrawInputTierHandle& tier,
                  std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  if (tier.semantic_tier.empty() || tier.semantic_owner.empty()) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_tier_identity",
        "Draw backend tier handle is missing semantic tier or owner.",
        tier.provenance_refs));
    return false;
  }
  if (IsBackendOwner(tier.semantic_owner)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_tier_owner",
        "Draw backends must not own chart or overlay tier semantics.",
        tier.provenance_refs));
    ok = false;
  }
  if (TierLooksOfficial(tier)) {
    if (tier.semantic_owner != "presentation_compiler" ||
        (tier.source_standard.empty() && tier.provenance_refs.empty() &&
         tier.primitive_ids.empty())) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "draw_backend_tier_official",
          "Tier 1 official chart artifacts must stay owned by the presentation "
          "compiler and carry chart-standard or provenance handles.",
          tier.provenance_refs));
      ok = false;
    }
  } else if (TierLooksOverlay(tier)) {
    if (tier.semantic_owner == "presentation_compiler" ||
        ContainsChartStandard(tier.source_standard) ||
        tier.registry_id.empty()) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "draw_backend_tier_masquerade",
          "Tier 2/3 Helm overlays and UI assets need their own registry/owner "
          "and must not masquerade as official chart truth.",
          tier.provenance_refs));
      ok = false;
    }
  }
  return ok;
}

bool PrimitiveAcceptsTarget(const NauticalPrimitive& primitive,
                            const std::string& target) {
  if (primitive.handoff.accepted_backend_targets.empty()) return true;
  return std::find(primitive.handoff.accepted_backend_targets.begin(),
                   primitive.handoff.accepted_backend_targets.end(),
                   target) != primitive.handoff.accepted_backend_targets.end();
}

bool ValidatePrimitiveHandoff(const NauticalPrimitive& primitive,
                              const std::string& target,
                              std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  if (primitive.handoff.backend_contract != "draw_only" ||
      IsBackendOwner(primitive.handoff.semantic_owner)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_primitive_boundary",
        "Primitive handoff must be draw-only and must not assign nautical "
        "semantics to the backend.",
        primitive.trace.provenance_refs));
    ok = false;
  }
  if (!PrimitiveAcceptsTarget(primitive, target)) {
    diagnostics->push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_target",
        "Primitive does not list this backend target as an accepted draw target.",
        primitive.trace.provenance_refs));
    ok = false;
  }
  return ok;
}

}  // namespace

const char* ToString(DrawBackendTarget target) {
  switch (target) {
    case DrawBackendTarget::kVsgVulkan:
      return "vsg";
    case DrawBackendTarget::kWebGpu:
      return "webgpu";
    case DrawBackendTarget::kWebGlMapLibre:
      return "webgl_maplibre";
    case DrawBackendTarget::kServerRaster:
      return "server_raster";
  }
  return "unknown";
}

DrawBackendContract BuildDrawBackendContract(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendCapabilities& capabilities) {
  DrawBackendContract contract;
  contract.input_model_id = model.model_id;
  contract.input_model_epoch = model.source_epoch;
  contract.artifact_manifest_id = artifacts.manifest_id;
  contract.capabilities = capabilities;

  std::ostringstream id;
  id << capabilities.backend_id << ":" << ToString(capabilities.target) << ":"
     << model.model_id << ":" << model.source_epoch << ":"
     << artifacts.manifest_id;
  contract.contract_id = id.str();

  for (const GpuArtifactRecord& artifact : artifacts.artifacts) {
    AddUniqueTier(&contract.input_tiers, artifact.tier);
  }
  contract.diagnostics = artifacts.diagnostics;
  return contract;
}

bool ValidateDrawBackendContract(
    const DrawBackendContract& contract,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (contract.schema_version != kDrawBackendContractSchemaVersion ||
      contract.contract_id.empty() || contract.input_model_id.empty() ||
      contract.input_model_epoch.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "draw_backend_identity",
                                 "Draw backend contract has invalid identity."));
    ok = false;
  }
  if (contract.input_contract != "backend-neutral-nautical-render-model" ||
      contract.artifact_contract != "machine-local-gpu-artifacts" ||
      contract.backend_owner != "draw_backend" ||
      contract.semantic_owner != "presentation_compiler" ||
      contract.cache_owner != "runtime_gpu_artifact_cache" ||
      contract.scheduler_owner != "adapter_scheduler") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_boundary",
        "Draw backend contract must preserve model/cache/scheduler ownership."));
    ok = false;
  }
  if (contract.capabilities.backend_id.empty() ||
      contract.capabilities.device_profile.empty() ||
      contract.capabilities.material_profile.empty() ||
      (!contract.capabilities.accepts_neutral_model &&
       !contract.capabilities.accepts_gpu_artifacts)) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "draw_backend_capabilities",
                                 "Draw backend capabilities are incomplete."));
    ok = false;
  }
  if (!contract.capabilities.supports_offscreen &&
      !contract.capabilities.supports_swapchain) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_surface",
        "Draw backend must declare at least one target surface."));
    ok = false;
  }
  if (ContainsPolicyLeak(contract.capabilities.backend_id) ||
      ContainsPolicyLeak(contract.capabilities.material_profile)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_policy_leak",
        "Draw backend id/material profile names source, presentation, quilting, "
        "or scheduler policy."));
    ok = false;
  }
  if (contract.input_tiers.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "draw_backend_tier_coverage",
                                 "Draw backend contract is missing tier handles."));
    ok = false;
  }
  bool saw_official = false;
  for (const DrawInputTierHandle& tier : contract.input_tiers) {
    ok = ValidateTier(tier, &out) && ok;
    saw_official = saw_official || TierLooksOfficial(tier);
  }
  if (!saw_official) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_tier_coverage",
        "Draw backend contract must include Tier 1 official chart artifact "
        "handles when drawing nautical charts."));
    ok = false;
  }
  return ok;
}

bool ValidateDrawBackendHandoff(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& artifacts,
    const DrawBackendContract& contract,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateDrawBackendContract(contract, &out);
  ok = ValidateNauticalRenderModel(model, &out) && ok;
  ok = ValidateGpuArtifactCacheManifest(artifacts, &out) && ok;

  if (contract.input_model_id != model.model_id ||
      contract.input_model_epoch != model.source_epoch ||
      contract.artifact_manifest_id != artifacts.manifest_id) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_input_mismatch",
        "Draw backend contract does not match the model or artifact manifest."));
    ok = false;
  }
  if (contract.capabilities.accepts_gpu_artifacts &&
      artifacts.artifacts.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_empty_artifacts",
        "Backend declared artifact input but no artifact records are present."));
    ok = false;
  }
  if (artifacts.input_contract != contract.input_contract ||
      artifacts.cache_scope != contract.artifact_contract ||
      artifacts.cache_owner != contract.cache_owner) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "draw_backend_artifact_boundary",
        "Draw backend artifact manifest does not match the contract boundary."));
    ok = false;
  }

  const std::string target = TargetKey(contract.capabilities.target);
  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      ok = ValidatePrimitiveHandoff(primitive, target, &out) && ok;
    }
  }
  return ok;
}

}  // namespace ocpn::render
