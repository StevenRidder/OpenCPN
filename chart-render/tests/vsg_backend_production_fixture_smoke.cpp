// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"
#include "vsg/vsg_backend.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

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

std::uint64_t Fnva64Bytes(std::uint64_t hash, const std::uint8_t* bytes,
                          std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::uint64_t Fnva64Uint32(std::uint64_t hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const std::uint8_t byte =
        static_cast<std::uint8_t>((value >> shift) & 0xffU);
    hash = Fnva64Bytes(hash, &byte, 1U);
  }
  return hash;
}

std::string HexHash(const ocpn::render::PixelBuffer& pixels) {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = Fnva64Uint32(hash, pixels.pixel_size.width);
  hash = Fnva64Uint32(hash, pixels.pixel_size.height);
  if (!pixels.rgba8.empty()) {
    hash = Fnva64Bytes(hash, pixels.rgba8.data(), pixels.rgba8.size());
  }

  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

bool HasVisibleVariation(const ocpn::render::PixelBuffer& pixels) {
  if (pixels.rgba8.size() < 8U) return false;
  const std::uint8_t r0 = pixels.rgba8[0];
  const std::uint8_t g0 = pixels.rgba8[1];
  const std::uint8_t b0 = pixels.rgba8[2];
  for (std::size_t offset = 4U; offset + 3U < pixels.rgba8.size();
       offset += 4U) {
    if (pixels.rgba8[offset] != r0 || pixels.rgba8[offset + 1U] != g0 ||
        pixels.rgba8[offset + 2U] != b0) {
      return true;
    }
  }
  return false;
}

bool ValidateBackendResult(const ocpn::render::RenderResult& result,
                           const char* label) {
  if (!result.ok || HasError(result.diagnostics)) {
    std::cerr << label << " VSG backend render failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : result.diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return false;
  }
  if (result.pixels.pixel_size.width != 256U ||
      result.pixels.pixel_size.height != 256U ||
      result.pixels.rgba8.size() != 256U * 256U * 4U ||
      !HasVisibleVariation(result.pixels)) {
    std::cerr << label
              << " VSG backend did not produce a deterministic image buffer\n";
    return false;
  }
  for (const std::string& code :
       {"backend.vsg_gpu_cache", "backend.vsg_production_fixture",
        "backend.vsg_artifact_fallback"}) {
    if (!HasDiagnostic(result.diagnostics, code)) {
      std::cerr << label << " VSG backend missing diagnostic " << code
                << "\n";
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(ocpn::render::BuildS57ConverterFixtureCell());
  std::vector<ocpn::render::Diagnostic> package_diagnostics;
  if (!ocpn::render::ValidateS57ConverterFixturePackage(
          package, &package_diagnostics)) {
    std::cerr << "S-57 package fixture failed validation before backend test\n";
    return 1;
  }

  ocpn::render::RenderView view;
  view.view_id = "backend2-s57-package-vsg";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 5000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  const ocpn::render::NauticalRenderModel model =
      ocpn::render::s52::CompileS52PackagePresentation(package, view, display);
  std::vector<ocpn::render::Diagnostic> model_diagnostics;
  if (!ocpn::render::ValidateNauticalRenderModel(model, &model_diagnostics)) {
    std::cerr << "S-57 package presentation model failed validation before "
                 "backend test\n";
    for (const ocpn::render::Diagnostic& diagnostic : model_diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  ocpn::render::RenderTarget target;
  target.kind = ocpn::render::RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.device_pixel_ratio = 1.0;
  target.target_id = "backend2-offscreen-golden";

  ocpn::render::vsg::VsgBackend backend;
  const ocpn::render::RenderResult result =
      backend.RenderModel(model, target);
  if (!ValidateBackendResult(result, "offscreen")) return 1;

  const ocpn::render::RenderResult repeat =
      backend.RenderModel(model, target);
  if (!ValidateBackendResult(repeat, "repeat")) return 1;

  const std::string fixture_hash = HexHash(result.pixels);
  const std::string repeat_hash = HexHash(repeat.pixels);
  const std::string expected_hash = "009410097424697d";
  if (fixture_hash != repeat_hash || fixture_hash != expected_hash) {
    std::cerr << "VSG backend production fixture hash mismatch: got "
              << fixture_hash << ", repeat " << repeat_hash << ", expected "
              << expected_hash << "\n";
    return 1;
  }

  ocpn::render::RenderTarget swapchain = target;
  swapchain.kind = ocpn::render::RenderTargetKind::kSwapchain;
  swapchain.target_id = "backend2-swapchain-diagnostic";
  const ocpn::render::RenderResult swapchain_result =
      backend.RenderModel(model, swapchain);
  if (!ValidateBackendResult(swapchain_result, "swapchain")) return 1;

  ocpn::render::RenderTarget invalid_target = target;
  invalid_target.pixel_size = {0, 256};
  const ocpn::render::RenderResult invalid_target_result =
      backend.RenderModel(model, invalid_target);
  if (invalid_target_result.ok ||
      !HasDiagnostic(invalid_target_result.diagnostics,
                     "backend.vsg_target_invalid")) {
    std::cerr << "VSG backend accepted an invalid render target\n";
    return 1;
  }

  ocpn::render::NauticalRenderModel invalid_model = model;
  invalid_model.layers.front().primitives.front().handoff.semantic_owner =
      "backend";
  const ocpn::render::RenderResult invalid_handoff =
      backend.RenderModel(invalid_model, target);
  if (invalid_handoff.ok ||
      !HasDiagnostic(invalid_handoff.diagnostics,
                     "backend.vsg_handoff_rejected")) {
    std::cerr << "VSG backend accepted backend-owned chart semantics\n";
    return 1;
  }

  std::cout << "ok vsg-backend-production-fixture: hash=" << fixture_hash
            << " bytes=" << result.pixels.rgba8.size()
            << " diagnostics=" << result.diagnostics.size() << "\n";
  return 0;
}
