// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "production_performance_fixture.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasDiagnostic(const std::vector<ocpn::render::Diagnostic>& diagnostics,
                   const std::string& code) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

bool HasError(const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.severity == ocpn::render::DiagnosticSeverity::kError) {
      return true;
    }
  }
  return false;
}

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

const ocpn::render::TimedPerformanceSample* RequireSample(
    const ocpn::render::ProductionFixturePerformanceEvidence& evidence,
    ocpn::render::PerformanceMetric metric) {
  const ocpn::render::TimedPerformanceSample* sample =
      ocpn::render::FindPerformanceSample(evidence, metric);
  if (!sample) {
    std::cerr << "Missing PERF-2 sample " << ocpn::render::ToString(metric)
              << "\n";
  }
  return sample;
}

bool RequireMeasured(
    const ocpn::render::ProductionFixturePerformanceEvidence& evidence,
    ocpn::render::PerformanceMetric metric) {
  const ocpn::render::TimedPerformanceSample* sample =
      RequireSample(evidence, metric);
  if (!sample) return false;
  if (sample->status != ocpn::render::PerformanceEvidenceStatus::kMeasured ||
      sample->iterations == 0U || sample->unit != "ms" ||
      sample->evidence_id.empty()) {
    std::cerr << "PERF-2 sample was not a measured timing: "
              << ocpn::render::ToString(metric) << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  const ocpn::render::ProductionPerformanceBudget budget =
      ocpn::render::BuildFirstProductionPerformanceBudget();
  const ocpn::render::ProductionFixturePerformanceEvidence evidence =
      ocpn::render::MeasureProductionFixturePerformance(8);

  if (!evidence.ok || HasError(evidence.diagnostics)) {
    std::cerr << "PERF-2 production fixture evidence failed\n";
    PrintDiagnostics(evidence.diagnostics);
    return 1;
  }
  if (!HasDiagnostic(evidence.diagnostics,
                     "production_performance_power_unavailable")) {
    std::cerr << "PERF-2 evidence failed to state that power telemetry is "
                 "unavailable on this host\n";
    PrintDiagnostics(evidence.diagnostics);
    return 1;
  }
  if (evidence.package_hash != "fnv1a64:e2c66b175a87d42b" ||
      evidence.golden_image_hash != "009410097424697d" ||
      evidence.primitive_count != 3U || evidence.gpu_artifact_count != 13U ||
      evidence.inspection_row_count != 3U || evidence.package_bytes == 0U ||
      evidence.gpu_artifact_bytes == 0U ||
      evidence.golden_image_bytes != 256U * 256U * 4U ||
      !evidence.package_round_trip_stable ||
      !evidence.gpu_cache_repeat_stable ||
      !evidence.render_repeat_stable) {
    std::cerr << "PERF-2 evidence did not measure the QA-5 production slice\n";
    return 1;
  }

  for (ocpn::render::PerformanceMetric metric :
       {ocpn::render::PerformanceMetric::kPackageLoadMs,
        ocpn::render::PerformanceMetric::kPresentationCompileMs,
        ocpn::render::PerformanceMetric::kGpuArtifactCompileMs,
        ocpn::render::PerformanceMetric::kGpuArtifactCacheHitMs,
        ocpn::render::PerformanceMetric::kBackendRenderMs,
        ocpn::render::PerformanceMetric::kViewportScheduleMs}) {
    if (!RequireMeasured(evidence, metric)) return 1;
  }

  const ocpn::render::TimedPerformanceSample* memory =
      RequireSample(evidence,
                    ocpn::render::PerformanceMetric::kResidentMemoryBytes);
  const ocpn::render::TimedPerformanceSample* disk =
      RequireSample(evidence, ocpn::render::PerformanceMetric::kDiskCacheBytes);
  const ocpn::render::TimedPerformanceSample* active_power =
      RequireSample(evidence, ocpn::render::PerformanceMetric::kActivePowerWatts);
  const ocpn::render::TimedPerformanceSample* idle_power =
      RequireSample(evidence, ocpn::render::PerformanceMetric::kIdlePowerWatts);
  if (!memory || !disk || !active_power || !idle_power ||
      memory->status != ocpn::render::PerformanceEvidenceStatus::kDerived ||
      disk->status != ocpn::render::PerformanceEvidenceStatus::kDerived ||
      active_power->status !=
          ocpn::render::PerformanceEvidenceStatus::kUnavailable ||
      idle_power->status !=
          ocpn::render::PerformanceEvidenceStatus::kUnavailable) {
    std::cerr << "PERF-2 memory/disk/power statuses are wrong\n";
    return 1;
  }

  ocpn::render::ProductionFixturePerformanceEvidence unstable = evidence;
  unstable.render_repeat_stable = false;
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateProductionFixturePerformanceEvidence(
          budget, unstable, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_performance_repeat_stability")) {
    std::cerr << "PERF-2 accepted unstable repeat-render evidence\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionFixturePerformanceEvidence over_budget = evidence;
  for (ocpn::render::TimedPerformanceSample& sample : over_budget.samples) {
    if (sample.metric == ocpn::render::PerformanceMetric::kBackendRenderMs) {
      sample.value = 999.0;
      sample.max_value = 999.0;
    }
  }
  diagnostics.clear();
  if (ocpn::render::ValidateProductionFixturePerformanceEvidence(
          budget, over_budget, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_performance_budget_exceeded")) {
    std::cerr << "PERF-2 accepted a backend render budget overrun\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionFixturePerformanceEvidence missing = evidence;
  missing.samples.erase(
      std::remove_if(missing.samples.begin(), missing.samples.end(),
                     [](const ocpn::render::TimedPerformanceSample& sample) {
                       return sample.metric ==
                              ocpn::render::PerformanceMetric::
                                  kGpuArtifactCacheHitMs;
                     }),
      missing.samples.end());
  diagnostics.clear();
  if (ocpn::render::ValidateProductionFixturePerformanceEvidence(
          budget, missing, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_performance_missing_metric")) {
    std::cerr << "PERF-2 accepted missing cache-hit evidence\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  std::cout << "ok production-performance-fixture: package_load_ms="
            << RequireSample(evidence,
                             ocpn::render::PerformanceMetric::kPackageLoadMs)
                   ->value
            << " presentation_ms="
            << RequireSample(
                   evidence,
                   ocpn::render::PerformanceMetric::kPresentationCompileMs)
                   ->value
            << " artifact_compile_ms="
            << RequireSample(
                   evidence,
                   ocpn::render::PerformanceMetric::kGpuArtifactCompileMs)
                   ->value
            << " cache_hit_ms="
            << RequireSample(
                   evidence,
                   ocpn::render::PerformanceMetric::kGpuArtifactCacheHitMs)
                   ->value
            << " render_ms="
            << RequireSample(evidence,
                             ocpn::render::PerformanceMetric::kBackendRenderMs)
                   ->value
            << " viewport_ms="
            << RequireSample(
                   evidence,
                   ocpn::render::PerformanceMetric::kViewportScheduleMs)
                   ->value
            << " resident_bytes=" << memory->value
            << " disk_bytes=" << disk->value
            << " image_hash=" << evidence.golden_image_hash << "\n";
  return 0;
}
