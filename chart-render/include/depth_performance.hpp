// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_backend.hpp"

#include <cstdint>
#include <vector>

namespace ocpn::render::depth {

struct DepthPerformanceOptions {
  std::uint32_t iterations = 32;
  PixelSize tile_size = {256, 256};
  double scale_denom = 12000.0;
};

struct DepthPerformanceSample {
  std::uint32_t iterations = 0;
  std::uint64_t command_count = 0;
  std::uint64_t diagnostic_count = 0;
  std::uint64_t pixel_bytes = 0;
  std::uint64_t backend_ok_count = 0;
  double elapsed_ms = 0.0;
};

DepthPerformanceSample RunDepthPerformanceSmoke(
    const DepthPerformanceOptions& options);

bool ValidateDepthPerformanceSample(const DepthPerformanceOptions& options,
                                    const DepthPerformanceSample& sample,
                                    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::depth
