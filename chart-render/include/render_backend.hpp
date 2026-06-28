// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "nautical_render_model.hpp"
#include "render_scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

enum class RenderTargetKind {
  kOffscreen,
  kSwapchain
};

struct RenderTarget {
  RenderTargetKind kind = RenderTargetKind::kOffscreen;
  PixelSize pixel_size;
  double device_pixel_ratio = 1.0;
  std::string target_id;
};

struct PixelBuffer {
  PixelSize pixel_size;
  std::vector<std::uint8_t> rgba8;
};

struct RenderResult {
  bool ok = false;
  PixelBuffer pixels;
  std::vector<Diagnostic> diagnostics;
};

class IRenderBackend {
 public:
  virtual ~IRenderBackend() = default;

  virtual const char* Name() const = 0;
  virtual RenderResult Render(const RenderScene& scene,
                              const RenderTarget& target) = 0;
  virtual RenderResult RenderModel(const NauticalRenderModel& model,
                                   const RenderTarget& target) = 0;
};

}  // namespace ocpn::render
