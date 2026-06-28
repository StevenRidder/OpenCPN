// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_baseline.hpp"

#include <iostream>

int main() {
  const ocpn::render::chart1::AcceptanceCatalog catalog =
      ocpn::render::chart1::BuildAcceptanceCatalog();
  const ocpn::render::chart1::BaselineComparisonManifest manifest =
      ocpn::render::chart1::BuildBaselineComparisonManifest();

  std::vector<ocpn::render::Diagnostic> diagnostics;
  if (!ocpn::render::chart1::ValidateBaselineComparisonManifest(
          catalog, manifest, &diagnostics)) {
    std::cerr << "Chart 1 baseline manifest failed validation\n";
    for (const ocpn::render::Diagnostic& diagnostic : diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
    }
    return 1;
  }

  return 0;
}
