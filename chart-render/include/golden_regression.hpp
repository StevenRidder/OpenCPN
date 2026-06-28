// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "depth_performance.hpp"

#include <cstdint>
#include <vector>

namespace ocpn::render::qa {

struct GoldenRegressionReport {
  std::uint32_t chart1_case_count = 0;
  std::uint32_t pending_baseline_count = 0;
  depth::DepthPerformanceSample depth_sample;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

GoldenRegressionReport RunGoldenRegressionSmoke();

bool ValidateGoldenRegressionReport(const GoldenRegressionReport& report,
                                    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::qa
