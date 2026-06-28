// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s52/s52_command_builder.hpp"

#include "conversion_trace.hpp"

#include <utility>

namespace ocpn::render::s52 {
namespace {

ProvenanceRecord FixtureProvenance(std::string id, std::string object_id,
                                   std::string object_class) {
  ProvenanceRecord p;
  p.provenance_id = std::move(id);
  p.source_chart_id = "chart-1-fixture";
  p.source_chart_edition = "poc";
  p.source_object_id = std::move(object_id);
  p.source_object_class = std::move(object_class);
  p.source_geometry_hash = "fixture";
  p.conversion_stage = "s52_command_builder_fixture";
  p.transform_chain = {"source:wgs84", "projection:web_mercator_tile",
                       "target:px"};
  p.quilt_decision_id = "single-chart";
  return p;
}

Geometry TargetRing(std::string id, std::vector<Point2> points) {
  Geometry g;
  g.geometry_id = std::move(id);
  g.coordinate_space = CoordinateSpace::kTarget;
  g.rings.push_back(std::move(points));
  return g;
}

Geometry TargetLine(std::string id, std::vector<Point2> points) {
  Geometry g;
  g.geometry_id = std::move(id);
  g.coordinate_space = CoordinateSpace::kTarget;
  g.points = std::move(points);
  return g;
}

RenderCommand Command(CommandType type, std::string id, std::string role,
                      std::string provenance_id) {
  RenderCommand command;
  command.type = type;
  command.command_id = std::move(id);
  command.role = std::move(role);
  command.coordinate_space = CoordinateSpace::kTarget;
  const std::string provenance_ref = std::move(provenance_id);
  command.metadata["s52_rule"] = "fixture:" + command.role;
  command.provenance_refs.push_back(provenance_ref);
  command.conversion_trace_refs.push_back("trace:" + provenance_ref);
  return command;
}

}  // namespace

RenderScene S52CommandBuilder::BuildEmptyScene(RenderView view,
                                               DisplayState display) const {
  RenderScene scene;
  scene.scene_id = "empty";
  scene.source_epoch = "none";
  scene.render_view = std::move(view);
  scene.display_state = std::move(display);
  return scene;
}

RenderScene S52CommandBuilder::BuildSceneFromChartSource(
    const ChartSourceProduct& product, RenderView view,
    DisplayState display) const {
  RenderScene scene = BuildEmptyScene(std::move(view), std::move(display));
  scene.scene_id =
      product.product_id.empty() ? "chart-source-product" : product.product_id;
  scene.source_epoch = "chart-source:" + std::to_string(product.schema_version);
  scene.provenance_table = product.provenance_table;
  scene.diagnostics = product.diagnostics;

  if (!product.objects.empty() || !product.raster_sheets.empty()) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::kInfo;
    diagnostic.code = "s52_command_builder_placeholder";
    diagnostic.message =
        "Chart-source product accepted; S-52 symbolization is not implemented "
        "in this skeleton yet.";
    diagnostic.suggested_action =
        "Map normalized chart objects and raster sheets into render commands.";
    scene.diagnostics.push_back(std::move(diagnostic));
  }

  return scene;
}

RenderScene S52CommandBuilder::BuildFixtureScene(RenderView view,
                                                 DisplayState display) const {
  RenderScene scene = BuildEmptyScene(std::move(view), std::move(display));
  scene.scene_id = "chart-1-fixture";
  scene.source_epoch = "fixture:v1";

  scene.resource_table.resources.push_back(
      {"depth-area-fill", ResourceType::kPalette, "fixture", "prov-depth-area"});
  scene.resource_table.resources.push_back(
      {"depth-contour-line", ResourceType::kLineStyle, "fixture",
       "prov-depth-contour"});
  scene.resource_table.resources.push_back(
      {"buoy-symbol", ResourceType::kSymbol, "fixture", "prov-buoy"});
  scene.resource_table.resources.push_back(
      {"label-font", ResourceType::kFont, "fixture", "prov-label"});
  scene.resource_table.resources.push_back(
      {"coverage-mask", ResourceType::kRasterTexture, "fixture",
       "prov-coverage"});

  scene.provenance_table.push_back(
      FixtureProvenance("prov-depth-area", "DEPARE.1", "DEPARE"));
  scene.provenance_table.push_back(
      FixtureProvenance("prov-depth-contour", "DEPCNT.1", "DEPCNT"));
  scene.provenance_table.push_back(
      FixtureProvenance("prov-buoy", "BOYLAT.1", "BOYLAT"));
  scene.provenance_table.push_back(
      FixtureProvenance("prov-label", "LNDARE.1", "LNDARE"));
  scene.provenance_table.push_back(
      FixtureProvenance("prov-sounding", "SOUNDG.1", "SOUNDG"));
  scene.provenance_table.push_back(
      FixtureProvenance("prov-coverage", "M_COVR.1", "M_COVR"));

  CommandGroup base;
  base.group_id = "s52-base";
  base.chart_priority = 0;
  base.s52_layer = "area";
  base.quilt_rank = 0;

  RenderCommand area =
      Command(CommandType::kFillArea, "cmd-depth-area", "depth_area",
              "prov-depth-area");
  area.fill_ref = "depth-area-fill";
  area.pattern_ref = "none";
  area.geometries.push_back(TargetRing(
      "geom-depth-area",
      {{16.0, 16.0}, {240.0, 16.0}, {240.0, 220.0}, {16.0, 220.0}}));
  base.commands.push_back(std::move(area));

  RenderCommand coverage =
      Command(CommandType::kDrawRasterSheet, "cmd-coverage-mask",
              "coverage_mask", "prov-coverage");
  coverage.texture_ref = "coverage-mask";
  coverage.opacity = 1.0;
  coverage.metadata["collar_policy"] = "not_applicable";
  coverage.metadata["coverage_policy"] = "no_data_transparent";
  coverage.geometries.push_back(TargetRing(
      "geom-coverage",
      {{0.0, 0.0}, {256.0, 0.0}, {256.0, 256.0}, {0.0, 256.0}}));
  base.commands.push_back(std::move(coverage));

  RenderCommand contour =
      Command(CommandType::kStrokeLine, "cmd-depth-contour", "depth_contour",
              "prov-depth-contour");
  contour.line_style_ref = "depth-contour-line";
  contour.width_px = 1.5;
  contour.metadata["contour_m"] = "10";
  contour.geometries.push_back(TargetLine(
      "geom-depth-contour",
      {{24.0, 160.0}, {72.0, 142.0}, {128.0, 130.0}, {224.0, 96.0}}));
  base.commands.push_back(std::move(contour));

  CommandGroup symbols;
  symbols.group_id = "s52-symbols";
  symbols.chart_priority = 0;
  symbols.s52_layer = "symbols";
  symbols.quilt_rank = 0;

  RenderCommand buoy =
      Command(CommandType::kPlaceSymbol, "cmd-buoy", "buoy", "prov-buoy");
  buoy.symbol_ref = "buoy-symbol";
  buoy.position = {128.0, 92.0};
  buoy.anchor = "center";
  buoy.priority = "navigation";
  symbols.commands.push_back(std::move(buoy));

  RenderCommand label =
      Command(CommandType::kDrawText, "cmd-label", "object_label",
              "prov-label");
  label.text = "Fixture Harbor";
  label.font_ref = "label-font";
  label.position = {138.0, 82.0};
  label.anchor = "left";
  label.priority = "label";
  symbols.commands.push_back(std::move(label));

  RenderCommand sounding =
      Command(CommandType::kDrawSounding, "cmd-sounding", "sounding",
              "prov-sounding");
  sounding.depth_m = 7.4;
  sounding.text = "7.4";
  sounding.font_ref = "label-font";
  sounding.position = {94.0, 176.0};
  sounding.priority = "sounding";
  sounding.metadata["safety_class"] = "safe";
  symbols.commands.push_back(std::move(sounding));

  scene.command_groups.push_back(std::move(base));
  scene.command_groups.push_back(std::move(symbols));
  std::vector<Diagnostic> trace_diagnostics;
  ValidateRenderSceneTraceability(scene, &trace_diagnostics);
  scene.diagnostics.insert(scene.diagnostics.end(), trace_diagnostics.begin(),
                           trace_diagnostics.end());
  return scene;
}

}  // namespace ocpn::render::s52
