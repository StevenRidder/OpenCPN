// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_view.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kRenderSceneSchemaVersion = 1;

enum class CoordinateSpace {
  kGeographic,
  kProjected,
  kTarget,
  kGlyph,
  kRaster
};

enum class ResourceType {
  kSymbol,
  kLineStyle,
  kAreaPattern,
  kFont,
  kRasterTexture,
  kGeometryBuffer,
  kPalette
};

enum class CommandType {
  kFillArea,
  kStrokeLine,
  kPlaceSymbol,
  kDrawText,
  kDrawSounding,
  kDrawRasterSheet,
  kPushClip,
  kPopClip
};

enum class DiagnosticSeverity {
  kInfo,
  kWarning,
  kError
};

struct Point2 {
  double x = 0.0;
  double y = 0.0;
};

struct Geometry {
  std::string geometry_id;
  CoordinateSpace coordinate_space = CoordinateSpace::kGeographic;
  std::vector<Point2> points;
  std::vector<std::vector<Point2>> rings;
};

struct ResourceRecord {
  std::string resource_id;
  ResourceType type = ResourceType::kSymbol;
  std::string content_hash;
  std::string provenance_id;
  std::map<std::string, std::string> metrics;
  std::map<std::string, std::string> backend_hints;
};

struct ResourceTable {
  std::vector<ResourceRecord> resources;
};

struct ProvenanceRecord {
  std::string provenance_id;
  std::string source_chart_id;
  std::string source_chart_edition;
  std::string source_update;
  std::string source_object_id;
  std::string source_object_class;
  std::string source_geometry_hash;
  std::string generated_geometry_hash;
  std::string target_geometry_hash;
  std::string s52_rule_id;
  std::string render_command_id;
  std::string conversion_stage;
  std::vector<std::string> transform_chain;
  std::string quilt_decision_id;
  std::vector<std::string> warnings;
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::kInfo;
  std::string code;
  std::string message;
  std::vector<std::string> provenance_refs;
  std::string suggested_action;
};

struct RenderCommand {
  CommandType type = CommandType::kFillArea;
  std::string command_id;
  std::string role;
  CoordinateSpace coordinate_space = CoordinateSpace::kGeographic;
  std::vector<Geometry> geometries;
  std::string fill_ref;
  std::string pattern_ref;
  std::string stroke_ref;
  std::string line_style_ref;
  std::string symbol_ref;
  std::string font_ref;
  std::string texture_ref;
  std::string clip_ref;
  std::string text;
  double depth_m = 0.0;
  double width_px = 0.0;
  double opacity = 1.0;
  double rotation_deg = 0.0;
  double scale = 1.0;
  Point2 position;
  std::string anchor;
  std::string priority;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> conversion_trace_refs;
  std::map<std::string, std::string> metadata;
};

struct CommandGroup {
  std::string group_id;
  int chart_priority = 0;
  std::string s52_layer;
  int quilt_rank = 0;
  std::vector<RenderCommand> commands;
};

struct RenderScene {
  std::uint32_t schema_version = kRenderSceneSchemaVersion;
  std::string scene_id;
  std::string source_epoch;
  RenderView render_view;
  DisplayState display_state;
  ResourceTable resource_table;
  std::vector<CommandGroup> command_groups;
  std::vector<ProvenanceRecord> provenance_table;
  std::vector<Diagnostic> diagnostics;
};

}  // namespace ocpn::render
