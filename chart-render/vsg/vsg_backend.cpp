// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "vsg/vsg_backend.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace ocpn::render::vsg {

const char* VsgBackend::Name() const {
  return "vulkan-scenegraph-placeholder";
}

RenderResult VsgBackend::Render(const RenderScene&,
                                const RenderTarget& target) {
  RenderResult result;
  result.ok = false;
  result.pixels.pixel_size = target.pixel_size;
  const std::size_t px_count =
      static_cast<std::size_t>(target.pixel_size.width) * target.pixel_size.height;
  result.pixels.rgba8.resize(px_count * 4);
  std::fill(result.pixels.rgba8.begin(), result.pixels.rgba8.end(), 0);

  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kWarning;
  diagnostic.code = "backend.vsg_placeholder";
  diagnostic.message =
      "VulkanSceneGraph backend skeleton is present but not implemented.";
  diagnostic.suggested_action =
      "VSG lane should replace this placeholder with command replay.";
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

}  // namespace ocpn::render::vsg
