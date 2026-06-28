// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"
#include "s52/s52_command_builder.hpp"
#include "vsg/vsg_backend.hpp"
#include "vsg/vsg_gpu_cache.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasKind(const ocpn::render::vsg::VsgGpuCacheManifest& manifest,
             ocpn::render::vsg::VsgGpuAssetKind kind) {
  for (const ocpn::render::vsg::VsgGpuAsset& asset : manifest.assets) {
    if (asset.kind == kind) return true;
  }
  return false;
}

bool HasResidency(const ocpn::render::vsg::VsgGpuCacheManifest& manifest,
                  ocpn::render::vsg::VsgGpuResidency residency) {
  for (const ocpn::render::vsg::VsgGpuAsset& asset : manifest.assets) {
    if (asset.residency == residency) return true;
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

bool HasError(const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.severity == ocpn::render::DiagnosticSeverity::kError) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> AssetIds(
    const ocpn::render::vsg::VsgGpuCacheManifest& manifest) {
  std::vector<std::string> ids;
  for (const ocpn::render::vsg::VsgGpuAsset& asset : manifest.assets) {
    ids.push_back(asset.asset_id);
  }
  return ids;
}

bool ValidateManifest(const ocpn::render::vsg::VsgGpuCacheManifest& manifest,
                      const char* label) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::vsg::ValidateVsgGpuCacheManifest(manifest,
                                                     &diagnostics)) {
    return true;
  }
  std::cerr << label << " VSG GPU cache manifest failed validation\n";
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "vsg-gpu-cache-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  ocpn::render::s52::S52CommandBuilder builder;
  const ocpn::render::RenderScene fixture_scene =
      builder.BuildFixtureScene(view, display);
  const ocpn::render::NauticalRenderModel fixture_model =
      ocpn::render::BuildNauticalRenderModel(fixture_scene);

  const ocpn::render::vsg::VsgGpuCacheManifest fixture_manifest =
      ocpn::render::vsg::BuildVsgGpuCacheManifest(fixture_model);
  if (!ValidateManifest(fixture_manifest, "fixture")) return 1;

  if (!HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kTexture) ||
      !HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kAtlas) ||
      !HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kVertexBuffer) ||
      !HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kIndexBuffer) ||
      !HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kInstanceBuffer) ||
      !HasKind(fixture_manifest,
               ocpn::render::vsg::VsgGpuAssetKind::kUniformBlock)) {
    std::cerr << "VSG GPU cache did not produce the expected asset kinds\n";
    return 1;
  }

  if (!HasResidency(fixture_manifest,
                    ocpn::render::vsg::VsgGpuResidency::kMachineLocal) ||
      !HasResidency(fixture_manifest,
                    ocpn::render::vsg::VsgGpuResidency::kSceneLocal) ||
      !HasResidency(fixture_manifest,
                    ocpn::render::vsg::VsgGpuResidency::kFrameLocal)) {
    std::cerr << "VSG GPU cache did not classify asset residency levels\n";
    return 1;
  }

  bool saw_provenance = false;
  for (const ocpn::render::vsg::VsgGpuAsset& asset :
       fixture_manifest.assets) {
    if (!asset.primitive_ids.empty() && !asset.provenance_refs.empty()) {
      saw_provenance = true;
    }
  }
  if (!saw_provenance) {
    std::cerr << "VSG GPU cache dropped neutral primitive provenance\n";
    return 1;
  }

  const ocpn::render::vsg::VsgGpuCacheManifest fixture_manifest_repeat =
      ocpn::render::vsg::BuildVsgGpuCacheManifest(fixture_model);
  if (AssetIds(fixture_manifest) != AssetIds(fixture_manifest_repeat)) {
    std::cerr << "VSG GPU cache asset ids are not deterministic\n";
    return 1;
  }

  const ocpn::render::ChartSourceProduct quilting_product =
      ocpn::render::depth::BuildRasterQuiltingFixtureProduct();
  const ocpn::render::RenderScene quilting_scene =
      builder.BuildSceneFromChartSource(quilting_product, view, display);
  const ocpn::render::NauticalRenderModel quilting_model =
      ocpn::render::BuildNauticalRenderModel(quilting_scene);
  const ocpn::render::vsg::VsgGpuCacheManifest quilting_manifest =
      ocpn::render::vsg::BuildVsgGpuCacheManifest(quilting_model);
  if (!ValidateManifest(quilting_manifest, "quilting")) return 1;
  if (quilting_manifest.stats.texture_assets < 4 ||
      quilting_manifest.stats.estimated_bytes < 1024U * 1024U) {
    std::cerr << "VSG GPU cache did not retain raster texture assets\n";
    return 1;
  }

  ocpn::render::vsg::VsgGpuCacheManifest invalid = fixture_manifest;
  invalid.semantic_owner = "backend";
  std::vector<ocpn::render::Diagnostic> invalid_diagnostics;
  if (ocpn::render::vsg::ValidateVsgGpuCacheManifest(
          invalid, &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "vsg_cache_boundary")) {
    std::cerr << "VSG GPU cache accepted backend-owned semantics\n";
    return 1;
  }

  ocpn::render::vsg::VsgBackend backend;
  ocpn::render::RenderTarget target;
  target.kind = ocpn::render::RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.target_id = "vsg-gpu-cache-smoke";
  const ocpn::render::RenderResult result =
      backend.RenderModel(quilting_model, target);
  if (!HasDiagnostic(result.diagnostics, "backend.vsg_gpu_cache") ||
      HasError(result.diagnostics)) {
    std::cerr << "VSG backend did not prepare a valid GPU cache manifest\n";
    return 1;
  }

  std::cout << "ok vsg-gpu-cache: " << fixture_manifest.assets.size()
            << " fixture assets, " << quilting_manifest.assets.size()
            << " raster assets, "
            << quilting_manifest.stats.estimated_bytes
            << " estimated raster bytes\n";
  return 0;
}
