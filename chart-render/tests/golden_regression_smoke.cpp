// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "golden_regression.hpp"

#include <iostream>

int main() {
  const ocpn::render::qa::GoldenRegressionReport report =
      ocpn::render::qa::RunGoldenRegressionSmoke();

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::qa::ValidateGoldenRegressionReport(report,
                                                        &diagnostics)) {
    std::cerr << "Golden regression smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  std::cout << "golden_regression chart1_cases="
            << report.chart1_case_count
            << " pending_baselines=" << report.pending_baseline_count
            << " depth_commands=" << report.depth_sample.command_count
            << " depth_pixel_bytes=" << report.depth_sample.pixel_bytes
            << " depth_elapsed_ms=" << report.depth_sample.elapsed_ms << "\n";
  return 0;
}
