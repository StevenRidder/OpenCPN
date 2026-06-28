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

RenderResult VsgBackend::Render(const RenderScene& scene,
                                const RenderTarget& target) {
  return RenderModel(BuildNauticalRenderModel(scene), target);
}

RenderResult VsgBackend::RenderModel(const NauticalRenderModel& model,
                                     const RenderTarget& target) {
  RenderResult result;
  result.ok = false;
  result.pixels.pixel_size = target.pixel_size;
  const std::size_t px_count =
      static_cast<std::size_t>(target.pixel_size.width) * target.pixel_size.height;
  result.pixels.rgba8.resize(px_count * 4);
  std::fill(result.pixels.rgba8.begin(), result.pixels.rgba8.end(), 0);

  std::vector<Diagnostic> validation_diagnostics;
  ValidateNauticalRenderModel(model, &validation_diagnostics);
  result.diagnostics.insert(result.diagnostics.end(),
                            validation_diagnostics.begin(),
                            validation_diagnostics.end());

  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kWarning;
  diagnostic.code = "backend.vsg_placeholder";
  diagnostic.message =
      "VulkanSceneGraph backend skeleton accepted a neutral nautical render "
      "model but is not implemented.";
  diagnostic.suggested_action =
      "VSG lane should replay neutral primitives without owning chart-source, "
      "S-52, quilting, or scheduling semantics.";
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

}  // namespace ocpn::render::vsg
