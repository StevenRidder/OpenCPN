// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "production_golden_corpus.hpp"

#include "s52_presentation_compiler.hpp"
#include "s57_portable_package_converter.hpp"
#include "vsg/vsg_backend.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Update the QA-5 production golden corpus only when the source fixture, "
      "package, presentation, GPU cache, backend image, and inspection trace "
      "are intentionally re-baselined together.";
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

std::uint64_t Fnva64String(std::uint64_t hash, const std::string& value) {
  return Fnva64Bytes(
      hash, reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
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

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto found = metadata.find(key);
  return found == metadata.end() ? std::string{} : found->second;
}

std::string FirstSourceGeometryHash(
    const NauticalPrimitive& primitive,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  for (const std::string& provenance_ref : primitive.trace.provenance_refs) {
    const auto found = provenance_by_id.find(provenance_ref);
    if (found != provenance_by_id.end()) {
      return found->second.source_geometry_hash + "|" +
             found->second.generated_geometry_hash;
    }
  }
  return {};
}

std::string GeometryShapeKey(const NauticalPrimitive& primitive) {
  std::ostringstream out;
  for (const Geometry& geometry : primitive.geometries) {
    out << geometry.geometry_id << ":" << static_cast<int>(geometry.coordinate_space)
        << ":p" << geometry.points.size() << ":r" << geometry.rings.size()
        << ";";
  }
  out << "pos=" << std::setprecision(17) << primitive.position.x << ","
      << primitive.position.y;
  return out.str();
}

std::string PrimitiveStableHash(
    const NauticalPrimitive& primitive,
    const std::map<std::string, ProvenanceRecord>& provenance_by_id) {
  std::ostringstream key;
  key << primitive.primitive_id << "|" << ToString(primitive.type) << "|"
      << primitive.role << "|" << primitive.trace.source_chart_id << "|"
      << primitive.trace.source_object_id << "|"
      << primitive.trace.source_object_class << "|"
      << primitive.trace.presentation_rule_id << "|"
      << primitive.cache_key.scene_key << "|" << primitive.cache_key.primitive_key
      << "|" << primitive.cache_key.resource_key << "|"
      << FirstSourceGeometryHash(primitive, provenance_by_id) << "|"
      << GeometryShapeKey(primitive) << "|" << primitive.fill_ref << "|"
      << primitive.line_style_ref << "|" << primitive.symbol_ref << "|"
      << primitive.font_ref << "|" << primitive.text << "|"
      << MetadataValue(primitive.metadata, "source_standard") << "|"
      << MetadataValue(primitive.metadata, "semantic_tier") << "|"
      << MetadataValue(primitive.metadata, "safety_class") << "|"
      << MetadataValue(primitive.metadata, "safety_contour");
  return HexHash(Fnva64String(1469598103934665603ULL, key.str()));
}

std::map<std::string, ProvenanceRecord> ProvenanceById(
    const std::vector<ProvenanceRecord>& records) {
  std::map<std::string, ProvenanceRecord> by_id;
  for (const ProvenanceRecord& record : records) {
    if (!record.provenance_id.empty()) {
      by_id[record.provenance_id] = record;
    }
  }
  return by_id;
}

std::vector<ProductionGoldenPrimitive> CapturePrimitives(
    const NauticalRenderModel& model) {
  std::vector<ProductionGoldenPrimitive> primitives;
  const std::map<std::string, ProvenanceRecord> provenance_by_id =
      ProvenanceById(model.provenance_table);
  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      ProductionGoldenPrimitive golden;
      golden.primitive_id = primitive.primitive_id;
      golden.source_object_id = primitive.trace.source_object_id;
      golden.object_class = primitive.trace.source_object_class;
      golden.presentation_rule_id = primitive.trace.presentation_rule_id;
      golden.primitive_type = ToString(primitive.type);
      golden.stable_hash = PrimitiveStableHash(primitive, provenance_by_id);
      primitives.push_back(std::move(golden));
    }
  }
  std::sort(primitives.begin(), primitives.end(),
            [](const ProductionGoldenPrimitive& lhs,
               const ProductionGoldenPrimitive& rhs) {
              return lhs.primitive_id < rhs.primitive_id;
            });
  return primitives;
}

RenderView ProductionView() {
  RenderView view;
  view.view_id = "qa5-production-golden-corpus";
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
  target.target_id = "qa5-offscreen-golden";
  return target;
}

bool HasDiagnostic(const std::vector<Diagnostic>& diagnostics,
                   const std::string& code) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [&](const Diagnostic& diagnostic) {
                       return diagnostic.code == code;
                     });
}

bool HasTraceText(const SourceToRenderInspectionReport& report,
                  const std::string& expected) {
  const std::vector<std::string> trace_lines =
      BuildHumanReadableSourceToRenderTrace(report);
  return std::any_of(trace_lines.begin(), trace_lines.end(),
                     [&](const std::string& line) {
                       return line.find(expected) != std::string::npos;
                     });
}

const SourceToRenderInspectionRow* FindRow(
    const SourceToRenderInspectionReport& report,
    const std::string& primitive_id) {
  return FindInspectionByPrimitiveId(report, primitive_id);
}

std::string PrimitiveMismatchMessage(const ProductionGoldenPrimitive& expected,
                                     const ProductionGoldenPrimitive& actual) {
  std::ostringstream out;
  out << "Primitive " << expected.primitive_id
      << " drifted: expected source=" << expected.source_object_id
      << ", class=" << expected.object_class
      << ", rule=" << expected.presentation_rule_id
      << ", type=" << expected.primitive_type
      << ", hash=" << expected.stable_hash << "; got source="
      << actual.source_object_id << ", class=" << actual.object_class
      << ", rule=" << actual.presentation_rule_id
      << ", type=" << actual.primitive_type
      << ", hash=" << actual.stable_hash;
  return out.str();
}

bool ContainsAllLimitations(const std::vector<std::string>& actual,
                            const std::vector<std::string>& required) {
  for (const std::string& limitation : required) {
    if (std::find(actual.begin(), actual.end(), limitation) == actual.end()) {
      return false;
    }
  }
  return true;
}

void AppendDiagnostics(std::vector<Diagnostic>* destination,
                       const std::vector<Diagnostic>& incoming) {
  destination->insert(destination->end(), incoming.begin(), incoming.end());
}

}  // namespace

ProductionGoldenCorpusExpected BuildDefaultProductionGoldenCorpusExpected() {
  ProductionGoldenCorpusExpected expected;
  expected.package_hash = "fnv1a64:e2c66b175a87d42b";
  expected.primitives = {
      {"prim-US5CONVERT2:BOYLAT.3001-navaid_symbol", "BOYLAT.3001",
       "BOYLAT", "s52:BOYLAT:navaid_symbol", "symbol_instance",
       "0b7248262d277af7"},
      {"prim-US5CONVERT2:DEPCNT.2001-safety_contour", "DEPCNT.2001",
       "DEPCNT", "s52:DEPCNT:safety_contour", "contour_line",
       "db53669dc2d37672"},
      {"prim-US5CONVERT2:DEPARE.1001-depth_area", "DEPARE.1001", "DEPARE",
       "s52:DEPARE:depth_area", "area_fill", "eca7dd700e12bcea"}};
  expected.required_limitations = {
      "synthetic_redistributable_s57_fixture",
      "first_slice_object_classes_depare_depcnt_boylat_only",
      "light_sectors_are_diagnostic_only",
      "vsg_backend_is_deterministic_fixture_renderer_not_live_vsg_replay",
      "qa5_is_not_full_s52_or_ecdis_certification"};
  return expected;
}

ProductionGoldenCorpusSnapshot BuildProductionGoldenCorpusSnapshot(
    const ProductionGoldenCorpusExpected& expected) {
  ProductionGoldenCorpusSnapshot snapshot;
  snapshot.corpus_id = expected.corpus_id;

  const S57FixtureCell cell = BuildS57ConverterFixtureCell();
  S57PortablePackageConverter converter;
  const PortableNauticalPackage package = converter.Convert(cell);

  if (!package.product.sources.empty()) {
    const ChartSourceRef& source = package.product.sources.front();
    snapshot.source_id = source.source_id;
    snapshot.source_edition = source.edition;
    snapshot.source_update = source.update;
    snapshot.distribution_class =
        MetadataValue(source.metadata, "distribution_class");
  }
  snapshot.package_id = package.manifest.package_id;
  snapshot.package_profile = package.manifest.profile;
  snapshot.package_hash = package.checksums.package_hash;

  std::vector<Diagnostic> package_diagnostics;
  ValidateS57ConverterFixturePackage(package, &package_diagnostics);
  AppendDiagnostics(&snapshot.diagnostics, package_diagnostics);

  const NauticalRenderModel model =
      s52::CompileS52PackagePresentation(package, ProductionView(),
                                         ProductionDisplay());
  snapshot.model_id = model.model_id;
  snapshot.model_epoch = model.source_epoch;
  snapshot.primitives = CapturePrimitives(model);

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(model, &model_diagnostics);
  AppendDiagnostics(&snapshot.diagnostics, model_diagnostics);

  snapshot.cache_manifest =
      BuildGpuArtifactCacheManifest(model, ProductionCacheOptions());
  std::vector<Diagnostic> cache_diagnostics;
  ValidateGpuArtifactCacheManifest(snapshot.cache_manifest, &cache_diagnostics);
  AppendDiagnostics(&snapshot.diagnostics, cache_diagnostics);

  const RenderTarget target = ProductionTarget();
  vsg::VsgBackend backend;
  const RenderResult backend_result = backend.RenderModel(model, target);
  snapshot.backend_name = backend.Name();
  snapshot.golden_image_hash = PixelHash(backend_result.pixels);
  snapshot.golden_image_bytes = backend_result.pixels.rgba8.size();
  AppendDiagnostics(&snapshot.diagnostics, backend_result.diagnostics);
  if (!backend_result.ok) {
    snapshot.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_backend",
        "VSG backend failed while building the QA-5 golden image."));
  }

  SourceToRenderInspectionOptions inspection_options;
  inspection_options.report_id = "qa5-production-source-to-pixel";
  inspection_options.source_product_id = package.product.product_id;
  inspection_options.converter_id = package.manifest.converter_id;
  inspection_options.portable_package_id = package.manifest.package_id;
  inspection_options.backend_name = snapshot.backend_name;
  inspection_options.target = target;
  inspection_options.backend_result = &backend_result;
  snapshot.inspection_report = BuildSourceToRenderInspectionReport(
      model, snapshot.cache_manifest, inspection_options);
  AppendDiagnostics(&snapshot.diagnostics, snapshot.inspection_report.diagnostics);

  snapshot.known_limitations = expected.required_limitations;
  std::vector<Diagnostic> validation_diagnostics;
  snapshot.ok = ValidateProductionGoldenCorpusSnapshot(
      snapshot, expected, &validation_diagnostics);
  AppendDiagnostics(&snapshot.diagnostics, validation_diagnostics);
  snapshot.ok = snapshot.ok && !HasError(snapshot.diagnostics);
  return snapshot;
}

bool ValidateProductionGoldenCorpusSnapshot(
    const ProductionGoldenCorpusSnapshot& snapshot,
    const ProductionGoldenCorpusExpected& expected,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (snapshot.schema_version != kProductionGoldenCorpusSchemaVersion ||
      expected.schema_version != kProductionGoldenCorpusSchemaVersion ||
      snapshot.corpus_id != expected.corpus_id) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "production_golden_identity",
                                 "QA-5 corpus identity or schema drifted."));
    ok = false;
  }
  if (snapshot.source_id != expected.source_id ||
      snapshot.source_edition != expected.source_edition ||
      snapshot.source_update != expected.source_update ||
      snapshot.distribution_class != expected.distribution_class) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_source_identity",
        "Redistributable S-57 fixture source identity drifted."));
    ok = false;
  }
  if (snapshot.package_id != expected.package_id ||
      snapshot.package_profile != expected.package_profile ||
      snapshot.package_hash.empty() ||
      (!expected.package_hash.empty() && expected.package_hash != "pending" &&
       snapshot.package_hash != expected.package_hash)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_package_hash",
        "Portable package identity or stable package hash drifted."));
    ok = false;
  }
  if (snapshot.model_id != expected.model_id || snapshot.model_epoch.empty()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_model_identity",
        "Neutral presentation model identity or epoch drifted."));
    ok = false;
  }
  if (snapshot.primitives.size() != expected.primitive_count ||
      snapshot.primitives.size() != expected.primitives.size()) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_primitive_count",
        "Neutral primitive count drifted before pixels changed."));
    ok = false;
  }

  std::map<std::string, ProductionGoldenPrimitive> actual_by_id;
  for (const ProductionGoldenPrimitive& primitive : snapshot.primitives) {
    actual_by_id[primitive.primitive_id] = primitive;
  }
  for (const ProductionGoldenPrimitive& primitive : expected.primitives) {
    const auto actual = actual_by_id.find(primitive.primitive_id);
    if (actual == actual_by_id.end()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_golden_primitive_missing",
          "Expected neutral primitive is missing: " + primitive.primitive_id));
      ok = false;
      continue;
    }
    const bool hash_expected =
        !primitive.stable_hash.empty() && primitive.stable_hash != "pending";
    if (actual->second.source_object_id != primitive.source_object_id ||
        actual->second.object_class != primitive.object_class ||
        actual->second.presentation_rule_id != primitive.presentation_rule_id ||
        actual->second.primitive_type != primitive.primitive_type ||
        (hash_expected &&
         actual->second.stable_hash != primitive.stable_hash)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_golden_primitive_hash",
          PrimitiveMismatchMessage(primitive, actual->second)));
      ok = false;
    }
  }

  if (snapshot.cache_manifest.artifacts.size() != expected.artifact_count ||
      snapshot.cache_manifest.stats.estimated_bytes == 0 ||
      snapshot.cache_manifest.stats.over_budget ||
      snapshot.cache_manifest.semantic_owner == "backend") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_artifact_manifest",
        "GPU artifact cache metadata drifted or gave semantics to a backend."));
    ok = false;
  }
  if (!HasDiagnostic(snapshot.cache_manifest.diagnostics,
                     "gpu_artifact_cache_budget") &&
      snapshot.cache_manifest.stats.memory_budget_bytes !=
          ProductionCacheOptions().memory_budget_bytes) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_artifact_budget",
        "GPU artifact cache budget metadata drifted."));
    ok = false;
  }
  std::set<std::string> artifact_primitive_ids;
  for (const GpuArtifactRecord& artifact : snapshot.cache_manifest.artifacts) {
    if (artifact.tier.semantic_tier != "tier1_official_chart" ||
        artifact.tier.semantic_owner == "backend") {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_golden_artifact_tier",
          "GPU artifact lost tier/owner provenance."));
      ok = false;
    }
    artifact_primitive_ids.insert(artifact.tier.primitive_ids.begin(),
                                  artifact.tier.primitive_ids.end());
  }
  for (const ProductionGoldenPrimitive& primitive : expected.primitives) {
    if (artifact_primitive_ids.find(primitive.primitive_id) ==
        artifact_primitive_ids.end()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_golden_artifact_link",
          "GPU artifact manifest no longer links primitive " +
              primitive.primitive_id));
      ok = false;
    }
  }

  if (snapshot.backend_name != expected.backend_name ||
      snapshot.golden_image_hash != expected.golden_image_hash ||
      snapshot.golden_image_bytes != expected.golden_image_bytes) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_image_hash",
        "Golden image output drifted."));
    ok = false;
  }

  const SourceToRenderInspectionReport& report = snapshot.inspection_report;
  if (!report.ok || report.rows.size() != expected.inspection_row_count ||
      report.scene_artifacts.size() != expected.scene_artifact_count ||
      report.cache_manifest_id != snapshot.cache_manifest.manifest_id) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_inspection_report",
        "Source-to-render inspection report drifted or became incomplete."));
    ok = false;
  }
  const SourceToRenderInspectionRow* buoy =
      FindRow(report, "prim-US5CONVERT2:BOYLAT.3001-navaid_symbol");
  if (!buoy || !buoy->query.sampled_rendered_pixel ||
      buoy->query.sample_x != expected.buoy_sample_x ||
      buoy->query.sample_y != expected.buoy_sample_y ||
      buoy->query.sample_rgba8 != expected.buoy_sample_rgba8 ||
      buoy->query.rendered_pixel_hash != expected.golden_image_hash) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_pixel_trace",
        "Inspection trace lost the expected buoy pixel sample."));
    ok = false;
  }
  for (const ProductionGoldenPrimitive& primitive : expected.primitives) {
    const SourceToRenderInspectionRow* row =
        FindRow(report, primitive.primitive_id);
    if (!row || row->tier.semantic_tier != "tier1_official_chart" ||
        row->tier.semantic_owner != "presentation_compiler" ||
        row->tier.source_standard != "S-57" ||
        row->backend.backend_contract != "draw_only" ||
        row->human_trace.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "production_golden_row_trace",
          "Inspection row lost tier, owner, source standard, backend contract, "
          "or human trace for " +
              primitive.primitive_id));
      ok = false;
    }
  }
  if (!HasTraceText(report, "source s57:US5CONVERT2/DEPARE.1001") ||
      !HasTraceText(report, "package s57:US5CONVERT2:package") ||
      !HasTraceText(report, "presentation s52:BOYLAT:navaid_symbol") ||
      !HasTraceText(report, "image_hash=" + expected.golden_image_hash) ||
      !HasTraceText(report,
                    "wrong_symbol=converter_presentation_or_backend")) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_human_trace",
        "Human-readable source-to-pixel trace lost required evidence."));
    ok = false;
  }

  if (!ContainsAllLimitations(snapshot.known_limitations,
                              expected.required_limitations)) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "production_golden_limitations",
        "Known limitations are missing from the production corpus."));
    ok = false;
  }
  return ok;
}

}  // namespace ocpn::render
