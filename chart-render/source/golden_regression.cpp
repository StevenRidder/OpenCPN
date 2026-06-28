// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "golden_regression.hpp"

#include "chart1_baseline.hpp"
#include "chart1_conformance.hpp"

#include <utility>

namespace ocpn::render::qa {
namespace {

Diagnostic Error(std::string code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Fix the golden regression inputs before enabling strict image diff.";
  return diagnostic;
}

RenderView Chart1View() {
  RenderView view;
  view.view_id = "golden-chart-1";
  view.projection = Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.82, 24.45, -81.78, 24.49};
  view.center = {-81.80, 24.47};
  view.scale_denom = 12000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;
  return view;
}

DisplayState ChartDisplay() {
  DisplayState display;
  display.shallow_contour_m = 2.0;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  display.deep_contour_m = 20.0;
  return display;
}

}  // namespace

GoldenRegressionReport RunGoldenRegressionSmoke() {
  GoldenRegressionReport report;

  const chart1::AcceptanceCatalog catalog = chart1::BuildAcceptanceCatalog();
  chart1::ValidateAcceptanceCatalog(catalog, &report.diagnostics);
  report.chart1_case_count = catalog.cases.size();

  const chart1::ConformanceScene conformance =
      chart1::BuildConformanceScene(Chart1View(), ChartDisplay());
  report.diagnostics.insert(report.diagnostics.end(),
                            conformance.diagnostics.begin(),
                            conformance.diagnostics.end());

  const chart1::BaselineComparisonManifest manifest =
      chart1::BuildBaselineComparisonManifest();
  chart1::ValidateBaselineComparisonManifest(catalog, manifest,
                                             &report.diagnostics);
  for (const chart1::BaselineComparisonCase& comparison : manifest.cases) {
    if (comparison.baseline_capture_required) {
      ++report.pending_baseline_count;
    }
  }

  depth::DepthPerformanceOptions depth_options;
  depth_options.iterations = 8;
  report.depth_sample = depth::RunDepthPerformanceSmoke(depth_options);
  depth::ValidateDepthPerformanceSample(depth_options, report.depth_sample,
                                        &report.diagnostics);

  report.ok = report.diagnostics.empty() && conformance.ok;
  return report;
}

bool ValidateGoldenRegressionReport(const GoldenRegressionReport& report,
                                    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = report.ok;
  if (report.chart1_case_count == 0) {
    out.push_back(
        Error("golden_chart1_cases", "Golden regression has no Chart 1 cases."));
    ok = false;
  }
  if (report.depth_sample.command_count == 0 ||
      report.depth_sample.pixel_bytes == 0) {
    out.push_back(Error("golden_depth_sample",
                        "Golden regression has no depth performance sample."));
    ok = false;
  }
  if (!report.diagnostics.empty()) {
    out.insert(out.end(), report.diagnostics.begin(), report.diagnostics.end());
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render::qa
