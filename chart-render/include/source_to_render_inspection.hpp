// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "gpu_artifact_cache_contract.hpp"
#include "render_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kSourceToRenderInspectionSchemaVersion = 1;

struct SourceToRenderInspectionOptions {
  std::string report_id = "source-to-render-inspection";
  std::string source_product_id;
  std::string converter_id = "replaceable-chart-converter";
  std::string portable_package_id;
  std::string backend_name = "unbound-backend";
  RenderTarget target;
};

struct InspectionTierHandle {
  std::string semantic_tier = "tier1_official_chart";
  std::string semantic_owner = "presentation_compiler";
  std::string source_standard;
  std::string wrong_location_owner = "converter_or_projection";
  std::string wrong_symbol_owner = "converter_presentation_or_backend";
};

struct InspectionGeometryHandle {
  std::string geometry_id;
  CoordinateSpace coordinate_space = CoordinateSpace::kTarget;
  std::size_t point_count = 0;
  std::size_t ring_count = 0;
  std::string source_geometry_hash;
  std::string generated_geometry_hash;
  std::string target_geometry_hash;
};

struct InspectionSourceHandle {
  std::string source_chart_id;
  std::string source_chart_edition;
  std::string source_update;
  std::string source_object_id;
  std::string source_object_class;
  std::vector<std::string> provenance_refs;
  std::vector<std::string> conversion_trace_refs;
};

struct InspectionConverterHandle {
  std::string converter_id;
  std::string source_product_id;
  std::string portable_package_id;
  std::string converter_output_id;
  std::string normalized_feature_id;
  std::string conversion_stage;
  InspectionGeometryHandle normalized_geometry;
  std::vector<std::string> projection_transform;
};

struct InspectionPresentationHandle {
  std::string presentation_rule_id;
  std::string display_category;
  std::string layer_id;
  std::string presentation_layer;
  int draw_order = 0;
  std::string primitive_id;
  std::string primitive_type;
  std::string primitive_role;
};

struct InspectionCacheHandle {
  std::string scene_key;
  std::string primitive_key;
  std::string resource_key;
  std::string tile_cache_key;
};

struct InspectionArtifactHandle {
  std::string manifest_id;
  std::string artifact_id;
  std::string artifact_kind;
  std::string residency;
  std::string cache_key;
  std::string backend_resource_id;
  std::string material_key;
  std::string pipeline_key;
  std::string invalidation_domain;
  std::vector<std::string> primitive_ids;
  std::vector<std::string> provenance_refs;
};

struct InspectionBackendHandle {
  std::string backend_name;
  std::string backend_contract = "draw_only";
  std::string target_id;
  std::string backend_resource_id;
  std::string final_draw_item_id;
  std::string final_gpu_asset_id;
  std::string final_web_asset_id;
  CoordinateSpace coordinate_space = CoordinateSpace::kTarget;
  std::vector<std::string> accepted_backend_targets;
};

struct InspectionQueryHandle {
  std::string object_query_id;
  std::string pixel_query_id;
  std::string hit_test_index_id;
  std::string view_id;
  PixelSize target_pixel_size;
};

struct SourceToRenderInspectionRow {
  std::string row_id;
  InspectionTierHandle tier;
  InspectionSourceHandle source;
  InspectionConverterHandle converter;
  InspectionPresentationHandle presentation;
  InspectionCacheHandle cache;
  InspectionBackendHandle backend;
  InspectionQueryHandle query;
  std::vector<InspectionArtifactHandle> artifacts;
  std::vector<Diagnostic> diagnostics;
};

struct SourceToRenderInspectionReport {
  std::uint32_t schema_version = kSourceToRenderInspectionSchemaVersion;
  std::string report_id = "source-to-render-inspection";
  std::string input_contract = "backend-neutral-nautical-render-model";
  std::string input_model_id;
  std::string input_model_epoch;
  std::string cache_manifest_id;
  std::string backend_name;
  RenderTarget target;
  std::vector<SourceToRenderInspectionRow> rows;
  std::vector<InspectionArtifactHandle> scene_artifacts;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

SourceToRenderInspectionReport BuildSourceToRenderInspectionReport(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest* cache_manifest,
    SourceToRenderInspectionOptions options = {});

SourceToRenderInspectionReport BuildSourceToRenderInspectionReport(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& cache_manifest,
    SourceToRenderInspectionOptions options = {});

bool ValidateSourceToRenderInspectionReport(
    const SourceToRenderInspectionReport& report,
    std::vector<Diagnostic>* diagnostics);

const SourceToRenderInspectionRow* FindInspectionByPrimitiveId(
    const SourceToRenderInspectionReport& report,
    const std::string& primitive_id);

std::vector<const SourceToRenderInspectionRow*> FindInspectionsBySourceObjectId(
    const SourceToRenderInspectionReport& report,
    const std::string& source_object_id);

}  // namespace ocpn::render
