// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "gpu_artifact_cache_contract.hpp"

#include <algorithm>
#include <cctype>
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
      "Keep source, presentation, quilting, and scheduler semantics before the "
      "machine-local GPU artifact cache; cache records are rebuildable backend "
      "artifacts with provenance handles.";
  return diagnostic;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ContainsForbiddenPolicyWord(const std::string& value) {
  const std::string lower = ToLower(value);
  for (const char* token :
       {"s52", "s-52", "s101", "s-101", "chart_source", "chart-source",
        "mbtiles", "pmtiles", "quilting", "scheduler", "prefetch",
        "display_category", "scamin"}) {
    if (lower.find(token) != std::string::npos) return true;
  }
  return false;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto found = metadata.find(key);
  return found == metadata.end() ? std::string{} : found->second;
}

std::map<std::string, ResourceRecord> ResourcesById(const ResourceTable& table) {
  std::map<std::string, ResourceRecord> by_id;
  for (const ResourceRecord& resource : table.resources) {
    if (!resource.resource_id.empty()) by_id[resource.resource_id] = resource;
  }
  return by_id;
}

void AppendUnique(std::vector<std::string>* values,
                  const std::vector<std::string>& incoming) {
  for (const std::string& value : incoming) {
    if (!value.empty() &&
        std::find(values->begin(), values->end(), value) == values->end()) {
      values->push_back(value);
    }
  }
}

std::string RequiredRef(std::string value) {
  return value == "none" ? std::string{} : std::move(value);
}

std::string ResourceRefFor(const NauticalPrimitive& primitive) {
  if (!RequiredRef(primitive.texture_ref).empty()) return primitive.texture_ref;
  if (!RequiredRef(primitive.symbol_ref).empty()) return primitive.symbol_ref;
  if (!RequiredRef(primitive.pattern_ref).empty()) return primitive.pattern_ref;
  if (!RequiredRef(primitive.line_style_ref).empty()) return primitive.line_style_ref;
  if (!RequiredRef(primitive.fill_ref).empty()) return primitive.fill_ref;
  if (!RequiredRef(primitive.font_ref).empty()) return primitive.font_ref;
  return RequiredRef(primitive.cache_key.resource_key);
}

std::uint32_t GeometryPointCount(const NauticalPrimitive& primitive) {
  std::uint32_t count = 0;
  for (const Geometry& geometry : primitive.geometries) {
    count += static_cast<std::uint32_t>(geometry.points.size());
    for (const std::vector<Point2>& ring : geometry.rings) {
      count += static_cast<std::uint32_t>(ring.size());
    }
  }
  return count;
}

std::uint32_t GeometryIndexCount(const NauticalPrimitive& primitive) {
  std::uint32_t count = 0;
  for (const Geometry& geometry : primitive.geometries) {
    if (geometry.points.size() > 1) {
      count += static_cast<std::uint32_t>((geometry.points.size() - 1) * 2);
    }
    for (const std::vector<Point2>& ring : geometry.rings) {
      if (ring.size() >= 3) {
        count += static_cast<std::uint32_t>((ring.size() - 2) * 3);
      }
    }
  }
  return count;
}

std::uint64_t ByteSizeForPixels(const std::map<std::string, std::string>& metrics,
                                std::uint64_t fallback) {
  const std::string value = MetadataValue(metrics, "pixel_size");
  if (value.empty()) return fallback;
  std::size_t sep = value.find('x');
  if (sep == std::string::npos) sep = value.find(',');
  if (sep == std::string::npos) return fallback;
  try {
    const std::uint64_t width = std::stoull(value.substr(0, sep));
    const std::uint64_t height = std::stoull(value.substr(sep + 1));
    if (width == 0 || height == 0) return fallback;
    return width * height * 4U;
  } catch (...) {
    return fallback;
  }
}

std::string ModelKeyFor(const NauticalRenderModel& model) {
  std::ostringstream out;
  out << model.model_id << ":" << model.source_epoch << ":"
      << model.render_view.view_id;
  return out.str();
}

GpuArtifactCacheKey CacheKey(const GpuArtifactCacheOptions& options,
                             const std::string& model_key,
                             std::string artifact_key,
                             std::string content_key) {
  GpuArtifactCacheKey key;
  key.namespace_id = options.cache_namespace;
  key.backend_target = options.backend_target;
  key.device_profile = options.device_profile;
  key.material_profile = options.material_profile;
  key.model_key = model_key;
  key.artifact_key = std::move(artifact_key);
  key.content_key = std::move(content_key);
  key.invalidation_epoch = options.invalidation_epoch;
  return key;
}

std::string StableArtifactId(const GpuArtifactCacheKey& key,
                             GpuArtifactKind kind) {
  std::ostringstream out;
  out << key.namespace_id << "/" << key.backend_target << "/"
      << key.device_profile << "/" << key.material_profile << "/"
      << ToString(kind) << "/" << key.model_key << "/" << key.artifact_key
      << "/" << key.content_key << "/" << key.invalidation_epoch;
  return out.str();
}

std::string ResourceTypeString(ResourceType type) {
  switch (type) {
    case ResourceType::kSymbol:
      return "symbol";
    case ResourceType::kLineStyle:
      return "line_style";
    case ResourceType::kAreaPattern:
      return "area_pattern";
    case ResourceType::kFont:
      return "font";
    case ResourceType::kRasterTexture:
      return "raster_texture";
    case ResourceType::kGeometryBuffer:
      return "geometry_buffer";
    case ResourceType::kPalette:
      return "palette";
  }
  return "unknown";
}

GpuArtifactKind ResourceArtifactKind(ResourceType type) {
  switch (type) {
    case ResourceType::kRasterTexture:
      return GpuArtifactKind::kRasterTexture;
    case ResourceType::kSymbol:
    case ResourceType::kAreaPattern:
      return GpuArtifactKind::kTextureAtlas;
    case ResourceType::kFont:
      return GpuArtifactKind::kGlyphAtlas;
    case ResourceType::kLineStyle:
      return GpuArtifactKind::kLinePattern;
    case ResourceType::kPalette:
      return GpuArtifactKind::kUniformBlock;
    case ResourceType::kGeometryBuffer:
      return GpuArtifactKind::kVertexBuffer;
  }
  return GpuArtifactKind::kTextureAtlas;
}

std::string ContentKeyFor(const ResourceRecord& resource) {
  if (!resource.content_hash.empty()) {
    return resource.resource_id + ":" + resource.content_hash;
  }
  return resource.resource_id;
}

std::uint64_t ResourceByteSize(const ResourceRecord& resource,
                               GpuArtifactKind kind) {
  if (kind == GpuArtifactKind::kRasterTexture ||
      kind == GpuArtifactKind::kTextureAtlas ||
      kind == GpuArtifactKind::kGlyphAtlas) {
    return ByteSizeForPixels(resource.metrics, 4096U);
  }
  return 256U;
}

GpuArtifactTierHandle TierFor(const NauticalPrimitive& primitive) {
  GpuArtifactTierHandle tier;
  tier.semantic_owner = primitive.handoff.semantic_owner.empty()
                            ? std::string("presentation_compiler")
                            : primitive.handoff.semantic_owner;
  const std::string explicit_tier =
      MetadataValue(primitive.metadata, "semantic_tier");
  if (!explicit_tier.empty()) tier.semantic_tier = explicit_tier;
  tier.source_standard = MetadataValue(primitive.metadata, "source_standard");
  if (tier.source_standard.empty() &&
      !primitive.trace.presentation_rule_id.empty()) {
    tier.source_standard = "s52-compatible";
  }
  tier.provenance_refs = primitive.trace.provenance_refs;
  tier.primitive_ids = {primitive.primitive_id};
  return tier;
}

bool IsGeometryPrimitive(NauticalPrimitiveType type) {
  switch (type) {
    case NauticalPrimitiveType::kAreaFill:
    case NauticalPrimitiveType::kLineStroke:
    case NauticalPrimitiveType::kRasterPatch:
    case NauticalPrimitiveType::kContourLine:
    case NauticalPrimitiveType::kClipBoundary:
      return true;
    case NauticalPrimitiveType::kSymbolInstance:
    case NauticalPrimitiveType::kTextLabel:
    case NauticalPrimitiveType::kSounding:
      return false;
  }
  return false;
}

bool IsInstancePrimitive(NauticalPrimitiveType type) {
  switch (type) {
    case NauticalPrimitiveType::kSymbolInstance:
    case NauticalPrimitiveType::kTextLabel:
    case NauticalPrimitiveType::kSounding:
      return true;
    case NauticalPrimitiveType::kAreaFill:
    case NauticalPrimitiveType::kLineStroke:
    case NauticalPrimitiveType::kRasterPatch:
    case NauticalPrimitiveType::kContourLine:
    case NauticalPrimitiveType::kClipBoundary:
      return false;
  }
  return false;
}

void AddArtifact(GpuArtifactCacheManifest* manifest,
                 GpuArtifactRecord artifact) {
  manifest->stats.estimated_bytes += artifact.byte_size;
  switch (artifact.kind) {
    case GpuArtifactKind::kVertexBuffer:
    case GpuArtifactKind::kIndexBuffer:
    case GpuArtifactKind::kUniformBlock:
      ++manifest->stats.buffer_artifacts;
      break;
    case GpuArtifactKind::kRasterTexture:
      ++manifest->stats.texture_artifacts;
      break;
    case GpuArtifactKind::kTextureAtlas:
    case GpuArtifactKind::kGlyphAtlas:
    case GpuArtifactKind::kLinePattern:
      ++manifest->stats.atlas_artifacts;
      break;
    case GpuArtifactKind::kMaterialPipeline:
      ++manifest->stats.material_artifacts;
      break;
    case GpuArtifactKind::kViewportTileEntry:
      ++manifest->stats.viewport_artifacts;
      break;
  }
  manifest->artifacts.push_back(std::move(artifact));
}

void AddGeometryArtifacts(const NauticalPrimitive& primitive,
                          const GpuArtifactCacheOptions& options,
                          const std::string& model_key,
                          GpuArtifactCacheManifest* manifest) {
  const std::uint32_t vertices = std::max<std::uint32_t>(1U, GeometryPointCount(primitive));
  const std::uint32_t indices = GeometryIndexCount(primitive);
  const std::string type = ToString(primitive.type);

  GpuArtifactRecord vertex;
  vertex.kind = GpuArtifactKind::kVertexBuffer;
  vertex.residency = GpuArtifactResidency::kMachineLocal;
  vertex.cache_key = CacheKey(options, model_key,
                              primitive.cache_key.primitive_key + ":vertices",
                              type + ":" + primitive.primitive_id);
  vertex.artifact_id = StableArtifactId(vertex.cache_key, vertex.kind);
  vertex.tier = TierFor(primitive);
  vertex.usage = "neutral_geometry_vertices";
  vertex.primitive_type = type;
  vertex.material_key = "geometry-material:" + primitive.role;
  vertex.pipeline_key = options.material_profile + ":geometry";
  vertex.invalidation_domain = "presentation_epoch";
  vertex.vertices = vertices;
  vertex.byte_size = static_cast<std::uint64_t>(vertices) * 24U;
  AddArtifact(manifest, std::move(vertex));

  if (indices == 0) return;
  GpuArtifactRecord index;
  index.kind = GpuArtifactKind::kIndexBuffer;
  index.residency = GpuArtifactResidency::kMachineLocal;
  index.cache_key = CacheKey(options, model_key,
                             primitive.cache_key.primitive_key + ":indices",
                             type + ":" + primitive.primitive_id);
  index.artifact_id = StableArtifactId(index.cache_key, index.kind);
  index.tier = TierFor(primitive);
  index.usage = "neutral_geometry_indices";
  index.primitive_type = type;
  index.material_key = "geometry-material:" + primitive.role;
  index.pipeline_key = options.material_profile + ":geometry";
  index.invalidation_domain = "presentation_epoch";
  index.indices = indices;
  index.byte_size = static_cast<std::uint64_t>(indices) * 4U;
  AddArtifact(manifest, std::move(index));
}

void AddInstanceArtifact(const NauticalPrimitive& primitive,
                         const GpuArtifactCacheOptions& options,
                         const std::string& model_key,
                         GpuArtifactCacheManifest* manifest) {
  GpuArtifactRecord artifact;
  artifact.kind = GpuArtifactKind::kUniformBlock;
  artifact.residency = GpuArtifactResidency::kFrameLocal;
  artifact.cache_key = CacheKey(options, model_key,
                                primitive.cache_key.primitive_key + ":instance",
                                std::string(ToString(primitive.type)) + ":" +
                                    primitive.primitive_id);
  artifact.artifact_id = StableArtifactId(artifact.cache_key, artifact.kind);
  artifact.tier = TierFor(primitive);
  artifact.usage = "neutral_instance_data";
  artifact.primitive_type = ToString(primitive.type);
  artifact.resource_id = ResourceRefFor(primitive);
  artifact.material_key = "instance-material:" + primitive.role;
  artifact.pipeline_key = options.material_profile + ":instance";
  artifact.invalidation_domain = "frame_state";
  artifact.instances = 1;
  artifact.byte_size = 96U;
  artifact.memory_priority = 10;
  AddArtifact(manifest, std::move(artifact));
}

void AddMaterialArtifact(const NauticalPrimitive& primitive,
                         const GpuArtifactCacheOptions& options,
                         const std::string& model_key,
                         GpuArtifactCacheManifest* manifest) {
  GpuArtifactRecord artifact;
  artifact.kind = GpuArtifactKind::kMaterialPipeline;
  artifact.residency = GpuArtifactResidency::kMachineLocal;
  artifact.cache_key = CacheKey(options, model_key,
                                "material:" + std::string(ToString(primitive.type)) +
                                    ":" + primitive.role,
                                options.material_profile + ":" +
                                    std::string(ToString(primitive.type)));
  artifact.artifact_id = StableArtifactId(artifact.cache_key, artifact.kind);
  artifact.tier = TierFor(primitive);
  artifact.usage = "neutral_material_pipeline";
  artifact.primitive_type = ToString(primitive.type);
  artifact.material_key = std::string("material:") + ToString(primitive.type) +
                          ":" + primitive.role;
  artifact.pipeline_key = options.backend_target + ":" + options.material_profile +
                          ":" + ToString(primitive.type);
  artifact.invalidation_domain = "material_profile";
  artifact.byte_size = 512U;
  artifact.memory_priority = 90;
  AddArtifact(manifest, std::move(artifact));
}

void AddResourceArtifact(const ResourceRecord& resource,
                         const GpuArtifactCacheOptions& options,
                         const std::string& model_key,
                         const std::vector<std::string>& primitive_ids,
                         const std::vector<std::string>& provenance_refs,
                         GpuArtifactCacheManifest* manifest) {
  const GpuArtifactKind kind = ResourceArtifactKind(resource.type);
  GpuArtifactRecord artifact;
  artifact.kind = kind;
  artifact.residency = GpuArtifactResidency::kMachineLocal;
  artifact.cache_key = CacheKey(options, model_key,
                                "resource:" + resource.resource_id,
                                ContentKeyFor(resource));
  artifact.artifact_id = StableArtifactId(artifact.cache_key, artifact.kind);
  artifact.tier.semantic_tier =
      MetadataValue(resource.backend_hints, "semantic_tier").empty()
          ? "tier1_official_chart"
          : MetadataValue(resource.backend_hints, "semantic_tier");
  artifact.tier.semantic_owner = "presentation_compiler";
  artifact.tier.source_standard = MetadataValue(resource.backend_hints, "source_standard");
  artifact.tier.primitive_ids = primitive_ids;
  artifact.tier.provenance_refs = provenance_refs;
  if (!resource.provenance_id.empty() &&
      std::find(artifact.tier.provenance_refs.begin(),
                artifact.tier.provenance_refs.end(),
                resource.provenance_id) == artifact.tier.provenance_refs.end()) {
    artifact.tier.provenance_refs.push_back(resource.provenance_id);
  }
  artifact.resource_id = resource.resource_id;
  artifact.resource_type = ResourceTypeString(resource.type);
  artifact.usage =
      kind == GpuArtifactKind::kRasterTexture
          ? "sampled_raster_texture"
          : (kind == GpuArtifactKind::kGlyphAtlas
                 ? "sampled_glyph_atlas"
                 : (kind == GpuArtifactKind::kLinePattern
                        ? "sampled_line_pattern"
                        : "sampled_texture_atlas"));
  artifact.material_key = "resource-material:" + artifact.resource_type;
  artifact.pipeline_key = options.material_profile + ":resource";
  artifact.invalidation_domain = "resource_content";
  artifact.byte_size = ResourceByteSize(resource, kind);
  artifact.compressed = kind == GpuArtifactKind::kRasterTexture &&
                        MetadataValue(resource.backend_hints, "compressed") == "true";
  AddArtifact(manifest, std::move(artifact));
}

void AddViewUniformArtifact(const NauticalRenderModel& model,
                            const GpuArtifactCacheOptions& options,
                            const std::string& model_key,
                            GpuArtifactCacheManifest* manifest) {
  GpuArtifactRecord artifact;
  artifact.kind = GpuArtifactKind::kUniformBlock;
  artifact.residency = GpuArtifactResidency::kSceneLocal;
  artifact.cache_key = CacheKey(options, model_key, "view-display-state",
                                model.render_view.view_id);
  artifact.artifact_id = StableArtifactId(artifact.cache_key, artifact.kind);
  artifact.usage = "view_display_uniforms";
  artifact.material_key = "view-display";
  artifact.pipeline_key = options.material_profile + ":view";
  artifact.invalidation_domain = "view_display_state";
  artifact.byte_size = 256U;
  artifact.memory_priority = 1;
  AddArtifact(manifest, std::move(artifact));
}

void AddViewportTileArtifact(const NauticalRenderModel& model,
                             const GpuArtifactCacheOptions& options,
                             const std::string& model_key,
                             GpuArtifactCacheManifest* manifest) {
  if (!options.include_viewport_tile_entries) return;
  GpuArtifactRecord artifact;
  artifact.kind = GpuArtifactKind::kViewportTileEntry;
  artifact.residency = GpuArtifactResidency::kViewportLocal;
  artifact.cache_key = CacheKey(options, model_key, "viewport-tile-entry",
                                model.render_view.view_id);
  artifact.artifact_id = StableArtifactId(artifact.cache_key, artifact.kind);
  artifact.usage = "viewport_artifact_entry";
  artifact.material_key = "viewport";
  artifact.pipeline_key = options.material_profile + ":viewport";
  artifact.invalidation_domain = "viewport_cache_epoch";
  artifact.byte_size = 128U;
  artifact.memory_priority = 5;
  AddArtifact(manifest, std::move(artifact));
}

}  // namespace

const char* ToString(GpuArtifactKind kind) {
  switch (kind) {
    case GpuArtifactKind::kVertexBuffer:
      return "vertex_buffer";
    case GpuArtifactKind::kIndexBuffer:
      return "index_buffer";
    case GpuArtifactKind::kUniformBlock:
      return "uniform_block";
    case GpuArtifactKind::kRasterTexture:
      return "raster_texture";
    case GpuArtifactKind::kTextureAtlas:
      return "texture_atlas";
    case GpuArtifactKind::kGlyphAtlas:
      return "glyph_atlas";
    case GpuArtifactKind::kLinePattern:
      return "line_pattern";
    case GpuArtifactKind::kMaterialPipeline:
      return "material_pipeline";
    case GpuArtifactKind::kViewportTileEntry:
      return "viewport_tile_entry";
  }
  return "unknown";
}

const char* ToString(GpuArtifactResidency residency) {
  switch (residency) {
    case GpuArtifactResidency::kFrameLocal:
      return "frame_local";
    case GpuArtifactResidency::kViewportLocal:
      return "viewport_local";
    case GpuArtifactResidency::kSceneLocal:
      return "scene_local";
    case GpuArtifactResidency::kMachineLocal:
      return "machine_local";
  }
  return "unknown";
}

GpuArtifactCacheManifest BuildGpuArtifactCacheManifest(
    const NauticalRenderModel& model,
    const GpuArtifactCacheOptions& requested_options) {
  GpuArtifactCacheOptions options = requested_options;
  if (options.invalidation_epoch.empty()) {
    options.invalidation_epoch = model.source_epoch + ":" + model.render_view.view_id;
  }

  GpuArtifactCacheManifest manifest;
  manifest.manifest_id = options.cache_namespace + ":" + options.backend_target +
                         ":" + model.model_id + ":" + model.source_epoch +
                         ":" + options.device_profile;
  manifest.input_model_id = model.model_id;
  manifest.input_model_epoch = model.source_epoch;
  manifest.options = options;
  manifest.stats.memory_budget_bytes = options.memory_budget_bytes;

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(model, &model_diagnostics);
  manifest.diagnostics.insert(manifest.diagnostics.end(),
                              model_diagnostics.begin(),
                              model_diagnostics.end());

  const std::string model_key = ModelKeyFor(model);
  AddViewUniformArtifact(model, options, model_key, &manifest);
  AddViewportTileArtifact(model, options, model_key, &manifest);

  const std::map<std::string, ResourceRecord> resource_by_id =
      ResourcesById(model.resource_table);
  std::map<std::string, std::vector<std::string>> primitive_ids_by_resource;
  std::map<std::string, std::vector<std::string>> provenance_refs_by_resource;
  std::set<std::string> material_keys;

  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      const std::string resource_id = ResourceRefFor(primitive);
      if (!resource_id.empty()) {
        primitive_ids_by_resource[resource_id].push_back(primitive.primitive_id);
        AppendUnique(&provenance_refs_by_resource[resource_id],
                     primitive.trace.provenance_refs);
      }

      if (IsGeometryPrimitive(primitive.type)) {
        AddGeometryArtifacts(primitive, options, model_key, &manifest);
      }
      if (IsInstancePrimitive(primitive.type)) {
        AddInstanceArtifact(primitive, options, model_key, &manifest);
      }

      const std::string material_key = std::string(ToString(primitive.type)) +
                                       ":" + primitive.role;
      if (material_keys.insert(material_key).second) {
        AddMaterialArtifact(primitive, options, model_key, &manifest);
      }
    }
  }

  for (const auto& entry : primitive_ids_by_resource) {
    const auto resource_it = resource_by_id.find(entry.first);
    if (resource_it == resource_by_id.end()) continue;
    AddResourceArtifact(resource_it->second, options, model_key, entry.second,
                        provenance_refs_by_resource[entry.first], &manifest);
  }

  manifest.stats.over_budget =
      manifest.stats.memory_budget_bytes > 0 &&
      manifest.stats.estimated_bytes > manifest.stats.memory_budget_bytes;
  if (manifest.stats.over_budget) {
    manifest.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kWarning, "gpu_artifact_cache_budget",
        "GPU artifact cache estimate exceeds the configured memory budget."));
  }

  return manifest;
}

bool ValidateGpuArtifactCacheManifest(
    const GpuArtifactCacheManifest& manifest,
    std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (manifest.schema_version != kGpuArtifactCacheSchemaVersion ||
      manifest.manifest_id.empty() || manifest.input_model_id.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "gpu_artifact_cache_identity",
                                 "GPU artifact cache manifest has invalid identity."));
    ok = false;
  }
  if (manifest.input_contract != "backend-neutral-nautical-render-model" ||
      manifest.cache_scope != "machine-local-gpu-artifacts" ||
      manifest.cache_owner != "runtime_gpu_artifact_cache" ||
      manifest.semantic_owner == "backend") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "gpu_artifact_cache_boundary",
        "GPU artifact cache must be fed by the neutral model and must not own "
        "chart semantics."));
    ok = false;
  }
  if (manifest.options.backend_target.empty() ||
      manifest.options.device_profile.empty() ||
      manifest.options.material_profile.empty() ||
      manifest.options.cache_namespace.empty() ||
      manifest.options.memory_budget_bytes == 0) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "gpu_artifact_cache_options",
                                 "GPU artifact cache options are incomplete."));
    ok = false;
  }

  bool saw_machine_asset = false;
  bool saw_provenance = false;
  bool saw_material = false;
  bool saw_viewport = false;
  std::uint64_t estimated_bytes = 0;
  std::set<std::string> artifact_ids;
  for (const GpuArtifactRecord& artifact : manifest.artifacts) {
    estimated_bytes += artifact.byte_size;
    if (artifact.artifact_id.empty() || artifact.cache_key.artifact_key.empty() ||
        artifact.cache_key.content_key.empty() ||
        artifact.cache_key.invalidation_epoch.empty()) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "gpu_artifact_cache_key",
                                   "GPU artifact cache record is missing a stable key.",
                                   artifact.tier.provenance_refs));
      ok = false;
    }
    if (!artifact_ids.insert(artifact.artifact_id).second) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "gpu_artifact_cache_duplicate",
                                   "GPU artifact cache id is duplicated.",
                                   artifact.tier.provenance_refs));
      ok = false;
    }
    if (!artifact.rebuildable || !artifact.device_specific) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "gpu_artifact_cache_rebuildable",
          "GPU artifact cache records must be rebuildable and device-specific.",
          artifact.tier.provenance_refs));
      ok = false;
    }
    if (artifact.byte_size == 0) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "gpu_artifact_cache_empty_artifact",
                                   "GPU artifact cache record has zero byte size.",
                                   artifact.tier.provenance_refs));
      ok = false;
    }
    if (ContainsForbiddenPolicyWord(artifact.usage) ||
        ContainsForbiddenPolicyWord(artifact.material_key) ||
        ContainsForbiddenPolicyWord(artifact.pipeline_key)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "gpu_artifact_cache_policy_leak",
          "GPU artifact usage/material/pipeline key names source or presentation "
          "policy.",
          artifact.tier.provenance_refs));
      ok = false;
    }
    if (artifact.tier.semantic_tier.empty() ||
        artifact.tier.semantic_owner.empty()) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "gpu_artifact_cache_tier",
          "GPU artifact cache record is missing tier or semantic-owner handle.",
          artifact.tier.provenance_refs));
      ok = false;
    }
    if (!artifact.tier.provenance_refs.empty() ||
        !artifact.tier.primitive_ids.empty()) {
      saw_provenance = true;
    }
    if (artifact.residency == GpuArtifactResidency::kMachineLocal) {
      saw_machine_asset = true;
    }
    if (artifact.kind == GpuArtifactKind::kMaterialPipeline) {
      saw_material = true;
    }
    if (artifact.kind == GpuArtifactKind::kViewportTileEntry) {
      saw_viewport = true;
    }
  }

  if (!saw_machine_asset || !saw_material || !saw_viewport || !saw_provenance) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "gpu_artifact_cache_coverage",
        "GPU artifact cache must include machine-local artifacts, material "
        "records, viewport entries, and provenance/tier handles."));
    ok = false;
  }
  if (manifest.stats.estimated_bytes != estimated_bytes ||
      manifest.stats.memory_budget_bytes != manifest.options.memory_budget_bytes) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "gpu_artifact_cache_stats",
                                 "GPU artifact cache stats do not match records."));
    ok = false;
  }
  if (manifest.stats.over_budget) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kWarning, "gpu_artifact_cache_budget",
        "GPU artifact cache estimate exceeds the configured memory budget."));
  }
  return ok;
}

}  // namespace ocpn::render
