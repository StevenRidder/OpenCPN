// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_backend.hpp"

namespace ocpn::render::vsg {

class VsgBackend final : public IRenderBackend {
 public:
  const char* Name() const override;
  RenderResult Render(const RenderScene& scene,
                      const RenderTarget& target) override;
  RenderResult RenderModel(const NauticalRenderModel& model,
                           const RenderTarget& target) override;
};

}  // namespace ocpn::render::vsg
