// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_quilting.hpp"

#include "conversion_trace.hpp"

#include <utility>

namespace ocpn::render::depth {
namespace {

GeoBounds Bounds(double west, double south, double east, double north) {
  GeoBounds bounds;
  bounds.west = west;
  bounds.south = south;
  bounds.east = east;
  bounds.north = north;
  return bounds;
}

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Inspect raster collar, boundary, and quilt metadata before clipping "
      "or caching raster charts.";
  return diagnostic;
}

ProvenanceRecord RasterProvenance(std::string id, std::string source_id,
                                  std::string object_id,
                                  std::string object_class) {
  ProvenanceRecord provenance;
  provenance.provenance_id = std::move(id);
  provenance.source_chart_id = std::move(source_id);
  provenance.source_chart_edition = "poc";
  provenance.source_object_id = std::move(object_id);
  provenance.source_object_class = std::move(object_class);
  provenance.source_geometry_hash = "fixture";
  provenance.conversion_stage = "depth_quilting_fixture";
  provenance.transform_chain = {"source:raster_native",
                                "chart_source:normalize_bounds",
                                "chart_source:quilt_policy"};
  provenance.quilt_decision_id = "depth-5-raster-quilting";
  return provenance;
}

ChartSourceRef RasterSource(std::string source_id, std::string native_name,
                            GeoBounds bounds, int scale_denom) {
  ChartSourceRef source;
  source.source_id = std::move(source_id);
  source.kind = ChartSourceKind::kRasterChart;
  source.role = ChartSourceRole::kPrimary;
  source.native_name = std::move(native_name);
  source.edition = "poc";
  source.content_hash = "fixture";
  source.native_projection = "mercator";
  source.native_scale_denom = scale_denom;
  source.geographic_bbox = bounds;
  source.metadata["boundary_model"] = "chart_bounds+visible_bounds+collar";
  return source;
}

RasterSheet Sheet(std::string sheet_id, RasterSheetKind kind,
                  std::string source_id, GeoBounds image_bounds,
                  GeoBounds chart_bounds, GeoBounds visible_bounds,
                  GeoBounds collar_bounds, PixelSize pixel_size,
                  std::string provenance_id) {
  RasterSheet sheet;
  sheet.sheet_id = std::move(sheet_id);
  sheet.kind = kind;
  sheet.source_id = std::move(source_id);
  sheet.geographic_bbox = image_bounds;
  sheet.chart_bounds = chart_bounds;
  sheet.visible_bounds = visible_bounds;
  sheet.collar_bounds = collar_bounds;
  sheet.pixel_size = pixel_size;
  sheet.coordinate_space = CoordinateSpace::kGeographic;
  sheet.content_hash = "fixture";
  sheet.no_data_policy = "transparent_alpha_no_data";
  sheet.collar_policy = "mask_source_collar";
  sheet.boundary_policy = "clip_to_visible_bounds";
  sheet.quilt_policy = "single_chart_or_quilted";
  sheet.provenance_refs.push_back(std::move(provenance_id));
  return sheet;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

bool HasMetadata(const RenderCommand& command, const char* key) {
  return command.metadata.find(key) != command.metadata.end();
}

}  // namespace

ChartSourceProduct BuildRasterQuiltingFixtureProduct() {
  ChartSourceProduct product;
  product.product_id = "depth-5-raster-quilting-fixture";
  product.metadata["fixture"] = "depth-5";
  product.metadata["normalized_cache_scope"] =
      "source_id+content_hash+visible_bounds+collar_policy+quilt_policy";

  const GeoBounds approach_image = Bounds(-81.86, 24.42, -81.74, 24.53);
  const GeoBounds approach_chart = Bounds(-81.845, 24.435, -81.755, 24.515);
  const GeoBounds approach_visible = Bounds(-81.852, 24.430, -81.748, 24.520);
  const GeoBounds harbor_image = Bounds(-81.815, 24.445, -81.770, 24.490);
  const GeoBounds harbor_chart = Bounds(-81.808, 24.452, -81.777, 24.484);

  product.sources.push_back(RasterSource(
      "raster-key-west-approach", "Key West approach raster fixture",
      approach_image, 40000));
  product.sources.push_back(RasterSource(
      "raster-key-west-harbor", "Key West harbor inset raster fixture",
      harbor_image, 10000));

  product.provenance_table.push_back(RasterProvenance(
      "prov-raster-single", "raster-key-west-approach", "KAP.APPROACH",
      "RASTER"));
  product.provenance_table.push_back(RasterProvenance(
      "prov-raster-base", "raster-key-west-approach", "KAP.APPROACH.BASE",
      "RASTER"));
  product.provenance_table.push_back(RasterProvenance(
      "prov-raster-detail", "raster-key-west-harbor", "KAP.HARBOR.DETAIL",
      "RASTER"));
  product.provenance_table.push_back(RasterProvenance(
      "prov-raster-collar", "raster-key-west-harbor", "KAP.HARBOR.COLLAR",
      "COLLAR"));

  RasterSheet single = Sheet("sheet-single-chart-visible",
                             RasterSheetKind::kChartImage,
                             "raster-key-west-approach", approach_image,
                             approach_chart, approach_visible, approach_image,
                             {2048, 2048}, "prov-raster-single");
  single.collar_policy = "preserve_visible_pixels_outside_neatline";
  single.boundary_policy = "single_chart_draws_visible_bounds";
  single.quilt_policy = "single_chart_mode";
  single.allow_visible_outside_chart_bounds = true;
  single.metadata["fixture_case"] = "single-chart-visible-outside-boundary";
  single.metadata["cache_layer"] = "normalized_scene";
  product.raster_sheets.push_back(std::move(single));

  RasterSheet base = Sheet("sheet-quilt-base", RasterSheetKind::kChartImage,
                           "raster-key-west-approach", approach_image,
                           approach_chart, approach_chart, approach_image,
                           {2048, 2048}, "prov-raster-base");
  base.boundary_policy = "multi_chart_clips_to_chart_bounds";
  base.quilt_policy = "quilt_base_rank0";
  base.quilt_rank = 0;
  base.metadata["fixture_case"] = "multi-chart-base";
  base.metadata["cache_layer"] = "normalized_scene";
  product.raster_sheets.push_back(std::move(base));

  RasterSheet detail = Sheet("sheet-quilt-detail", RasterSheetKind::kChartImage,
                             "raster-key-west-harbor", harbor_image,
                             harbor_chart, harbor_chart, harbor_image,
                             {1024, 1024}, "prov-raster-detail");
  detail.boundary_policy = "multi_chart_detail_over_base";
  detail.quilt_policy = "quilt_detail_rank1";
  detail.quilt_rank = 1;
  detail.metadata["fixture_case"] = "multi-chart-detail";
  detail.metadata["cache_layer"] = "normalized_scene";
  product.raster_sheets.push_back(std::move(detail));

  RasterSheet collar = Sheet("sheet-quilt-detail-collar-mask",
                             RasterSheetKind::kCollarMask,
                             "raster-key-west-harbor", harbor_image,
                             harbor_chart, harbor_image, harbor_image,
                             {1024, 1024}, "prov-raster-collar");
  collar.no_data_policy = "mask_alpha_0";
  collar.collar_policy = "exclude_collar_from_quilted_color";
  collar.boundary_policy = "mask_detail_sheet_collar";
  collar.quilt_policy = "quilt_mask_rank1";
  collar.quilt_rank = 1;
  collar.metadata["applies_to_sheet"] = "sheet-quilt-detail";
  collar.metadata["fixture_case"] = "multi-chart-collar-mask";
  collar.metadata["cache_layer"] = "gpu_resource";
  product.raster_sheets.push_back(std::move(collar));

  DebugArtifact footprint;
  footprint.artifact_id = "debug-raster-quilting-footprint";
  footprint.kind = DebugArtifactKind::kRasterFootprint;
  footprint.source_id = "raster-key-west-approach";
  footprint.content_hash = "fixture";
  footprint.media_type = "application/vnd.opencpn.raster-footprint+json";
  footprint.producer = "depth_quilting";
  footprint.provenance_refs = {"prov-raster-single", "prov-raster-base",
                               "prov-raster-detail"};
  footprint.metadata["contains"] =
      "image_bounds,chart_bounds,visible_bounds,collar_bounds,quilt_rank";
  product.debug_artifacts.push_back(std::move(footprint));

  return product;
}

bool ValidateRasterQuiltingProduct(const ChartSourceProduct& product,
                                   std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateChartSourceProduct(product, &out);
  bool saw_single_visible_outside = false;
  bool saw_quilt_base = false;
  bool saw_quilt_detail = false;
  bool saw_collar_mask = false;
  bool saw_normalized_cache_layer = false;

  for (const RasterSheet& sheet : product.raster_sheets) {
    const std::string fixture_case =
        MetadataValue(sheet.metadata, "fixture_case");
    if (fixture_case == "single-chart-visible-outside-boundary" &&
        sheet.allow_visible_outside_chart_bounds &&
        sheet.quilt_policy == "single_chart_mode") {
      saw_single_visible_outside = true;
    }
    if (sheet.quilt_policy == "quilt_base_rank0" && sheet.quilt_rank == 0) {
      saw_quilt_base = true;
    }
    if (sheet.quilt_policy == "quilt_detail_rank1" && sheet.quilt_rank == 1) {
      saw_quilt_detail = true;
    }
    if (sheet.kind == RasterSheetKind::kCollarMask &&
        MetadataValue(sheet.metadata, "applies_to_sheet") ==
            "sheet-quilt-detail") {
      saw_collar_mask = true;
    }
    if (MetadataValue(sheet.metadata, "cache_layer") == "normalized_scene") {
      saw_normalized_cache_layer = true;
    }
  }

  if (!saw_single_visible_outside || !saw_quilt_base || !saw_quilt_detail ||
      !saw_collar_mask || !saw_normalized_cache_layer) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_quilt_missing_fixture_case",
        "Raster quilting fixture must cover single-chart visible pixels, "
        "multi-chart base/detail ranks, collar mask, and cache-layer metadata."));
    ok = false;
  }

  bool saw_footprint = false;
  for (const DebugArtifact& artifact : product.debug_artifacts) {
    if (artifact.kind == DebugArtifactKind::kRasterFootprint) {
      saw_footprint = true;
    }
  }
  if (!saw_footprint) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "depth_quilt_missing_footprint",
                                 "Raster quilting fixture needs a footprint "
                                 "debug artifact for cache inspection."));
    ok = false;
  }

  return ok;
}

bool ValidateRasterQuiltingScene(const RenderScene& scene,
                                 std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateRenderSceneTraceability(scene, &out);
  bool saw_single = false;
  bool saw_base_rank = false;
  bool saw_detail_rank = false;
  bool saw_collar_mask = false;
  bool saw_cache_key = false;

  for (const CommandGroup& group : scene.command_groups) {
    if (group.quilt_rank == 0) {
      saw_base_rank = true;
    }
    if (group.quilt_rank == 1) {
      saw_detail_rank = true;
    }
    for (const RenderCommand& command : group.commands) {
      if (command.type != CommandType::kDrawRasterSheet) {
        continue;
      }
      for (const char* key :
           {"no_data_policy", "collar_policy", "boundary_policy",
            "quilt_policy", "chart_bounds", "visible_bounds",
            "collar_bounds", "source_id"}) {
        if (!HasMetadata(command, key)) {
          out.push_back(MakeDiagnostic(
              DiagnosticSeverity::kError, "depth_quilt_command_metadata",
              std::string("Raster command is missing ") + key + " metadata.",
              command.provenance_refs));
          ok = false;
        }
      }
      if (MetadataValue(command.metadata,
                        "allow_visible_outside_chart_bounds") == "true") {
        saw_single = true;
      }
      if (command.role == "collar_mask") {
        saw_collar_mask = true;
      }
      if (HasMetadata(command, "normalized_cache_key")) {
        saw_cache_key = true;
      }
    }
  }

  if (!saw_single || !saw_base_rank || !saw_detail_rank || !saw_collar_mask ||
      !saw_cache_key) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "depth_quilt_scene_coverage",
        "Raster quilting scene must preserve single-chart, rank 0/rank 1, "
        "collar-mask, and cache-key decisions in the command stream."));
    ok = false;
  }

  return ok;
}

}  // namespace ocpn::render::depth
