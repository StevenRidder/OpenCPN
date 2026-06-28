// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_tessellation.hpp"

#include "conversion_trace.hpp"

#include <sstream>
#include <utility>

namespace ocpn::render::depth {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Inspect depth tessellation input before changing backend code.";
  return diagnostic;
}

std::string TriangleCountText(std::size_t value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

std::vector<Triangle> TriangleFan(const std::vector<Point2>& ring) {
  std::vector<Triangle> triangles;
  if (ring.size() < 3) {
    return triangles;
  }
  for (std::size_t i = 1; i + 1 < ring.size(); ++i) {
    triangles.push_back({ring.front(), ring[i], ring[i + 1]});
  }
  return triangles;
}

bool IsArea(DepthFeatureKind kind) {
  return kind == DepthFeatureKind::kLandArea ||
         kind == DepthFeatureKind::kDepthArea ||
         kind == DepthFeatureKind::kAreaPattern;
}

RenderCommand BaseCommand(const TessellationInput& input, CommandType type,
                          std::string role) {
  RenderCommand command;
  command.type = type;
  command.command_id = "cmd-" + input.feature_id;
  command.role = std::move(role);
  command.coordinate_space = CoordinateSpace::kTarget;
  command.provenance_refs.push_back(input.provenance_id);
  command.conversion_trace_refs.push_back("trace:" + input.provenance_id);
  command.metadata["feature_kind"] = ToString(input.kind);
  command.metadata["s52_rule"] = input.s52_rule_id;
  return command;
}

Geometry RingGeometry(std::string id, std::vector<Point2> ring) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.rings.push_back(std::move(ring));
  return geometry;
}

Geometry LineGeometry(std::string id, std::vector<Point2> points) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.points = std::move(points);
  return geometry;
}

ProvenanceRecord Provenance(std::string provenance_id, std::string object_id,
                            std::string object_class) {
  ProvenanceRecord provenance;
  provenance.provenance_id = std::move(provenance_id);
  provenance.source_chart_id = "depth-tessellation-fixture";
  provenance.source_chart_edition = "poc";
  provenance.source_object_id = std::move(object_id);
  provenance.source_object_class = std::move(object_class);
  provenance.source_geometry_hash = "fixture";
  provenance.conversion_stage = "depth_tessellation_fixture";
  provenance.transform_chain = {"source:wgs84", "projection:web_mercator_tile",
                                "target:px", "tessellation:triangle_fan"};
  return provenance;
}

}  // namespace

const char* ToString(DepthFeatureKind kind) {
  switch (kind) {
    case DepthFeatureKind::kShoreline:
      return "shoreline";
    case DepthFeatureKind::kLandArea:
      return "land_area";
    case DepthFeatureKind::kDepthArea:
      return "depth_area";
    case DepthFeatureKind::kAreaPattern:
      return "area_pattern";
  }
  return "unknown";
}

TessellationResult TessellateDepthFeature(const TessellationInput& input) {
  TessellationResult result;
  if (input.feature_id.empty() || input.provenance_id.empty() ||
      input.s52_rule_id.empty()) {
    result.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_tessellation_identity",
        "Depth tessellation input is missing feature, provenance, or rule id."));
    return result;
  }

  if (IsArea(input.kind)) {
    if (input.geometry.rings.empty() || input.geometry.rings.front().size() < 3) {
      result.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "depth_tessellation_area_geometry",
          "Area tessellation requires at least one ring with three points.",
          {input.provenance_id}));
      return result;
    }

    TessellationMesh mesh;
    mesh.geometry_id = input.geometry.geometry_id;
    mesh.outline = input.geometry.rings.front();
    mesh.triangles = TriangleFan(mesh.outline);

    RenderCommand command =
        BaseCommand(input, CommandType::kFillArea, ToString(input.kind));
    command.fill_ref = input.fill_ref;
    command.pattern_ref = input.pattern_ref.empty() ? "none" : input.pattern_ref;
    command.geometries.push_back(input.geometry);
    command.metadata["tessellation"] = "triangle_fan";
    command.metadata["triangle_count"] =
        TriangleCountText(mesh.triangles.size());

    result.meshes.push_back(std::move(mesh));
    result.commands.push_back(std::move(command));
    result.ok = true;
    return result;
  }

  if (input.geometry.points.size() < 2) {
    result.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_tessellation_line_geometry",
        "Shoreline tessellation requires at least two points.",
        {input.provenance_id}));
    return result;
  }

  RenderCommand command =
      BaseCommand(input, CommandType::kStrokeLine, ToString(input.kind));
  command.line_style_ref = input.line_style_ref;
  command.width_px = 1.0;
  command.geometries.push_back(input.geometry);
  command.metadata["tessellation"] = "polyline";

  result.commands.push_back(std::move(command));
  result.ok = true;
  return result;
}

RenderScene BuildDepthTessellationFixture(RenderView view,
                                          DisplayState display) {
  RenderScene scene;
  scene.scene_id = "depth-tessellation-fixture";
  scene.source_epoch = "fixture:v1";
  scene.render_view = std::move(view);
  scene.display_state = std::move(display);
  scene.resource_table.resources.push_back(
      {"land-fill", ResourceType::kPalette, "fixture", "prov-land-area"});
  scene.resource_table.resources.push_back(
      {"depth-area-fill", ResourceType::kPalette, "fixture", "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"drying-pattern", ResourceType::kAreaPattern, "fixture",
       "prov-drying-area"});
  scene.resource_table.resources.push_back(
      {"shoreline-line", ResourceType::kLineStyle, "fixture",
       "prov-shoreline"});

  scene.provenance_table.push_back(
      Provenance("prov-land-area", "LNDARE.1", "LNDARE"));
  scene.provenance_table.push_back(
      Provenance("prov-depth-area", "DEPARE.1", "DEPARE"));
  scene.provenance_table.push_back(
      Provenance("prov-drying-area", "DRGARE.1", "DRGARE"));
  scene.provenance_table.push_back(
      Provenance("prov-shoreline", "COALNE.1", "COALNE"));

  std::vector<TessellationInput> inputs = {
      {"land-area",
       DepthFeatureKind::kLandArea,
       RingGeometry("geom-land-area",
                    {{16.0, 16.0}, {118.0, 24.0}, {104.0, 96.0},
                     {24.0, 82.0}}),
       "land-fill",
       "none",
       "",
       "prov-land-area",
       "fixture:land_area"},
      {"depth-area",
       DepthFeatureKind::kDepthArea,
       RingGeometry("geom-depth-area",
                    {{8.0, 112.0}, {236.0, 110.0}, {232.0, 232.0},
                     {12.0, 228.0}}),
       "depth-area-fill",
       "none",
       "",
       "prov-depth-area",
       "fixture:depth_area"},
      {"drying-area",
       DepthFeatureKind::kAreaPattern,
       RingGeometry("geom-drying-area",
                    {{138.0, 40.0}, {218.0, 38.0}, {220.0, 86.0},
                     {142.0, 90.0}}),
       "depth-area-fill",
       "drying-pattern",
       "",
       "prov-drying-area",
       "fixture:drying_pattern"},
      {"shoreline",
       DepthFeatureKind::kShoreline,
       LineGeometry("geom-shoreline",
                    {{20.0, 90.0}, {72.0, 104.0}, {126.0, 96.0},
                     {184.0, 118.0}, {232.0, 112.0}}),
       "",
       "",
       "shoreline-line",
       "prov-shoreline",
       "fixture:shoreline"}};

  CommandGroup group;
  group.group_id = "depth-tessellation";
  group.s52_layer = "area_line";
  for (const TessellationInput& input : inputs) {
    TessellationResult tessellation = TessellateDepthFeature(input);
    scene.diagnostics.insert(scene.diagnostics.end(),
                             tessellation.diagnostics.begin(),
                             tessellation.diagnostics.end());
    group.commands.insert(group.commands.end(), tessellation.commands.begin(),
                          tessellation.commands.end());
  }
  scene.command_groups.push_back(std::move(group));
  return scene;
}

bool ValidateDepthTessellationScene(const RenderScene& scene,
                                    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool has_fill = false;
  bool has_stroke = false;
  bool ok = ValidateRenderSceneTraceability(scene, &out);
  for (const CommandGroup& group : scene.command_groups) {
    for (const RenderCommand& command : group.commands) {
      if (command.type == CommandType::kFillArea) {
        has_fill = true;
        if (command.metadata.find("triangle_count") == command.metadata.end()) {
          out.push_back(MakeDiagnostic(
              DiagnosticSeverity::kError, "depth_missing_triangle_count",
              "Depth fill command is missing triangle_count metadata.",
              command.provenance_refs));
          ok = false;
        }
      }
      if (command.type == CommandType::kStrokeLine) {
        has_stroke = true;
      }
    }
  }
  if (!has_fill || !has_stroke) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_missing_command_kind",
        "Depth tessellation scene must include fill and shoreline stroke."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render::depth
