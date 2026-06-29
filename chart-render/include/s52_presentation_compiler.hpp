// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart_source.hpp"
#include "nautical_render_model.hpp"
#include "portable_nautical_package.hpp"

#include <string>

namespace ocpn::render::s52 {

struct PresentationAssets {
  std::string depth_shallow_fill = "s52-depth-shallow-fill";
  std::string depth_safe_fill = "s52-depth-safe-fill";
  std::string safety_contour_line = "s52-safety-contour-line";
  std::string depth_contour_line = "s52-depth-contour-line";
  std::string lateral_buoy_symbol = "s52-lateral-buoy-symbol";
  std::string default_symbol = "s52-default-symbol";
  std::string label_font = "s52-label-font";
};

struct PresentationOptions {
  bool emit_unsupported_diagnostics = true;
  bool emit_filtered_diagnostics = true;
  std::string compiler_id = "s52-presentation-compiler";
};

class S52PresentationCompiler {
 public:
  NauticalRenderModel Compile(const ChartSourceProduct& normalized_features,
                              RenderView view, DisplayState display,
                              PresentationAssets assets = {},
                              PresentationOptions options = {}) const;

  NauticalRenderModel Compile(const PortableNauticalPackage& package,
                              RenderView view, DisplayState display,
                              PresentationAssets assets = {},
                              PresentationOptions options = {}) const;
};

NauticalRenderModel CompileS52Presentation(
    const ChartSourceProduct& normalized_features, RenderView view,
    DisplayState display, PresentationAssets assets = {},
    PresentationOptions options = {});

NauticalRenderModel CompileS52PackagePresentation(
    const PortableNauticalPackage& package, RenderView view,
    DisplayState display, PresentationAssets assets = {},
    PresentationOptions options = {});

}  // namespace ocpn::render::s52
