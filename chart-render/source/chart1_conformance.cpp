// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "chart1_conformance.hpp"

#include "conversion_trace.hpp"
#include "s52/s52_command_builder.hpp"

#include <set>
#include <utility>

namespace ocpn::render::chart1 {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.suggested_action =
      "Fix the Chart 1 conformance scene before image comparison.";
  return diagnostic;
}

const RenderCommand* FindCommand(const RenderScene& scene,
                                 const std::string& command_id) {
  for (const CommandGroup& group : scene.command_groups) {
    for (const RenderCommand& command : group.commands) {
      if (command.command_id == command_id) {
        return &command;
      }
    }
  }
  return nullptr;
}

const ResourceRecord* FindResource(const RenderScene& scene,
                                   const std::string& resource_id) {
  for (const ResourceRecord& resource : scene.resource_table.resources) {
    if (resource.resource_id == resource_id) {
      return &resource;
    }
  }
  return nullptr;
}

std::string ResourceRefForCase(const RenderCommand& command,
                               ResourceType expected_type) {
  switch (expected_type) {
    case ResourceType::kPalette:
    case ResourceType::kAreaPattern:
      return command.pattern_ref.empty() || command.pattern_ref == "none"
                 ? command.fill_ref
                 : command.pattern_ref;
    case ResourceType::kLineStyle:
      return command.line_style_ref.empty() ? command.stroke_ref
                                            : command.line_style_ref;
    case ResourceType::kSymbol:
      return command.symbol_ref;
    case ResourceType::kFont:
      return command.font_ref;
    case ResourceType::kRasterTexture:
      return command.texture_ref;
    case ResourceType::kGeometryBuffer:
      return std::string{};
  }
  return std::string{};
}

std::string MetadataValue(const RenderCommand& command, const char* key) {
  const auto it = command.metadata.find(key);
  return it == command.metadata.end() ? std::string{} : it->second;
}

bool ValidateCase(const AcceptanceCase& acceptance_case,
                  const RenderScene& scene,
                  std::vector<Diagnostic>* diagnostics) {
  bool ok = true;
  for (const std::string& command_id : acceptance_case.fixture_command_ids) {
    const RenderCommand* command = FindCommand(scene, command_id);
    if (!command) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_missing_command",
          "Chart 1 case " + acceptance_case.case_id +
              " references missing command " + command_id + "."));
      ok = false;
      continue;
    }
    if (command->type != acceptance_case.expected_command_type) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_command_type",
          "Chart 1 case " + acceptance_case.case_id +
              " command type does not match catalog."));
      ok = false;
    }
    if (command->role != acceptance_case.expected_command_role) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_command_role",
          "Chart 1 case " + acceptance_case.case_id +
              " command role does not match catalog."));
      ok = false;
    }
    if (MetadataValue(*command, "s52_rule") != acceptance_case.s52_rule_id) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_s52_rule",
          "Chart 1 case " + acceptance_case.case_id +
              " command is missing expected S-52 rule metadata."));
      ok = false;
    }
    if (command->provenance_refs.empty() ||
        command->conversion_trace_refs.empty()) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_traceability",
          "Chart 1 case " + acceptance_case.case_id +
              " command is missing provenance or conversion trace refs."));
      ok = false;
    }

    const std::string resource_id =
        ResourceRefForCase(*command, acceptance_case.expected_resource_type);
    const ResourceRecord* resource = FindResource(scene, resource_id);
    if (!resource || resource->type != acceptance_case.expected_resource_type) {
      diagnostics->push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "chart1_resource_type",
          "Chart 1 case " + acceptance_case.case_id +
              " does not resolve the expected resource type."));
      ok = false;
    }
  }
  return ok;
}

RenderScene FilterSceneToCatalog(const RenderScene& fixture_scene,
                                 const AcceptanceCatalog& catalog) {
  std::set<std::string> accepted_commands;
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    accepted_commands.insert(acceptance_case.fixture_command_ids.begin(),
                             acceptance_case.fixture_command_ids.end());
  }

  RenderScene scene = fixture_scene;
  scene.scene_id = "chart-1-conformance";
  scene.command_groups.clear();

  for (const CommandGroup& group : fixture_scene.command_groups) {
    CommandGroup filtered = group;
    filtered.commands.clear();
    for (const RenderCommand& command : group.commands) {
      if (accepted_commands.count(command.command_id) != 0) {
        filtered.commands.push_back(command);
      }
    }
    if (!filtered.commands.empty()) {
      scene.command_groups.push_back(std::move(filtered));
    }
  }

  return scene;
}

}  // namespace

bool ValidateAcceptanceAgainstScene(const AcceptanceCatalog& catalog,
                                    const RenderScene& scene,
                                    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = ValidateAcceptanceCatalog(catalog, &out);
  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    ok = ValidateCase(acceptance_case, scene, &out) && ok;
  }
  ok = ValidateRenderSceneTraceability(scene, &out) && ok;
  return ok;
}

ConformanceScene BuildConformanceScene(RenderView view, DisplayState display) {
  s52::S52CommandBuilder builder;
  AcceptanceCatalog catalog = BuildAcceptanceCatalog();
  RenderScene fixture_scene =
      builder.BuildFixtureScene(std::move(view), std::move(display));

  ConformanceScene result;
  result.scene = FilterSceneToCatalog(fixture_scene, catalog);
  result.ok = ValidateAcceptanceAgainstScene(catalog, result.scene,
                                             &result.diagnostics);

  for (const AcceptanceCase& acceptance_case : catalog.cases) {
    for (const std::string& command_id : acceptance_case.fixture_command_ids) {
      result.case_results.push_back(
          {acceptance_case.case_id, command_id,
           FindCommand(result.scene, command_id) != nullptr});
    }
  }

  return result;
}

}  // namespace ocpn::render::chart1
