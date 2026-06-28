// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "depth_performance.hpp"

#include <iostream>

int main() {
  ocpn::render::depth::DepthPerformanceOptions options;
  options.iterations = 8;

  const ocpn::render::depth::DepthPerformanceSample sample =
      ocpn::render::depth::RunDepthPerformanceSmoke(options);

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::depth::ValidateDepthPerformanceSample(options, sample,
                                                           &diagnostics)) {
    std::cerr << "Depth performance smoke failed\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  std::cout << "depth_perf iterations=" << sample.iterations
            << " commands=" << sample.command_count
            << " pixel_bytes=" << sample.pixel_bytes
            << " diagnostics=" << sample.diagnostic_count
            << " elapsed_ms=" << sample.elapsed_ms << "\n";
  return 0;
}
