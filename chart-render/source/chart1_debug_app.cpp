// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_debug_app.hpp"

#include "s52_presentation_compiler.hpp"
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

std::map<std::string, const AcceptanceCase*> AcceptanceByObjectClass(
    const AcceptanceCatalog& catalog) {
  std::map<std::string, const AcceptanceCase*> by_object_class;
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    by_object_class[acceptance_case.source_object_class] = &acceptance_case;
  }
  return by_object_class;
}

std::map<std::string, const NormalizedChartObject*> ObjectById(
    const ChartSourceProduct& product) {
  std::map<std::string, const NormalizedChartObject*> by_id;
  for (const NormalizedChartObject& object : product.objects) {
    by_id[object.object_id] = &object;
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

Geometry FirstGeometry(const NauticalPrimitive& primitive,
                       const NormalizedChartObject& object) {
  if (!primitive.geometries.empty()) {
    return primitive.geometries.front();
  }
  return object.geometry;
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
    const AcceptanceCatalog& catalog, const ChartSourceProduct& product,
    const NauticalRenderModel& model, const RenderTarget& target,
    const char* backend_name, DebugReport* report) {
  const std::map<std::string, const AcceptanceCase*> acceptance_by_object =
      AcceptanceByObjectClass(catalog);
  const std::map<std::string, const NormalizedChartObject*> object_by_id =
      ObjectById(product);
  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(model.provenance_table);

  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      const auto object_found =
          object_by_id.find(primitive.trace.source_object_id);
      if (object_found == object_by_id.end()) {
        report->diagnostics.push_back(Error(
            "chart1_debug_missing_source_object",
            "Debug app could not map primitive back to a source object.",
            primitive.trace.provenance_refs));
        continue;
      }

      const NormalizedChartObject& object = *object_found->second;
      const auto case_found = acceptance_by_object.find(object.object_class);
      if (case_found == acceptance_by_object.end()) {
        continue;
      }

      const ProvenanceRecord* provenance =
          FirstProvenance(primitive.trace, provenance_by_id);

      ObjectInspection inspection;
      inspection.case_id = case_found->second->case_id;
      inspection.command_id = case_found->second->fixture_command_ids.empty()
                                  ? std::string{}
                                  : case_found->second->fixture_command_ids[0];
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
          InspectGeometry(object.geometry, provenance);
      inspection.normalized_geometry =
          InspectGeometry(FirstGeometry(primitive, object), provenance);
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

Geometry PointGeometry(std::string id, double x, double y) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.points.push_back({x, y});
  return geometry;
}

Geometry LineGeometry(std::string id) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.points = {{24.0, 160.0}, {72.0, 142.0}, {128.0, 130.0},
                     {224.0, 96.0}};
  return geometry;
}

Geometry AreaGeometry(std::string id) {
  Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = CoordinateSpace::kTarget;
  geometry.rings.push_back(
      {{16.0, 16.0}, {240.0, 16.0}, {240.0, 220.0}, {16.0, 220.0}});
  return geometry;
}

ChartAttribute Attr(std::string acronym, std::string value) {
  ChartAttribute attr;
  attr.acronym = std::move(acronym);
  attr.value = std::move(value);
  attr.display_value = attr.value;
  return attr;
}

ProvenanceRecord SourceProvenance(std::string id, std::string object_id,
                                  std::string object_class,
                                  std::string geometry_id) {
  ProvenanceRecord provenance;
  provenance.provenance_id = std::move(id);
  provenance.source_chart_id = "chart-1-fixture";
  provenance.source_chart_edition = "poc";
  provenance.source_object_id = std::move(object_id);
  provenance.source_object_class = std::move(object_class);
  provenance.source_geometry_hash = "source:" + geometry_id;
  provenance.generated_geometry_hash = "normalized:" + geometry_id;
  provenance.target_geometry_hash = "target:" + geometry_id;
  provenance.conversion_stage = "chart1_debug_source_fixture";
  provenance.transform_chain = {"source:wgs84", "normalization",
                                "projection:web_mercator_tile", "target:px"};
  provenance.quilt_decision_id = "single-chart";
  return provenance;
}

NormalizedChartObject Object(
    std::string object_id, std::string object_class,
    NormalizedGeometryKind kind, Geometry geometry,
    std::vector<ChartAttribute> attributes = {}) {
  NormalizedChartObject object;
  object.object_id = std::move(object_id);
  object.object_class = std::move(object_class);
  object.geometry_kind = kind;
  object.geometry = std::move(geometry);
  object.attributes = std::move(attributes);
  object.min_scale_denom = 50000.0;
  object.provenance_refs.push_back("prov-" + object.object_id);
  object.metadata["display_category"] = "standard";
  return object;
}

}  // namespace

RenderTarget Chart1DebugTarget(PixelSize pixel_size) {
  RenderTarget target;
  target.kind = RenderTargetKind::kOffscreen;
  target.pixel_size = pixel_size;
  target.target_id = "chart-1-debug";
  return target;
}

ChartSourceProduct BuildDebugSourceProduct() {
  ChartSourceProduct product;
  product.product_id = "chart-1-debug-source";

  ChartSourceRef source;
  source.source_id = "chart-1-fixture";
  source.kind = ChartSourceKind::kDebugFixture;
  source.role = ChartSourceRole::kDebug;
  source.native_name = "Chart 1 debug fixture";
  source.edition = "poc";
  source.content_hash = "chart-1-debug-fixture";
  source.native_projection = "target-pixel-fixture";
  source.native_scale_denom = 50000.0;
  source.geographic_bbox = {-81.82, 24.45, -81.78, 24.49};
  product.sources.push_back(std::move(source));

  auto add = [&](NormalizedChartObject object) {
    const std::string geometry_id = object.geometry.geometry_id;
    product.provenance_table.push_back(SourceProvenance(
        "prov-" + object.object_id, object.object_id, object.object_class,
        geometry_id));
    product.objects.push_back(std::move(object));
  };

  add(Object("DEPARE.1", "DEPARE", NormalizedGeometryKind::kArea,
             AreaGeometry("geom-depth-area"),
             {Attr("DRVAL1", "0"), Attr("DRVAL2", "4")}));
  add(Object("DEPCNT.1", "DEPCNT", NormalizedGeometryKind::kLine,
             LineGeometry("geom-depth-contour"), {Attr("VALDCO", "10")}));
  add(Object("BOYLAT.1", "BOYLAT", NormalizedGeometryKind::kPoint,
             PointGeometry("geom-buoy", 128.0, 92.0)));

  return product;
}

DebugReport BuildDebugReport(RenderView view, DisplayState display) {
  return BuildDebugReport(view, display, Chart1DebugTarget(view.pixel_size));
}

DebugReport BuildDebugReport(RenderView view, DisplayState display,
                             RenderTarget target) {
  DebugReport report;
  report.conformance =
      BuildConformanceScene(std::move(view), std::move(display));
  report.source_product = BuildDebugSourceProduct();

  std::vector<Diagnostic> source_diagnostics;
  const bool source_ok =
      ValidateChartSourceProduct(report.source_product, &source_diagnostics);
  report.diagnostics = report.conformance.diagnostics;
  report.diagnostics.insert(report.diagnostics.end(),
                            source_diagnostics.begin(),
                            source_diagnostics.end());

  report.model = s52::CompileS52Presentation(
      report.source_product, report.conformance.scene.render_view,
      report.conformance.scene.display_state);

  vsg::VsgBackend backend;
  report.backend_result = backend.RenderModel(report.model, target);

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(report.model, &model_diagnostics);
  report.diagnostics.insert(report.diagnostics.end(), model_diagnostics.begin(),
                            model_diagnostics.end());
  report.diagnostics.insert(report.diagnostics.end(),
                            report.backend_result.diagnostics.begin(),
                            report.backend_result.diagnostics.end());

  const AcceptanceCatalog catalog = BuildAcceptanceCatalog();
  AddLayerInspections(report.model, &report);
  AddObjectInspections(catalog, report.source_product, report.model, target,
                       backend.Name(), &report);

  report.ok = report.conformance.ok && source_ok && !report.layers.empty() &&
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
