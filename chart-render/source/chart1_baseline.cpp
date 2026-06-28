// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_baseline.hpp"

#include <set>
#include <utility>

namespace ocpn::render::chart1 {
namespace {

Diagnostic Error(std::string code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Fix the Chart 1 baseline manifest before running image comparison.";
  return diagnostic;
}

BaselineComparisonCase Case(std::string case_id,
                            std::uint32_t max_changed_pixels,
                            std::uint8_t max_channel_delta,
                            double max_rms_delta,
                            std::string documented_difference_policy) {
  BaselineComparisonCase comparison;
  comparison.case_id = std::move(case_id);
  comparison.opencpn_baseline_path =
      "baselines/opencpn/" + comparison.case_id + ".png";
  comparison.shared_renderer_path =
      "outputs/shared/" + comparison.case_id + ".png";
  comparison.diff_artifact_path = "diffs/" + comparison.case_id + ".png";
  comparison.tolerance = {max_changed_pixels, max_channel_delta, max_rms_delta};
  comparison.documented_difference_policy =
      std::move(documented_difference_policy);
  return comparison;
}

}  // namespace

BaselineComparisonManifest BuildBaselineComparisonManifest() {
  BaselineComparisonManifest manifest;
  manifest.cases = {
      Case("chart1-area-depth", 128, 4, 0.75,
           "Minor antialiasing differences at polygon edges are acceptable; "
           "fill category, coverage, and target bounds must match."),
      Case("chart1-line-depth-contour", 96, 6, 0.90,
           "Line joins and caps may differ by antialiasing tolerance; contour "
           "placement and line style identity must match."),
      Case("chart1-point-buoy", 160, 8, 1.20,
           "Symbol antialiasing may differ; symbol identity, anchor, priority, "
           "and source provenance must match."),
  };
  return manifest;
}

bool ValidateBaselineComparisonManifest(
    const AcceptanceCatalog& catalog, const BaselineComparisonManifest& manifest,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (manifest.schema_version != kChart1BaselineSchemaVersion) {
    out.push_back(Error("chart1_baseline_schema",
                        "Unsupported Chart 1 baseline schema version."));
    ok = false;
  }
  if (manifest.manifest_id.empty()) {
    out.push_back(Error("chart1_baseline_manifest_id",
                        "Chart 1 baseline manifest is missing manifest_id."));
    ok = false;
  }

  std::set<std::string> acceptance_case_ids;
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    acceptance_case_ids.insert(acceptance_case.case_id);
  }

  std::set<std::string> manifest_case_ids;
  for (const BaselineComparisonCase& comparison : manifest.cases) {
    if (!manifest_case_ids.insert(comparison.case_id).second) {
      out.push_back(Error("chart1_baseline_duplicate_case",
                          "Chart 1 baseline case id is duplicated."));
      ok = false;
    }
    if (acceptance_case_ids.count(comparison.case_id) == 0) {
      out.push_back(Error("chart1_baseline_unknown_case",
                          "Chart 1 baseline case is not in acceptance catalog."));
      ok = false;
    }
    if (comparison.opencpn_baseline_path.empty() ||
        comparison.shared_renderer_path.empty() ||
        comparison.diff_artifact_path.empty()) {
      out.push_back(Error("chart1_baseline_paths",
                          "Chart 1 baseline case is missing artifact paths."));
      ok = false;
    }
    if (comparison.documented_difference_policy.empty()) {
      out.push_back(Error("chart1_baseline_difference_policy",
                          "Chart 1 baseline case is missing difference policy."));
      ok = false;
    }
  }

  for (const std::string& case_id : acceptance_case_ids) {
    if (manifest_case_ids.count(case_id) == 0) {
      out.push_back(Error("chart1_baseline_missing_case",
                          "Chart 1 acceptance case is missing baseline metadata."));
      ok = false;
    }
  }

  return ok;
}

}  // namespace ocpn::render::chart1
