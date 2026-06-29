// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"
#include "gpu_artifact_cache_contract.hpp"
#include "s52_presentation_compiler.hpp"
#include "s52/s52_command_builder.hpp"
#include "s57_portable_package_converter.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasKind(const ocpn::render::GpuArtifactCacheManifest& manifest,
             ocpn::render::GpuArtifactKind kind) {
  for (const ocpn::render::GpuArtifactRecord& artifact : manifest.artifacts) {
    if (artifact.kind == kind) return true;
  }
  return false;
}

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

bool ContainsTierLeakToken(const std::string& value) {
  for (const std::string& token :
       {"helm", "tier2", "tier3", "overlay", "ui_icon",
        "icon_registry"}) {
    if (value.find(token) != std::string::npos) return true;
  }
  return false;
}

bool ValidateManifest(const ocpn::render::GpuArtifactCacheManifest& manifest,
                      const char* label) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateGpuArtifactCacheManifest(manifest, &diagnostics)) {
    return true;
  }
  std::cerr << label << " GPU artifact cache manifest failed validation\n";
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

std::vector<std::string> ArtifactIds(
    const ocpn::render::GpuArtifactCacheManifest& manifest) {
  std::vector<std::string> ids;
  for (const ocpn::render::GpuArtifactRecord& artifact : manifest.artifacts) {
    ids.push_back(artifact.artifact_id);
  }
  return ids;
}

bool CheckPackageVsgArtifacts(
    const ocpn::render::GpuArtifactCacheManifest& manifest,
    const ocpn::render::PortableNauticalPackage& package) {
  if (manifest.options.backend_target != "vsg" ||
      manifest.options.device_profile != "vulkan-proof-device" ||
      manifest.input_model_id != "s57:US5CONVERT2:package:presentation" ||
      manifest.input_model_epoch.find(package.checksums.package_hash) ==
          std::string::npos) {
    std::cerr << "Package VSG artifact manifest lost backend or package "
                 "identity\n";
    return false;
  }

  for (const ocpn::render::GpuArtifactKind kind :
       {ocpn::render::GpuArtifactKind::kVertexBuffer,
        ocpn::render::GpuArtifactKind::kIndexBuffer,
        ocpn::render::GpuArtifactKind::kUniformBlock,
        ocpn::render::GpuArtifactKind::kTextureAtlas,
        ocpn::render::GpuArtifactKind::kLinePattern,
        ocpn::render::GpuArtifactKind::kMaterialPipeline,
        ocpn::render::GpuArtifactKind::kViewportTileEntry}) {
    if (!HasKind(manifest, kind)) {
      std::cerr << "Package VSG artifact manifest is missing "
                << ocpn::render::ToString(kind) << "\n";
      return false;
    }
  }

  bool saw_s57_artifact = false;
  bool saw_s57_resource_artifact = false;
  for (const ocpn::render::GpuArtifactRecord& artifact :
       manifest.artifacts) {
    if (artifact.cache_key.backend_target != "vsg" ||
        artifact.cache_key.model_key.find(package.checksums.package_hash) ==
            std::string::npos ||
        artifact.cache_key.invalidation_epoch.find(
            package.checksums.package_hash) == std::string::npos ||
        artifact.material_key.empty() || artifact.pipeline_key.empty() ||
        artifact.invalidation_domain.empty() || !artifact.rebuildable ||
        !artifact.device_specific || artifact.byte_size == 0) {
      std::cerr << "Package VSG artifact lost cache, invalidation, or "
                   "rebuildability metadata\n";
      return false;
    }
    if (ContainsTierLeakToken(artifact.usage) ||
        ContainsTierLeakToken(artifact.material_key) ||
        ContainsTierLeakToken(artifact.pipeline_key)) {
      std::cerr << "Package VSG artifact mixed Helm Tier 2/3 policy into "
                   "the official chart cache\n";
      return false;
    }
    if (!artifact.tier.primitive_ids.empty() ||
        !artifact.tier.provenance_refs.empty()) {
      if (artifact.tier.semantic_tier != "tier1_official_chart" ||
          artifact.tier.semantic_owner != "presentation_compiler" ||
          artifact.tier.source_standard != "S-57" ||
          artifact.tier.provenance_refs.empty()) {
        std::cerr << "Package VSG artifact lost Tier 1 S-57 provenance\n";
        return false;
      }
      saw_s57_artifact = true;
    }
    if (!artifact.resource_id.empty() &&
        artifact.tier.source_standard == "S-57") {
      saw_s57_resource_artifact = true;
    }
  }

  if (!saw_s57_artifact || !saw_s57_resource_artifact ||
      manifest.stats.estimated_bytes == 0 ||
      manifest.stats.estimated_bytes > manifest.stats.memory_budget_bytes) {
    std::cerr << "Package VSG artifact manifest did not retain S-57 "
                 "resources or memory budget evidence\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "gpu-artifact-cache-smoke";
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
  const ocpn::render::RenderScene fixture_scene =
      builder.BuildFixtureScene(view, display);
  ocpn::render::NauticalRenderModel fixture_model =
      ocpn::render::BuildNauticalRenderModel(fixture_scene);
  for (ocpn::render::NauticalLayer& layer : fixture_model.layers) {
    for (ocpn::render::NauticalPrimitive& primitive : layer.primitives) {
      primitive.metadata["semantic_tier"] = "tier1_official_chart";
      primitive.metadata["source_standard"] = "s52-compatible";
    }
  }

  ocpn::render::GpuArtifactCacheOptions options;
  options.backend_target = "vsg";
  options.device_profile = "vulkan-proof-device";
  options.material_profile = "neutral-material-v1";
  options.cache_namespace = "opencpn-runtime-gpu";
  options.memory_budget_bytes = 32ULL * 1024ULL * 1024ULL;

  const ocpn::render::GpuArtifactCacheManifest fixture_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(fixture_model, options);
  if (!ValidateManifest(fixture_manifest, "fixture")) return 1;

  if (!HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kVertexBuffer) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kIndexBuffer) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kTextureAtlas) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kGlyphAtlas) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kLinePattern) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kMaterialPipeline) ||
      !HasKind(fixture_manifest,
               ocpn::render::GpuArtifactKind::kViewportTileEntry)) {
    std::cerr << "GPU artifact cache did not cover expected artifact kinds\n";
    return 1;
  }

  bool saw_tier = false;
  bool saw_provenance = false;
  bool saw_epoch = false;
  for (const ocpn::render::GpuArtifactRecord& artifact :
       fixture_manifest.artifacts) {
    if (artifact.tier.semantic_tier == "tier1_official_chart" &&
        artifact.tier.semantic_owner == "presentation_compiler") {
      saw_tier = true;
    }
    if (!artifact.tier.provenance_refs.empty() ||
        !artifact.tier.primitive_ids.empty()) {
      saw_provenance = true;
    }
    if (!artifact.cache_key.invalidation_epoch.empty() &&
        !artifact.invalidation_domain.empty()) {
      saw_epoch = true;
    }
  }
  if (!saw_tier || !saw_provenance || !saw_epoch) {
    std::cerr << "GPU artifact cache dropped tier, provenance, or invalidation handles\n";
    return 1;
  }

  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(ocpn::render::BuildS57ConverterFixtureCell());
  std::vector<ocpn::render::Diagnostic> package_diagnostics;
  if (!ocpn::render::ValidateS57ConverterFixturePackage(
          package, &package_diagnostics)) {
    std::cerr << "S-57 package fixture failed validation before cache compile\n";
    return 1;
  }

  ocpn::render::RenderView package_view = view;
  package_view.view_id = "cache2-s57-package-vsg";
  package_view.scale_denom = 5000.0;
  const ocpn::render::NauticalRenderModel package_model =
      ocpn::render::s52::CompileS52PackagePresentation(package, package_view,
                                                       display);
  std::vector<ocpn::render::Diagnostic> package_model_diagnostics;
  if (!ocpn::render::ValidateNauticalRenderModel(
          package_model, &package_model_diagnostics)) {
    std::cerr << "S-57 package presentation model failed validation before "
                 "cache compile\n";
    return 1;
  }

  ocpn::render::GpuArtifactCacheOptions package_options = options;
  package_options.material_profile = "vsg-neutral-package-v1";
  package_options.cache_namespace = "opencpn-vsg-production-slice";
  package_options.memory_budget_bytes = 4ULL * 1024ULL * 1024ULL;
  const ocpn::render::GpuArtifactCacheManifest package_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(package_model,
                                                  package_options);
  if (!ValidateManifest(package_manifest, "package") ||
      !CheckPackageVsgArtifacts(package_manifest, package)) {
    return 1;
  }

  const ocpn::render::GpuArtifactCacheManifest package_manifest_repeat =
      ocpn::render::BuildGpuArtifactCacheManifest(package_model,
                                                  package_options);
  if (ArtifactIds(package_manifest) != ArtifactIds(package_manifest_repeat)) {
    std::cerr << "Package VSG artifact cache ids are not deterministic\n";
    return 1;
  }

  const ocpn::render::GpuArtifactCacheManifest repeat_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(fixture_model, options);
  if (ArtifactIds(fixture_manifest) != ArtifactIds(repeat_manifest)) {
    std::cerr << "GPU artifact cache ids are not deterministic\n";
    return 1;
  }

  ocpn::render::GpuArtifactCacheOptions tiny_budget = options;
  tiny_budget.memory_budget_bytes = 512U;
  const ocpn::render::GpuArtifactCacheManifest over_budget =
      ocpn::render::BuildGpuArtifactCacheManifest(fixture_model, tiny_budget);
  std::vector<ocpn::render::Diagnostic> budget_diagnostics;
  if (!ocpn::render::ValidateGpuArtifactCacheManifest(over_budget,
                                                      &budget_diagnostics) ||
      !HasDiagnostic(budget_diagnostics, "gpu_artifact_cache_budget")) {
    std::cerr << "GPU artifact cache did not report memory budget pressure\n";
    return 1;
  }

  ocpn::render::GpuArtifactCacheManifest invalid = fixture_manifest;
  invalid.semantic_owner = "backend";
  std::vector<ocpn::render::Diagnostic> invalid_diagnostics;
  if (ocpn::render::ValidateGpuArtifactCacheManifest(invalid,
                                                     &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "gpu_artifact_cache_boundary")) {
    std::cerr << "GPU artifact cache accepted backend-owned semantics\n";
    return 1;
  }

  invalid = fixture_manifest;
  invalid.artifacts.front().usage = "s52_symbol_selection";
  invalid_diagnostics.clear();
  if (ocpn::render::ValidateGpuArtifactCacheManifest(invalid,
                                                     &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "gpu_artifact_cache_policy_leak")) {
    std::cerr << "GPU artifact cache accepted presentation policy in usage\n";
    return 1;
  }

  invalid = package_manifest;
  invalid.artifacts.front().pipeline_key =
      "helm_tier2_overlay_icon_registry";
  invalid_diagnostics.clear();
  if (ocpn::render::ValidateGpuArtifactCacheManifest(
          invalid, &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "gpu_artifact_cache_policy_leak")) {
    std::cerr << "GPU artifact cache accepted Helm overlay policy in VSG "
                 "package artifacts\n";
    return 1;
  }

  const ocpn::render::ChartSourceProduct quilting_product =
      ocpn::render::depth::BuildRasterQuiltingFixtureProduct();
  const ocpn::render::RenderScene quilting_scene =
      builder.BuildSceneFromChartSource(quilting_product, view, display);
  const ocpn::render::NauticalRenderModel quilting_model =
      ocpn::render::BuildNauticalRenderModel(quilting_scene);
  const ocpn::render::GpuArtifactCacheManifest quilting_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(quilting_model, options);
  if (!ValidateManifest(quilting_manifest, "quilting")) return 1;
  if (!HasKind(quilting_manifest,
               ocpn::render::GpuArtifactKind::kRasterTexture)) {
    std::cerr << "GPU artifact cache did not retain raster texture artifacts\n";
    return 1;
  }

  std::cout << "ok gpu-artifact-cache: " << fixture_manifest.artifacts.size()
            << " fixture artifacts, " << quilting_manifest.artifacts.size()
            << " raster artifacts, " << package_manifest.artifacts.size()
            << " package artifacts, " << fixture_manifest.stats.estimated_bytes
            << " estimated bytes\n";
  return 0;
}
