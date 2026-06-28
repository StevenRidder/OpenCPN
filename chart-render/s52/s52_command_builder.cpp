// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s52/s52_command_builder.hpp"

#include "conversion_trace.hpp"

#include <sstream>
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

ResourceRecord Resource(std::string id, ResourceType type,
                        std::string content_hash,
                        std::string provenance_id) {
  ResourceRecord resource;
  resource.resource_id = std::move(id);
  resource.type = type;
  resource.content_hash = std::move(content_hash);
  resource.provenance_id = std::move(provenance_id);
  return resource;
}

std::string BoundsString(const GeoBounds& bounds) {
  std::ostringstream out;
  out << bounds.west << "," << bounds.south << "," << bounds.east << ","
      << bounds.north;
  return out.str();
}

std::string PixelSizeString(const PixelSize& size) {
  std::ostringstream out;
  out << size.width << "x" << size.height;
  return out.str();
}

std::string RasterProvenanceRef(const RasterSheet& sheet) {
  if (!sheet.provenance_refs.empty()) {
    return sheet.provenance_refs.front();
  }
  return "prov:" + sheet.sheet_id;
}

Geometry GeographicBoundsRing(std::string id, const GeoBounds& bounds) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kGeographic;
  geometry.rings.push_back({{bounds.west, bounds.south},
                            {bounds.east, bounds.south},
                            {bounds.east, bounds.north},
                            {bounds.west, bounds.north}});
  return geometry;
}

RenderCommand RasterSheetCommand(const RasterSheet& sheet) {
  const std::string provenance_ref = RasterProvenanceRef(sheet);
  RenderCommand command =
      Command(CommandType::kDrawRasterSheet, "cmd-" + sheet.sheet_id,
              ToString(sheet.kind), provenance_ref);
  command.coordinate_space = CoordinateSpace::kGeographic;
  command.texture_ref = sheet.sheet_id;
  command.opacity = 1.0;
  command.geometries.push_back(
      GeographicBoundsRing("geom-" + sheet.sheet_id, sheet.visible_bounds));
  command.metadata.insert(sheet.metadata.begin(), sheet.metadata.end());
  command.metadata["source_id"] = sheet.source_id;
  command.metadata["kind"] = ToString(sheet.kind);
  command.metadata["no_data_policy"] = sheet.no_data_policy;
  command.metadata["collar_policy"] = sheet.collar_policy;
  command.metadata["boundary_policy"] = sheet.boundary_policy;
  command.metadata["quilt_policy"] = sheet.quilt_policy;
  command.metadata["quilt_rank"] = std::to_string(sheet.quilt_rank);
  command.metadata["allow_visible_outside_chart_bounds"] =
      sheet.allow_visible_outside_chart_bounds ? "true" : "false";
  command.metadata["image_bounds"] = BoundsString(sheet.geographic_bbox);
  command.metadata["chart_bounds"] = BoundsString(sheet.chart_bounds);
  command.metadata["visible_bounds"] = BoundsString(sheet.visible_bounds);
  command.metadata["collar_bounds"] = BoundsString(sheet.collar_bounds);
  command.metadata["pixel_size"] = PixelSizeString(sheet.pixel_size);
  command.metadata["normalized_cache_key"] =
      "raster:" + sheet.source_id + ":" + sheet.sheet_id + ":" +
      sheet.content_hash + ":" + command.metadata["visible_bounds"] + ":" +
      sheet.collar_policy + ":" + sheet.quilt_policy;
  command.metadata["s52_rule"] = "chart_source:" + command.role;
  return command;
}

ResourceRecord RasterResource(const RasterSheet& sheet) {
  ResourceRecord resource;
  resource.resource_id = sheet.sheet_id;
  resource.type = ResourceType::kRasterTexture;
  resource.content_hash = sheet.content_hash;
  resource.provenance_id = RasterProvenanceRef(sheet);
  resource.metrics["pixel_size"] = PixelSizeString(sheet.pixel_size);
  resource.backend_hints["coordinate_space"] = "geographic";
  resource.backend_hints["no_data_policy"] = sheet.no_data_policy;
  resource.backend_hints["collar_policy"] = sheet.collar_policy;
  resource.backend_hints["quilt_policy"] = sheet.quilt_policy;
  resource.backend_hints["normalized_cache_key"] =
      "raster:" + sheet.source_id + ":" + sheet.sheet_id + ":" +
      sheet.content_hash;
  return resource;
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

  for (const RasterSheet& sheet : product.raster_sheets) {
    scene.resource_table.resources.push_back(RasterResource(sheet));

    CommandGroup raster_group;
    raster_group.group_id = "chart-source-raster-" + sheet.sheet_id;
    raster_group.chart_priority = sheet.quilt_rank;
    raster_group.s52_layer = "raster";
    raster_group.quilt_rank = sheet.quilt_rank;
    raster_group.commands.push_back(RasterSheetCommand(sheet));
    scene.command_groups.push_back(std::move(raster_group));
  }

  return scene;
}

RenderScene S52CommandBuilder::BuildFixtureScene(RenderView view,
                                                 DisplayState display) const {
  RenderScene scene = BuildEmptyScene(std::move(view), std::move(display));
  scene.scene_id = "chart-1-fixture";
  scene.source_epoch = "fixture:v1";

  scene.resource_table.resources.push_back(
      Resource("depth-area-fill", ResourceType::kPalette, "fixture",
               "prov-depth-area"));
  scene.resource_table.resources.push_back(
      Resource("depth-contour-line", ResourceType::kLineStyle, "fixture",
               "prov-depth-contour"));
  scene.resource_table.resources.push_back(
      Resource("buoy-symbol", ResourceType::kSymbol, "fixture", "prov-buoy"));
  scene.resource_table.resources.push_back(
      Resource("label-font", ResourceType::kFont, "fixture", "prov-label"));
  scene.resource_table.resources.push_back(
      Resource("coverage-mask", ResourceType::kRasterTexture, "fixture",
               "prov-coverage"));

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
