// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "conversion_trace.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace ocpn::render {
namespace {

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Inspect conversion provenance before changing renderer code.";
  return diagnostic;
}

GeoBounds EmptyBounds() {
  GeoBounds bounds;
  bounds.west = std::numeric_limits<double>::max();
  bounds.south = std::numeric_limits<double>::max();
  bounds.east = std::numeric_limits<double>::lowest();
  bounds.north = std::numeric_limits<double>::lowest();
  return bounds;
}

bool IsEmptyBounds(const GeoBounds& bounds) {
  return bounds.west > bounds.east || bounds.south > bounds.north;
}

void IncludePoint(GeoBounds* bounds, Point2 point) {
  bounds->west = std::min(bounds->west, point.x);
  bounds->south = std::min(bounds->south, point.y);
  bounds->east = std::max(bounds->east, point.x);
  bounds->north = std::max(bounds->north, point.y);
}

GeoBounds BoundsForGeometry(const Geometry& geometry) {
  GeoBounds bounds = EmptyBounds();
  for (const Point2& point : geometry.points) {
    IncludePoint(&bounds, point);
  }
  for (const std::vector<Point2>& ring : geometry.rings) {
    for (const Point2& point : ring) {
      IncludePoint(&bounds, point);
    }
  }
  return IsEmptyBounds(bounds) ? GeoBounds{} : bounds;
}

std::string MetadataValue(const RenderCommand& command, const char* key) {
  const auto it = command.metadata.find(key);
  return it == command.metadata.end() ? std::string{} : it->second;
}

bool CommandNeedsGeometry(const RenderCommand& command) {
  switch (command.type) {
    case CommandType::kFillArea:
    case CommandType::kStrokeLine:
    case CommandType::kDrawRasterSheet:
    case CommandType::kPushClip:
      return true;
    case CommandType::kPlaceSymbol:
    case CommandType::kDrawText:
    case CommandType::kDrawSounding:
    case CommandType::kPopClip:
      return false;
  }
  return false;
}

}  // namespace

const char* ToString(ConversionTraceStage stage) {
  switch (stage) {
    case ConversionTraceStage::kSourceObject:
      return "source_object";
    case ConversionTraceStage::kProjection:
      return "projection";
    case ConversionTraceStage::kGeometryGeneration:
      return "geometry_generation";
    case ConversionTraceStage::kS52Rule:
      return "s52_rule";
    case ConversionTraceStage::kTilePlacement:
      return "tile_placement";
    case ConversionTraceStage::kRenderCommand:
      return "render_command";
  }
  return "unknown";
}

ConversionTraceRecord BuildConversionTrace(const ProvenanceRecord& provenance,
                                           const RenderCommand& command,
                                           const Geometry& geometry,
                                           const RenderView& view,
                                           const std::string& s52_rule_id) {
  ConversionTraceRecord trace;
  trace.trace_id = "trace:" + command.command_id + ":" + geometry.geometry_id;
  trace.provenance_id = provenance.provenance_id;
  trace.source_chart_id = provenance.source_chart_id;
  trace.source_object_id = provenance.source_object_id;
  trace.source_object_class = provenance.source_object_class;
  trace.source_geometry_hash = provenance.source_geometry_hash;
  trace.generated_geometry_id = geometry.geometry_id;
  trace.render_command_id = command.command_id;
  trace.s52_rule_id =
      s52_rule_id.empty() ? MetadataValue(command, "s52_rule") : s52_rule_id;
  trace.target_id = view.view_id;
  trace.source_bounds = view.geographic_bbox;
  trace.target_bounds = BoundsForGeometry(geometry);
  trace.transform_chain = provenance.transform_chain;
  trace.transform_chain.push_back(ToString(ConversionTraceStage::kS52Rule));
  trace.transform_chain.push_back(ToString(ConversionTraceStage::kRenderCommand));

  if (!geometry.points.empty()) {
    CoordinateTraceSample sample;
    sample.projected = geometry.points.front();
    sample.target = geometry.points.front();
    trace.coordinate_samples.push_back(sample);
  } else if (!geometry.rings.empty() && !geometry.rings.front().empty()) {
    CoordinateTraceSample sample;
    sample.projected = geometry.rings.front().front();
    sample.target = geometry.rings.front().front();
    trace.coordinate_samples.push_back(sample);
  }

  return trace;
}

void AttachConversionTrace(RenderCommand* command,
                           const ConversionTraceRecord& trace) {
  if (!command) {
    return;
  }
  command->conversion_trace_refs.push_back(trace.trace_id);
  command->metadata["conversion_trace_id"] = trace.trace_id;
  command->metadata["generated_geometry_id"] = trace.generated_geometry_id;
  command->metadata["s52_rule"] = trace.s52_rule_id;
  command->metadata["target_id"] = trace.target_id;
}

Diagnostic MakeWrongLocationDiagnostic(const ConversionTraceRecord& trace,
                                       const std::string& message) {
  Diagnostic diagnostic =
      MakeDiagnostic(DiagnosticSeverity::kError, "wrong_location_trace",
                     message, {trace.provenance_id});
  diagnostic.message += " source_chart=" + trace.source_chart_id;
  diagnostic.message += " source_object=" + trace.source_object_id;
  diagnostic.message += " command=" + trace.render_command_id;
  diagnostic.message += " geometry=" + trace.generated_geometry_id;
  diagnostic.message += " s52_rule=" + trace.s52_rule_id;
  return diagnostic;
}

bool ValidateRenderSceneTraceability(const RenderScene& scene,
                                     std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  for (const CommandGroup& group : scene.command_groups) {
    for (const RenderCommand& command : group.commands) {
      if (command.command_id.empty()) {
        out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                     "trace_missing_command_id",
                                     "Render command is missing command_id.",
                                     command.provenance_refs));
        ok = false;
      }
      if (command.provenance_refs.empty()) {
        out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                     "trace_missing_provenance",
                                     "Render command has no provenance_refs.",
                                     command.provenance_refs));
        ok = false;
      }
      if (command.conversion_trace_refs.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "trace_missing_conversion_trace",
            "Render command has no conversion_trace_refs.",
            command.provenance_refs));
        ok = false;
      }
      if (MetadataValue(command, "s52_rule").empty()) {
        out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                     "trace_missing_s52_rule",
                                     "Render command is missing s52_rule.",
                                     command.provenance_refs));
        ok = false;
      }
      if (CommandNeedsGeometry(command) && command.geometries.empty()) {
        out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                     "trace_missing_geometry",
                                     "Render command has no geometry.",
                                     command.provenance_refs));
        ok = false;
      }
      for (const Geometry& geometry : command.geometries) {
        if (geometry.geometry_id.empty()) {
          out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                       "trace_missing_geometry_id",
                                       "Render geometry is missing geometry_id.",
                                       command.provenance_refs));
          ok = false;
        }
      }
    }
  }

  return ok;
}

}  // namespace ocpn::render
