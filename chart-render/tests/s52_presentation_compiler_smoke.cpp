// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"
#include "vsg/vsg_backend.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

ocpn::render::Geometry PointGeometry(std::string id, double x, double y) {
  ocpn::render::Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = ocpn::render::CoordinateSpace::kTarget;
  geometry.points.push_back({x, y});
  return geometry;
}

ocpn::render::Geometry LineGeometry(std::string id) {
  ocpn::render::Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = ocpn::render::CoordinateSpace::kTarget;
  geometry.points = {{24.0, 160.0}, {80.0, 144.0}, {180.0, 120.0}};
  return geometry;
}

ocpn::render::Geometry AreaGeometry(std::string id) {
  ocpn::render::Geometry geometry;
  geometry.geometry_id = std::move(id);
  geometry.coordinate_space = ocpn::render::CoordinateSpace::kTarget;
  geometry.rings.push_back(
      {{16.0, 16.0}, {240.0, 16.0}, {240.0, 220.0}, {16.0, 220.0}});
  return geometry;
}

ocpn::render::ProvenanceRecord Provenance(std::string id,
                                          std::string object_id,
                                          std::string object_class) {
  ocpn::render::ProvenanceRecord provenance;
  provenance.provenance_id = std::move(id);
  provenance.source_chart_id = "chart-1-normalized-fixture";
  provenance.source_chart_edition = "poc";
  provenance.source_object_id = std::move(object_id);
  provenance.source_object_class = std::move(object_class);
  provenance.source_geometry_hash = "fixture";
  provenance.conversion_stage = "normalized_feature_fixture";
  provenance.transform_chain = {"source:wgs84", "normalization",
                                "target:px"};
  return provenance;
}

ocpn::render::ChartAttribute Attr(std::string acronym, std::string value) {
  ocpn::render::ChartAttribute attr;
  attr.acronym = std::move(acronym);
  attr.value = std::move(value);
  attr.display_value = attr.value;
  return attr;
}

ocpn::render::NormalizedChartObject Object(
    std::string object_id, std::string object_class,
    ocpn::render::NormalizedGeometryKind kind, ocpn::render::Geometry geometry,
    std::vector<ocpn::render::ChartAttribute> attributes = {}) {
  ocpn::render::NormalizedChartObject object;
  object.object_id = std::move(object_id);
  object.object_class = std::move(object_class);
  object.geometry_kind = kind;
  object.geometry = std::move(geometry);
  object.attributes = std::move(attributes);
  object.provenance_refs.push_back("prov-" + object.object_id);
  return object;
}

ocpn::render::ChartSourceProduct BuildFixtureProduct() {
  ocpn::render::ChartSourceProduct product;
  product.product_id = "s52-presentation-fixture";

  auto add = [&](ocpn::render::NormalizedChartObject object) {
    product.provenance_table.push_back(
        Provenance("prov-" + object.object_id, object.object_id,
                   object.object_class));
    product.objects.push_back(std::move(object));
  };

  add(Object("DEPARE.1", "DEPARE", ocpn::render::NormalizedGeometryKind::kArea,
             AreaGeometry("geom-depare"),
             {Attr("DRVAL1", "0"), Attr("DRVAL2", "4")}));
  add(Object("DEPCNT.1", "DEPCNT", ocpn::render::NormalizedGeometryKind::kLine,
             LineGeometry("geom-depcnt"), {Attr("VALDCO", "10")}));
  add(Object("BOYLAT.1", "BOYLAT",
             ocpn::render::NormalizedGeometryKind::kPoint,
             PointGeometry("geom-boylat", 128.0, 92.0)));
  add(Object("SOUNDG.1", "SOUNDG",
             ocpn::render::NormalizedGeometryKind::kPoint,
             PointGeometry("geom-soundg", 94.0, 176.0),
             {Attr("VALSOU", "7.4")}));
  add(Object("LNDARE.1", "LNDARE",
             ocpn::render::NormalizedGeometryKind::kPoint,
             PointGeometry("geom-lndare", 138.0, 82.0),
             {Attr("OBJNAM", "Fixture Harbor")}));

  ocpn::render::NormalizedChartObject display_filtered =
      Object("OBSTRN.1", "OBSTRN",
             ocpn::render::NormalizedGeometryKind::kPoint,
             PointGeometry("geom-obstrn", 42.0, 42.0));
  display_filtered.metadata["display_category"] = "all";
  add(std::move(display_filtered));

  ocpn::render::NormalizedChartObject scamin_filtered =
      Object("WRECKS.1", "WRECKS",
             ocpn::render::NormalizedGeometryKind::kPoint,
             PointGeometry("geom-wrecks", 220.0, 220.0));
  scamin_filtered.min_scale_denom = 10000.0;
  add(std::move(scamin_filtered));

  return product;
}

bool HasDiagnostic(const ocpn::render::NauticalRenderModel& model,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : model.diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

bool HasDiagnosticWithProvenance(
    const ocpn::render::NauticalRenderModel& model,
    const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : model.diagnostics) {
    if (diagnostic.code == code && !diagnostic.provenance_refs.empty()) {
      return true;
    }
  }
  return false;
}

void AddTypes(const ocpn::render::NauticalRenderModel& model,
              std::set<ocpn::render::NauticalPrimitiveType>* types) {
  for (const ocpn::render::NauticalLayer& layer : model.layers) {
    for (const ocpn::render::NauticalPrimitive& primitive :
         layer.primitives) {
      types->insert(primitive.type);
    }
  }
}

bool CheckBoundary(const ocpn::render::NauticalRenderModel& model) {
  if (model.metadata.at("backend_contract") != "draw_only") {
    std::cerr << "Model backend contract is not draw_only\n";
    return false;
  }
  int last_order = -1;
  bool saw_safety_contour = false;
  bool saw_safe_sounding = false;
  for (const ocpn::render::NauticalLayer& layer : model.layers) {
    if (layer.draw_order < last_order) {
      std::cerr << "Layer order is not stable\n";
      return false;
    }
    last_order = layer.draw_order;
    for (const ocpn::render::NauticalPrimitive& primitive :
         layer.primitives) {
      if (primitive.handoff.backend_contract != "draw_only" ||
          primitive.handoff.semantic_owner != "presentation_compiler") {
        std::cerr << "Primitive leaks semantic ownership to backend\n";
        return false;
      }
      if (primitive.trace.provenance_refs.empty() ||
          primitive.trace.source_object_class.empty() ||
          primitive.trace.presentation_rule_id.empty()) {
        std::cerr << "Primitive missing trace handle\n";
        return false;
      }
      if (primitive.metadata.find("s52_rule") != primitive.metadata.end() ||
          primitive.metadata.find("source_object_class") !=
              primitive.metadata.end()) {
        std::cerr << "Primitive metadata carries backend-facing S-52 keys\n";
        return false;
      }
      if (primitive.type == ocpn::render::NauticalPrimitiveType::kContourLine &&
          primitive.line_style_ref == "s52-safety-contour-line-day" &&
          primitive.metadata.at("safety_contour") == "true") {
        saw_safety_contour = true;
      }
      if (primitive.type == ocpn::render::NauticalPrimitiveType::kSounding &&
          primitive.metadata.at("safety_class") == "safe") {
        saw_safe_sounding = true;
      }
    }
  }
  if (!saw_safety_contour) {
    std::cerr << "Safety contour style was not resolved before backend\n";
    return false;
  }
  if (!saw_safe_sounding) {
    std::cerr << "Sounding safety class was not resolved before backend\n";
    return false;
  }
  return true;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

const ocpn::render::NauticalPrimitive* FindPrimitiveBySourceAndRole(
    const ocpn::render::NauticalRenderModel& model,
    const std::string& source_object_id, const std::string& role) {
  for (const ocpn::render::NauticalLayer& layer : model.layers) {
    for (const ocpn::render::NauticalPrimitive& primitive :
         layer.primitives) {
      if (primitive.trace.source_object_id == source_object_id &&
          primitive.role == role) {
        return &primitive;
      }
    }
  }
  return nullptr;
}

bool PrimitivesSortedByKey(const ocpn::render::NauticalLayer& layer) {
  return std::is_sorted(
      layer.primitives.begin(), layer.primitives.end(),
      [](const ocpn::render::NauticalPrimitive& lhs,
         const ocpn::render::NauticalPrimitive& rhs) {
        return lhs.cache_key.primitive_key < rhs.cache_key.primitive_key;
      });
}

bool CheckPackageBoundary(
    const ocpn::render::NauticalRenderModel& model,
    const ocpn::render::PortableNauticalPackage& package) {
  if (MetadataValue(model.metadata, "source_contract") !=
          "portable_nautical_package_to_s52_presentation" ||
      MetadataValue(model.metadata, "package_id") !=
          "s57:US5CONVERT2:package" ||
      MetadataValue(model.metadata, "package_profile") != "s57-s52-poc" ||
      MetadataValue(model.metadata, "package_checksum") !=
          package.checksums.package_hash ||
      MetadataValue(model.metadata, "source_standard") != "S-57" ||
      MetadataValue(model.metadata, "semantic_tier") !=
          "tier1_official_chart" ||
      MetadataValue(model.metadata, "package_valid") != "true") {
    std::cerr << "Package presentation metadata lost source identity or "
                 "Tier 1 ownership\n";
    return false;
  }
  if (model.source_epoch.find(package.manifest.source_epoch) ==
          std::string::npos ||
      model.source_epoch.find(package.checksums.package_hash) ==
          std::string::npos) {
    std::cerr << "Package presentation source epoch does not include package "
                 "identity\n";
    return false;
  }

  int last_order = -1;
  for (const ocpn::render::NauticalLayer& layer : model.layers) {
    if (layer.draw_order < last_order || !PrimitivesSortedByKey(layer)) {
      std::cerr << "Package presentation layers or primitives are not "
                   "deterministically ordered\n";
      return false;
    }
    last_order = layer.draw_order;
  }

  const ocpn::render::NauticalPrimitive* area =
      FindPrimitiveBySourceAndRole(model, "DEPARE.1001", "depth_area");
  const ocpn::render::NauticalPrimitive* contour =
      FindPrimitiveBySourceAndRole(model, "DEPCNT.2001", "safety_contour");
  const ocpn::render::NauticalPrimitive* buoy =
      FindPrimitiveBySourceAndRole(model, "BOYLAT.3001", "navaid_symbol");
  if (!area || !contour || !buoy) {
    std::cerr << "Package fixture did not compile area, contour, and symbol "
                 "primitives\n";
    return false;
  }

  for (const ocpn::render::NauticalPrimitive* primitive :
       {area, contour, buoy}) {
    if (primitive->trace.source_chart_id != "s57:US5CONVERT2" ||
        primitive->trace.source_object_class.empty() ||
        primitive->trace.presentation_rule_id.rfind("s52:", 0) != 0 ||
        primitive->cache_key.scene_key.find(package.checksums.package_hash) ==
            std::string::npos ||
        primitive->handoff.backend_contract != "draw_only" ||
        primitive->handoff.semantic_owner != "presentation_compiler" ||
        MetadataValue(primitive->metadata, "source_standard") != "S-57" ||
        MetadataValue(primitive->metadata, "semantic_tier") !=
            "tier1_official_chart" ||
        MetadataValue(primitive->metadata, "palette") != "day" ||
        primitive->lod.min_scale_denom <= 0.0 ||
        primitive->lod.max_scale_denom <= 0.0 ||
        primitive->lod.display_category != "standard") {
      std::cerr << "Package primitive lost provenance, cache, display, or "
                   "handoff metadata\n";
      return false;
    }
  }

  if (area->fill_ref != "s52-depth-shallow-fill-day" ||
      MetadataValue(area->metadata, "safety_class") != "shallow" ||
      contour->line_style_ref != "s52-safety-contour-line-day" ||
      MetadataValue(contour->metadata, "safety_contour") != "true" ||
      buoy->symbol_ref != "s52-lateral-buoy-symbol-day") {
    std::cerr << "Package fixture did not resolve palette or safety "
                 "presentation before backend handoff\n";
    return false;
  }

  if (!HasDiagnosticWithProvenance(model, "s57.unsupported_object_class")) {
    std::cerr << "Package fixture lost unsupported-source diagnostic "
                 "provenance\n";
    return false;
  }
  return true;
}

bool Validate(const ocpn::render::NauticalRenderModel& model) {
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateNauticalRenderModel(model, &diagnostics)) {
    return true;
  }
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
  return false;
}

}  // namespace

int main() {
  ocpn::render::RenderView view;
  view.view_id = "s52-presentation-smoke";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 20000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.display_category = ocpn::render::DisplayCategory::kStandard;
  display.palette = ocpn::render::Palette::kDay;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  const ocpn::render::ChartSourceProduct product = BuildFixtureProduct();
  const ocpn::render::NauticalRenderModel model =
      ocpn::render::s52::CompileS52Presentation(product, view, display);

  if (!Validate(model) || !CheckBoundary(model)) {
    return 1;
  }

  std::set<ocpn::render::NauticalPrimitiveType> types;
  AddTypes(model, &types);
  for (const ocpn::render::NauticalPrimitiveType required :
       {ocpn::render::NauticalPrimitiveType::kAreaFill,
        ocpn::render::NauticalPrimitiveType::kContourLine,
        ocpn::render::NauticalPrimitiveType::kSymbolInstance,
        ocpn::render::NauticalPrimitiveType::kTextLabel,
        ocpn::render::NauticalPrimitiveType::kSounding}) {
    if (types.count(required) == 0) {
      std::cerr << "Missing compiled primitive type "
                << ocpn::render::ToString(required) << "\n";
      return 1;
    }
  }

  if (!HasDiagnostic(model, "s52.display_category_filtered") ||
      !HasDiagnostic(model, "s52.scamin_filtered")) {
    std::cerr << "Missing display category or SCAMIN filter diagnostic\n";
    return 1;
  }

  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(ocpn::render::BuildS57ConverterFixtureCell());
  std::vector<ocpn::render::Diagnostic> package_diagnostics;
  if (!ocpn::render::ValidateS57ConverterFixturePackage(
          package, &package_diagnostics)) {
    std::cerr << "S-57 portable package fixture failed validation before "
                 "presentation compile\n";
    for (const ocpn::render::Diagnostic& diagnostic : package_diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  ocpn::render::RenderView package_view = view;
  package_view.view_id = "s57-package-presentation-smoke";
  package_view.scale_denom = 5000.0;
  const ocpn::render::NauticalRenderModel package_model =
      ocpn::render::s52::CompileS52PackagePresentation(package, package_view,
                                                       display);
  if (!Validate(package_model) ||
      !CheckPackageBoundary(package_model, package)) {
    return 1;
  }

  ocpn::render::vsg::VsgBackend backend;
  ocpn::render::RenderTarget target;
  target.kind = ocpn::render::RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.target_id = "s52-presentation-smoke";
  const ocpn::render::RenderResult result = backend.RenderModel(model, target);
  if (result.pixels.rgba8.size() != 256U * 256U * 4U ||
      result.diagnostics.empty()) {
    std::cerr << "VSG backend did not accept compiled neutral model\n";
    return 1;
  }

  return 0;
}
