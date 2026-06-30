// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "gpu_artifact_cache_contract.hpp"
#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"
#include "source_to_render_inspection.hpp"
#include "vsg/vsg_backend.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

bool ContainsText(const std::vector<std::string>& lines,
                  const std::string& expected) {
  for (const std::string& line : lines) {
    if (line.find(expected) != std::string::npos) return true;
  }
  return false;
}

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

bool ValidateProductionRow(
    const ocpn::render::SourceToRenderInspectionRow& row,
    const std::string& source_object_id,
    const std::string& object_class,
    const std::string& rule_id,
    const std::string& primitive_type,
    const std::string& expected_hash) {
  if (row.source.source_chart_id != "s57:US5CONVERT2" ||
      row.source.source_object_id != source_object_id ||
      row.source.source_object_class != object_class ||
      row.converter.converter_id != "synthetic-s57-portable-converter" ||
      row.converter.source_product_id != "s57:US5CONVERT2:normalized" ||
      row.converter.portable_package_id != "s57:US5CONVERT2:package" ||
      row.converter.normalized_feature_id != source_object_id ||
      row.presentation.presentation_rule_id != rule_id ||
      row.presentation.primitive_type != primitive_type ||
      row.tier.semantic_tier != "tier1_official_chart" ||
      row.tier.semantic_owner != "presentation_compiler" ||
      row.tier.source_standard != "S-57" ||
      row.tier.wrong_location_owner != "converter_or_projection" ||
      row.tier.wrong_symbol_owner != "converter_presentation_or_backend" ||
      row.backend.backend_contract != "draw_only" ||
      row.backend.backend_name != "vulkan-scenegraph-placeholder" ||
      row.backend.backend_resource_id.empty() ||
      row.backend.final_draw_item_id.empty() ||
      row.backend.final_gpu_asset_id.empty() ||
      row.query.object_query_id.empty() ||
      row.query.pixel_query_id.empty() ||
      !row.query.sampled_rendered_pixel ||
      row.query.sample_rgba8.size() != 8U ||
      row.query.rendered_pixel_hash != expected_hash ||
      row.human_trace.size() < 7U) {
    std::cerr << "Production source-to-pixel row is incomplete for "
              << source_object_id << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  ocpn::render::S57PortablePackageConverter converter;
  const ocpn::render::PortableNauticalPackage package =
      converter.Convert(ocpn::render::BuildS57ConverterFixtureCell());
  std::vector<ocpn::render::Diagnostic> package_diagnostics;
  if (!ocpn::render::ValidateS57ConverterFixturePackage(
          package, &package_diagnostics)) {
    std::cerr << "S-57 package fixture failed validation before DEBUG-2\n";
    PrintDiagnostics(package_diagnostics);
    return 1;
  }

  ocpn::render::RenderView view;
  view.view_id = "debug2-s57-source-to-pixel";
  view.projection = ocpn::render::Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 5000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;

  ocpn::render::DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;

  const ocpn::render::NauticalRenderModel model =
      ocpn::render::s52::CompileS52PackagePresentation(package, view, display);
  std::vector<ocpn::render::Diagnostic> model_diagnostics;
  if (!ocpn::render::ValidateNauticalRenderModel(model, &model_diagnostics)) {
    std::cerr << "S-57 package presentation model failed validation before "
                 "DEBUG-2\n";
    PrintDiagnostics(model_diagnostics);
    return 1;
  }

  ocpn::render::GpuArtifactCacheOptions cache_options;
  cache_options.backend_target = "vsg";
  cache_options.device_profile = "vulkan-proof-device";
  cache_options.material_profile = "vsg-neutral-package-v1";
  cache_options.cache_namespace = "opencpn-vsg-production-slice";
  cache_options.memory_budget_bytes = 4ULL * 1024ULL * 1024ULL;
  const ocpn::render::GpuArtifactCacheManifest cache_manifest =
      ocpn::render::BuildGpuArtifactCacheManifest(model, cache_options);
  std::vector<ocpn::render::Diagnostic> cache_diagnostics;
  if (!ocpn::render::ValidateGpuArtifactCacheManifest(cache_manifest,
                                                      &cache_diagnostics)) {
    std::cerr << "S-57 package artifact manifest failed validation before "
                 "DEBUG-2\n";
    PrintDiagnostics(cache_diagnostics);
    return 1;
  }

  ocpn::render::RenderTarget target;
  target.kind = ocpn::render::RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.device_pixel_ratio = 1.0;
  target.target_id = "debug2-offscreen-source-to-pixel";

  ocpn::render::vsg::VsgBackend backend;
  const ocpn::render::RenderResult backend_result =
      backend.RenderModel(model, target);
  if (!backend_result.ok ||
      !HasDiagnostic(backend_result.diagnostics,
                     "backend.vsg_production_fixture")) {
    std::cerr << "VSG backend did not render the DEBUG-2 production fixture\n";
    PrintDiagnostics(backend_result.diagnostics);
    return 1;
  }

  ocpn::render::SourceToRenderInspectionOptions options;
  options.report_id = "debug2-production-source-to-pixel";
  options.source_product_id = package.product.product_id;
  options.converter_id = package.manifest.converter_id;
  options.portable_package_id = package.manifest.package_id;
  options.backend_name = backend.Name();
  options.target = target;
  options.backend_result = &backend_result;
  const ocpn::render::SourceToRenderInspectionReport report =
      ocpn::render::BuildSourceToRenderInspectionReport(model, cache_manifest,
                                                        options);
  if (!report.ok || report.rows.size() != 3U || report.scene_artifacts.empty()) {
    std::cerr << "DEBUG-2 production inspection report is incomplete\n";
    PrintDiagnostics(report.diagnostics);
    return 1;
  }

  const ocpn::render::SourceToRenderInspectionRow* area =
      ocpn::render::FindInspectionByPrimitiveId(
          report, "prim-US5CONVERT2:DEPARE.1001-depth_area");
  const ocpn::render::SourceToRenderInspectionRow* contour =
      ocpn::render::FindInspectionByPrimitiveId(
          report, "prim-US5CONVERT2:DEPCNT.2001-safety_contour");
  const ocpn::render::SourceToRenderInspectionRow* buoy =
      ocpn::render::FindInspectionByPrimitiveId(
          report, "prim-US5CONVERT2:BOYLAT.3001-navaid_symbol");
  if (!area || !contour || !buoy) {
    std::cerr << "DEBUG-2 production inspection missed an expected primitive\n";
    return 1;
  }

  const std::string expected_hash = "009410097424697d";
  if (!ValidateProductionRow(*area, "DEPARE.1001", "DEPARE",
                             "s52:DEPARE:depth_area", "area_fill",
                             expected_hash) ||
      !ValidateProductionRow(*contour, "DEPCNT.2001", "DEPCNT",
                             "s52:DEPCNT:safety_contour", "contour_line",
                             expected_hash) ||
      !ValidateProductionRow(*buoy, "BOYLAT.3001", "BOYLAT",
                             "s52:BOYLAT:navaid_symbol", "symbol_instance",
                             expected_hash)) {
    return 1;
  }

  const std::vector<std::string> trace_lines =
      ocpn::render::BuildHumanReadableSourceToRenderTrace(report);
  if (!ContainsText(trace_lines, "source s57:US5CONVERT2/DEPARE.1001") ||
      !ContainsText(trace_lines,
                    "converter synthetic-s57-portable-converter") ||
      !ContainsText(trace_lines, "package s57:US5CONVERT2:package") ||
      !ContainsText(trace_lines, "presentation s52:BOYLAT:navaid_symbol") ||
      !ContainsText(trace_lines, "pixel pixel-query:") ||
      !ContainsText(trace_lines, "image_hash=009410097424697d") ||
      !ContainsText(trace_lines,
                    "wrong_symbol=converter_presentation_or_backend")) {
    std::cerr << "DEBUG-2 human-readable trace is missing key source-to-pixel "
                 "steps\n";
    for (const std::string& line : trace_lines) {
      std::cerr << line << "\n";
    }
    return 1;
  }

  ocpn::render::SourceToRenderInspectionReport invalid = report;
  invalid.rows.front().query.sample_rgba8.clear();
  std::vector<ocpn::render::Diagnostic> invalid_diagnostics;
  if (ocpn::render::ValidateSourceToRenderInspectionReport(
          invalid, &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "source_to_render_pixel_sample")) {
    std::cerr << "Inspection contract accepted incomplete rendered-pixel "
                 "evidence\n";
    return 1;
  }

  invalid = report;
  invalid.rows.front().human_trace.clear();
  invalid_diagnostics.clear();
  if (ocpn::render::ValidateSourceToRenderInspectionReport(
          invalid, &invalid_diagnostics) ||
      !HasDiagnostic(invalid_diagnostics, "source_to_render_human_trace")) {
    std::cerr << "Inspection contract accepted a row without a human trace\n";
    return 1;
  }

  std::cout << "ok source-to-render-production-fixture: "
            << report.rows.size() << " rows, "
            << report.scene_artifacts.size() << " scene artifacts, hash="
            << area->query.rendered_pixel_hash << " sample="
            << buoy->query.sample_x << "," << buoy->query.sample_y << " "
            << buoy->query.sample_rgba8 << "\n";
  return 0;
}
