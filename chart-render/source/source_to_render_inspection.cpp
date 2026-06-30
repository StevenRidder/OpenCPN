// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "source_to_render_inspection.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
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
      "Keep source ids, converter/projection records, presentation rules, "
      "neutral primitives, cache artifacts, backend draw ids, and query handles "
      "linked in the inspection contract.";
  return diagnostic;
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::kError;
                     });
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool Contains(const std::string& value, const char* token) {
  return ToLower(value).find(token) != std::string::npos;
}

bool StartsWith(const std::string& value, const char* prefix) {
  const std::string prefix_text(prefix);
  return value.size() >= prefix_text.size() &&
         value.compare(0, prefix_text.size(), prefix_text) == 0;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto found = metadata.find(key);
  return found == metadata.end() ? std::string{} : found->second;
}

std::string CacheKeyText(const GpuArtifactCacheKey& key) {
  return key.namespace_id + "|" + key.backend_target + "|" +
         key.device_profile + "|" + key.material_profile + "|" +
         key.model_key + "|" + key.artifact_key + "|" + key.content_key + "|" +
         key.invalidation_epoch;
}

std::string CacheKeyText(const NauticalCacheKey& key) {
  return key.scene_key + "|" + key.primitive_key + "|" + key.resource_key;
}

std::uint64_t Fnva64Bytes(std::uint64_t hash, const std::uint8_t* bytes,
                          std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::uint64_t Fnva64Uint32(std::uint64_t hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const std::uint8_t byte =
        static_cast<std::uint8_t>((value >> shift) & 0xffU);
    hash = Fnva64Bytes(hash, &byte, 1U);
  }
  return hash;
}

std::string HexHash(std::uint64_t hash) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

std::string PixelHash(const PixelBuffer& pixels) {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = Fnva64Uint32(hash, pixels.pixel_size.width);
  hash = Fnva64Uint32(hash, pixels.pixel_size.height);
  if (!pixels.rgba8.empty()) {
    hash = Fnva64Bytes(hash, pixels.rgba8.data(), pixels.rgba8.size());
  }
  return HexHash(hash);
}

std::string RgbaText(const PixelBuffer& pixels, std::uint32_t x,
                     std::uint32_t y) {
  if (x >= pixels.pixel_size.width || y >= pixels.pixel_size.height ||
      pixels.rgba8.empty()) {
    return {};
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * pixels.pixel_size.width + x) * 4U;
  if (offset + 3U >= pixels.rgba8.size()) return {};
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(2)
      << static_cast<unsigned>(pixels.rgba8[offset]) << std::setw(2)
      << static_cast<unsigned>(pixels.rgba8[offset + 1U]) << std::setw(2)
      << static_cast<unsigned>(pixels.rgba8[offset + 2U]) << std::setw(2)
      << static_cast<unsigned>(pixels.rgba8[offset + 3U]);
  return out.str();
}

std::map<std::string, ProvenanceRecord> ProvenanceById(
    const std::vector<ProvenanceRecord>& records) {
  std::map<std::string, ProvenanceRecord> by_id;
  for (const ProvenanceRecord& record : records) {
    if (!record.provenance_id.empty()) {
      by_id[record.provenance_id] = record;
    }
  }
  return by_id;
}

const ProvenanceRecord* FirstProvenance(
    const SourceTraceHandle& trace,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  for (const std::string& provenance_ref : trace.provenance_refs) {
    const auto found = provenance_by_id.find(provenance_ref);
    if (found != provenance_by_id.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

InspectionGeometryHandle InspectGeometry(const NauticalPrimitive& primitive,
                                         const ProvenanceRecord* provenance) {
  InspectionGeometryHandle inspection;
  if (!primitive.geometries.empty()) {
    const Geometry& geometry = primitive.geometries.front();
    inspection.geometry_id = geometry.geometry_id;
    inspection.coordinate_space = geometry.coordinate_space;
    inspection.point_count = geometry.points.size();
    inspection.ring_count = geometry.rings.size();
  } else {
    inspection.geometry_id = primitive.primitive_id + ":position";
    inspection.coordinate_space = primitive.handoff.coordinate_space;
    switch (primitive.type) {
      case NauticalPrimitiveType::kSymbolInstance:
      case NauticalPrimitiveType::kTextLabel:
      case NauticalPrimitiveType::kSounding:
        inspection.point_count = 1;
        break;
      case NauticalPrimitiveType::kAreaFill:
      case NauticalPrimitiveType::kLineStroke:
      case NauticalPrimitiveType::kRasterPatch:
      case NauticalPrimitiveType::kContourLine:
      case NauticalPrimitiveType::kClipBoundary:
        break;
    }
  }
  if (provenance) {
    inspection.source_geometry_hash = provenance->source_geometry_hash;
    inspection.generated_geometry_hash = provenance->generated_geometry_hash;
    inspection.target_geometry_hash = provenance->target_geometry_hash;
  }
  return inspection;
}

std::string FallbackSourceProductId(const NauticalRenderModel& model) {
  const std::string explicit_id =
      MetadataValue(model.metadata, "source_product_id");
  return explicit_id.empty() ? model.model_id : explicit_id;
}

std::string SourceStandardFor(const NauticalPrimitive& primitive,
                              const GpuArtifactRecord* artifact) {
  const std::string explicit_standard =
      MetadataValue(primitive.metadata, "source_standard");
  if (!explicit_standard.empty()) return explicit_standard;
  if (artifact && !artifact->tier.source_standard.empty()) {
    return artifact->tier.source_standard;
  }
  if (StartsWith(primitive.trace.presentation_rule_id, "s52:")) {
    return "s52-compatible";
  }
  if (StartsWith(primitive.trace.presentation_rule_id, "s101:")) {
    return "s101-compatible";
  }
  return {};
}

std::string SemanticTierFor(const NauticalPrimitive& primitive,
                            const GpuArtifactRecord* artifact) {
  const std::string explicit_tier =
      MetadataValue(primitive.metadata, "semantic_tier");
  if (!explicit_tier.empty()) return explicit_tier;
  if (artifact && !artifact->tier.semantic_tier.empty()) {
    return artifact->tier.semantic_tier;
  }
  return "tier1_official_chart";
}

std::string SemanticOwnerFor(const NauticalPrimitive& primitive,
                             const GpuArtifactRecord* artifact) {
  if (!primitive.handoff.semantic_owner.empty()) {
    return primitive.handoff.semantic_owner;
  }
  if (artifact && !artifact->tier.semantic_owner.empty()) {
    return artifact->tier.semantic_owner;
  }
  return "presentation_compiler";
}

InspectionTierHandle TierFor(const NauticalPrimitive& primitive,
                             const GpuArtifactRecord* artifact) {
  InspectionTierHandle tier;
  tier.semantic_tier = SemanticTierFor(primitive, artifact);
  tier.semantic_owner = SemanticOwnerFor(primitive, artifact);
  tier.source_standard = SourceStandardFor(primitive, artifact);
  if (StartsWith(tier.semantic_tier, "tier1")) {
    tier.wrong_location_owner = "converter_or_projection";
    tier.wrong_symbol_owner = "converter_presentation_or_backend";
  } else if (StartsWith(tier.semantic_tier, "tier2")) {
    tier.wrong_location_owner = "helm_overlay_registry";
    tier.wrong_symbol_owner = "helm_overlay_registry";
  } else if (StartsWith(tier.semantic_tier, "tier3")) {
    tier.wrong_location_owner = "helm_ui_layer";
    tier.wrong_symbol_owner = "helm_ui_layer";
  } else {
    tier.wrong_location_owner = "unknown_visual_tier";
    tier.wrong_symbol_owner = "unknown_visual_tier";
  }
  return tier;
}

std::map<std::string, std::vector<const GpuArtifactRecord*>>
ArtifactsByPrimitiveId(const GpuArtifactCacheManifest* manifest) {
  std::map<std::string, std::vector<const GpuArtifactRecord*>> by_primitive;
  if (!manifest) return by_primitive;
  for (const GpuArtifactRecord& artifact : manifest->artifacts) {
    for (const std::string& primitive_id : artifact.tier.primitive_ids) {
      if (!primitive_id.empty()) {
        by_primitive[primitive_id].push_back(&artifact);
      }
    }
  }
  return by_primitive;
}

InspectionArtifactHandle ArtifactHandle(
    const GpuArtifactCacheManifest& manifest,
    const GpuArtifactRecord& artifact) {
  InspectionArtifactHandle handle;
  handle.manifest_id = manifest.manifest_id;
  handle.artifact_id = artifact.artifact_id;
  handle.artifact_kind = ToString(artifact.kind);
  handle.residency = ToString(artifact.residency);
  handle.cache_key = CacheKeyText(artifact.cache_key);
  handle.backend_resource_id = artifact.resource_id.empty()
                                   ? artifact.artifact_id
                                   : artifact.resource_id;
  handle.material_key = artifact.material_key;
  handle.pipeline_key = artifact.pipeline_key;
  handle.invalidation_domain = artifact.invalidation_domain;
  handle.primitive_ids = artifact.tier.primitive_ids;
  handle.provenance_refs = artifact.tier.provenance_refs;
  return handle;
}

std::vector<InspectionArtifactHandle> ArtifactHandlesFor(
    const GpuArtifactCacheManifest* manifest,
    const std::vector<const GpuArtifactRecord*>& artifacts) {
  std::vector<InspectionArtifactHandle> handles;
  if (!manifest) return handles;
  for (const GpuArtifactRecord* artifact : artifacts) {
    if (artifact) {
      handles.push_back(ArtifactHandle(*manifest, *artifact));
    }
  }
  return handles;
}

std::vector<InspectionArtifactHandle> SceneArtifactHandles(
    const GpuArtifactCacheManifest* manifest,
    const std::set<std::string>& primitive_ids) {
  std::vector<InspectionArtifactHandle> handles;
  if (!manifest) return handles;
  for (const GpuArtifactRecord& artifact : manifest->artifacts) {
    bool linked = false;
    for (const std::string& primitive_id : artifact.tier.primitive_ids) {
      if (primitive_ids.find(primitive_id) != primitive_ids.end()) {
        linked = true;
      }
    }
    if (!linked) {
      handles.push_back(ArtifactHandle(*manifest, artifact));
    }
  }
  return handles;
}

int ClampToPixel(double value, std::uint32_t limit) {
  if (limit == 0U) return 0;
  const double clamped =
      std::max(0.0, std::min(value, static_cast<double>(limit - 1U)));
  return static_cast<int>(std::round(clamped));
}

Point2 FirstPoint(const NauticalPrimitive& primitive) {
  if (primitive.type == NauticalPrimitiveType::kSymbolInstance ||
      primitive.type == NauticalPrimitiveType::kTextLabel ||
      primitive.type == NauticalPrimitiveType::kSounding) {
    return primitive.position;
  }
  for (const Geometry& geometry : primitive.geometries) {
    if (!geometry.points.empty()) return geometry.points.front();
    if (!geometry.rings.empty() && !geometry.rings.front().empty()) {
      return geometry.rings.front().front();
    }
  }
  return primitive.position;
}

CoordinateSpace CoordinateSpaceForSample(
    const NauticalPrimitive& primitive) {
  for (const Geometry& geometry : primitive.geometries) {
    if (!geometry.points.empty() ||
        (!geometry.rings.empty() && !geometry.rings.front().empty())) {
      return geometry.coordinate_space;
    }
  }
  return primitive.handoff.coordinate_space;
}

bool ProjectToPixel(const Point2& point, CoordinateSpace coordinate_space,
                    const RenderView& view, PixelSize size,
                    std::uint32_t* x, std::uint32_t* y) {
  if (size.width == 0U || size.height == 0U) return false;
  if (coordinate_space == CoordinateSpace::kGeographic) {
    const double lon_span = view.geographic_bbox.east - view.geographic_bbox.west;
    const double lat_span = view.geographic_bbox.north - view.geographic_bbox.south;
    if (std::abs(lon_span) <= std::numeric_limits<double>::epsilon() ||
        std::abs(lat_span) <= std::numeric_limits<double>::epsilon()) {
      return false;
    }
    *x = static_cast<std::uint32_t>(
        ClampToPixel((point.x - view.geographic_bbox.west) / lon_span *
                         static_cast<double>(size.width - 1U),
                     size.width));
    *y = static_cast<std::uint32_t>(
        ClampToPixel((view.geographic_bbox.north - point.y) / lat_span *
                         static_cast<double>(size.height - 1U),
                     size.height));
    return true;
  }

  const bool normalized =
      point.x >= 0.0 && point.x <= 1.0 && point.y >= 0.0 && point.y <= 1.0;
  *x = static_cast<std::uint32_t>(
      ClampToPixel(normalized ? point.x * (size.width - 1U) : point.x,
                   size.width));
  *y = static_cast<std::uint32_t>(
      ClampToPixel(normalized ? point.y * (size.height - 1U) : point.y,
                   size.height));
  return true;
}

void AddRenderedPixelSample(const NauticalRenderModel& model,
                            const NauticalPrimitive& primitive,
                            const SourceToRenderInspectionOptions& options,
                            InspectionQueryHandle* query) {
  const RenderResult* result = options.backend_result;
  if (!result || !result->ok || result->pixels.rgba8.empty()) return;

  std::uint32_t x = 0;
  std::uint32_t y = 0;
  if (!ProjectToPixel(FirstPoint(primitive),
                      CoordinateSpaceForSample(primitive), model.render_view,
                      result->pixels.pixel_size, &x, &y)) {
    return;
  }

  query->sample_x = x;
  query->sample_y = y;
  query->sample_rgba8 = RgbaText(result->pixels, x, y);
  query->rendered_pixel_hash = PixelHash(result->pixels);
  query->sampled_rendered_pixel = !query->sample_rgba8.empty();
}

std::string StableDrawItemId(const SourceToRenderInspectionOptions& options,
                             const NauticalPrimitive& primitive) {
  return "draw:" + options.backend_name + ":" + options.target.target_id + ":" +
         primitive.primitive_id;
}

std::string StableGpuAssetId(const SourceToRenderInspectionRow& row) {
  if (!row.artifacts.empty()) {
    return row.artifacts.front().artifact_id;
  }
  return "gpu:" + row.backend.backend_name + ":" + row.backend.target_id +
         ":" + row.presentation.primitive_type + ":" +
         row.cache.primitive_key;
}

std::string StableWebAssetId(const SourceToRenderInspectionRow& row) {
  return "web:" + row.backend.backend_name + ":" + row.backend.target_id +
         ":" + row.presentation.primitive_type + ":" + row.cache.primitive_key;
}

std::vector<std::string> TraceStepsFor(
    const SourceToRenderInspectionRow& row) {
  std::vector<std::string> steps;
  steps.push_back("source " + row.source.source_chart_id + "/" +
                  row.source.source_object_id + " class=" +
                  row.source.source_object_class);
  steps.push_back("converter " + row.converter.converter_id + " product=" +
                  row.converter.source_product_id + " feature=" +
                  row.converter.normalized_feature_id);
  steps.push_back("package " + row.converter.portable_package_id +
                  " output=" + row.converter.converter_output_id);
  steps.push_back("presentation " +
                  row.presentation.presentation_rule_id + " layer=" +
                  row.presentation.layer_id + " primitive=" +
                  row.presentation.primitive_id);
  steps.push_back("cache " + row.cache.tile_cache_key);
  steps.push_back("backend " + row.backend.backend_name + " resource=" +
                  row.backend.backend_resource_id + " draw=" +
                  row.backend.final_draw_item_id);
  std::string pixel = "pixel " + row.query.pixel_query_id;
  if (row.query.sampled_rendered_pixel) {
    pixel += " sample=(" + std::to_string(row.query.sample_x) + "," +
             std::to_string(row.query.sample_y) + ") rgba8=" +
             row.query.sample_rgba8 + " image_hash=" +
             row.query.rendered_pixel_hash;
  }
  steps.push_back(std::move(pixel));
  steps.push_back("owner wrong_location=" + row.tier.wrong_location_owner +
                  " wrong_symbol=" + row.tier.wrong_symbol_owner);
  return steps;
}

void NormalizeOptions(const NauticalRenderModel& model,
                      SourceToRenderInspectionOptions* options) {
  if (options->source_product_id.empty()) {
    options->source_product_id = FallbackSourceProductId(model);
  }
  if (options->portable_package_id.empty()) {
    options->portable_package_id = options->source_product_id + ":portable";
  }
  if (options->target.target_id.empty()) {
    options->target.target_id = model.render_view.view_id + ":inspection";
  }
  if (options->target.pixel_size.width == 0 ||
      options->target.pixel_size.height == 0) {
    options->target.pixel_size = model.render_view.pixel_size;
  }
}

SourceToRenderInspectionRow BuildRow(
    const NauticalRenderModel& model, const NauticalLayer& layer,
    const NauticalPrimitive& primitive,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id,
    const GpuArtifactCacheManifest* manifest,
    const std::vector<const GpuArtifactRecord*>& artifacts,
    const SourceToRenderInspectionOptions& options) {
  const ProvenanceRecord* provenance =
      FirstProvenance(primitive.trace, provenance_by_id);
  SourceToRenderInspectionRow row;
  row.row_id = options.report_id + ":" + primitive.primitive_id;
  row.artifacts = ArtifactHandlesFor(manifest, artifacts);
  row.tier = TierFor(primitive, artifacts.empty() ? nullptr : artifacts.front());

  row.source.source_chart_id = primitive.trace.source_chart_id;
  row.source.source_object_id = primitive.trace.source_object_id;
  row.source.source_object_class = primitive.trace.source_object_class;
  row.source.provenance_refs = primitive.trace.provenance_refs;
  row.source.conversion_trace_refs = primitive.trace.conversion_trace_refs;
  if (provenance) {
    row.source.source_chart_id =
        provenance->source_chart_id.empty() ? row.source.source_chart_id
                                            : provenance->source_chart_id;
    row.source.source_chart_edition = provenance->source_chart_edition;
    row.source.source_update = provenance->source_update;
    row.source.source_object_id =
        provenance->source_object_id.empty() ? row.source.source_object_id
                                             : provenance->source_object_id;
    row.source.source_object_class =
        provenance->source_object_class.empty()
            ? row.source.source_object_class
            : provenance->source_object_class;
  }

  row.converter.converter_id = options.converter_id;
  row.converter.source_product_id = options.source_product_id;
  row.converter.portable_package_id = options.portable_package_id;
  row.converter.converter_output_id =
      options.source_product_id + ":" + primitive.trace.source_object_id;
  row.converter.normalized_feature_id = primitive.trace.source_object_id;
  row.converter.conversion_stage =
      provenance ? provenance->conversion_stage : std::string{};
  row.converter.normalized_geometry = InspectGeometry(primitive, provenance);
  row.converter.projection_transform =
      provenance ? provenance->transform_chain : std::vector<std::string>{};

  row.presentation.presentation_rule_id =
      primitive.trace.presentation_rule_id;
  row.presentation.display_category = primitive.lod.display_category;
  row.presentation.layer_id = layer.layer_id;
  row.presentation.presentation_layer = layer.presentation_layer;
  row.presentation.draw_order = layer.draw_order;
  row.presentation.primitive_id = primitive.primitive_id;
  row.presentation.primitive_type = ToString(primitive.type);
  row.presentation.primitive_role = primitive.role;

  row.cache.scene_key = primitive.cache_key.scene_key;
  row.cache.primitive_key = primitive.cache_key.primitive_key;
  row.cache.resource_key = primitive.cache_key.resource_key;
  row.cache.tile_cache_key = CacheKeyText(primitive.cache_key);

  row.backend.backend_name = options.backend_name;
  row.backend.backend_contract = primitive.handoff.backend_contract;
  row.backend.target_id = options.target.target_id;
  row.backend.backend_resource_id =
      row.artifacts.empty() ? row.cache.resource_key
                            : row.artifacts.front().backend_resource_id;
  row.backend.final_draw_item_id = StableDrawItemId(options, primitive);
  row.backend.coordinate_space = primitive.handoff.coordinate_space;
  row.backend.accepted_backend_targets = primitive.handoff.accepted_backend_targets;

  row.query.view_id = model.render_view.view_id;
  row.query.target_pixel_size = options.target.pixel_size;
  row.query.object_query_id = "object-query:" + model.render_view.view_id +
                              ":" + row.source.source_chart_id + ":" +
                              row.source.source_object_id + ":" +
                              primitive.primitive_id;
  row.query.pixel_query_id = "pixel-query:" + options.target.target_id + ":" +
                             primitive.primitive_id;
  row.query.hit_test_index_id =
      "hit-test:" + layer.layer_id + ":" + primitive.primitive_id;
  AddRenderedPixelSample(model, primitive, options, &row.query);

  row.backend.final_gpu_asset_id = StableGpuAssetId(row);
  row.backend.final_web_asset_id = StableWebAssetId(row);
  row.human_trace = TraceStepsFor(row);
  return row;
}

bool IsTierOwnerValid(const InspectionTierHandle& tier) {
  if (StartsWith(tier.semantic_tier, "tier1")) {
    return !Contains(tier.wrong_symbol_owner, "helm") &&
           !Contains(tier.wrong_location_owner, "helm");
  }
  if (StartsWith(tier.semantic_tier, "tier2") ||
      StartsWith(tier.semantic_tier, "tier3")) {
    return Contains(tier.wrong_symbol_owner, "helm") &&
           Contains(tier.wrong_location_owner, "helm");
  }
  return false;
}

bool RowHasArtifactForPrimitive(const SourceToRenderInspectionRow& row) {
  for (const InspectionArtifactHandle& artifact : row.artifacts) {
    if (std::find(artifact.primitive_ids.begin(), artifact.primitive_ids.end(),
                  row.presentation.primitive_id) != artifact.primitive_ids.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace

SourceToRenderInspectionReport BuildSourceToRenderInspectionReport(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest* cache_manifest,
    SourceToRenderInspectionOptions options) {
  NormalizeOptions(model, &options);

  SourceToRenderInspectionReport report;
  report.report_id = options.report_id;
  report.input_model_id = model.model_id;
  report.input_model_epoch = model.source_epoch;
  report.cache_manifest_id =
      cache_manifest ? cache_manifest->manifest_id : std::string{};
  report.backend_name = options.backend_name;
  report.target = options.target;

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(model, &model_diagnostics);
  report.diagnostics.insert(report.diagnostics.end(), model_diagnostics.begin(),
                            model_diagnostics.end());

  if (cache_manifest) {
    std::vector<Diagnostic> manifest_diagnostics;
    ValidateGpuArtifactCacheManifest(*cache_manifest, &manifest_diagnostics);
    report.diagnostics.insert(report.diagnostics.end(),
                              manifest_diagnostics.begin(),
                              manifest_diagnostics.end());
  }

  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(model.provenance_table);
  const auto artifacts_by_primitive = ArtifactsByPrimitiveId(cache_manifest);
  std::set<std::string> primitive_ids;

  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      primitive_ids.insert(primitive.primitive_id);
      const auto artifacts_found =
          artifacts_by_primitive.find(primitive.primitive_id);
      const std::vector<const GpuArtifactRecord*> empty_artifacts;
      const std::vector<const GpuArtifactRecord*>& artifacts =
          artifacts_found == artifacts_by_primitive.end()
              ? empty_artifacts
              : artifacts_found->second;
      report.rows.push_back(BuildRow(model, layer, primitive, provenance_by_id,
                                     cache_manifest, artifacts, options));
    }
  }

  report.scene_artifacts = SceneArtifactHandles(cache_manifest, primitive_ids);

  std::vector<Diagnostic> validation_diagnostics;
  report.ok = ValidateSourceToRenderInspectionReport(
      report, &validation_diagnostics);
  report.diagnostics.insert(report.diagnostics.end(),
                            validation_diagnostics.begin(),
                            validation_diagnostics.end());
  report.ok = report.ok && !HasError(report.diagnostics);
  return report;
}

SourceToRenderInspectionReport BuildSourceToRenderInspectionReport(
    const NauticalRenderModel& model,
    const GpuArtifactCacheManifest& cache_manifest,
    SourceToRenderInspectionOptions options) {
  return BuildSourceToRenderInspectionReport(model, &cache_manifest,
                                             std::move(options));
}

bool ValidateSourceToRenderInspectionReport(
    const SourceToRenderInspectionReport& report,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (report.schema_version != kSourceToRenderInspectionSchemaVersion ||
      report.report_id.empty() || report.input_model_id.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "source_to_render_identity",
        "Source-to-render inspection report has invalid identity."));
    ok = false;
  }
  if (report.input_contract != "backend-neutral-nautical-render-model") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "source_to_render_input_contract",
        "Source-to-render inspection must consume the neutral render model."));
    ok = false;
  }
  if (report.rows.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "source_to_render_empty",
        "Source-to-render inspection report has no primitive rows."));
    ok = false;
  }

  std::set<std::string> row_ids;
  for (const SourceToRenderInspectionRow& row : report.rows) {
    if (row.row_id.empty() || !row_ids.insert(row.row_id).second) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_row_id",
          "Source-to-render inspection row is missing or duplicating row id.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.source.source_chart_id.empty() ||
        row.source.source_object_id.empty() ||
        row.source.source_object_class.empty() ||
        row.source.provenance_refs.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_source",
          "Inspection row is missing source chart/object provenance.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.converter.converter_id.empty() ||
        row.converter.source_product_id.empty() ||
        row.converter.converter_output_id.empty() ||
        row.converter.normalized_feature_id.empty() ||
        row.converter.normalized_geometry.geometry_id.empty() ||
        row.converter.projection_transform.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_converter",
          "Inspection row is missing converter output, normalized geometry, "
          "or projection transform.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.presentation.presentation_rule_id.empty() ||
        row.presentation.layer_id.empty() ||
        row.presentation.presentation_layer.empty() ||
        row.presentation.primitive_id.empty() ||
        row.presentation.primitive_type.empty() ||
        row.presentation.primitive_role.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_presentation",
          "Inspection row is missing presentation rule or layer handles.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.cache.scene_key.empty() || row.cache.primitive_key.empty() ||
        row.cache.tile_cache_key.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_cache",
          "Inspection row is missing neutral primitive cache key handles.",
          row.source.provenance_refs));
      ok = false;
    }
    if (!report.cache_manifest_id.empty() && row.artifacts.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_missing_artifact",
          "Inspection row has no GPU artifact/cache record for its primitive.",
          row.source.provenance_refs));
      ok = false;
    }
    if (!report.cache_manifest_id.empty() &&
        !RowHasArtifactForPrimitive(row)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_artifact_link",
          "Inspection row artifact does not link back to the primitive.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.backend.backend_name.empty() ||
        row.backend.backend_contract != "draw_only" ||
        row.backend.target_id.empty() ||
        row.backend.backend_resource_id.empty() ||
        row.backend.final_draw_item_id.empty() ||
        row.backend.final_gpu_asset_id.empty() ||
        row.backend.final_web_asset_id.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_backend",
          "Inspection row is missing draw-only backend resource or draw item "
          "handles.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.backend.backend_contract == "backend_owns_semantics" ||
        row.tier.semantic_owner == "backend") {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_semantic_owner",
          "Inspection row gives chart semantics to the backend.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.query.object_query_id.empty() ||
        row.query.pixel_query_id.empty() ||
        row.query.hit_test_index_id.empty() ||
        row.query.view_id.empty() ||
        row.query.target_pixel_size.width == 0 ||
        row.query.target_pixel_size.height == 0) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_query",
          "Inspection row is missing object/pixel query handles.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.query.sampled_rendered_pixel &&
        (row.query.sample_rgba8.size() != 8U ||
         row.query.rendered_pixel_hash.empty())) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_pixel_sample",
          "Inspection row has incomplete rendered-pixel sample evidence.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.human_trace.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_human_trace",
          "Inspection row is missing a human-readable source-to-pixel trace.",
          row.source.provenance_refs));
      ok = false;
    }
    if (row.tier.semantic_tier.empty() ||
        row.tier.semantic_owner.empty() ||
        row.tier.wrong_location_owner.empty() ||
        row.tier.wrong_symbol_owner.empty() ||
        !IsTierOwnerValid(row.tier)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_tier_owner",
          "Inspection row does not route wrong-location/wrong-symbol bugs to "
          "the owner for its visual tier.",
          row.source.provenance_refs));
      ok = false;
    }
    if (StartsWith(row.tier.semantic_tier, "tier1") &&
        (row.tier.source_standard.empty() ||
         row.presentation.presentation_rule_id.empty())) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "source_to_render_tier1_provenance",
          "Tier 1 chart inspection row is missing source standard or "
          "presentation rule provenance.",
          row.source.provenance_refs));
      ok = false;
    }
    for (const InspectionArtifactHandle& artifact : row.artifacts) {
      if (artifact.artifact_id.empty() || artifact.artifact_kind.empty() ||
          artifact.cache_key.empty() || artifact.invalidation_domain.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "source_to_render_artifact",
            "Inspection artifact handle is missing id, kind, cache key, or "
            "invalidation domain.",
            artifact.provenance_refs));
        ok = false;
      }
    }
  }
  return ok;
}

const SourceToRenderInspectionRow* FindInspectionByPrimitiveId(
    const SourceToRenderInspectionReport& report,
    const std::string& primitive_id) {
  const auto found = std::find_if(
      report.rows.begin(), report.rows.end(),
      [&](const SourceToRenderInspectionRow& row) {
        return row.presentation.primitive_id == primitive_id;
      });
  return found == report.rows.end() ? nullptr : &*found;
}

std::vector<const SourceToRenderInspectionRow*> FindInspectionsBySourceObjectId(
    const SourceToRenderInspectionReport& report,
    const std::string& source_object_id) {
  std::vector<const SourceToRenderInspectionRow*> rows;
  for (const SourceToRenderInspectionRow& row : report.rows) {
    if (row.source.source_object_id == source_object_id) {
      rows.push_back(&row);
    }
  }
  return rows;
}

std::vector<std::string> BuildHumanReadableSourceToRenderTrace(
    const SourceToRenderInspectionReport& report) {
  std::vector<std::string> lines;
  for (const SourceToRenderInspectionRow& row : report.rows) {
    std::ostringstream header;
    header << row.presentation.primitive_id << " ["
           << row.tier.semantic_tier << "/" << row.tier.source_standard
           << "]";
    lines.push_back(header.str());
    for (const std::string& step : row.human_trace) {
      lines.push_back("  " + step);
    }
  }
  return lines;
}

}  // namespace ocpn::render
