// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart_source.hpp"
#include "render_scene.hpp"

#include <vector>

namespace ocpn::render::depth {

ChartSourceProduct BuildRasterQuiltingFixtureProduct();

bool ValidateRasterQuiltingProduct(const ChartSourceProduct& product,
                                   std::vector<Diagnostic>* diagnostics);

bool ValidateRasterQuiltingScene(const RenderScene& scene,
                                 std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::depth
