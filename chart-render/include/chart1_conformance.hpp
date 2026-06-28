// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart1_acceptance.hpp"
#include "render_scene.hpp"

#include <string>
#include <vector>

namespace ocpn::render::chart1 {

struct ConformanceCaseResult {
  std::string case_id;
  std::string command_id;
  bool ok = false;
};

struct ConformanceScene {
  RenderScene scene;
  std::vector<ConformanceCaseResult> case_results;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

ConformanceScene BuildConformanceScene(RenderView view, DisplayState display);

bool ValidateAcceptanceAgainstScene(const AcceptanceCatalog& catalog,
                                    const RenderScene& scene,
                                    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::chart1
