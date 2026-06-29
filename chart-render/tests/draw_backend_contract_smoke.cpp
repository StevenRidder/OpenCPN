// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "draw_backend_contract.hpp"
#include "s52/s52_command_builder.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

bool ValidateHandoff(const ocpn::render::NauticalRenderModel& model,
                     const ocpn::render::GpuArtifactCacheManifest& artifacts,
                     const ocpn::render::DrawBackendContract& contract,
                     const char* label) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateDrawBackendHandoff(model, artifacts, contract,
                                               &diagnostics)) {
    return true;
  }
  std::cerr << label << " draw backend contract failed validation\n";
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

ocpn::render::DrawInputTierHandle HelmTier(std::string tier,
                                           std::string owner,
                                           std::string registry) {
  ocpn::render::DrawInputTierHandle handle;
  handle.semantic_tier = std::move(tier);
  handle.semantic_owner = std::move(owner);
  handle.registry_id = std::move(registry);
  handle.provenance_refs = {handle.registry_id};
  return handle;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "draw-backend-contract-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {512, 512};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::s52::S52CommandBuilder builder;
  const ocpn::render::RenderScene scene =
      builder.BuildFixtureScene(view, display);
  ocpn::render::NauticalRenderModel model =
      ocpn::render::BuildNauticalRenderModel(scene);
  for (ocpn::render::NauticalLayer& layer : model.layers) {
    for (ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      primitive.metadata["semantic_tier"] = "tier1_official_chart";
      primitive.metadata["source_standard"] = "s52-compatible";
    }
  }

  ocpn::render::GpuArtifactCacheOptions vsg_options;
  vsg_options.backend_target = "vsg";
  vsg_options.device_profile = "vulkan-proof-device";
  vsg_options.material_profile = "neutral-material-v1";
  vsg_options.cache_namespace = "opencpn-runtime-gpu";
  vsg_options.memory_budget_bytes = 32ULL * 1024ULL * 1024ULL;
  const ocpn::render::GpuArtifactCacheManifest vsg_artifacts =
      ocpn::render::BuildGpuArtifactCacheManifest(model, vsg_options);

  ocpn::render::DrawBackendCapabilities vsg;
  vsg.backend_id = "vsg-native-proof";
  vsg.target = ocpn::render::DrawBackendTarget::kVsgVulkan;
  vsg.device_profile = vsg_options.device_profile;
  vsg.material_profile = vsg_options.material_profile;
  vsg.accepts_gpu_artifacts = true;
  vsg.supports_swapchain = true;
  vsg.supports_offscreen = true;
  vsg.supports_readback = true;
  vsg.fallback_backend_ids = {"server-raster"};
  const ocpn::render::DrawBackendContract vsg_contract =
      ocpn::render::BuildDrawBackendContract(model, vsg_artifacts, vsg);
  if (!ValidateHandoff(model, vsg_artifacts, vsg_contract, "VSG")) return 1;

  bool saw_tier1 = false;
  for (const ocpn::render::DrawInputTierHandle& tier :
       vsg_contract.input_tiers) {
    if (tier.semantic_tier == "tier1_official_chart" &&
        tier.semantic_owner == "presentation_compiler" &&
        !tier.source_standard.empty() &&
        !tier.primitive_ids.empty()) {
      saw_tier1 = true;
    }
  }
  if (!saw_tier1) {
    std::cerr << "VSG backend contract dropped Tier 1 provenance handles\n";
    return 1;
  }

  ocpn::render::GpuArtifactCacheOptions webgpu_options = vsg_options;
  webgpu_options.backend_target = "webgpu";
  webgpu_options.device_profile = "webgpu-browser-device";
  webgpu_options.material_profile = "neutral-webgpu-material-v1";
  const ocpn::render::GpuArtifactCacheManifest webgpu_artifacts =
      ocpn::render::BuildGpuArtifactCacheManifest(model, webgpu_options);

  ocpn::render::DrawBackendCapabilities webgpu;
  webgpu.backend_id = "helm-webgpu-client";
  webgpu.target = ocpn::render::DrawBackendTarget::kWebGpu;
  webgpu.device_profile = webgpu_options.device_profile;
  webgpu.material_profile = webgpu_options.material_profile;
  webgpu.accepts_gpu_artifacts = true;
  webgpu.supports_overlay_composition = true;
  webgpu.supports_offscreen = true;
  webgpu.supports_readback = false;
  webgpu.fallback_backend_ids = {"webgl-maplibre", "server-raster"};
  ocpn::render::DrawBackendContract webgpu_contract =
      ocpn::render::BuildDrawBackendContract(model, webgpu_artifacts, webgpu);
  webgpu_contract.input_tiers.push_back(HelmTier(
      "tier2_helm_overlay", "helm_overlay_registry", "helm-overlay:ais"));
  webgpu_contract.input_tiers.push_back(HelmTier(
      "tier3_ui_asset", "helm_ui_registry", "helm-ui:route-handles"));
  if (!ValidateHandoff(model, webgpu_artifacts, webgpu_contract, "WebGPU")) {
    return 1;
  }

  ocpn::render::DrawBackendContract invalid = webgpu_contract;
  invalid.semantic_owner = "backend";
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateDrawBackendContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "draw_backend_boundary")) {
    std::cerr << "Draw backend contract accepted backend-owned semantics\n";
    return 1;
  }

  invalid = webgpu_contract;
  invalid.capabilities.material_profile = "webgpu-display-category-scheduler";
  diagnostics.clear();
  if (ocpn::render::ValidateDrawBackendContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "draw_backend_policy_leak")) {
    std::cerr << "Draw backend contract accepted policy words in material profile\n";
    return 1;
  }

  invalid = webgpu_contract;
  invalid.input_tiers.push_back(HelmTier(
      "tier2_helm_overlay", "presentation_compiler", "helm-overlay:ais"));
  invalid.input_tiers.back().source_standard = "s52-compatible";
  diagnostics.clear();
  if (ocpn::render::ValidateDrawBackendContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "draw_backend_tier_masquerade")) {
    std::cerr << "Draw backend contract accepted Tier 2/3 chart masquerade\n";
    return 1;
  }

  ocpn::render::NauticalRenderModel bad_model = model;
  bad_model.layers.front().primitives.front().handoff.semantic_owner = "backend";
  diagnostics.clear();
  if (ocpn::render::ValidateDrawBackendHandoff(bad_model, webgpu_artifacts,
                                               webgpu_contract, &diagnostics) ||
      !HasDiagnostic(diagnostics, "draw_backend_primitive_boundary")) {
    std::cerr << "Draw backend handoff accepted backend-owned primitive semantics\n";
    return 1;
  }

  std::cout << "ok draw-backend-contract: "
            << vsg_artifacts.artifacts.size() << " VSG artifacts, "
            << webgpu_artifacts.artifacts.size() << " WebGPU artifacts, "
            << webgpu_contract.input_tiers.size() << " tier handles\n";
  return 0;
}
