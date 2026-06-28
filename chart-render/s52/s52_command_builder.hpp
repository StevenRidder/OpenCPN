// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart_source.hpp"
#include "render_scene.hpp"

namespace ocpn::render::s52 {

class S52CommandBuilder {
 public:
  RenderScene BuildEmptyScene(RenderView view, DisplayState display) const;
  RenderScene BuildSceneFromChartSource(const ChartSourceProduct& product,
                                        RenderView view,
                                        DisplayState display) const;
  RenderScene BuildFixtureScene(RenderView view, DisplayState display) const;
};

}  // namespace ocpn::render::s52
