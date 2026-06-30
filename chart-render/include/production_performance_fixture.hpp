// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "performance_budget_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kProductionPerformanceEvidenceSchemaVersion = 1;

enum class PerformanceEvidenceStatus {
  kMeasured,
  kDerived,
  kUnavailable
};

struct TimedPerformanceSample {
  PerformanceMetric metric = PerformanceMetric::kPackageLoadMs;
  PerformanceEvidenceStatus status = PerformanceEvidenceStatus::kMeasured;
  double value = 0.0;
  double min_value = 0.0;
  double max_value = 0.0;
  std::uint32_t iterations = 0;
  std::string unit;
  std::string evidence_id;
  std::string note;
};

struct ProductionFixturePerformanceEvidence {
  std::uint32_t schema_version = kProductionPerformanceEvidenceSchemaVersion;
  std::string evidence_id = "perf2-production-fixture";
  std::string host_profile = "desktop_reference";
  std::string package_hash;
  std::string golden_image_hash;
  std::size_t primitive_count = 0;
  std::size_t gpu_artifact_count = 0;
  std::size_t inspection_row_count = 0;
  std::uint64_t package_bytes = 0;
  std::uint64_t gpu_artifact_bytes = 0;
  std::uint64_t golden_image_bytes = 0;
  bool package_round_trip_stable = false;
  bool gpu_cache_repeat_stable = false;
  bool render_repeat_stable = false;
  std::vector<TimedPerformanceSample> samples;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

ProductionFixturePerformanceEvidence MeasureProductionFixturePerformance(
    std::uint32_t iterations = 16);

bool ValidateProductionFixturePerformanceEvidence(
    const ProductionPerformanceBudget& budget,
    const ProductionFixturePerformanceEvidence& evidence,
    std::vector<Diagnostic>* diagnostics);

const TimedPerformanceSample* FindPerformanceSample(
    const ProductionFixturePerformanceEvidence& evidence,
    PerformanceMetric metric);

const char* ToString(PerformanceEvidenceStatus status);

}  // namespace ocpn::render
