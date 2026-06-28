// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart1_acceptance.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render::chart1 {

inline constexpr std::uint32_t kChart1BaselineSchemaVersion = 1;

struct ImageTolerance {
  std::uint32_t max_changed_pixels = 0;
  std::uint8_t max_channel_delta = 0;
  double max_rms_delta = 0.0;
};

struct BaselineComparisonCase {
  std::string case_id;
  std::string opencpn_baseline_path;
  std::string shared_renderer_path;
  std::string diff_artifact_path;
  ImageTolerance tolerance;
  std::string documented_difference_policy;
  bool baseline_capture_required = true;
};

struct BaselineComparisonManifest {
  std::uint32_t schema_version = kChart1BaselineSchemaVersion;
  std::string manifest_id = "chart-1-baseline-comparison";
  std::vector<BaselineComparisonCase> cases;
};

BaselineComparisonManifest BuildBaselineComparisonManifest();

bool ValidateBaselineComparisonManifest(
    const AcceptanceCatalog& catalog, const BaselineComparisonManifest& manifest,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::chart1
