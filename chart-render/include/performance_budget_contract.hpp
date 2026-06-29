// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kPerformanceBudgetSchemaVersion = 1;

enum class PerformanceDeviceClass {
  kDesktopReference,
  kBoatLowPower
};

enum class PerformanceMetric {
  kPackageLoadMs,
  kPresentationCompileMs,
  kGpuArtifactCompileMs,
  kGpuArtifactCacheHitMs,
  kBackendRenderMs,
  kResidentMemoryBytes,
  kDiskCacheBytes,
  kActivePowerWatts,
  kIdlePowerWatts,
  kViewportScheduleMs
};

struct PerformanceDeviceProfile {
  std::string profile_id;
  PerformanceDeviceClass device_class =
      PerformanceDeviceClass::kDesktopReference;
  std::string target_name;
  double target_frame_rate_hz = 60.0;
  std::uint32_t max_prefetch_tiles = 64;
  std::uint32_t power_save_prefetch_tiles = 16;
  bool low_power_mode_required = false;
  bool require_cache_hit_for_sustained_pan = true;
};

struct PerformanceBudgetTarget {
  std::string profile_id;
  PerformanceMetric metric = PerformanceMetric::kPackageLoadMs;
  double max_value = 0.0;
  std::string unit;
  std::string pipeline_stage;
  std::string owner;
  bool hard_gate = true;
};

struct ProductionPerformanceBudget {
  std::uint32_t schema_version = kPerformanceBudgetSchemaVersion;
  std::string budget_id = "first-production-slice-budget-v1";
  std::string package_contract = "portable-nautical-package";
  std::string presentation_contract = "backend-neutral-nautical-render-model";
  std::string artifact_contract = "machine-local-gpu-artifacts";
  std::string backend_contract = "draw-only-backend";
  std::string scheduler_contract = "adapter-viewport-cache-v1";
  std::vector<PerformanceDeviceProfile> profiles;
  std::vector<PerformanceBudgetTarget> targets;
  std::vector<Diagnostic> diagnostics;
};

struct PerformanceMeasurement {
  std::string profile_id;
  PerformanceMetric metric = PerformanceMetric::kPackageLoadMs;
  double value = 0.0;
  std::string unit;
  std::string evidence_id;
};

struct PerformanceBudgetEvaluation {
  std::string budget_id;
  std::string profile_id;
  bool ok = false;
  std::vector<std::string> exceeded_metrics;
  std::vector<Diagnostic> diagnostics;
};

const char* ToString(PerformanceDeviceClass device_class);
const char* ToString(PerformanceMetric metric);
const char* ExpectedUnit(PerformanceMetric metric);

ProductionPerformanceBudget BuildFirstProductionPerformanceBudget();

bool ValidatePerformanceBudget(
    const ProductionPerformanceBudget& budget,
    std::vector<Diagnostic>* diagnostics);

PerformanceBudgetEvaluation EvaluatePerformanceBudget(
    const ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    const std::vector<PerformanceMeasurement>& measurements);

}  // namespace ocpn::render
