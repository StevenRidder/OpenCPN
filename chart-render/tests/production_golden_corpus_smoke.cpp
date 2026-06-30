// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "production_golden_corpus.hpp"

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

void PrintDiagnostics(
    const std::vector<ocpn::render::Diagnostic>& diagnostics) {
  for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
  }
}

bool ContainsPending(const ocpn::render::ProductionGoldenCorpusExpected& expected) {
  if (expected.package_hash == "pending") return true;
  for (const ocpn::render::ProductionGoldenPrimitive& primitive :
       expected.primitives) {
    if (primitive.stable_hash == "pending") return true;
  }
  return false;
}

}  // namespace

int main() {
  const ocpn::render::ProductionGoldenCorpusExpected expected =
      ocpn::render::BuildDefaultProductionGoldenCorpusExpected();
  const ocpn::render::ProductionGoldenCorpusSnapshot snapshot =
      ocpn::render::BuildProductionGoldenCorpusSnapshot(expected);

  if (!snapshot.ok) {
    std::cerr << "QA-5 production golden corpus gate failed\n";
    PrintDiagnostics(snapshot.diagnostics);
    return 1;
  }
  if (ContainsPending(expected)) {
    std::cerr << "QA-5 production golden corpus expected values are pending\n";
    std::cerr << "package_hash=" << snapshot.package_hash << "\n";
    for (const ocpn::render::ProductionGoldenPrimitive& primitive :
         snapshot.primitives) {
      std::cerr << primitive.primitive_id << " hash="
                << primitive.stable_hash << "\n";
    }
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot semantic_drift = snapshot;
  semantic_drift.primitives.front().presentation_rule_id =
      "s52:BOYLAT:backend_owned_symbol";
  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          semantic_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_primitive_hash")) {
    std::cerr << "QA-5 gate accepted semantic primitive drift\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot package_drift = snapshot;
  package_drift.package_hash = "fnv1a64:changed";
  diagnostics.clear();
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          package_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_package_hash")) {
    std::cerr << "QA-5 gate accepted portable package hash drift\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot artifact_drift = snapshot;
  artifact_drift.cache_manifest.artifacts.front().tier.semantic_owner =
      "backend";
  diagnostics.clear();
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          artifact_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_artifact_tier")) {
    std::cerr << "QA-5 gate accepted GPU artifact semantic-owner drift\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot pixel_drift = snapshot;
  pixel_drift.golden_image_hash = "0000000000000000";
  diagnostics.clear();
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          pixel_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_image_hash")) {
    std::cerr << "QA-5 gate accepted golden image drift\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot trace_drift = snapshot;
  trace_drift.inspection_report.rows.back().human_trace.clear();
  diagnostics.clear();
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          trace_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_row_trace")) {
    std::cerr << "QA-5 gate accepted missing inspection trace evidence\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  ocpn::render::ProductionGoldenCorpusSnapshot limitations_drift = snapshot;
  limitations_drift.known_limitations.clear();
  diagnostics.clear();
  if (ocpn::render::ValidateProductionGoldenCorpusSnapshot(
          limitations_drift, expected, &diagnostics) ||
      !HasDiagnostic(diagnostics, "production_golden_limitations")) {
    std::cerr << "QA-5 gate accepted missing known limitations\n";
    PrintDiagnostics(diagnostics);
    return 1;
  }

  std::cout << "ok production-golden-corpus: package="
            << snapshot.package_hash << " primitives="
            << snapshot.primitives.size() << " artifacts="
            << snapshot.cache_manifest.artifacts.size() << " image_hash="
            << snapshot.golden_image_hash << " rows="
            << snapshot.inspection_report.rows.size() << " limitations="
            << snapshot.known_limitations.size() << "\n";
  return 0;
}
