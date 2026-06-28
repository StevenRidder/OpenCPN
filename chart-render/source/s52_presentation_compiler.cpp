// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s52_presentation_compiler.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

namespace ocpn::render::s52 {
namespace {

enum class S52DisplayClass {
  kBase,
  kStandard,
  kAll,
  kMariner
};

struct LayerSpec {
  std::string id;
  std::string presentation_layer;
  int draw_order = 0;
};

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

std::string AttributeValue(const NormalizedChartObject& object,
                           const char* acronym) {
  for (const ChartAttribute& attribute : object.attributes) {
    if (attribute.acronym == acronym) {
      return attribute.value.empty() ? attribute.display_value
                                     : attribute.value;
    }
  }
  return {};
}

double ParseDouble(const std::string& value, double fallback) {
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  return end == value.c_str() ? fallback : parsed;
}

double AttributeDouble(const NormalizedChartObject& object,
                       const char* acronym, double fallback) {
  return ParseDouble(AttributeValue(object, acronym), fallback);
}

std::string PaletteName(Palette palette) {
  switch (palette) {
    case Palette::kDay:
      return "day";
    case Palette::kDusk:
      return "dusk";
    case Palette::kNight:
      return "night";
  }
  return "day";
}

std::string PaletteResource(const std::string& base, Palette palette) {
  return base + "-" + PaletteName(palette);
}

std::string DisplayClassName(S52DisplayClass display_class) {
  switch (display_class) {
    case S52DisplayClass::kBase:
      return "base";
    case S52DisplayClass::kStandard:
      return "standard";
    case S52DisplayClass::kAll:
      return "all";
    case S52DisplayClass::kMariner:
      return "mariner";
  }
  return "standard";
}

S52DisplayClass DisplayClassFromString(const std::string& value,
                                       S52DisplayClass fallback) {
  if (value == "base") {
    return S52DisplayClass::kBase;
  }
  if (value == "standard") {
    return S52DisplayClass::kStandard;
  }
  if (value == "all") {
    return S52DisplayClass::kAll;
  }
  if (value == "mariner") {
    return S52DisplayClass::kMariner;
  }
  return fallback;
}

S52DisplayClass RequiredDisplayClass(const NormalizedChartObject& object) {
  const std::string explicit_category =
      MetadataValue(object.metadata, "display_category");
  if (!explicit_category.empty()) {
    return DisplayClassFromString(explicit_category,
                                  S52DisplayClass::kStandard);
  }
  if (object.object_class == "M_COVR") {
    return S52DisplayClass::kBase;
  }
  return S52DisplayClass::kStandard;
}

bool DisplayClassVisible(S52DisplayClass required, DisplayCategory selected) {
  switch (selected) {
    case DisplayCategory::kBase:
      return required == S52DisplayClass::kBase;
    case DisplayCategory::kStandard:
      return required == S52DisplayClass::kBase ||
             required == S52DisplayClass::kStandard;
    case DisplayCategory::kAll:
      return required == S52DisplayClass::kBase ||
             required == S52DisplayClass::kStandard ||
             required == S52DisplayClass::kAll;
    case DisplayCategory::kMariner:
      return true;
  }
  return true;
}

bool ScaminVisible(const NormalizedChartObject& object,
                   const RenderView& view) {
  if (object.min_scale_denom <= 0.0 || view.scale_denom <= 0.0) {
    return true;
  }
  return view.scale_denom <= object.min_scale_denom;
}

std::string PresentationRuleId(const NormalizedChartObject& object,
                               const std::string& role) {
  return "s52:" + object.object_class + ":" + role;
}

std::map<std::string, ProvenanceRecord> ProvenanceById(
    const ChartSourceProduct& product) {
  std::map<std::string, ProvenanceRecord> by_id;
  for (const ProvenanceRecord& record : product.provenance_table) {
    by_id[record.provenance_id] = record;
  }
  return by_id;
}

SourceTraceHandle TraceFor(
    const NormalizedChartObject& object, const std::string& rule_id,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  SourceTraceHandle trace;
  trace.provenance_refs = object.provenance_refs;
  trace.source_object_id = object.object_id;
  trace.source_object_class = object.object_class;
  trace.presentation_rule_id = rule_id;
  if (!object.provenance_refs.empty()) {
    const auto it = provenance_by_id.find(object.provenance_refs.front());
    if (it != provenance_by_id.end()) {
      trace.source_chart_id = it->second.source_chart_id;
      trace.source_object_id = it->second.source_object_id.empty()
                                   ? trace.source_object_id
                                   : it->second.source_object_id;
      trace.source_object_class = it->second.source_object_class.empty()
                                      ? trace.source_object_class
                                      : it->second.source_object_class;
    }
  }
  return trace;
}

BackendHandoffContract HandoffFor(CoordinateSpace coordinate_space) {
  BackendHandoffContract handoff;
  handoff.coordinate_space = coordinate_space;
  handoff.accepted_backend_targets = {"vsg", "opengl", "metal", "webgpu",
                                      "helm_offscreen"};
  return handoff;
}

std::string SceneKey(const NauticalRenderModel& model) {
  return model.model_id + ":" + model.source_epoch + ":" +
         model.render_view.view_id;
}

NauticalLodHint LodFor(const NormalizedChartObject& object,
                       const RenderView& view,
                       S52DisplayClass display_class,
                       const std::string& priority) {
  NauticalLodHint hint;
  hint.min_scale_denom = object.min_scale_denom;
  hint.max_scale_denom = object.max_scale_denom;
  hint.overzoom = view.overzoom;
  hint.display_category = DisplayClassName(display_class);
  hint.priority = priority;
  hint.visible = true;
  return hint;
}

Point2 FirstPoint(const Geometry& geometry) {
  if (!geometry.points.empty()) {
    return geometry.points.front();
  }
  if (!geometry.rings.empty() && !geometry.rings.front().empty()) {
    return geometry.rings.front().front();
  }
  return {};
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
      "Keep S-52/S-101 presentation decisions in the compiler before backend "
      "handoff.";
  return diagnostic;
}

ResourceRecord MakeResource(std::string id, ResourceType type,
                            std::string provenance_id,
                            std::string role) {
  ResourceRecord resource;
  resource.resource_id = std::move(id);
  resource.type = type;
  resource.content_hash = "s52-presentation-fixture";
  resource.provenance_id = std::move(provenance_id);
  resource.metrics["role"] = std::move(role);
  return resource;
}

void AddResource(NauticalRenderModel* model, std::set<std::string>* seen,
                 ResourceRecord resource) {
  if (seen->insert(resource.resource_id).second) {
    model->resource_table.resources.push_back(std::move(resource));
  }
}

NauticalPrimitive BasePrimitive(
    const NauticalRenderModel& model, const NormalizedChartObject& object,
    NauticalPrimitiveType type, const std::string& role,
    const std::string& resource_key, S52DisplayClass display_class,
    const std::string& priority,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  NauticalPrimitive primitive;
  primitive.primitive_id = "prim-" + object.object_id + "-" + role;
  primitive.type = type;
  primitive.role = role;
  primitive.trace =
      TraceFor(object, PresentationRuleId(object, role), provenance_by_id);
  primitive.lod = LodFor(object, model.render_view, display_class, priority);
  primitive.cache_key.scene_key = SceneKey(model);
  primitive.cache_key.primitive_key =
      model.model_id + ":" + object.object_id + ":" + role;
  primitive.cache_key.resource_key = resource_key;
  primitive.handoff = HandoffFor(object.geometry.coordinate_space);
  primitive.metadata["display_category"] = DisplayClassName(display_class);
  primitive.metadata["palette"] = PaletteName(model.display_state.palette);
  primitive.metadata["compiler"] = "s52_presentation_compiler";
  return primitive;
}

LayerSpec LayerFor(NauticalPrimitiveType type) {
  switch (type) {
    case NauticalPrimitiveType::kAreaFill:
      return {"s52-areas", "area", 0};
    case NauticalPrimitiveType::kContourLine:
    case NauticalPrimitiveType::kLineStroke:
      return {"s52-lines", "line", 1};
    case NauticalPrimitiveType::kSymbolInstance:
      return {"s52-symbols", "symbol", 2};
    case NauticalPrimitiveType::kTextLabel:
    case NauticalPrimitiveType::kSounding:
      return {"s52-text", "text", 3};
    case NauticalPrimitiveType::kRasterPatch:
      return {"s52-raster", "raster", 4};
    case NauticalPrimitiveType::kClipBoundary:
      return {"s52-clip", "clip", 5};
  }
  return {"s52-other", "other", 9};
}

void AddPrimitive(NauticalRenderModel* model, NauticalPrimitive primitive) {
  const LayerSpec spec = LayerFor(primitive.type);
  auto found = std::find_if(model->layers.begin(), model->layers.end(),
                            [&](const NauticalLayer& layer) {
                              return layer.layer_id == spec.id;
                            });
  if (found == model->layers.end()) {
    NauticalLayer layer;
    layer.layer_id = spec.id;
    layer.source_group_id = "s52-presentation-compiler";
    layer.presentation_layer = spec.presentation_layer;
    layer.draw_order = spec.draw_order;
    model->layers.push_back(std::move(layer));
    found = model->layers.end() - 1;
  }
  found->primitives.push_back(std::move(primitive));
}

std::string DepthAreaFill(const NormalizedChartObject& object,
                          const DisplayState& display,
                          const PresentationAssets& assets) {
  const double shallowest = AttributeDouble(object, "DRVAL1", 0.0);
  const double deepest = AttributeDouble(object, "DRVAL2", shallowest);
  const bool shallow =
      display.safety_depth_m > 0.0 && deepest <= display.safety_depth_m;
  return PaletteResource(shallow ? assets.depth_shallow_fill
                                 : assets.depth_safe_fill,
                         display.palette);
}

std::string DepthAreaSafetyClass(const NormalizedChartObject& object,
                                 const DisplayState& display) {
  const double shallowest = AttributeDouble(object, "DRVAL1", 0.0);
  const double deepest = AttributeDouble(object, "DRVAL2", shallowest);
  if (display.safety_depth_m > 0.0 && deepest <= display.safety_depth_m) {
    return "shallow";
  }
  if (display.safety_contour_m > 0.0 &&
      shallowest < display.safety_contour_m &&
      deepest >= display.safety_contour_m) {
    return "contains_safety_contour";
  }
  return "safe";
}

bool IsSafetyContour(const NormalizedChartObject& object,
                     const DisplayState& display) {
  const double contour_m = AttributeDouble(object, "VALDCO", -1.0);
  return display.safety_contour_m > 0.0 &&
         contour_m == display.safety_contour_m;
}

std::string ObjectLabelText(const NormalizedChartObject& object) {
  std::string label = AttributeValue(object, "OBJNAM");
  if (label.empty()) {
    label = AttributeValue(object, "NOBJNM");
  }
  if (label.empty()) {
    label = AttributeValue(object, "LITCHR");
  }
  return label;
}

std::string SoundingText(const NormalizedChartObject& object) {
  const std::string formatted = AttributeValue(object, "SOUNDING");
  if (!formatted.empty()) {
    return formatted;
  }
  const std::string value = AttributeValue(object, "VALSOU");
  return value.empty() ? "0" : value;
}

void CompileDepthArea(
    NauticalRenderModel* model, const NormalizedChartObject& object,
    S52DisplayClass display_class, const PresentationAssets& assets,
    std::set<std::string>* seen_resources,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  const std::string fill_ref =
      DepthAreaFill(object, model->display_state, assets);
  AddResource(model, seen_resources,
              MakeResource(fill_ref, ResourceType::kPalette,
                           object.provenance_refs.empty()
                               ? std::string{}
                               : object.provenance_refs.front(),
                           "depth_area_fill"));
  NauticalPrimitive primitive =
      BasePrimitive(*model, object, NauticalPrimitiveType::kAreaFill,
                    "depth_area", fill_ref, display_class, "area",
                    provenance_by_id);
  primitive.geometries.push_back(object.geometry);
  primitive.fill_ref = fill_ref;
  primitive.metadata["safety_class"] =
      DepthAreaSafetyClass(object, model->display_state);
  AddPrimitive(model, std::move(primitive));
}

void CompileDepthContour(
    NauticalRenderModel* model, const NormalizedChartObject& object,
    S52DisplayClass display_class, const PresentationAssets& assets,
    std::set<std::string>* seen_resources,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  const bool safety = IsSafetyContour(object, model->display_state);
  const std::string line_ref =
      PaletteResource(safety ? assets.safety_contour_line
                             : assets.depth_contour_line,
                      model->display_state.palette);
  AddResource(model, seen_resources,
              MakeResource(line_ref, ResourceType::kLineStyle,
                           object.provenance_refs.empty()
                               ? std::string{}
                               : object.provenance_refs.front(),
                           safety ? "safety_contour" : "depth_contour"));
  NauticalPrimitive primitive =
      BasePrimitive(*model, object, NauticalPrimitiveType::kContourLine,
                    safety ? "safety_contour" : "depth_contour", line_ref,
                    display_class, safety ? "navigation" : "line",
                    provenance_by_id);
  primitive.geometries.push_back(object.geometry);
  primitive.line_style_ref = line_ref;
  primitive.width_px = safety ? 2.0 : 1.0;
  primitive.depth_m = AttributeDouble(object, "VALDCO", 0.0);
  primitive.metadata["safety_contour"] = safety ? "true" : "false";
  AddPrimitive(model, std::move(primitive));
}

void CompileSymbol(
    NauticalRenderModel* model, const NormalizedChartObject& object,
    S52DisplayClass display_class, const PresentationAssets& assets,
    std::set<std::string>* seen_resources,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  const std::string symbol_ref =
      PaletteResource(object.object_class == "BOYLAT"
                          ? assets.lateral_buoy_symbol
                          : assets.default_symbol,
                      model->display_state.palette);
  AddResource(model, seen_resources,
              MakeResource(symbol_ref, ResourceType::kSymbol,
                           object.provenance_refs.empty()
                               ? std::string{}
                               : object.provenance_refs.front(),
                           "symbol"));
  NauticalPrimitive primitive =
      BasePrimitive(*model, object, NauticalPrimitiveType::kSymbolInstance,
                    "navaid_symbol", symbol_ref, display_class, "navigation",
                    provenance_by_id);
  primitive.symbol_ref = symbol_ref;
  primitive.position = FirstPoint(object.geometry);
  primitive.anchor = "center";
  AddPrimitive(model, std::move(primitive));
}

void CompileTextLabel(
    NauticalRenderModel* model, const NormalizedChartObject& object,
    S52DisplayClass display_class, const PresentationAssets& assets,
    std::set<std::string>* seen_resources,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  if (!model->display_state.show_text) {
    return;
  }
  const std::string text = ObjectLabelText(object);
  if (text.empty()) {
    return;
  }
  AddResource(model, seen_resources,
              MakeResource(assets.label_font, ResourceType::kFont,
                           object.provenance_refs.empty()
                               ? std::string{}
                               : object.provenance_refs.front(),
                           "text"));
  NauticalPrimitive primitive =
      BasePrimitive(*model, object, NauticalPrimitiveType::kTextLabel,
                    object.object_class == "LIGHTS" ? "light_label"
                                                    : "object_label",
                    assets.label_font, display_class, "label",
                    provenance_by_id);
  primitive.text = text;
  primitive.font_ref = assets.label_font;
  primitive.position = FirstPoint(object.geometry);
  primitive.anchor = "left";
  AddPrimitive(model, std::move(primitive));
}

void CompileSounding(
    NauticalRenderModel* model, const NormalizedChartObject& object,
    S52DisplayClass display_class, const PresentationAssets& assets,
    std::set<std::string>* seen_resources,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  if (!model->display_state.show_soundings) {
    return;
  }
  AddResource(model, seen_resources,
              MakeResource(assets.label_font, ResourceType::kFont,
                           object.provenance_refs.empty()
                               ? std::string{}
                               : object.provenance_refs.front(),
                           "sounding"));
  NauticalPrimitive primitive =
      BasePrimitive(*model, object, NauticalPrimitiveType::kSounding,
                    "sounding", assets.label_font, display_class, "sounding",
                    provenance_by_id);
  primitive.text = SoundingText(object);
  primitive.font_ref = assets.label_font;
  primitive.depth_m = AttributeDouble(object, "VALSOU", 0.0);
  primitive.position = FirstPoint(object.geometry);
  primitive.metadata["safety_class"] =
      primitive.depth_m <= model->display_state.safety_depth_m ? "shoal"
                                                              : "safe";
  AddPrimitive(model, std::move(primitive));
}

void SortModel(NauticalRenderModel* model) {
  std::sort(model->layers.begin(), model->layers.end(),
            [](const NauticalLayer& lhs, const NauticalLayer& rhs) {
              return lhs.draw_order < rhs.draw_order;
            });
  for (NauticalLayer& layer : model->layers) {
    std::sort(layer.primitives.begin(), layer.primitives.end(),
              [](const NauticalPrimitive& lhs, const NauticalPrimitive& rhs) {
                return lhs.cache_key.primitive_key <
                       rhs.cache_key.primitive_key;
              });
  }
}

}  // namespace

NauticalRenderModel S52PresentationCompiler::Compile(
    const ChartSourceProduct& normalized_features, RenderView view,
    DisplayState display, PresentationAssets assets,
    PresentationOptions options) const {
  NauticalRenderModel model;
  model.model_id = normalized_features.product_id.empty()
                       ? "s52-presentation-model"
                       : normalized_features.product_id;
  model.source_epoch =
      "s52-presentation:" + std::to_string(normalized_features.schema_version);
  model.render_view = std::move(view);
  model.display_state = std::move(display);
  model.provenance_table = normalized_features.provenance_table;
  model.diagnostics = normalized_features.diagnostics;
  model.metadata["model_boundary"] = "backend-neutral-nautical-render-model";
  model.metadata["semantic_owner"] = "presentation_compiler";
  model.metadata["presentation_compiler"] = options.compiler_id;
  model.metadata["backend_contract"] = "draw_only";
  model.metadata["source_contract"] =
      "normalized_features_and_provenance_enter_s52_presentation_compiler";

  std::set<std::string> seen_resources;
  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(normalized_features);

  for (const NormalizedChartObject& object : normalized_features.objects) {
    const S52DisplayClass display_class = RequiredDisplayClass(object);
    if (!DisplayClassVisible(display_class,
                             model.display_state.display_category)) {
      if (options.emit_filtered_diagnostics) {
        model.diagnostics.push_back(MakeDiagnostic(
            DiagnosticSeverity::kInfo, "s52.display_category_filtered",
            "S-52 object filtered before backend handoff by display category.",
            object.provenance_refs));
      }
      continue;
    }
    if (!ScaminVisible(object, model.render_view)) {
      if (options.emit_filtered_diagnostics) {
        model.diagnostics.push_back(MakeDiagnostic(
            DiagnosticSeverity::kInfo, "s52.scamin_filtered",
            "S-52 object filtered before backend handoff by SCAMIN scale.",
            object.provenance_refs));
      }
      continue;
    }

    if (object.object_class == "DEPARE") {
      CompileDepthArea(&model, object, display_class, assets, &seen_resources,
                       provenance_by_id);
    } else if (object.object_class == "DEPCNT") {
      CompileDepthContour(&model, object, display_class, assets,
                          &seen_resources, provenance_by_id);
    } else if (object.object_class == "BOYLAT" ||
               object.object_class == "BCNLAT" ||
               object.object_class == "WRECKS") {
      CompileSymbol(&model, object, display_class, assets, &seen_resources,
                    provenance_by_id);
    } else if (object.object_class == "SOUNDG") {
      CompileSounding(&model, object, display_class, assets, &seen_resources,
                      provenance_by_id);
    } else if (object.object_class == "LNDARE" ||
               object.object_class == "LIGHTS") {
      CompileTextLabel(&model, object, display_class, assets, &seen_resources,
                       provenance_by_id);
    } else if (options.emit_unsupported_diagnostics) {
      model.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kWarning, "s52.unsupported_object_class",
          "S-52 object class has no presentation compiler mapping yet.",
          object.provenance_refs));
    }
  }

  SortModel(&model);
  return model;
}

NauticalRenderModel CompileS52Presentation(
    const ChartSourceProduct& normalized_features, RenderView view,
    DisplayState display, PresentationAssets assets,
    PresentationOptions options) {
  return S52PresentationCompiler().Compile(normalized_features, std::move(view),
                                           std::move(display),
                                           std::move(assets),
                                           std::move(options));
}

}  // namespace ocpn::render::s52
