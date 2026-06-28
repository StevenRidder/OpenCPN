// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "nautical_render_model.hpp"

#include <cstdlib>
#include <map>
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
      "Keep chart semantics in the presentation compiler and hand only "
      "neutral nautical primitives to renderer backends.";
  return diagnostic;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

bool HasMetadata(const std::map<std::string, std::string>& metadata,
                 const char* key) {
  return metadata.find(key) != metadata.end();
}

double MetadataDouble(const std::map<std::string, std::string>& metadata,
                      const char* key, double fallback) {
  const std::string value = MetadataValue(metadata, key);
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  return end == value.c_str() ? fallback : parsed;
}

int MetadataInt(const std::map<std::string, std::string>& metadata,
                const char* key, int fallback) {
  const std::string value = MetadataValue(metadata, key);
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  return end == value.c_str() ? fallback : static_cast<int>(parsed);
}

bool MetadataBool(const std::map<std::string, std::string>& metadata,
                  const char* key) {
  const std::string value = MetadataValue(metadata, key);
  return value == "true" || value == "1" || value == "yes";
}

GeoBounds MetadataBounds(const std::map<std::string, std::string>& metadata,
                         const char* key) {
  GeoBounds bounds;
  const std::string value = MetadataValue(metadata, key);
  if (value.empty()) {
    return bounds;
  }
  std::istringstream in(value);
  char comma = '\0';
  in >> bounds.west >> comma >> bounds.south >> comma >> bounds.east >>
      comma >> bounds.north;
  return bounds;
}

NauticalPrimitiveType PrimitiveTypeFor(const RenderCommand& command) {
  switch (command.type) {
    case CommandType::kFillArea:
      return NauticalPrimitiveType::kAreaFill;
    case CommandType::kStrokeLine:
      if (command.role.find("contour") != std::string::npos ||
          HasMetadata(command.metadata, "contour_m")) {
        return NauticalPrimitiveType::kContourLine;
      }
      return NauticalPrimitiveType::kLineStroke;
    case CommandType::kPlaceSymbol:
      return NauticalPrimitiveType::kSymbolInstance;
    case CommandType::kDrawText:
      return NauticalPrimitiveType::kTextLabel;
    case CommandType::kDrawSounding:
      return NauticalPrimitiveType::kSounding;
    case CommandType::kDrawRasterSheet:
      return NauticalPrimitiveType::kRasterPatch;
    case CommandType::kPushClip:
    case CommandType::kPopClip:
      return NauticalPrimitiveType::kClipBoundary;
  }
  return NauticalPrimitiveType::kAreaFill;
}

std::string ResourceKeyFor(const RenderCommand& command) {
  if (!command.fill_ref.empty()) {
    return command.fill_ref;
  }
  if (!command.pattern_ref.empty()) {
    return command.pattern_ref;
  }
  if (!command.line_style_ref.empty()) {
    return command.line_style_ref;
  }
  if (!command.symbol_ref.empty()) {
    return command.symbol_ref;
  }
  if (!command.font_ref.empty()) {
    return command.font_ref;
  }
  if (!command.texture_ref.empty()) {
    return command.texture_ref;
  }
  return "none";
}

std::map<std::string, ProvenanceRecord> ProvenanceById(
    const std::vector<ProvenanceRecord>& records) {
  std::map<std::string, ProvenanceRecord> by_id;
  for (const ProvenanceRecord& record : records) {
    by_id[record.provenance_id] = record;
  }
  return by_id;
}

SourceTraceHandle TraceFor(
    const RenderCommand& command,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  SourceTraceHandle trace;
  trace.provenance_refs = command.provenance_refs;
  trace.conversion_trace_refs = command.conversion_trace_refs;
  trace.presentation_rule_id = MetadataValue(command.metadata, "s52_rule");
  if (!command.provenance_refs.empty()) {
    const auto it = provenance_by_id.find(command.provenance_refs.front());
    if (it != provenance_by_id.end()) {
      trace.source_chart_id = it->second.source_chart_id;
      trace.source_object_id = it->second.source_object_id;
      trace.source_object_class = it->second.source_object_class;
      if (trace.presentation_rule_id.empty()) {
        trace.presentation_rule_id = it->second.s52_rule_id;
      }
    }
  }
  return trace;
}

NauticalLodHint LodFor(const RenderCommand& command,
                       const RenderView& view,
                       const DisplayState& display) {
  NauticalLodHint hint;
  hint.min_scale_denom =
      MetadataDouble(command.metadata, "min_scale_denom", 0.0);
  hint.max_scale_denom =
      MetadataDouble(command.metadata, "max_scale_denom", 0.0);
  hint.overzoom = view.overzoom;
  hint.display_category =
      std::to_string(static_cast<int>(display.display_category));
  hint.priority = command.priority;
  hint.visible = MetadataValue(command.metadata, "zoom_visible") != "false";
  return hint;
}

NauticalCacheKey CacheKeyFor(const RenderScene& scene,
                             const RenderCommand& command,
                             NauticalPrimitiveType type) {
  NauticalCacheKey key;
  key.scene_key = scene.scene_id + ":" + scene.source_epoch + ":" +
                  scene.render_view.view_id;
  key.primitive_key = MetadataValue(command.metadata, "normalized_cache_key");
  if (key.primitive_key.empty()) {
    key.primitive_key =
        scene.scene_id + ":" + command.command_id + ":" + ToString(type);
  }
  key.resource_key = ResourceKeyFor(command);
  return key;
}

ChartCoverageMetadata CoverageFor(const RenderCommand& command,
                                  int group_quilt_rank) {
  ChartCoverageMetadata coverage;
  coverage.source_id = MetadataValue(command.metadata, "source_id");
  coverage.image_bounds = MetadataBounds(command.metadata, "image_bounds");
  coverage.chart_bounds = MetadataBounds(command.metadata, "chart_bounds");
  coverage.visible_bounds = MetadataBounds(command.metadata, "visible_bounds");
  coverage.collar_bounds = MetadataBounds(command.metadata, "collar_bounds");
  coverage.no_data_policy = MetadataValue(command.metadata, "no_data_policy");
  if (coverage.no_data_policy.empty()) {
    coverage.no_data_policy = MetadataValue(command.metadata, "coverage_policy");
  }
  coverage.collar_policy = MetadataValue(command.metadata, "collar_policy");
  coverage.boundary_policy = MetadataValue(command.metadata, "boundary_policy");
  coverage.quilt_policy = MetadataValue(command.metadata, "quilt_policy");
  coverage.quilt_rank =
      MetadataInt(command.metadata, "quilt_rank", group_quilt_rank);
  coverage.allow_visible_outside_chart_bounds =
      MetadataBool(command.metadata, "allow_visible_outside_chart_bounds");
  return coverage;
}

BackendHandoffContract HandoffFor(const RenderCommand& command) {
  BackendHandoffContract handoff;
  handoff.coordinate_space = command.coordinate_space;
  handoff.accepted_backend_targets = {"vsg", "opengl", "metal", "webgpu",
                                      "helm_offscreen"};
  return handoff;
}

NauticalPrimitive PrimitiveFor(
    const RenderScene& scene, const CommandGroup& group,
    const RenderCommand& command,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  NauticalPrimitive primitive;
  primitive.primitive_id = command.command_id;
  primitive.type = PrimitiveTypeFor(command);
  primitive.role = command.role;
  primitive.geometries = command.geometries;
  primitive.fill_ref = command.fill_ref;
  primitive.pattern_ref = command.pattern_ref;
  primitive.line_style_ref = command.line_style_ref;
  primitive.symbol_ref = command.symbol_ref;
  primitive.font_ref = command.font_ref;
  primitive.texture_ref = command.texture_ref;
  primitive.text = command.text;
  primitive.depth_m = command.depth_m;
  primitive.width_px = command.width_px;
  primitive.opacity = command.opacity;
  primitive.rotation_deg = command.rotation_deg;
  primitive.scale = command.scale;
  primitive.position = command.position;
  primitive.anchor = command.anchor;
  primitive.trace = TraceFor(command, provenance_by_id);
  primitive.lod = LodFor(command, scene.render_view, scene.display_state);
  primitive.cache_key = CacheKeyFor(scene, command, primitive.type);
  primitive.coverage = CoverageFor(command, group.quilt_rank);
  primitive.handoff = HandoffFor(command);
  primitive.metadata = command.metadata;
  return primitive;
}

}  // namespace

const char* ToString(NauticalPrimitiveType type) {
  switch (type) {
    case NauticalPrimitiveType::kAreaFill:
      return "area_fill";
    case NauticalPrimitiveType::kLineStroke:
      return "line_stroke";
    case NauticalPrimitiveType::kSymbolInstance:
      return "symbol_instance";
    case NauticalPrimitiveType::kTextLabel:
      return "text_label";
    case NauticalPrimitiveType::kSounding:
      return "sounding";
    case NauticalPrimitiveType::kRasterPatch:
      return "raster_patch";
    case NauticalPrimitiveType::kContourLine:
      return "contour_line";
    case NauticalPrimitiveType::kClipBoundary:
      return "clip_boundary";
  }
  return "unknown";
}

NauticalRenderModel BuildNauticalRenderModel(const RenderScene& scene) {
  NauticalRenderModel model;
  model.model_id = scene.scene_id.empty() ? "render-model" : scene.scene_id;
  model.source_epoch = scene.source_epoch;
  model.render_view = scene.render_view;
  model.display_state = scene.display_state;
  model.resource_table = scene.resource_table;
  model.provenance_table = scene.provenance_table;
  model.diagnostics = scene.diagnostics;
  model.metadata["model_boundary"] = "backend-neutral-nautical-render-model";
  model.metadata["semantic_owner"] = "presentation_compiler";
  model.metadata["backend_contract"] = "draw_only";
  model.metadata["source_contract"] =
      "chart_source_and_presentation_compiler_own_chart_semantics";

  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(scene.provenance_table);
  int draw_order = 0;
  for (const CommandGroup& group : scene.command_groups) {
    NauticalLayer layer;
    layer.layer_id = group.group_id.empty()
                         ? "layer-" + std::to_string(draw_order)
                         : group.group_id;
    layer.source_group_id = group.group_id;
    layer.presentation_layer = group.s52_layer;
    layer.draw_order = draw_order;
    layer.quilt_rank = group.quilt_rank;
    for (const RenderCommand& command : group.commands) {
      layer.primitives.push_back(
          PrimitiveFor(scene, group, command, provenance_by_id));
    }
    model.layers.push_back(std::move(layer));
    ++draw_order;
  }

  return model;
}

bool ValidateNauticalRenderModel(const NauticalRenderModel& model,
                                 std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (model.schema_version != kNauticalRenderModelSchemaVersion ||
      model.model_id.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "neutral_model_identity",
                                 "Neutral render model has invalid identity."));
    ok = false;
  }
  if (model.metadata.find("model_boundary") == model.metadata.end() ||
      MetadataValue(model.metadata, "backend_contract") != "draw_only") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "neutral_model_boundary",
        "Neutral render model must declare a draw-only backend contract."));
    ok = false;
  }

  bool saw_primitive = false;
  for (const NauticalLayer& layer : model.layers) {
    if (layer.layer_id.empty()) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "neutral_model_layer_id",
                                   "Neutral render model layer is missing id."));
      ok = false;
    }
    for (const NauticalPrimitive& primitive : layer.primitives) {
      saw_primitive = true;
      if (primitive.primitive_id.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "neutral_model_primitive_id",
            "Neutral primitive is missing primitive_id.",
            primitive.trace.provenance_refs));
        ok = false;
      }
      if (primitive.trace.provenance_refs.empty() ||
          primitive.trace.presentation_rule_id.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "neutral_model_traceability",
            "Neutral primitive is missing provenance or presentation rule.",
            primitive.trace.provenance_refs));
        ok = false;
      }
      if (primitive.cache_key.scene_key.empty() ||
          primitive.cache_key.primitive_key.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "neutral_model_cache_key",
            "Neutral primitive is missing cache key metadata.",
            primitive.trace.provenance_refs));
        ok = false;
      }
      if (primitive.handoff.backend_contract != "draw_only" ||
          primitive.handoff.semantic_owner == "backend") {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "neutral_model_backend_contract",
            "Neutral primitive leaks semantic ownership to the backend.",
            primitive.trace.provenance_refs));
        ok = false;
      }
      if (primitive.type == NauticalPrimitiveType::kRasterPatch &&
          primitive.coverage.no_data_policy.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "neutral_model_raster_policy",
            "RasterPatch primitive is missing no-data or coverage policy.",
            primitive.trace.provenance_refs));
        ok = false;
      }
    }
  }

  if (!saw_primitive) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "neutral_model_empty",
                                 "Neutral render model has no primitives."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render
