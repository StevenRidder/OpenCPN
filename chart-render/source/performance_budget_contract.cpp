// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "performance_budget_contract.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Keep production-slice performance budgets measurable across package "
      "load, presentation, GPU artifact cache, draw backend, viewport "
      "scheduler, memory, disk cache, and boat-class power behavior.";
  return diagnostic;
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::kError;
                     });
}

std::vector<PerformanceMetric> RequiredMetrics() {
  return {PerformanceMetric::kPackageLoadMs,
          PerformanceMetric::kPresentationCompileMs,
          PerformanceMetric::kGpuArtifactCompileMs,
          PerformanceMetric::kGpuArtifactCacheHitMs,
          PerformanceMetric::kBackendRenderMs,
          PerformanceMetric::kResidentMemoryBytes,
          PerformanceMetric::kDiskCacheBytes,
          PerformanceMetric::kActivePowerWatts,
          PerformanceMetric::kIdlePowerWatts,
          PerformanceMetric::kViewportScheduleMs};
}

const PerformanceDeviceProfile* FindProfile(
    const ProductionPerformanceBudget& budget,
    const std::string& profile_id) {
  const auto found =
      std::find_if(budget.profiles.begin(), budget.profiles.end(),
                   [&](const PerformanceDeviceProfile& profile) {
                     return profile.profile_id == profile_id;
                   });
  return found == budget.profiles.end() ? nullptr : &*found;
}

const PerformanceBudgetTarget* FindTarget(
    const ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    PerformanceMetric metric) {
  const auto found =
      std::find_if(budget.targets.begin(), budget.targets.end(),
                   [&](const PerformanceBudgetTarget& target) {
                     return target.profile_id == profile_id &&
                            target.metric == metric;
                   });
  return found == budget.targets.end() ? nullptr : &*found;
}

void AddTarget(ProductionPerformanceBudget* budget, std::string profile_id,
               PerformanceMetric metric, double max_value,
               std::string stage, std::string owner) {
  PerformanceBudgetTarget target;
  target.profile_id = std::move(profile_id);
  target.metric = metric;
  target.max_value = max_value;
  target.unit = ExpectedUnit(metric);
  target.pipeline_stage = std::move(stage);
  target.owner = std::move(owner);
  budget->targets.push_back(std::move(target));
}

}  // namespace

const char* ToString(PerformanceDeviceClass device_class) {
  switch (device_class) {
    case PerformanceDeviceClass::kDesktopReference:
      return "desktop_reference";
    case PerformanceDeviceClass::kBoatLowPower:
      return "boat_low_power";
  }
  return "unknown";
}

const char* ToString(PerformanceMetric metric) {
  switch (metric) {
    case PerformanceMetric::kPackageLoadMs:
      return "package_load_ms";
    case PerformanceMetric::kPresentationCompileMs:
      return "presentation_compile_ms";
    case PerformanceMetric::kGpuArtifactCompileMs:
      return "gpu_artifact_compile_ms";
    case PerformanceMetric::kGpuArtifactCacheHitMs:
      return "gpu_artifact_cache_hit_ms";
    case PerformanceMetric::kBackendRenderMs:
      return "backend_render_ms";
    case PerformanceMetric::kResidentMemoryBytes:
      return "resident_memory_bytes";
    case PerformanceMetric::kDiskCacheBytes:
      return "disk_cache_bytes";
    case PerformanceMetric::kActivePowerWatts:
      return "active_power_watts";
    case PerformanceMetric::kIdlePowerWatts:
      return "idle_power_watts";
    case PerformanceMetric::kViewportScheduleMs:
      return "viewport_schedule_ms";
  }
  return "unknown";
}

const char* ExpectedUnit(PerformanceMetric metric) {
  switch (metric) {
    case PerformanceMetric::kPackageLoadMs:
    case PerformanceMetric::kPresentationCompileMs:
    case PerformanceMetric::kGpuArtifactCompileMs:
    case PerformanceMetric::kGpuArtifactCacheHitMs:
    case PerformanceMetric::kBackendRenderMs:
    case PerformanceMetric::kViewportScheduleMs:
      return "ms";
    case PerformanceMetric::kResidentMemoryBytes:
    case PerformanceMetric::kDiskCacheBytes:
      return "bytes";
    case PerformanceMetric::kActivePowerWatts:
    case PerformanceMetric::kIdlePowerWatts:
      return "watts";
  }
  return "";
}

ProductionPerformanceBudget BuildFirstProductionPerformanceBudget() {
  ProductionPerformanceBudget budget;

  PerformanceDeviceProfile desktop;
  desktop.profile_id = "desktop_reference";
  desktop.device_class = PerformanceDeviceClass::kDesktopReference;
  desktop.target_name = "desktop-vsg-or-webgpu-reference";
  desktop.target_frame_rate_hz = 60.0;
  desktop.max_prefetch_tiles = 64;
  desktop.power_save_prefetch_tiles = 16;
  desktop.low_power_mode_required = false;
  desktop.require_cache_hit_for_sustained_pan = true;
  budget.profiles.push_back(desktop);

  PerformanceDeviceProfile boat;
  boat.profile_id = "boat_low_power";
  boat.device_class = PerformanceDeviceClass::kBoatLowPower;
  boat.target_name = "fanless-boat-class-gpu";
  boat.target_frame_rate_hz = 15.0;
  boat.max_prefetch_tiles = 16;
  boat.power_save_prefetch_tiles = 4;
  boat.low_power_mode_required = true;
  boat.require_cache_hit_for_sustained_pan = true;
  budget.profiles.push_back(boat);

  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kPackageLoadMs,
            50.0, "portable_package_load", "package_runtime");
  AddTarget(&budget, desktop.profile_id,
            PerformanceMetric::kPresentationCompileMs, 30.0,
            "presentation_compile", "presentation_compiler");
  AddTarget(&budget, desktop.profile_id,
            PerformanceMetric::kGpuArtifactCompileMs, 50.0,
            "gpu_artifact_compile", "runtime_gpu_artifact_cache");
  AddTarget(&budget, desktop.profile_id,
            PerformanceMetric::kGpuArtifactCacheHitMs, 5.0,
            "gpu_artifact_cache_hit", "runtime_gpu_artifact_cache");
  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kBackendRenderMs,
            16.7, "draw_backend_render", "draw_backend");
  AddTarget(&budget, desktop.profile_id,
            PerformanceMetric::kResidentMemoryBytes,
            512.0 * 1024.0 * 1024.0, "runtime_memory",
            "runtime_memory_budget");
  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kDiskCacheBytes,
            256.0 * 1024.0 * 1024.0, "disk_cache",
            "runtime_disk_cache");
  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kActivePowerWatts,
            45.0, "active_power", "device_power_profile");
  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kIdlePowerWatts,
            12.0, "idle_power", "device_power_profile");
  AddTarget(&budget, desktop.profile_id, PerformanceMetric::kViewportScheduleMs,
            5.0, "viewport_schedule", "adapter_scheduler");

  AddTarget(&budget, boat.profile_id, PerformanceMetric::kPackageLoadMs,
            150.0, "portable_package_load", "package_runtime");
  AddTarget(&budget, boat.profile_id,
            PerformanceMetric::kPresentationCompileMs, 120.0,
            "presentation_compile", "presentation_compiler");
  AddTarget(&budget, boat.profile_id,
            PerformanceMetric::kGpuArtifactCompileMs, 250.0,
            "gpu_artifact_compile", "runtime_gpu_artifact_cache");
  AddTarget(&budget, boat.profile_id,
            PerformanceMetric::kGpuArtifactCacheHitMs, 20.0,
            "gpu_artifact_cache_hit", "runtime_gpu_artifact_cache");
  AddTarget(&budget, boat.profile_id, PerformanceMetric::kBackendRenderMs,
            66.7, "draw_backend_render", "draw_backend");
  AddTarget(&budget, boat.profile_id,
            PerformanceMetric::kResidentMemoryBytes,
            256.0 * 1024.0 * 1024.0, "runtime_memory",
            "runtime_memory_budget");
  AddTarget(&budget, boat.profile_id, PerformanceMetric::kDiskCacheBytes,
            128.0 * 1024.0 * 1024.0, "disk_cache",
            "runtime_disk_cache");
  AddTarget(&budget, boat.profile_id, PerformanceMetric::kActivePowerWatts,
            10.0, "active_power", "device_power_profile");
  AddTarget(&budget, boat.profile_id, PerformanceMetric::kIdlePowerWatts,
            3.0, "idle_power", "device_power_profile");
  AddTarget(&budget, boat.profile_id, PerformanceMetric::kViewportScheduleMs,
            15.0, "viewport_schedule", "adapter_scheduler");

  return budget;
}

bool ValidatePerformanceBudget(const ProductionPerformanceBudget& budget,
                               std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (budget.schema_version != kPerformanceBudgetSchemaVersion ||
      budget.budget_id.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "performance_budget_identity",
                                 "Performance budget has invalid identity."));
    ok = false;
  }
  if (budget.package_contract.empty() || budget.presentation_contract.empty() ||
      budget.artifact_contract.empty() || budget.backend_contract.empty() ||
      budget.scheduler_contract.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_contracts",
        "Performance budget must name package, presentation, artifact, "
        "backend, and scheduler contracts."));
    ok = false;
  }

  bool saw_desktop = false;
  bool saw_low_power = false;
  std::set<std::string> profile_ids;
  for (const PerformanceDeviceProfile& profile : budget.profiles) {
    if (profile.profile_id.empty() ||
        !profile_ids.insert(profile.profile_id).second ||
        profile.target_name.empty() || profile.target_frame_rate_hz <= 0.0 ||
        profile.max_prefetch_tiles == 0 ||
        profile.power_save_prefetch_tiles == 0 ||
        profile.power_save_prefetch_tiles > profile.max_prefetch_tiles) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "performance_budget_profile",
          "Performance device profile is incomplete or has invalid prefetch "
          "limits."));
      ok = false;
    }
    if (profile.device_class == PerformanceDeviceClass::kDesktopReference) {
      saw_desktop = true;
    }
    if (profile.device_class == PerformanceDeviceClass::kBoatLowPower) {
      saw_low_power = true;
      if (!profile.low_power_mode_required ||
          profile.max_prefetch_tiles > 16 ||
          !profile.require_cache_hit_for_sustained_pan) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "performance_budget_low_power_policy",
            "Boat-class profile must require low-power mode, bounded prefetch, "
            "and cache hits for sustained pan/zoom."));
        ok = false;
      }
    }
  }
  if (!saw_desktop || !saw_low_power) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_device_coverage",
        "Performance budget must include desktop and boat-class profiles."));
    ok = false;
  }

  std::set<std::string> target_keys;
  for (const PerformanceBudgetTarget& target : budget.targets) {
    const std::string key =
        target.profile_id + ":" + std::string(ToString(target.metric));
    if (FindProfile(budget, target.profile_id) == nullptr ||
        target.max_value <= 0.0 ||
        target.unit != ExpectedUnit(target.metric) ||
        target.pipeline_stage.empty() || target.owner.empty() ||
        !target_keys.insert(key).second) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "performance_budget_target",
          "Performance budget target is incomplete, duplicated, or uses the "
          "wrong unit."));
      ok = false;
    }
  }

  for (const PerformanceDeviceProfile& profile : budget.profiles) {
    for (PerformanceMetric metric : RequiredMetrics()) {
      if (FindTarget(budget, profile.profile_id, metric) == nullptr) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError,
            "performance_budget_required_metric",
            "Performance budget is missing a required metric target for " +
                profile.profile_id + "."));
        ok = false;
      }
    }
  }

  const PerformanceBudgetTarget* desktop_memory =
      FindTarget(budget, "desktop_reference",
                 PerformanceMetric::kResidentMemoryBytes);
  const PerformanceBudgetTarget* boat_memory =
      FindTarget(budget, "boat_low_power",
                 PerformanceMetric::kResidentMemoryBytes);
  const PerformanceBudgetTarget* desktop_disk =
      FindTarget(budget, "desktop_reference", PerformanceMetric::kDiskCacheBytes);
  const PerformanceBudgetTarget* boat_disk =
      FindTarget(budget, "boat_low_power", PerformanceMetric::kDiskCacheBytes);
  const PerformanceBudgetTarget* desktop_active =
      FindTarget(budget, "desktop_reference",
                 PerformanceMetric::kActivePowerWatts);
  const PerformanceBudgetTarget* boat_active =
      FindTarget(budget, "boat_low_power", PerformanceMetric::kActivePowerWatts);
  const PerformanceBudgetTarget* desktop_idle =
      FindTarget(budget, "desktop_reference", PerformanceMetric::kIdlePowerWatts);
  const PerformanceBudgetTarget* boat_idle =
      FindTarget(budget, "boat_low_power", PerformanceMetric::kIdlePowerWatts);
  if (desktop_memory && boat_memory &&
      boat_memory->max_value > desktop_memory->max_value) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_memory_order",
        "Boat-class resident memory budget must not exceed desktop budget."));
    ok = false;
  }
  if (desktop_disk && boat_disk && boat_disk->max_value > desktop_disk->max_value) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_disk_order",
        "Boat-class disk cache budget must not exceed desktop budget."));
    ok = false;
  }
  if (desktop_active && boat_active &&
      boat_active->max_value >= desktop_active->max_value) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_power_order",
        "Boat-class active power budget must be below desktop power budget."));
    ok = false;
  }
  if (desktop_idle && boat_idle &&
      boat_idle->max_value >= desktop_idle->max_value) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_power_order",
        "Boat-class idle power budget must be below desktop idle power budget."));
    ok = false;
  }

  return ok;
}

PerformanceBudgetEvaluation EvaluatePerformanceBudget(
    const ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    const std::vector<PerformanceMeasurement>& measurements) {
  PerformanceBudgetEvaluation evaluation;
  evaluation.budget_id = budget.budget_id;
  evaluation.profile_id = profile_id;

  ValidatePerformanceBudget(budget, &evaluation.diagnostics);
  const PerformanceDeviceProfile* profile = FindProfile(budget, profile_id);
  if (!profile) {
    evaluation.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "performance_budget_unknown_profile",
        "Performance measurements reference an unknown device profile."));
    evaluation.ok = false;
    return evaluation;
  }

  std::map<PerformanceMetric, PerformanceMeasurement> by_metric;
  for (const PerformanceMeasurement& measurement : measurements) {
    if (measurement.profile_id != profile_id) continue;
    if (measurement.unit != ExpectedUnit(measurement.metric) ||
        measurement.value < 0.0 || measurement.evidence_id.empty()) {
      evaluation.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "performance_budget_bad_measurement",
          "Performance measurement has invalid unit, value, or evidence id."));
      continue;
    }
    if (!by_metric.emplace(measurement.metric, measurement).second) {
      evaluation.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "performance_budget_duplicate_measurement",
          "Performance measurement duplicates a metric for one profile."));
    }
  }

  for (const PerformanceBudgetTarget& target : budget.targets) {
    if (target.profile_id != profile->profile_id) continue;
    const auto found = by_metric.find(target.metric);
    if (found == by_metric.end()) {
      if (target.hard_gate) {
        evaluation.diagnostics.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "performance_budget_missing_measurement",
            "Performance evaluation is missing a hard-gate measurement."));
      }
      continue;
    }
    if (found->second.value > target.max_value) {
      evaluation.exceeded_metrics.push_back(ToString(target.metric));
      evaluation.diagnostics.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "performance_budget_exceeded",
          std::string("Performance measurement exceeds budget target: ") +
              ToString(target.metric) + "."));
    }
  }

  evaluation.ok = !HasError(evaluation.diagnostics);
  return evaluation;
}

}  // namespace ocpn::render
