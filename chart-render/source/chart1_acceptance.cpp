// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_acceptance.hpp"

#include <set>
#include <utility>

namespace ocpn::render::chart1 {
namespace {

Diagnostic Error(std::string code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::kError;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Fix the Chart 1 catalog before rendering conformance cases.";
  return diagnostic;
}

}  // namespace

const char* ToString(AcceptancePrimitive primitive) {
  switch (primitive) {
    case AcceptancePrimitive::kPoint:
      return "point";
    case AcceptancePrimitive::kLine:
      return "line";
    case AcceptancePrimitive::kArea:
      return "area";
  }
  return "unknown";
}

AcceptanceCatalog BuildAcceptanceCatalog() {
  AcceptanceCatalog catalog;
  catalog.cases = {
      {"chart1-area-depth",
       AcceptancePrimitive::kArea,
       "Depth area fill",
       "DEPARE",
       "fixture:depth_area",
       CommandType::kFillArea,
       ResourceType::kPalette,
       "depth_area",
       {"cmd-depth-area"},
       "Area case accepts a filled depth polygon with explicit provenance and "
       "target-space geometry."},
      {"chart1-line-depth-contour",
       AcceptancePrimitive::kLine,
       "Depth contour line",
       "DEPCNT",
       "fixture:depth_contour",
       CommandType::kStrokeLine,
       ResourceType::kLineStyle,
       "depth_contour",
       {"cmd-depth-contour"},
       "Line case accepts a styled contour polyline with preserved contour "
       "metadata."},
      {"chart1-point-buoy",
       AcceptancePrimitive::kPoint,
       "Lateral buoy symbol",
       "BOYLAT",
       "fixture:buoy",
       CommandType::kPlaceSymbol,
       ResourceType::kSymbol,
       "buoy",
       {"cmd-buoy"},
       "Point case accepts a symbol command with center anchoring and "
       "navigation priority."},
  };
  return catalog;
}

bool ValidateAcceptanceCatalog(const AcceptanceCatalog& catalog,
                               std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (catalog.schema_version != kChart1AcceptanceSchemaVersion) {
    out.push_back(Error("chart1_catalog_schema",
                        "Unsupported Chart 1 acceptance schema version."));
    ok = false;
  }
  if (catalog.catalog_id.empty()) {
    out.push_back(
        Error("chart1_catalog_id", "Chart 1 catalog is missing catalog_id."));
    ok = false;
  }

  std::set<std::string> case_ids;
  std::set<AcceptancePrimitive> primitives;
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    if (acceptance_case.case_id.empty()) {
      out.push_back(Error("chart1_case_id",
                          "Chart 1 acceptance case is missing case_id."));
      ok = false;
    }
    if (!case_ids.insert(acceptance_case.case_id).second) {
      out.push_back(Error("chart1_duplicate_case",
                          "Chart 1 acceptance case id is duplicated."));
      ok = false;
    }
    if (acceptance_case.source_object_class.empty()) {
      out.push_back(Error("chart1_object_class",
                          "Chart 1 case is missing source_object_class."));
      ok = false;
    }
    if (acceptance_case.s52_rule_id.empty()) {
      out.push_back(Error("chart1_s52_rule",
                          "Chart 1 case is missing s52_rule_id."));
      ok = false;
    }
    if (acceptance_case.fixture_command_ids.empty()) {
      out.push_back(Error("chart1_fixture_command",
                          "Chart 1 case has no fixture_command_ids."));
      ok = false;
    }
    primitives.insert(acceptance_case.primitive);
  }

  for (AcceptancePrimitive required :
       {AcceptancePrimitive::kPoint, AcceptancePrimitive::kLine,
        AcceptancePrimitive::kArea}) {
    if (primitives.count(required) == 0) {
      out.push_back(Error("chart1_missing_primitive",
                          std::string("Chart 1 catalog missing ") +
                              ToString(required) + " case."));
      ok = false;
    }
  }

  return ok;
}

}  // namespace ocpn::render::chart1
