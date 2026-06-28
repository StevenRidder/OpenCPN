// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_debug_app.hpp"

#include "vsg/vsg_backend.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

namespace ocpn::render::chart1 {
namespace {

std::string DisplayCategoryName(DisplayCategory category) {
  switch (category) {
    case DisplayCategory::kBase:
      return "base";
    case DisplayCategory::kStandard:
      return "standard";
    case DisplayCategory::kAll:
      return "all";
    case DisplayCategory::kMariner:
      return "mariner";
  }
  return "standard";
}

std::string DisplayCategoryLabel(const std::string& value,
                                 DisplayCategory fallback) {
  if (value.empty()) {
    return DisplayCategoryName(fallback);
  }
  if (value == "0") {
    return "base";
  }
  if (value == "1") {
    return "standard";
  }
  if (value == "2") {
    return "all";
  }
  if (value == "3") {
    return "mariner";
  }
  return value;
}

std::map<std::string, ProvenanceRecord> ProvenanceById(
    const std::vector<ProvenanceRecord>& records) {
  std::map<std::string, ProvenanceRecord> by_id;
  for (const ProvenanceRecord& record : records) {
    by_id[record.provenance_id] = record;
  }
  return by_id;
}

std::map<std::string, const AcceptanceCase*> AcceptanceByCommandId(
    const AcceptanceCatalog& catalog) {
  std::map<std::string, const AcceptanceCase*> by_command;
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    for (const std::string& command_id : acceptance_case.fixture_command_ids) {
      by_command[command_id] = &acceptance_case;
    }
  }
  return by_command;
}

std::map<std::string, const RenderCommand*> CommandById(
    const RenderScene& scene) {
  std::map<std::string, const RenderCommand*> by_id;
  for (const CommandGroup& group : scene.command_groups) {
    for (const RenderCommand& command : group.commands) {
      by_id[command.command_id] = &command;
    }
  }
  return by_id;
}

const ProvenanceRecord* FirstProvenance(
    const SourceTraceHandle& trace,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  if (trace.provenance_refs.empty()) {
    return nullptr;
  }
  const auto found = provenance_by_id.find(trace.provenance_refs.front());
  return found == provenance_by_id.end() ? nullptr : &found->second;
}

Geometry SyntheticPositionGeometry(const RenderCommand& command) {
  Geometry geometry;
  geometry.geometry_id = "position:" + command.command_id;
  geometry.coordinate_space = command.coordinate_space;
  geometry.points.push_back(command.position);
  return geometry;
}

Geometry FirstGeometry(const RenderCommand& command) {
  if (!command.geometries.empty()) {
    return command.geometries.front();
  }
  return SyntheticPositionGeometry(command);
}

Geometry FirstGeometry(const NauticalPrimitive& primitive,
                       const RenderCommand& command) {
  if (!primitive.geometries.empty()) {
    return primitive.geometries.front();
  }
  return SyntheticPositionGeometry(command);
}

GeometryInspection InspectGeometry(const Geometry& geometry,
                                   const ProvenanceRecord* provenance) {
  GeometryInspection inspection;
  inspection.geometry_id = geometry.geometry_id;
  inspection.coordinate_space = geometry.coordinate_space;
  inspection.point_count = geometry.points.size();
  inspection.ring_count = geometry.rings.size();
  if (provenance) {
    inspection.source_geometry_hash = provenance->source_geometry_hash;
    inspection.generated_geometry_hash = provenance->generated_geometry_hash;
    inspection.target_geometry_hash = provenance->target_geometry_hash;
  }
  return inspection;
}

std::string JoinedCacheKey(const NauticalCacheKey& key) {
  return key.scene_key + "|" + key.primitive_key + "|" + key.resource_key;
}

std::string StableAssetId(const char* backend_name, const RenderTarget& target,
                          const NauticalPrimitive& primitive,
                          const char* surface) {
  std::ostringstream out;
  out << surface << ":" << backend_name << ":" << target.target_id << ":"
      << primitive.primitive_id << ":" << primitive.cache_key.resource_key;
  return out.str();
}

Diagnostic Error(std::string code, std::string message,
                 std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Keep Chart 1 debug inspection complete from source feature through "
      "neutral primitive and backend asset handoff.";
  return diagnostic;
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::kError;
                     });
}

void AddLayerInspections(const NauticalRenderModel& model,
                         DebugReport* report) {
  for (const NauticalLayer& layer : model.layers) {
    LayerInspection inspection;
    inspection.layer_id = layer.layer_id;
    inspection.presentation_layer = layer.presentation_layer;
    inspection.draw_order = layer.draw_order;
    for (const NauticalPrimitive& primitive : layer.primitives) {
      inspection.backend_primitive_ids.push_back(primitive.primitive_id);
    }
    report->layers.push_back(std::move(inspection));
  }
}

void AddObjectInspections(
    const AcceptanceCatalog& catalog, const RenderScene& scene,
    const NauticalRenderModel& model, const RenderTarget& target,
    const char* backend_name, DebugReport* report) {
  const std::map<std::string, const AcceptanceCase*> acceptance_by_command =
      AcceptanceByCommandId(catalog);
  const std::map<std::string, const RenderCommand*> command_by_id =
      CommandById(scene);
  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(model.provenance_table);

  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      const auto command_found = command_by_id.find(primitive.primitive_id);
      if (command_found == command_by_id.end()) {
        report->diagnostics.push_back(Error(
            "chart1_debug_missing_command",
            "Debug app could not map primitive back to a render command.",
            primitive.trace.provenance_refs));
        continue;
      }

      const RenderCommand& command = *command_found->second;
      const auto case_found = acceptance_by_command.find(command.command_id);
      if (case_found == acceptance_by_command.end()) {
        continue;
      }

      const ProvenanceRecord* provenance =
          FirstProvenance(primitive.trace, provenance_by_id);

      ObjectInspection inspection;
      inspection.case_id = case_found->second->case_id;
      inspection.command_id = command.command_id;
      inspection.layer_id = layer.layer_id;
      inspection.presentation_layer = layer.presentation_layer;
      inspection.source_chart_id = primitive.trace.source_chart_id;
      inspection.source_feature_id = primitive.trace.source_object_id;
      inspection.source_object_class = primitive.trace.source_object_class;
      inspection.s52_rule_id = primitive.trace.presentation_rule_id;
      inspection.display_category = DisplayCategoryLabel(
          primitive.lod.display_category, model.display_state.display_category);
      inspection.projection_transform =
          provenance ? provenance->transform_chain : std::vector<std::string>{};
      inspection.original_geometry =
          InspectGeometry(FirstGeometry(command), provenance);
      inspection.normalized_geometry =
          InspectGeometry(FirstGeometry(primitive, command), provenance);
      inspection.scale.min_scale_denom = primitive.lod.min_scale_denom;
      inspection.scale.max_scale_denom = primitive.lod.max_scale_denom;
      inspection.scale.view_scale_denom = model.render_view.scale_denom;
      inspection.scale.overzoom = primitive.lod.overzoom;
      inspection.cache.scene_key = primitive.cache_key.scene_key;
      inspection.cache.primitive_key = primitive.cache_key.primitive_key;
      inspection.cache.resource_key = primitive.cache_key.resource_key;
      inspection.cache.tile_cache_key = JoinedCacheKey(primitive.cache_key);
      inspection.backend_primitive_id = primitive.primitive_id;
      inspection.final_gpu_asset_id =
          StableAssetId(backend_name, target, primitive, "gpu");
      inspection.final_web_asset_id =
          StableAssetId(backend_name, target, primitive, "web");
      inspection.provenance_refs = primitive.trace.provenance_refs;
      inspection.conversion_trace_refs =
          primitive.trace.conversion_trace_refs;

      if (inspection.source_chart_id.empty() ||
          inspection.source_feature_id.empty() ||
          inspection.source_object_class.empty() ||
          inspection.s52_rule_id.empty() ||
          inspection.original_geometry.geometry_id.empty() ||
          inspection.normalized_geometry.geometry_id.empty() ||
          inspection.projection_transform.empty() ||
          inspection.cache.tile_cache_key.empty() ||
          inspection.final_gpu_asset_id.empty() ||
          inspection.final_web_asset_id.empty()) {
        report->diagnostics.push_back(Error(
            "chart1_debug_incomplete_object_trace",
            "Debug inspection row is missing required source-to-render fields.",
            inspection.provenance_refs));
      }

      report->objects.push_back(std::move(inspection));
    }
  }

  if (report->objects.size() != catalog.cases.size()) {
    report->diagnostics.push_back(Error(
        "chart1_debug_case_coverage",
        "Debug app did not produce one inspection row per Chart 1 case."));
  }
}

}  // namespace

RenderTarget Chart1DebugTarget(PixelSize pixel_size) {
  RenderTarget target;
  target.kind = RenderTargetKind::kOffscreen;
  target.pixel_size = pixel_size;
  target.target_id = "chart-1-debug";
  return target;
}

DebugReport BuildDebugReport(RenderView view, DisplayState display) {
  return BuildDebugReport(view, display, Chart1DebugTarget(view.pixel_size));
}

DebugReport BuildDebugReport(RenderView view, DisplayState display,
                             RenderTarget target) {
  DebugReport report;
  report.conformance =
      BuildConformanceScene(std::move(view), std::move(display));
  report.model = BuildNauticalRenderModel(report.conformance.scene);

  vsg::VsgBackend backend;
  report.backend_result = backend.RenderModel(report.model, target);
  report.diagnostics = report.conformance.diagnostics;

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(report.model, &model_diagnostics);
  report.diagnostics.insert(report.diagnostics.end(), model_diagnostics.begin(),
                            model_diagnostics.end());
  report.diagnostics.insert(report.diagnostics.end(),
                            report.backend_result.diagnostics.begin(),
                            report.backend_result.diagnostics.end());

  const AcceptanceCatalog catalog = BuildAcceptanceCatalog();
  AddLayerInspections(report.model, &report);
  AddObjectInspections(catalog, report.conformance.scene, report.model, target,
                       backend.Name(), &report);

  report.ok = report.conformance.ok && !report.layers.empty() &&
              report.objects.size() == catalog.cases.size() &&
              !HasError(report.diagnostics);
  return report;
}

const ObjectInspection* FindObjectBySourceFeatureId(
    const DebugReport& report, const std::string& source_feature_id) {
  const auto found =
      std::find_if(report.objects.begin(), report.objects.end(),
                   [&](const ObjectInspection& inspection) {
                     return inspection.source_feature_id == source_feature_id;
                   });
  return found == report.objects.end() ? nullptr : &*found;
}

const ObjectInspection* FindObjectByBackendPrimitiveId(
    const DebugReport& report, const std::string& backend_primitive_id) {
  const auto found =
      std::find_if(report.objects.begin(), report.objects.end(),
                   [&](const ObjectInspection& inspection) {
                     return inspection.backend_primitive_id ==
                            backend_primitive_id;
                   });
  return found == report.objects.end() ? nullptr : &*found;
}

std::vector<const ObjectInspection*> FindObjectsInLayer(
    const DebugReport& report, const std::string& layer_id) {
  std::vector<const ObjectInspection*> objects;
  for (const ObjectInspection& inspection : report.objects) {
    if (inspection.layer_id == layer_id) {
      objects.push_back(&inspection);
    }
  }
  return objects;
}

}  // namespace ocpn::render::chart1
