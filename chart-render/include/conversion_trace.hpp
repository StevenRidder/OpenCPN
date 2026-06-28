// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <string>
#include <vector>

namespace ocpn::render {

enum class ConversionTraceStage {
  kSourceObject,
  kProjection,
  kGeometryGeneration,
  kS52Rule,
  kTilePlacement,
  kRenderCommand
};

struct CoordinateTraceSample {
  GeoPoint geographic;
  Point2 projected;
  Point2 target;
};

struct ConversionTraceRecord {
  std::string trace_id;
  std::string provenance_id;
  std::string source_chart_id;
  std::string source_object_id;
  std::string source_object_class;
  std::string source_geometry_hash;
  std::string generated_geometry_id;
  std::string render_command_id;
  std::string s52_rule_id;
  std::string target_id;
  GeoBounds source_bounds;
  GeoBounds target_bounds;
  std::vector<std::string> transform_chain;
  std::vector<CoordinateTraceSample> coordinate_samples;
};

const char* ToString(ConversionTraceStage stage);

ConversionTraceRecord BuildConversionTrace(const ProvenanceRecord& provenance,
                                           const RenderCommand& command,
                                           const Geometry& geometry,
                                           const RenderView& view,
                                           const std::string& s52_rule_id);

void AttachConversionTrace(RenderCommand* command,
                           const ConversionTraceRecord& trace);

Diagnostic MakeWrongLocationDiagnostic(const ConversionTraceRecord& trace,
                                       const std::string& message);

bool ValidateRenderSceneTraceability(const RenderScene& scene,
                                     std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
