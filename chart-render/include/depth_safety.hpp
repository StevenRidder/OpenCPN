// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "depth_tessellation.hpp"

#include <string>
#include <vector>

namespace ocpn::render::depth {

enum class DepthSafetyClass {
  kShallow,
  kUnsafe,
  kSafetyBand,
  kDeep
};

struct DepthAreaStyle {
  DepthSafetyClass safety_class = DepthSafetyClass::kDeep;
  std::string fill_ref;
  std::string palette_bucket;
};

const char* ToString(DepthSafetyClass safety_class);

DepthSafetyClass ClassifyDepth(double depth_m, const DisplayState& display);

DepthAreaStyle StyleDepthArea(double depth_m, const DisplayState& display);

bool ShouldShowDepthContour(double contour_m, const RenderView& view,
                            const DisplayState& display);

RenderScene BuildSafetyDepthFixture(RenderView view, DisplayState display);

bool ValidateSafetyDepthScene(const RenderScene& scene,
                              std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::depth
