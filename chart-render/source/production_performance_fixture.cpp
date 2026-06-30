// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "production_performance_fixture.hpp"

#include "gpu_artifact_cache_contract.hpp"
#include "production_golden_corpus.hpp"
#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"
#include "source_to_render_inspection.hpp"
#include "viewport_tile_scheduler.hpp"
#include "vsg/vsg_backend.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Use PERF-2 evidence to tune the C++ renderer path; do not claim "
      "production viability when power or boat-class measurements are "
      "unavailable.";
  return diagnostic;
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::kError;
                     });
}

std::uint64_t Fnva64Bytes(std::uint64_t hash, const std::uint8_t* bytes,
                          std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::uint64_t Fnva64Uint32(std::uint64_t hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const std::uint8_t byte =
        static_cast<std::uint8_t>((value >> shift) & 0xffU);
    hash = Fnva64Bytes(hash, &byte, 1U);
  }
  return hash;
}

std::string HexHash(std::uint64_t hash) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

std::string PixelHash(const PixelBuffer& pixels) {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = Fnva64Uint32(hash, pixels.pixel_size.width);
  hash = Fnva64Uint32(hash, pixels.pixel_size.height);
  if (!pixels.rgba8.empty()) {
    hash = Fnva64Bytes(hash, pixels.rgba8.data(), pixels.rgba8.size());
  }
  return HexHash(hash);
}

RenderView ProductionView() {
  RenderView view;
  view.view_id = "perf2-production-fixture";
  view.projection = Projection::kWebMercatorTile;
  view.geographic_bbox = {-81.86, 24.42, -81.74, 24.53};
  view.center = {-81.80, 24.47};
  view.scale_denom = 5000.0;
  view.pixel_size = {256, 256};
  view.overscan_px = 16;
  return view;
}

DisplayState ProductionDisplay() {
  DisplayState display;
  display.safety_depth_m = 5.0;
  display.safety_contour_m = 10.0;
  return display;
}

GpuArtifactCacheOptions ProductionCacheOptions() {
  GpuArtifactCacheOptions options;
  options.backend_target = "vsg";
  options.device_profile = "vulkan-proof-device";
  options.material_profile = "vsg-neutral-package-v1";
  options.cache_namespace = "opencpn-vsg-production-slice";
  options.memory_budget_bytes = 4ULL * 1024ULL * 1024ULL;
  return options;
}

RenderTarget ProductionTarget() {
  RenderTarget target;
  target.kind = RenderTargetKind::kOffscreen;
  target.pixel_size = {256, 256};
  target.device_pixel_ratio = 1.0;
  target.target_id = "perf2-offscreen-render";
  return target;
}

ViewportTileSchedulerInput ProductionSchedulerInput(const RenderView& view) {
  ViewportTileSchedulerInput input;
  input.render_view = view;
  input.center_tile = {8, 125, 90};
  input.fractional_zoom = 0.6;
  input.epoch.chart_epoch = "perf2-production-fixture";
  input.epoch.presentation_epoch = "s52-production-fixture";
  input.epoch.display_epoch = "day-standard";
  input.epoch.scheduler_epoch = "perf2";
  input.epoch.source_group_id = "perf2";
  return input;
}

std::size_t PrimitiveCount(const NauticalRenderModel& model) {
  std::size_t count = 0;
  for (const NauticalLayer& layer : model.layers) {
    count += layer.primitives.size();
  }
  return count;
}

std::vector<std::string> ArtifactIds(
    const GpuArtifactCacheManifest& manifest) {
  std::vector<std::string> ids;
  ids.reserve(manifest.artifacts.size());
  for (const GpuArtifactRecord& artifact : manifest.artifacts) {
    ids.push_back(artifact.artifact_id);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

double Mean(const std::vector<double>& values) {
  if (values.empty()) return 0.0;
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

TimedPerformanceSample MakeSample(PerformanceMetric metric,
                                  PerformanceEvidenceStatus status,
                                  double value, std::uint32_t iterations,
                                  std::string evidence_id,
                                  std::string note = {}) {
  TimedPerformanceSample sample;
  sample.metric = metric;
  sample.status = status;
  sample.value = value;
  sample.min_value = value;
  sample.max_value = value;
  sample.iterations = iterations;
  sample.unit = ExpectedUnit(metric);
  sample.evidence_id = std::move(evidence_id);
  sample.note = std::move(note);
  return sample;
}

template <typename Callable>
TimedPerformanceSample MeasureTimed(PerformanceMetric metric,
                                    std::uint32_t iterations,
                                    std::string evidence_id,
                                    Callable callable,
                                    std::string note = {}) {
  std::vector<double> elapsed_ms;
  elapsed_ms.reserve(iterations);
  for (std::uint32_t i = 0; i < iterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    callable();
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> duration = end - start;
    elapsed_ms.push_back(duration.count());
  }

  TimedPerformanceSample sample;
  sample.metric = metric;
  sample.status = PerformanceEvidenceStatus::kMeasured;
  sample.value = Mean(elapsed_ms);
  sample.min_value = *std::min_element(elapsed_ms.begin(), elapsed_ms.end());
  sample.max_value = *std::max_element(elapsed_ms.begin(), elapsed_ms.end());
  sample.iterations = iterations;
  sample.unit = ExpectedUnit(metric);
  sample.evidence_id = std::move(evidence_id);
  sample.note = std::move(note);
  return sample;
}

const PerformanceBudgetTarget* FindTarget(
    const ProductionPerformanceBudget& budget,
    const std::string& profile_id,
    PerformanceMetric metric) {
  for (const PerformanceBudgetTarget& target : budget.targets) {
    if (target.profile_id == profile_id && target.metric == metric) {
      return &target;
    }
  }
  return nullptr;
}

bool IsPowerMetric(PerformanceMetric metric) {
  return metric == PerformanceMetric::kActivePowerWatts ||
         metric == PerformanceMetric::kIdlePowerWatts;
}

bool HasSample(const ProductionFixturePerformanceEvidence& evidence,
               PerformanceMetric metric) {
  return FindPerformanceSample(evidence, metric) != nullptr;
}

}  // namespace

const char* ToString(PerformanceEvidenceStatus status) {
  switch (status) {
    case PerformanceEvidenceStatus::kMeasured:
      return "measured";
    case PerformanceEvidenceStatus::kDerived:
      return "derived";
    case PerformanceEvidenceStatus::kUnavailable:
      return "unavailable";
  }
  return "unknown";
}

ProductionFixturePerformanceEvidence MeasureProductionFixturePerformance(
    std::uint32_t iterations) {
  if (iterations == 0U) iterations = 1U;

  ProductionFixturePerformanceEvidence evidence;
  evidence.evidence_id = "perf2-production-fixture";
  evidence.host_profile = "desktop_reference";

  const ProductionGoldenCorpusExpected expected =
      BuildDefaultProductionGoldenCorpusExpected();
  S57PortablePackageConverter converter;
  const S57FixtureCell cell = BuildS57ConverterFixtureCell();
  const PortableNauticalPackage package = converter.Convert(cell);
  const std::string encoded_package = WritePortableNauticalPackage(package);
  evidence.package_hash = package.checksums.package_hash;
  evidence.package_bytes = encoded_package.size();

  std::vector<Diagnostic> package_diagnostics;
  ValidateS57ConverterFixturePackage(package, &package_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              package_diagnostics.begin(),
                              package_diagnostics.end());

  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kPackageLoadMs, iterations, "perf2:package-load",
      [&]() {
        PortableNauticalPackage loaded;
        std::vector<Diagnostic> diagnostics;
        ReadPortableNauticalPackage(encoded_package, &loaded, &diagnostics);
      },
      "Decode the portable package emitted by the production S-57 fixture "
      "converter."));

  PortableNauticalPackage loaded_package;
  std::vector<Diagnostic> read_diagnostics;
  const bool read_ok =
      ReadPortableNauticalPackage(encoded_package, &loaded_package,
                                  &read_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              read_diagnostics.begin(),
                              read_diagnostics.end());
  evidence.package_round_trip_stable =
      read_ok && WritePortableNauticalPackage(loaded_package) == encoded_package;

  const RenderView view = ProductionView();
  const DisplayState display = ProductionDisplay();

  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kPresentationCompileMs, iterations,
      "perf2:presentation-compile",
      [&]() {
        const NauticalRenderModel model =
            s52::CompileS52PackagePresentation(loaded_package, view, display);
        (void)model;
      },
      "Compile the production fixture portable package into neutral S-52 "
      "primitives."));

  const NauticalRenderModel model =
      s52::CompileS52PackagePresentation(loaded_package, view, display);
  evidence.primitive_count = PrimitiveCount(model);
  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(model, &model_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              model_diagnostics.begin(),
                              model_diagnostics.end());

  const GpuArtifactCacheOptions cache_options = ProductionCacheOptions();
  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kGpuArtifactCompileMs, iterations,
      "perf2:gpu-artifact-compile",
      [&]() {
        const GpuArtifactCacheManifest manifest =
            BuildGpuArtifactCacheManifest(model, cache_options);
        (void)manifest;
      },
      "Compile machine-local GPU artifact metadata from neutral primitives."));

  const GpuArtifactCacheManifest manifest =
      BuildGpuArtifactCacheManifest(model, cache_options);
  evidence.gpu_artifact_count = manifest.artifacts.size();
  evidence.gpu_artifact_bytes = manifest.stats.estimated_bytes;
  std::vector<Diagnostic> cache_diagnostics;
  ValidateGpuArtifactCacheManifest(manifest, &cache_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              cache_diagnostics.begin(),
                              cache_diagnostics.end());

  const std::vector<std::string> cold_artifact_ids = ArtifactIds(manifest);
  std::map<std::string, std::vector<std::string>> artifact_cache;
  artifact_cache.emplace(manifest.manifest_id, cold_artifact_ids);
  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kGpuArtifactCacheHitMs, iterations,
      "perf2:gpu-artifact-cache-hit",
      [&]() {
        const auto found = artifact_cache.find(manifest.manifest_id);
        if (found == artifact_cache.end()) return;
        const volatile std::size_t size = found->second.size();
        (void)size;
      },
      "Repeat lookup of the deterministic GPU artifact manifest identity."));
  const GpuArtifactCacheManifest repeat_manifest =
      BuildGpuArtifactCacheManifest(model, cache_options);
  evidence.gpu_cache_repeat_stable =
      cold_artifact_ids == ArtifactIds(repeat_manifest);

  const RenderTarget target = ProductionTarget();
  vsg::VsgBackend backend;
  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kBackendRenderMs, iterations, "perf2:backend-render",
      [&]() {
        const RenderResult result = backend.RenderModel(model, target);
        (void)result;
      },
      "Render the production fixture through the VSG draw/cache-only backend."));

  const RenderResult first_render = backend.RenderModel(model, target);
  const RenderResult repeat_render = backend.RenderModel(model, target);
  evidence.golden_image_hash = PixelHash(first_render.pixels);
  evidence.golden_image_bytes = first_render.pixels.rgba8.size();
  evidence.render_repeat_stable =
      first_render.ok && repeat_render.ok &&
      evidence.golden_image_hash == PixelHash(repeat_render.pixels);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              first_render.diagnostics.begin(),
                              first_render.diagnostics.end());

  const ViewportTileSchedulerInput scheduler_input =
      ProductionSchedulerInput(view);
  ViewportTileSchedulerPolicy scheduler_policy;
  scheduler_policy.max_prefetch_tiles = 16;
  evidence.samples.push_back(MeasureTimed(
      PerformanceMetric::kViewportScheduleMs, iterations,
      "perf2:viewport-schedule",
      [&]() {
        const ViewportTilePlan plan =
            BuildViewportTilePlan(scheduler_input, scheduler_policy);
        (void)plan;
      },
      "Build the adapter viewport tile plan for the production fixture view."));
  const ViewportTilePlan tile_plan =
      BuildViewportTilePlan(scheduler_input, scheduler_policy);
  std::vector<Diagnostic> scheduler_diagnostics;
  ValidateViewportTilePlan(tile_plan, &scheduler_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              scheduler_diagnostics.begin(),
                              scheduler_diagnostics.end());

  SourceToRenderInspectionOptions inspection_options;
  inspection_options.report_id = "perf2-production-source-to-pixel";
  inspection_options.source_product_id = loaded_package.product.product_id;
  inspection_options.converter_id = loaded_package.manifest.converter_id;
  inspection_options.portable_package_id = loaded_package.manifest.package_id;
  inspection_options.backend_name = backend.Name();
  inspection_options.target = target;
  inspection_options.backend_result = &first_render;
  const SourceToRenderInspectionReport inspection =
      BuildSourceToRenderInspectionReport(model, manifest, inspection_options);
  evidence.inspection_row_count = inspection.rows.size();
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              inspection.diagnostics.begin(),
                              inspection.diagnostics.end());

  evidence.samples.push_back(MakeSample(
      PerformanceMetric::kResidentMemoryBytes,
      PerformanceEvidenceStatus::kDerived,
      static_cast<double>(evidence.package_bytes + evidence.gpu_artifact_bytes +
                          evidence.golden_image_bytes),
      1U, "perf2:resident-memory-derived",
      "Derived from package bytes, GPU artifact estimate, and offscreen "
      "golden image bytes."));
  evidence.samples.push_back(MakeSample(
      PerformanceMetric::kDiskCacheBytes, PerformanceEvidenceStatus::kDerived,
      static_cast<double>(evidence.package_bytes + evidence.gpu_artifact_bytes),
      1U, "perf2:disk-cache-derived",
      "Derived from portable package bytes plus machine-local GPU artifact "
      "estimate."));
  evidence.samples.push_back(MakeSample(
      PerformanceMetric::kActivePowerWatts,
      PerformanceEvidenceStatus::kUnavailable, 0.0, 0U,
      "perf2:active-power-unavailable",
      "Host watt telemetry is not available from this C++ fixture run."));
  evidence.samples.push_back(MakeSample(
      PerformanceMetric::kIdlePowerWatts,
      PerformanceEvidenceStatus::kUnavailable, 0.0, 0U,
      "perf2:idle-power-unavailable",
      "Host watt telemetry is not available from this C++ fixture run."));

  std::vector<Diagnostic> validation_diagnostics;
  const ProductionPerformanceBudget budget =
      BuildFirstProductionPerformanceBudget();
  evidence.ok = ValidateProductionFixturePerformanceEvidence(
      budget, evidence, &validation_diagnostics);
  evidence.diagnostics.insert(evidence.diagnostics.end(),
                              validation_diagnostics.begin(),
                              validation_diagnostics.end());
  if (evidence.package_hash != expected.package_hash ||
      evidence.golden_image_hash != expected.golden_image_hash ||
      evidence.primitive_count != expected.primitive_count ||
      evidence.gpu_artifact_count != expected.artifact_count ||
      evidence.inspection_row_count != expected.inspection_row_count) {
    evidence.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_performance_golden_drift",
        "PERF-2 evidence no longer measures the QA-5 golden production slice."));
    evidence.ok = false;
  }
  evidence.ok = evidence.ok && !HasError(evidence.diagnostics);
  return evidence;
}

bool ValidateProductionFixturePerformanceEvidence(
    const ProductionPerformanceBudget& budget,
    const ProductionFixturePerformanceEvidence& evidence,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (evidence.schema_version != kProductionPerformanceEvidenceSchemaVersion ||
      evidence.evidence_id.empty() || evidence.host_profile.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_performance_identity",
        "Production fixture performance evidence has invalid identity."));
    ok = false;
  }
  if (evidence.package_hash.empty() || evidence.golden_image_hash.empty() ||
      evidence.primitive_count == 0 || evidence.gpu_artifact_count == 0 ||
      evidence.inspection_row_count == 0) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_performance_fixture_identity",
        "Production fixture performance evidence is missing fixture identity."));
    ok = false;
  }
  if (!evidence.package_round_trip_stable || !evidence.gpu_cache_repeat_stable ||
      !evidence.render_repeat_stable) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_performance_repeat_stability",
        "Package round trip, GPU cache repeat, or render repeat was unstable."));
    ok = false;
  }

  std::vector<Diagnostic> budget_diagnostics;
  ValidatePerformanceBudget(budget, &budget_diagnostics);
  out.insert(out.end(), budget_diagnostics.begin(), budget_diagnostics.end());

  for (const TimedPerformanceSample& sample : evidence.samples) {
    if (sample.unit != ExpectedUnit(sample.metric) || sample.evidence_id.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_sample_identity",
          "Performance sample has the wrong unit or no evidence id."));
      ok = false;
    }
    if (sample.status == PerformanceEvidenceStatus::kMeasured &&
        (sample.iterations == 0U || sample.value < 0.0 ||
         sample.min_value < 0.0 || sample.max_value < sample.min_value)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_sample_timing",
          "Measured performance sample has invalid timing statistics."));
      ok = false;
    }
    if (sample.status == PerformanceEvidenceStatus::kDerived &&
        sample.value <= 0.0) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_sample_derived",
          "Derived performance sample must have a positive value."));
      ok = false;
    }
  }

  for (const PerformanceBudgetTarget& target : budget.targets) {
    if (target.profile_id != evidence.host_profile) continue;
    const TimedPerformanceSample* sample =
        FindPerformanceSample(evidence, target.metric);
    if (!sample) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_missing_metric",
          std::string("PERF-2 evidence is missing metric ") +
              ToString(target.metric) + "."));
      ok = false;
      continue;
    }
    if (sample->status == PerformanceEvidenceStatus::kUnavailable) {
      if (IsPowerMetric(target.metric)) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kWarning, "production_performance_power_unavailable",
            std::string("Power telemetry unavailable for metric ") +
                ToString(target.metric) +
                "; do not claim production viability from this run."));
      } else {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError,
            "production_performance_metric_unavailable",
            std::string("Required metric unavailable: ") +
                ToString(target.metric) + "."));
        ok = false;
      }
      continue;
    }
    if (sample->value > target.max_value) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_budget_exceeded",
          std::string("Measured production fixture metric exceeds budget: ") +
              ToString(target.metric) + "."));
      ok = false;
    }
  }

  for (PerformanceMetric metric :
       {PerformanceMetric::kPackageLoadMs,
        PerformanceMetric::kPresentationCompileMs,
        PerformanceMetric::kGpuArtifactCompileMs,
        PerformanceMetric::kGpuArtifactCacheHitMs,
        PerformanceMetric::kBackendRenderMs,
        PerformanceMetric::kResidentMemoryBytes,
        PerformanceMetric::kDiskCacheBytes,
        PerformanceMetric::kViewportScheduleMs,
        PerformanceMetric::kActivePowerWatts,
        PerformanceMetric::kIdlePowerWatts}) {
    if (!HasSample(evidence, metric)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_performance_required_metric",
          std::string("PERF-2 evidence omitted ") + ToString(metric) + "."));
      ok = false;
    }
  }

  return ok && !HasError(out);
}

const TimedPerformanceSample* FindPerformanceSample(
    const ProductionFixturePerformanceEvidence& evidence,
    PerformanceMetric metric) {
  const auto found =
      std::find_if(evidence.samples.begin(), evidence.samples.end(),
                   [&](const TimedPerformanceSample& sample) {
                     return sample.metric == metric;
                   });
  return found == evidence.samples.end() ? nullptr : &*found;
}

}  // namespace ocpn::render
