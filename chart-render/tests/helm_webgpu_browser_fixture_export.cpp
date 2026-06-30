// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "helm_webgpu_browser_fixture.hpp"
#include "s52/s52_command_builder.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string JsonEscape(const std::string& value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

void WriteString(std::ostream& out, const std::string& value) {
  out << '"' << JsonEscape(value) << '"';
}

void WriteStringArray(std::ostream& out,
                      const std::vector<std::string>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0U) out << ',';
    WriteString(out, values[i]);
  }
  out << ']';
}

const char* Bool(bool value) {
  return value ? "true" : "false";
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

struct BrowserFixtureInputs {
  ocpn::render::NauticalRenderModel model;
  ocpn::render::GpuArtifactCacheManifest artifacts;
  ocpn::render::DrawBackendContract draw_contract;
  ocpn::render::SourceToRenderInspectionReport inspection;
  ocpn::render::HelmWebgpuConsumerContract contract;
};

BrowserFixtureInputs BuildInputs() {
  ocpn::render::RenderView view;
  view.view_id = "helm-webgpu-browser-playwright";
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
  inspection_options.report_id = "helm-webgpu-browser-playwright-inspection";
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

ocpn::render::HelmWebgpuBrowserConsumerFixture BuildFixture(
    const ocpn::render::HelmWebgpuConsumerContract& contract,
    std::string profile_id, bool webgpu_available, bool webgl_available,
    bool server_raster_available) {
  ocpn::render::HelmWebgpuBrowserFeatureProfile features;
  features.profile_id = std::move(profile_id);
  features.webgpu_available = webgpu_available;
  features.webgl_maplibre_available = webgl_available;
  features.server_raster_available = server_raster_available;
  return ocpn::render::BuildHelmWebgpuBrowserConsumerFixture(contract, features);
}

void WriteFixtureSummary(
    std::ostream& out, const std::string& key,
    const ocpn::render::HelmWebgpuBrowserConsumerFixture& fixture) {
  WriteString(out, key);
  out << ":{";
  out << "\"ok\":" << Bool(fixture.ok) << ',';
  out << "\"selectedTarget\":";
  WriteString(out, ocpn::render::ToString(fixture.selected_target));
  out << ",\"fallbackRouteId\":";
  WriteString(out, fixture.fallback_route_id);
  out << ",\"fallbackReason\":";
  WriteString(out, fixture.fallback_reason);
  out << ",\"webgpuPathActive\":" << Bool(fixture.webgpu_path_active);
  out << '}';
}

void WriteFallbacks(
    std::ostream& out,
    const std::vector<ocpn::render::HelmWebgpuFallbackRoute>& fallbacks) {
  out << '[';
  for (std::size_t i = 0; i < fallbacks.size(); ++i) {
    const ocpn::render::HelmWebgpuFallbackRoute& route = fallbacks[i];
    if (i != 0U) out << ',';
    out << '{';
    out << "\"from\":";
    WriteString(out, ocpn::render::ToString(route.from));
    out << ",\"to\":";
    WriteString(out, ocpn::render::ToString(route.to));
    out << ",\"routeId\":";
    WriteString(out, route.route_id);
    out << ",\"reason\":";
    WriteString(out, route.reason);
    out << ",\"semanticPreserving\":" << Bool(route.semantic_preserving);
    out << ",\"visibleDiagnostic\":" << Bool(route.visible_diagnostic);
    out << ",\"usesServerAuthority\":" << Bool(route.uses_server_authority);
    out << '}';
  }
  out << ']';
}

void WriteArtifacts(
    std::ostream& out,
    const std::vector<ocpn::render::HelmWebgpuComposedArtifact>& artifacts) {
  out << '[';
  for (std::size_t i = 0; i < artifacts.size(); ++i) {
    const ocpn::render::HelmWebgpuComposedArtifact& artifact = artifacts[i];
    if (i != 0U) out << ',';
    out << '{';
    out << "\"artifactId\":";
    WriteString(out, artifact.artifact_id);
    out << ",\"family\":";
    WriteString(out, ocpn::render::ToString(artifact.family));
    out << ",\"semanticTier\":";
    WriteString(out, artifact.semantic_tier);
    out << ",\"semanticOwner\":";
    WriteString(out, artifact.semantic_owner);
    out << ",\"sourceStandard\":";
    WriteString(out, artifact.source_standard);
    out << ",\"registryId\":";
    WriteString(out, artifact.registry_id);
    out << ",\"compositionRole\":";
    WriteString(out, artifact.composition_role);
    out << ",\"browserDecisionScope\":";
    WriteString(out, artifact.browser_decision_scope);
    out << ",\"chartSemanticsServerAuthoritative\":"
        << Bool(artifact.chart_semantics_server_authoritative);
    out << ",\"browserMayDecideChartSemantics\":"
        << Bool(artifact.browser_may_decide_chart_semantics);
    out << ",\"inspectionRequired\":" << Bool(artifact.inspection_required);
    out << ",\"provenanceRefs\":";
    WriteStringArray(out, artifact.provenance_refs);
    out << ",\"queryIds\":";
    WriteStringArray(out, artifact.query_ids);
    out << '}';
  }
  out << ']';
}

void WriteSafetyTraces(
    std::ostream& out,
    const std::vector<ocpn::render::HelmWebgpuSafetyInspectionTrace>& traces) {
  out << '[';
  for (std::size_t i = 0; i < traces.size(); ++i) {
    const ocpn::render::HelmWebgpuSafetyInspectionTrace& trace = traces[i];
    if (i != 0U) out << ',';
    out << '{';
    out << "\"traceId\":";
    WriteString(out, trace.trace_id);
    out << ",\"sourceChartId\":";
    WriteString(out, trace.source_chart_id);
    out << ",\"sourceObjectId\":";
    WriteString(out, trace.source_object_id);
    out << ",\"presentationRuleId\":";
    WriteString(out, trace.presentation_rule_id);
    out << ",\"primitiveId\":";
    WriteString(out, trace.primitive_id);
    out << ",\"artifactId\":";
    WriteString(out, trace.artifact_id);
    out << ",\"finalWebAssetId\":";
    WriteString(out, trace.final_web_asset_id);
    out << ",\"objectQueryId\":";
    WriteString(out, trace.object_query_id);
    out << ",\"pixelQueryId\":";
    WriteString(out, trace.pixel_query_id);
    out << ",\"visibilityState\":";
    WriteString(out, trace.visibility_state);
    out << ",\"safetyRelevant\":" << Bool(trace.safety_relevant);
    out << ",\"serverDeclaredHiddenOrSimplified\":"
        << Bool(trace.server_declared_hidden_or_simplified);
    out << ",\"browserMayWarnOrQuery\":"
        << Bool(trace.browser_may_warn_or_query);
    out << ",\"browserMayDecideSafetySemantics\":"
        << Bool(trace.browser_may_decide_safety_semantics);
    out << '}';
  }
  out << ']';
}

bool WriteFixtureJson(const std::string& output_path) {
  const BrowserFixtureInputs inputs = BuildInputs();

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::ValidateHelmWebgpuConsumerHandoff(
          inputs.model, inputs.artifacts, inputs.draw_contract,
          inputs.inspection, inputs.contract, &diagnostics)) {
    std::cerr << "Cannot export invalid HELMWEBGPU handoff\n";
    return false;
  }

  const ocpn::render::HelmWebgpuBrowserConsumerFixture webgpu =
      BuildFixture(inputs.contract, "playwright-webgpu", true, true, true);
  const ocpn::render::HelmWebgpuBrowserConsumerFixture webgl =
      BuildFixture(inputs.contract, "playwright-webgl", false, true, true);
  const ocpn::render::HelmWebgpuBrowserConsumerFixture raster =
      BuildFixture(inputs.contract, "playwright-server-raster", false, false,
                   true);
  if (!webgpu.ok || !webgl.ok || !raster.ok) {
    std::cerr << "Cannot export invalid browser fixture variants\n";
    return false;
  }

  std::ofstream file(output_path);
  if (!file) {
    std::cerr << "Cannot open " << output_path << " for writing\n";
    return false;
  }

  file << "{\n";
  file << "\"schemaVersion\":1,";
  file << "\"fixtureId\":\"helm-webgpu-playwright-fixture\",";
  file << "\"contractId\":";
  WriteString(file, inputs.contract.contract_id);
  file << ",\"artifactCount\":" << webgpu.composed_artifacts.size();
  file << ",\"safetyTraceCount\":" << webgpu.safety_traces.size();
  file << ",\"fallbacks\":";
  WriteFallbacks(file, inputs.contract.fallbacks);
  file << ",\"artifacts\":";
  WriteArtifacts(file, webgpu.composed_artifacts);
  file << ",\"safetyTraces\":";
  WriteSafetyTraces(file, webgpu.safety_traces);
  file << ",\"expected\":{";
  WriteFixtureSummary(file, "webgpu", webgpu);
  file << ',';
  WriteFixtureSummary(file, "webgl", webgl);
  file << ',';
  WriteFixtureSummary(file, "serverRaster", raster);
  file << "}}\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <fixture-json-output>\n";
    return EXIT_FAILURE;
  }
  if (!WriteFixtureJson(argv[1])) return EXIT_FAILURE;
  std::cout << "ok helm-webgpu-playwright-fixture: " << argv[1] << "\n";
  return EXIT_SUCCESS;
}
