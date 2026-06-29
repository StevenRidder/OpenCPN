// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "helm_webgpu_artifact_consumer.hpp"
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

bool ValidateConsumer(
    const ocpn::render::NauticalRenderModel& model,
    const ocpn::render::GpuArtifactCacheManifest& artifacts,
    const ocpn::render::DrawBackendContract& draw_contract,
    const ocpn::render::SourceToRenderInspectionReport& inspection,
    const ocpn::render::HelmWebgpuConsumerContract& consumer,
    const char* label) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateHelmWebgpuConsumerHandoff(
          model, artifacts, draw_contract, inspection, consumer,
          &diagnostics)) {
    return true;
  }
  std::cerr << label << " Helm WebGPU consumer failed validation\n";
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

ocpn::render::HelmWebgpuRegistryAsset RegistryAsset(
    std::string asset_id, std::string registry_id, std::string tier,
    std::string owner, std::string role) {
  ocpn::render::HelmWebgpuRegistryAsset asset;
  asset.asset_id = std::move(asset_id);
  asset.registry_id = std::move(registry_id);
  asset.semantic_tier = std::move(tier);
  asset.semantic_owner = std::move(owner);
  asset.asset_role = std::move(role);
  asset.provenance_refs = {asset.registry_id};
  return asset;
}

bool HasArtifactFamily(
    const ocpn::render::HelmWebgpuConsumerContract& contract,
    ocpn::render::HelmWebgpuArtifactFamily family) {
  for (const ocpn::render::HelmWebgpuArtifactSlice& artifact :
       contract.artifacts) {
    if (artifact.family == family) return true;
  }
  return false;
}

bool HasFallback(const ocpn::render::HelmWebgpuConsumerContract& contract,
                 ocpn::render::HelmWebgpuClientTarget target) {
  for (const ocpn::render::HelmWebgpuFallbackRoute& fallback :
       contract.fallbacks) {
    if (fallback.to == target && fallback.semantic_preserving &&
        fallback.visible_diagnostic) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "helm-webgpu-consumer-smoke";
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
  model.metadata["source_product_id"] = "chart1-package";
  for (ocpn::render::NauticalLayer& layer : model.layers) {
    for (ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      primitive.metadata["semantic_tier"] = "tier1_official_chart";
      primitive.metadata["source_standard"] = "s52-compatible";
      primitive.handoff.accepted_backend_targets = {"webgpu",
                                                    "webgl_maplibre",
                                                    "server_raster"};
    }
  }

  ocpn::render::GpuArtifactCacheOptions artifact_options;
  artifact_options.backend_target = "webgpu";
  artifact_options.device_profile = "webgpu-browser-device";
  artifact_options.material_profile = "neutral-webgpu-material-v1";
  artifact_options.cache_namespace = "helm-browser-gpu-artifacts";
  artifact_options.invalidation_epoch = "scheduler-epoch:chart1";
  artifact_options.memory_budget_bytes = 24ULL * 1024ULL * 1024ULL;
  const ocpn::render::GpuArtifactCacheManifest artifacts =
      ocpn::render::BuildGpuArtifactCacheManifest(model, artifact_options);

  ocpn::render::DrawBackendCapabilities webgpu;
  webgpu.backend_id = "helm-webgpu-client";
  webgpu.target = ocpn::render::DrawBackendTarget::kWebGpu;
  webgpu.device_profile = artifact_options.device_profile;
  webgpu.material_profile = artifact_options.material_profile;
  webgpu.accepts_gpu_artifacts = true;
  webgpu.supports_overlay_composition = true;
  webgpu.supports_offscreen = true;
  webgpu.supports_readback = false;
  webgpu.fallback_backend_ids = {"webgl-maplibre", "server-raster"};
  const ocpn::render::DrawBackendContract draw_contract =
      ocpn::render::BuildDrawBackendContract(model, artifacts, webgpu);

  ocpn::render::SourceToRenderInspectionOptions inspection_options;
  inspection_options.report_id = "helm-webgpu-consumer-inspection";
  inspection_options.source_product_id = "chart1-package";
  inspection_options.converter_id = "chart1-debug-source-fixture";
  inspection_options.portable_package_id = "chart1-package:portable";
  inspection_options.backend_name = webgpu.backend_id;
  inspection_options.target.target_id = "helm-webgpu-canvas";
  inspection_options.target.pixel_size = view.pixel_size;
  const ocpn::render::SourceToRenderInspectionReport inspection =
      ocpn::render::BuildSourceToRenderInspectionReport(
          model, artifacts, inspection_options);
  if (!inspection.ok) {
    std::cerr << "source-to-render inspection was not ok\n";
    return 1;
  }

  ocpn::render::HelmWebgpuConsumerOptions options;
  options.client_id = "helm-browser-client";
  options.route_prefix = "/artifacts/chart";
  options.registry_assets = {
      RegistryAsset("helm-overlay:ais-targets",
                    "helm-tools-9:overlay-registry:ais",
                    "tier2_helm_overlay", "helm_overlay_registry", "ais"),
      RegistryAsset("helm-ui:route-handles",
                    "helm-tools-10:ui-registry:route-handles",
                    "tier3_ui_asset", "helm_ui_registry", "route-ui")};
  const ocpn::render::HelmWebgpuConsumerContract consumer =
      ocpn::render::BuildHelmWebgpuConsumerContract(
          model, artifacts, draw_contract, inspection, options);

  if (!ValidateConsumer(model, artifacts, draw_contract, inspection, consumer,
                        "valid")) {
    return 1;
  }
  if (consumer.primary_target != ocpn::render::HelmWebgpuClientTarget::kWebGpu ||
      !HasArtifactFamily(
          consumer,
          ocpn::render::HelmWebgpuArtifactFamily::kCompiledPrimitivePacket) ||
      !HasArtifactFamily(
          consumer,
          ocpn::render::HelmWebgpuArtifactFamily::kInspectionPacket) ||
      !HasArtifactFamily(
          consumer,
          ocpn::render::HelmWebgpuArtifactFamily::kRasterFallbackTile) ||
      !HasFallback(consumer,
                   ocpn::render::HelmWebgpuClientTarget::kWebGlMapLibre) ||
      !HasFallback(consumer,
                   ocpn::render::HelmWebgpuClientTarget::kServerRaster) ||
      consumer.inspection_hooks.empty()) {
    std::cerr << "consumer contract dropped primary target, packets, fallback, "
                 "or inspection hooks\n";
    return 1;
  }

  ocpn::render::HelmWebgpuConsumerContract invalid = consumer;
  invalid.primary_target = ocpn::render::HelmWebgpuClientTarget::kServerRaster;
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateHelmWebgpuConsumerContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_primary_target")) {
    std::cerr << "accepted a non-WebGPU primary target\n";
    return 1;
  }

  invalid = consumer;
  invalid.artifacts.front().semantic_owner = "helm_overlay_registry";
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuConsumerContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_tier1_owner")) {
    std::cerr << "accepted Helm-owned Tier 1 chart truth\n";
    return 1;
  }

  invalid = consumer;
  invalid.artifacts.back().semantic_owner = "presentation_compiler";
  invalid.artifacts.back().source_standard = "s52-compatible";
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuConsumerContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_tier_masquerade")) {
    std::cerr << "accepted Tier 2/3 registry asset masquerading as chart truth\n";
    return 1;
  }

  invalid = consumer;
  invalid.artifacts.front().browser_may_decide_chart_semantics = true;
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuConsumerContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_semantic_authority")) {
    std::cerr << "accepted browser-owned chart semantics\n";
    return 1;
  }

  invalid = consumer;
  invalid.fallbacks.front().semantic_preserving = false;
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuConsumerContract(invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_fallback")) {
    std::cerr << "accepted hidden or semantic-changing fallback\n";
    return 1;
  }

  ocpn::render::DrawBackendCapabilities vsg = webgpu;
  vsg.backend_id = "vsg-native-proof";
  vsg.target = ocpn::render::DrawBackendTarget::kVsgVulkan;
  const ocpn::render::DrawBackendContract vsg_contract =
      ocpn::render::BuildDrawBackendContract(model, artifacts, vsg);
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuConsumerHandoff(
          model, artifacts, vsg_contract, inspection, consumer, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_backend_target")) {
    std::cerr << "accepted VSG backend as Helm WebGPU consumer target\n";
    return 1;
  }

  std::cout << "ok helm-webgpu-artifact-consumer: "
            << consumer.artifacts.size() << " artifact slices, "
            << consumer.inspection_hooks.size() << " inspection hooks, "
            << consumer.fallbacks.size() << " fallbacks\n";
  return 0;
}
