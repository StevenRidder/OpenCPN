// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_safety.hpp"

#include "conversion_trace.hpp"

#include <cmath>
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
      "Inspect depth safety metadata before changing renderer code.";
  return diagnostic;
}

std::string Number(double value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

Geometry ContourGeometry() {
  Geometry geometry;
  geometry.geometry_id = "geom-safety-contour";
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.points = {{20.0, 168.0}, {78.0, 154.0}, {136.0, 148.0},
                     {212.0, 126.0}};
  return geometry;
}

RenderCommand SafetyContourCommand(const RenderView& view,
                                   const DisplayState& display) {
  RenderCommand command;
  command.type = CommandType::kStrokeLine;
  command.command_id = "cmd-safety-contour";
  command.role = "safety_contour";
  command.coordinate_space = CoordinateSpace::kTarget;
  command.line_style_ref = "safety-contour-line";
  command.width_px = 2.0;
  command.geometries.push_back(ContourGeometry());
  command.provenance_refs.push_back("prov-safety-contour");
  command.conversion_trace_refs.push_back("trace:prov-safety-contour");
  command.metadata["s52_rule"] = "fixture:safety_contour";
  command.metadata["contour_m"] = Number(display.safety_contour_m);
  command.metadata["safety_class"] = ToString(DepthSafetyClass::kSafetyBand);
  command.metadata["zoom_visible"] =
      ShouldShowDepthContour(display.safety_contour_m, view, display) ? "true"
                                                                      : "false";
  return command;
}

bool HasMetadata(const RenderCommand& command, const char* key) {
  return command.metadata.find(key) != command.metadata.end();
}

}  // namespace

const char* ToString(DepthSafetyClass safety_class) {
  switch (safety_class) {
    case DepthSafetyClass::kShallow:
      return "shallow";
    case DepthSafetyClass::kUnsafe:
      return "unsafe";
    case DepthSafetyClass::kSafetyBand:
      return "safety_band";
    case DepthSafetyClass::kDeep:
      return "deep";
  }
  return "unknown";
}

DepthSafetyClass ClassifyDepth(double depth_m, const DisplayState& display) {
  if (depth_m <= display.shallow_contour_m) {
    return DepthSafetyClass::kShallow;
  }
  if (depth_m <= display.safety_depth_m) {
    return DepthSafetyClass::kUnsafe;
  }
  if (depth_m <= display.safety_contour_m) {
    return DepthSafetyClass::kSafetyBand;
  }
  return DepthSafetyClass::kDeep;
}

DepthAreaStyle StyleDepthArea(double depth_m, const DisplayState& display) {
  DepthAreaStyle style;
  style.safety_class = ClassifyDepth(depth_m, display);
  style.palette_bucket = ToString(style.safety_class);
  style.fill_ref = "depth-fill-" + style.palette_bucket;
  return style;
}

bool ShouldShowDepthContour(double contour_m, const RenderView& view,
                            const DisplayState& display) {
  if (std::fabs(contour_m - display.safety_contour_m) < 0.01) {
    return true;
  }
  return view.scale_denom > 0.0 && view.scale_denom <= 50000.0;
}

RenderScene BuildSafetyDepthFixture(RenderView view, DisplayState display) {
  RenderScene scene = BuildDepthTessellationFixture(view, display);
  scene.scene_id = "safety-depth-fixture";
  scene.resource_table.resources.push_back(
      {"depth-fill-shallow", ResourceType::kPalette, "fixture",
       "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"depth-fill-unsafe", ResourceType::kPalette, "fixture",
       "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"depth-fill-safety_band", ResourceType::kPalette, "fixture",
       "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"depth-fill-deep", ResourceType::kPalette, "fixture",
       "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"safety-contour-line", ResourceType::kLineStyle, "fixture",
       "prov-safety-contour"});

  ProvenanceRecord contour_provenance;
  contour_provenance.provenance_id = "prov-safety-contour";
  contour_provenance.source_chart_id = "safety-depth-fixture";
  contour_provenance.source_chart_edition = "poc";
  contour_provenance.source_object_id = "DEPCNT.SAFETY";
  contour_provenance.source_object_class = "DEPCNT";
  contour_provenance.source_geometry_hash = "fixture";
  contour_provenance.conversion_stage = "depth_safety_fixture";
  contour_provenance.transform_chain = {"source:wgs84",
                                        "projection:web_mercator_tile",
                                        "target:px"};
  scene.provenance_table.push_back(std::move(contour_provenance));

  for (CommandGroup& group : scene.command_groups) {
    for (RenderCommand& command : group.commands) {
      if (command.type != CommandType::kFillArea ||
          command.role != "depth_area") {
        continue;
      }
      const DepthAreaStyle style = StyleDepthArea(7.0, scene.display_state);
      command.fill_ref = style.fill_ref;
      command.metadata["depth_m"] = "7";
      command.metadata["safety_class"] = ToString(style.safety_class);
      command.metadata["palette_bucket"] = style.palette_bucket;
      command.metadata["safety_depth_m"] =
          Number(scene.display_state.safety_depth_m);
      command.metadata["safety_contour_m"] =
          Number(scene.display_state.safety_contour_m);
    }
  }

  if (!scene.command_groups.empty() &&
      ShouldShowDepthContour(scene.display_state.safety_contour_m,
                             scene.render_view, scene.display_state)) {
    scene.command_groups.front().commands.push_back(
        SafetyContourCommand(scene.render_view, scene.display_state));
  }

  return scene;
}

bool ValidateSafetyDepthScene(const RenderScene& scene,
                              std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateDepthTessellationScene(scene, &out);
  bool saw_depth_area = false;
  bool saw_safety_contour = false;
  for (const CommandGroup& group : scene.command_groups) {
    for (const RenderCommand& command : group.commands) {
      if (command.role == "depth_area") {
        saw_depth_area = true;
        for (const char* key : {"depth_m", "safety_class", "palette_bucket",
                                "safety_depth_m", "safety_contour_m"}) {
          if (!HasMetadata(command, key)) {
            out.push_back(MakeDiagnostic(
                DiagnosticSeverity::kError, "depth_safety_missing_metadata",
                std::string("Depth area is missing ") + key + " metadata.",
                command.provenance_refs));
            ok = false;
          }
        }
      }
      if (command.role == "safety_contour") {
        saw_safety_contour = true;
        if (!HasMetadata(command, "contour_m") ||
            !HasMetadata(command, "zoom_visible")) {
          out.push_back(MakeDiagnostic(
              DiagnosticSeverity::kError, "depth_contour_missing_metadata",
              "Safety contour is missing contour or zoom metadata.",
              command.provenance_refs));
          ok = false;
        }
      }
    }
  }
  if (!saw_depth_area || !saw_safety_contour) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_safety_missing_commands",
        "Safety depth scene must include a depth area and safety contour."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render::depth
