// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "helm_webgpu_browser_fixture.hpp"
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

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
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

bool HasRole(const ocpn::render::HelmWebgpuBrowserConsumerFixture& fixture,
             const std::string& role) {
  for (const ocpn::render::HelmWebgpuComposedArtifact& item :
       fixture.composed_artifacts) {
    if (item.composition_role == role) return true;
  }
  return false;
}

bool HasHiddenOrSimplifiedSafetyTrace(
    const ocpn::render::HelmWebgpuBrowserConsumerFixture& fixture) {
  for (const ocpn::render::HelmWebgpuSafetyInspectionTrace& trace :
       fixture.safety_traces) {
    if (trace.server_declared_hidden_or_simplified &&
        !trace.object_query_id.empty() && !trace.pixel_query_id.empty() &&
        !trace.final_web_asset_id.empty() &&
        !trace.browser_may_decide_safety_semantics) {
      return true;
    }
  }
  return false;
}

bool AllTier1ComposeOnly(
    const ocpn::render::HelmWebgpuBrowserConsumerFixture& fixture) {
  bool saw_tier1 = false;
  for (const ocpn::render::HelmWebgpuComposedArtifact& item :
       fixture.composed_artifacts) {
    if (item.semantic_tier.find("tier1") != 0) continue;
    saw_tier1 = true;
    if (item.semantic_owner != "presentation_compiler" ||
        item.browser_decision_scope != "compose_server_chart_artifact_only" ||
        !item.chart_semantics_server_authoritative ||
        item.browser_may_decide_chart_semantics ||
        item.query_ids.empty()) {
      return false;
    }
  }
  return saw_tier1;
}

struct BrowserFixtureInputs {
  ocpn::render::NauticalRenderModel model;
  ocpn::render::GpuArtifactCacheManifest artifacts;
  ocpn::render::DrawBackendContract draw_contract;
  ocpn::render::SourceToRenderInspectionReport inspection;
  ocpn::render::HelmWebgpuConsumerContract contract;
};

BrowserFixtureInputs BuildInputs() {
  ocpn::render::RenderView view;
  view.view_id = "helm-webgpu-browser-fixture";
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

  BrowserFixtureInputs inputs;
  inputs.model = ocpn::render::BuildNauticalRenderModel(scene);
  inputs.model.metadata["source_product_id"] = "chart1-package";
  for (ocpn::render::NauticalLayer& layer : inputs.model.layers) {
    for (ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      primitive.metadata["semantic_tier"] = "tier1_official_chart";
      primitive.metadata["source_standard"] = "s52-compatible";
      primitive.handoff.accepted_backend_targets = {
          "webgpu", "webgl_maplibre", "server_raster"};
    }
  }

  ocpn::render::GpuArtifactCacheOptions artifact_options;
  artifact_options.backend_target = "webgpu";
  artifact_options.device_profile = "webgpu-browser-device";
  artifact_options.material_profile = "neutral-webgpu-material-v1";
  artifact_options.cache_namespace = "helm-browser-gpu-artifacts";
  artifact_options.invalidation_epoch = "scheduler-epoch:chart1";
  artifact_options.memory_budget_bytes = 24ULL * 1024ULL * 1024ULL;
  inputs.artifacts =
      ocpn::render::BuildGpuArtifactCacheManifest(inputs.model, artifact_options);

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
  inputs.draw_contract =
      ocpn::render::BuildDrawBackendContract(inputs.model, inputs.artifacts,
                                             webgpu);

  ocpn::render::SourceToRenderInspectionOptions inspection_options;
  inspection_options.report_id = "helm-webgpu-browser-fixture-inspection";
  inspection_options.source_product_id = "chart1-package";
  inspection_options.converter_id = "chart1-debug-source-fixture";
  inspection_options.portable_package_id = "chart1-package:portable";
  inspection_options.backend_name = webgpu.backend_id;
  inspection_options.target.target_id = "helm-webgpu-canvas";
  inspection_options.target.pixel_size = view.pixel_size;
  inputs.inspection = ocpn::render::BuildSourceToRenderInspectionReport(
      inputs.model, inputs.artifacts, inspection_options);

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
  options.environmental_fields =
      ocpn::render::BuildHelmWebgpuEnvironmentalFieldExamples(
          inputs.model.source_epoch);
  inputs.contract = ocpn::render::BuildHelmWebgpuConsumerContract(
      inputs.model, inputs.artifacts, inputs.draw_contract, inputs.inspection,
      options);
  return inputs;
}

}  // namespace

int main() {
  const BrowserFixtureInputs inputs = BuildInputs();

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::ValidateHelmWebgpuConsumerHandoff(
          inputs.model, inputs.artifacts, inputs.draw_contract,
          inputs.inspection, inputs.contract, &diagnostics)) {
    std::cerr << "HELMWEBGPU-1 handoff failed before browser fixture\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::HelmWebgpuBrowserFeatureProfile webgpu_features;
  const ocpn::render::HelmWebgpuBrowserConsumerFixture webgpu_fixture =
      ocpn::render::BuildHelmWebgpuBrowserConsumerFixture(inputs.contract,
                                                          webgpu_features);
  if (!webgpu_fixture.ok ||
      webgpu_fixture.selected_target !=
          ocpn::render::HelmWebgpuClientTarget::kWebGpu ||
      !webgpu_fixture.webgpu_path_active ||
      !AllTier1ComposeOnly(webgpu_fixture) ||
      !HasRole(webgpu_fixture, "official_chart_artifact") ||
      !HasRole(webgpu_fixture, "helm_overlay_asset") ||
      !HasRole(webgpu_fixture, "helm_environment_overlay") ||
      !HasRole(webgpu_fixture, "helm_ui_asset") ||
      !HasHiddenOrSimplifiedSafetyTrace(webgpu_fixture)) {
    std::cerr << "WebGPU browser consumer fixture failed\n";
    PrintDiagnostics(webgpu_fixture.diagnostics);
    return 1;
  }

  ocpn::render::HelmWebgpuBrowserFeatureProfile webgl_features;
  webgl_features.profile_id = "helm-browser-webgl-fallback";
  webgl_features.webgpu_available = false;
  const ocpn::render::HelmWebgpuBrowserConsumerFixture webgl_fixture =
      ocpn::render::BuildHelmWebgpuBrowserConsumerFixture(inputs.contract,
                                                          webgl_features);
  if (!webgl_fixture.ok ||
      webgl_fixture.selected_target !=
          ocpn::render::HelmWebgpuClientTarget::kWebGlMapLibre ||
      webgl_fixture.fallback_route_id != "webgpu-to-webgl_maplibre" ||
      webgl_fixture.webgpu_path_active ||
      !AllTier1ComposeOnly(webgl_fixture)) {
    std::cerr << "WebGL fallback fixture failed\n";
    PrintDiagnostics(webgl_fixture.diagnostics);
    return 1;
  }

  ocpn::render::HelmWebgpuBrowserFeatureProfile raster_features;
  raster_features.profile_id = "helm-browser-server-raster-fallback";
  raster_features.webgpu_available = false;
  raster_features.webgl_maplibre_available = false;
  const ocpn::render::HelmWebgpuBrowserConsumerFixture raster_fixture =
      ocpn::render::BuildHelmWebgpuBrowserConsumerFixture(inputs.contract,
                                                          raster_features);
  if (!raster_fixture.ok ||
      raster_fixture.selected_target !=
          ocpn::render::HelmWebgpuClientTarget::kServerRaster ||
      raster_fixture.fallback_route_id != "webgpu-to-server_raster" ||
      raster_fixture.webgpu_path_active ||
      !AllTier1ComposeOnly(raster_fixture)) {
    std::cerr << "server-raster fallback fixture failed\n";
    PrintDiagnostics(raster_fixture.diagnostics);
    return 1;
  }

  ocpn::render::HelmWebgpuBrowserConsumerFixture invalid = webgpu_fixture;
  for (ocpn::render::HelmWebgpuComposedArtifact& item :
       invalid.composed_artifacts) {
    if (item.semantic_tier.find("tier1") == 0) {
      item.browser_may_decide_chart_semantics = true;
      break;
    }
  }
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuBrowserConsumerFixture(
          inputs.contract, webgpu_features, invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_fixture_semantic_authority")) {
    std::cerr << "accepted browser-owned chart semantics in fixture\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  invalid = webgpu_fixture;
  for (ocpn::render::HelmWebgpuComposedArtifact& item :
       invalid.composed_artifacts) {
    if (item.semantic_tier.find("tier1") == 0) {
      item.semantic_owner = "helm_overlay_registry";
      break;
    }
  }
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuBrowserConsumerFixture(
          inputs.contract, webgpu_features, invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_fixture_tier1")) {
    std::cerr << "accepted Helm-owned Tier 1 chart item in fixture\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  invalid = webgpu_fixture;
  invalid.safety_traces.clear();
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuBrowserConsumerFixture(
          inputs.contract, webgpu_features, invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_fixture_safety_trace")) {
    std::cerr << "accepted browser fixture without safety traces\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  invalid = webgpu_fixture;
  invalid.webgpu_path_active = false;
  invalid.fallback_route_id = "webgpu-to-server_raster";
  diagnostics.clear();
  if (ocpn::render::ValidateHelmWebgpuBrowserConsumerFixture(
          inputs.contract, webgpu_features, invalid, &diagnostics) ||
      !HasDiagnostic(diagnostics, "helm_webgpu_fixture_feature_detection")) {
    std::cerr << "accepted fallback route while WebGPU features are usable\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  std::cout << "ok helm-webgpu-browser-fixture: items="
            << webgpu_fixture.composed_artifacts.size()
            << " safety_traces=" << webgpu_fixture.safety_traces.size()
            << " webgl_fallback=" << webgl_fixture.fallback_route_id
            << " raster_fallback=" << raster_fixture.fallback_route_id
            << "\n";
  return 0;
}
