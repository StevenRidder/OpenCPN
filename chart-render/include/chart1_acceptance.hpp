// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render::chart1 {

inline constexpr std::uint32_t kChart1AcceptanceSchemaVersion = 1;

enum class AcceptancePrimitive {
  kPoint,
  kLine,
  kArea
};

struct AcceptanceCase {
  std::string case_id;
  AcceptancePrimitive primitive = AcceptancePrimitive::kPoint;
  std::string title;
  std::string source_object_class;
  std::string s52_rule_id;
  CommandType expected_command_type = CommandType::kPlaceSymbol;
  ResourceType expected_resource_type = ResourceType::kSymbol;
  std::string expected_command_role;
  std::vector<std::string> fixture_command_ids;
  std::string acceptance_note;
};

struct AcceptanceCatalog {
  std::uint32_t schema_version = kChart1AcceptanceSchemaVersion;
  std::string catalog_id = "chart-1-acceptance";
  std::vector<AcceptanceCase> cases;
};

const char* ToString(AcceptancePrimitive primitive);

AcceptanceCatalog BuildAcceptanceCatalog();

bool ValidateAcceptanceCatalog(const AcceptanceCatalog& catalog,
                               std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render::chart1
