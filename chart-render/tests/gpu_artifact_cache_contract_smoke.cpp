// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"
#include "gpu_artifact_cache_contract.hpp"
#include "s52/s52_command_builder.hpp"

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
            << " raster artifacts, " << fixture_manifest.stats.estimated_bytes
            << " estimated bytes\n";
  return 0;
}
