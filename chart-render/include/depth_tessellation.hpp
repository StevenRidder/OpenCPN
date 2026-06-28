// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <string>
#include <vector>

namespace ocpn::render::depth {

enum class DepthFeatureKind {
  kShoreline,
  kLandArea,
  kDepthArea,
  kAreaPattern
};

struct Triangle {
  Point2 a;
  Point2 b;
  Point2 c;
};

struct TessellationInput {
  std::string feature_id;
  DepthFeatureKind kind = DepthFeatureKind::kDepthArea;
  Geometry geometry;
  std::string fill_ref;
  std::string pattern_ref;
  std::string line_style_ref;
  std::string provenance_id;
  std::string s52_rule_id;
};

struct TessellationMesh {
  std::string geometry_id;
  std::vector<Triangle> triangles;
  std::vector<Point2> outline;
};

struct TessellationResult {
  std::vector<RenderCommand> commands;
  std::vector<TessellationMesh> meshes;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

const char* ToString(DepthFeatureKind kind);

TessellationResult TessellateDepthFeature(const TessellationInput& input);

RenderScene BuildDepthTessellationFixture(RenderView view,
                                          DisplayState display);

bool ValidateDepthTessellationScene(const RenderScene& scene,
                                    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::depth
