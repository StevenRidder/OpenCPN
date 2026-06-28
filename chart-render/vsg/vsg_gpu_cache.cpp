// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "vsg/vsg_gpu_cache.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace ocpn::render::vsg {
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
      "Keep chart-source, S-52/S-101, quilting, and scheduler semantics before "
      "the neutral render model; VSG cache stores only compiled GPU assets.";
  return diagnostic;
}

std::string MetadataValue(const std::map<std::string, std::string>& metadata,
                          const char* key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
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
        "mbtiles", "pmtiles", "quilting", "scheduler", "prefetch"}) {
    if (lower.find(token) != std::string::npos) return true;
  }
  return false;
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

std::map<std::string, ResourceRecord> ResourcesById(const ResourceTable& table) {
  std::map<std::string, ResourceRecord> by_id;
  for (const ResourceRecord& resource : table.resources) {
    if (!resource.resource_id.empty()) by_id[resource.resource_id] = resource;
  }
  return by_id;
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

std::string ModelKeyFor(const NauticalRenderModel& model) {
  std::ostringstream out;
  out << model.model_id << ":" << model.source_epoch << ":"
      << model.render_view.view_id;
  return out.str();
}

std::string StableAssetId(const VsgGpuCacheKey& key, VsgGpuAssetKind kind) {
  std::ostringstream out;
  out << key.namespace_id << "/" << key.device_profile << "/"
      << key.shader_profile << "/" << ToString(kind) << "/" << key.model_key
      << "/" << key.asset_key << "/" << key.content_key;
  return out.str();
}

VsgGpuCacheKey CacheKey(const VsgGpuCacheOptions& options,
                        const std::string& model_key,
                        std::string asset_key,
                        std::string content_key) {
  VsgGpuCacheKey key;
  key.namespace_id = options.cache_namespace;
  key.device_profile = options.device_profile;
  key.shader_profile = options.shader_profile;
  key.model_key = model_key;
  key.asset_key = std::move(asset_key);
  key.content_key = std::move(content_key);
  return key;
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

void AppendUnique(std::vector<std::string>* values,
                  const std::vector<std::string>& incoming) {
  for (const std::string& value : incoming) {
    if (!value.empty() &&
        std::find(values->begin(), values->end(), value) == values->end()) {
      values->push_back(value);
    }
  }
}

VsgGpuAssetKind ResourceAssetKind(ResourceType type) {
  switch (type) {
    case ResourceType::kRasterTexture:
      return VsgGpuAssetKind::kTexture;
    case ResourceType::kSymbol:
    case ResourceType::kAreaPattern:
    case ResourceType::kFont:
      return VsgGpuAssetKind::kAtlas;
    case ResourceType::kPalette:
    case ResourceType::kLineStyle:
      return VsgGpuAssetKind::kUniformBlock;
    case ResourceType::kGeometryBuffer:
      return VsgGpuAssetKind::kVertexBuffer;
  }
  return VsgGpuAssetKind::kTexture;
}

std::string ContentKeyFor(const ResourceRecord& resource) {
  if (!resource.content_hash.empty()) {
    return resource.resource_id + ":" + resource.content_hash;
  }
  return resource.resource_id;
}

std::uint64_t ResourceByteSize(const ResourceRecord& resource,
                               VsgGpuAssetKind kind) {
  if (kind == VsgGpuAssetKind::kTexture || kind == VsgGpuAssetKind::kAtlas) {
    return ByteSizeForPixels(resource.metrics, 4096U);
  }
  return 256U;
}

void AddAsset(VsgGpuCacheManifest* manifest, VsgGpuAsset asset) {
  manifest->stats.estimated_bytes += asset.byte_size;
  switch (asset.kind) {
    case VsgGpuAssetKind::kTexture:
    case VsgGpuAssetKind::kAtlas:
      ++manifest->stats.texture_assets;
      break;
    case VsgGpuAssetKind::kVertexBuffer:
    case VsgGpuAssetKind::kIndexBuffer:
      ++manifest->stats.vector_buffer_assets;
      break;
    case VsgGpuAssetKind::kInstanceBuffer:
      ++manifest->stats.instance_buffer_assets;
      break;
    case VsgGpuAssetKind::kUniformBlock:
      ++manifest->stats.uniform_assets;
      break;
  }
  manifest->assets.push_back(std::move(asset));
}

void AddResourceAsset(const ResourceRecord& resource,
                      const VsgGpuCacheOptions& options,
                      const std::string& model_key,
                      const std::vector<std::string>& primitive_ids,
                      const std::vector<std::string>& provenance_refs,
                      VsgGpuCacheManifest* manifest) {
  const VsgGpuAssetKind kind = ResourceAssetKind(resource.type);
  VsgGpuAsset asset;
  asset.kind = kind;
  asset.residency = VsgGpuResidency::kMachineLocal;
  asset.cache_key = CacheKey(options, model_key,
                             "resource:" + resource.resource_id,
                             ContentKeyFor(resource));
  asset.asset_id = StableAssetId(asset.cache_key, asset.kind);
  asset.resource_id = resource.resource_id;
  asset.resource_type = ResourceTypeString(resource.type);
  asset.usage = (kind == VsgGpuAssetKind::kTexture)
                    ? "sampled_texture"
                    : (kind == VsgGpuAssetKind::kAtlas ? "sampled_atlas"
                                                       : "descriptor_constants");
  asset.primitive_ids = primitive_ids;
  asset.provenance_refs = provenance_refs;
  if (!resource.provenance_id.empty() &&
      std::find(asset.provenance_refs.begin(), asset.provenance_refs.end(),
                resource.provenance_id) == asset.provenance_refs.end()) {
    asset.provenance_refs.push_back(resource.provenance_id);
  }
  asset.byte_size = ResourceByteSize(resource, kind);
  asset.descriptor_ready = true;
  AddAsset(manifest, std::move(asset));
}

void AddGeometryAssets(const NauticalPrimitive& primitive,
                       const VsgGpuCacheOptions& options,
                       const std::string& model_key,
                       VsgGpuCacheManifest* manifest) {
  const std::uint32_t vertices = std::max<std::uint32_t>(1U, GeometryPointCount(primitive));
  const std::uint32_t indices = GeometryIndexCount(primitive);
  const std::string type = ToString(primitive.type);

  VsgGpuAsset vertex;
  vertex.kind = VsgGpuAssetKind::kVertexBuffer;
  vertex.residency = VsgGpuResidency::kMachineLocal;
  vertex.cache_key = CacheKey(options, model_key,
                              primitive.cache_key.primitive_key + ":vertices",
                              type + ":" + primitive.primitive_id);
  vertex.asset_id = StableAssetId(vertex.cache_key, vertex.kind);
  vertex.primitive_type = type;
  vertex.usage = "neutral_geometry_vertices";
  vertex.primitive_ids = {primitive.primitive_id};
  vertex.provenance_refs = primitive.trace.provenance_refs;
  vertex.vertices = vertices;
  vertex.byte_size = static_cast<std::uint64_t>(vertices) * 24U;
  AddAsset(manifest, std::move(vertex));

  if (indices == 0) return;
  VsgGpuAsset index;
  index.kind = VsgGpuAssetKind::kIndexBuffer;
  index.residency = VsgGpuResidency::kMachineLocal;
  index.cache_key = CacheKey(options, model_key,
                             primitive.cache_key.primitive_key + ":indices",
                             type + ":" + primitive.primitive_id);
  index.asset_id = StableAssetId(index.cache_key, index.kind);
  index.primitive_type = type;
  index.usage = "neutral_geometry_indices";
  index.primitive_ids = {primitive.primitive_id};
  index.provenance_refs = primitive.trace.provenance_refs;
  index.indices = indices;
  index.byte_size = static_cast<std::uint64_t>(indices) * 4U;
  AddAsset(manifest, std::move(index));
}

void AddInstanceAsset(const NauticalPrimitive& primitive,
                      const VsgGpuCacheOptions& options,
                      const std::string& model_key,
                      VsgGpuCacheManifest* manifest) {
  VsgGpuAsset asset;
  asset.kind = VsgGpuAssetKind::kInstanceBuffer;
  asset.residency = VsgGpuResidency::kFrameLocal;
  asset.cache_key = CacheKey(options, model_key,
                             primitive.cache_key.primitive_key + ":instances",
                             std::string(ToString(primitive.type)) + ":" +
                                 primitive.primitive_id);
  asset.asset_id = StableAssetId(asset.cache_key, asset.kind);
  asset.primitive_type = ToString(primitive.type);
  asset.resource_id = ResourceRefFor(primitive);
  asset.usage = "neutral_instance_data";
  asset.primitive_ids = {primitive.primitive_id};
  asset.provenance_refs = primitive.trace.provenance_refs;
  asset.instances = 1;
  asset.byte_size = 96U;
  AddAsset(manifest, std::move(asset));
}

void AddViewUniformAsset(const NauticalRenderModel& model,
                         const VsgGpuCacheOptions& options,
                         const std::string& model_key,
                         VsgGpuCacheManifest* manifest) {
  VsgGpuAsset asset;
  asset.kind = VsgGpuAssetKind::kUniformBlock;
  asset.residency = VsgGpuResidency::kSceneLocal;
  asset.cache_key =
      CacheKey(options, model_key, "view-display-state", model.render_view.view_id);
  asset.asset_id = StableAssetId(asset.cache_key, asset.kind);
  asset.usage = "view_display_uniforms";
  asset.byte_size = 256U;
  asset.descriptor_ready = true;
  AddAsset(manifest, std::move(asset));
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

}  // namespace

const char* ToString(VsgGpuAssetKind kind) {
  switch (kind) {
    case VsgGpuAssetKind::kVertexBuffer:
      return "vertex_buffer";
    case VsgGpuAssetKind::kIndexBuffer:
      return "index_buffer";
    case VsgGpuAssetKind::kInstanceBuffer:
      return "instance_buffer";
    case VsgGpuAssetKind::kTexture:
      return "texture";
    case VsgGpuAssetKind::kAtlas:
      return "atlas";
    case VsgGpuAssetKind::kUniformBlock:
      return "uniform_block";
  }
  return "unknown";
}

const char* ToString(VsgGpuResidency residency) {
  switch (residency) {
    case VsgGpuResidency::kFrameLocal:
      return "frame_local";
    case VsgGpuResidency::kSceneLocal:
      return "scene_local";
    case VsgGpuResidency::kMachineLocal:
      return "machine_local";
  }
  return "unknown";
}

VsgGpuCacheManifest BuildVsgGpuCacheManifest(
    const NauticalRenderModel& model,
    const VsgGpuCacheOptions& options) {
  VsgGpuCacheManifest manifest;
  manifest.manifest_id = options.cache_namespace + ":" + model.model_id + ":" +
                         model.source_epoch + ":" + options.device_profile;
  manifest.input_model_id = model.model_id;
  manifest.input_model_epoch = model.source_epoch;
  manifest.options = options;

  std::vector<Diagnostic> model_diagnostics;
  ValidateNauticalRenderModel(model, &model_diagnostics);
  manifest.diagnostics.insert(manifest.diagnostics.end(),
                              model_diagnostics.begin(),
                              model_diagnostics.end());

  const std::string model_key = ModelKeyFor(model);
  AddViewUniformAsset(model, options, model_key, &manifest);

  const std::map<std::string, ResourceRecord> resource_by_id =
      ResourcesById(model.resource_table);
  std::map<std::string, std::vector<std::string>> primitive_ids_by_resource;
  std::map<std::string, std::vector<std::string>> provenance_refs_by_resource;

  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      const std::string resource_id = ResourceRefFor(primitive);
      if (!resource_id.empty()) {
        primitive_ids_by_resource[resource_id].push_back(primitive.primitive_id);
        AppendUnique(&provenance_refs_by_resource[resource_id],
                     primitive.trace.provenance_refs);
        if (resource_by_id.find(resource_id) == resource_by_id.end()) {
          manifest.diagnostics.push_back(MakeDiagnostic(
              DiagnosticSeverity::kError, "vsg_cache_missing_resource",
              "Neutral primitive references a resource that is not in the "
              "resource table: " +
                  resource_id,
              primitive.trace.provenance_refs));
        }
      }

      if (IsGeometryPrimitive(primitive.type)) {
        AddGeometryAssets(primitive, options, model_key, &manifest);
      }
      if (options.include_frame_local_assets &&
          IsInstancePrimitive(primitive.type)) {
        AddInstanceAsset(primitive, options, model_key, &manifest);
      }
    }
  }

  for (const auto& entry : primitive_ids_by_resource) {
    const auto resource_it = resource_by_id.find(entry.first);
    if (resource_it == resource_by_id.end()) continue;
    AddResourceAsset(resource_it->second, options, model_key, entry.second,
                     provenance_refs_by_resource[entry.first], &manifest);
  }

  return manifest;
}

bool ValidateVsgGpuCacheManifest(const VsgGpuCacheManifest& manifest,
                                 std::vector<Diagnostic>* diagnostics) {
  std::vector<Diagnostic> local_diagnostics;
  auto& out = diagnostics ? *diagnostics : local_diagnostics;

  bool ok = true;
  if (manifest.schema_version != kVsgGpuCacheSchemaVersion ||
      manifest.manifest_id.empty() || manifest.input_model_id.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "vsg_cache_identity",
                                 "VSG GPU cache manifest has invalid identity."));
    ok = false;
  }
  if (manifest.input_contract != "backend-neutral-nautical-render-model" ||
      manifest.cache_scope != "machine-local-vsg-gpu-assets" ||
      manifest.semantic_owner == "backend") {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "vsg_cache_boundary",
        "VSG GPU cache must be fed by the neutral model and must not own "
        "chart semantics."));
    ok = false;
  }
  if (manifest.options.device_profile.empty() ||
      manifest.options.shader_profile.empty() ||
      manifest.options.cache_namespace.empty()) {
    out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                 "vsg_cache_options",
                                 "VSG GPU cache options are incomplete."));
    ok = false;
  }

  bool saw_machine_asset = false;
  bool saw_buffer_asset = false;
  std::set<std::string> asset_ids;
  for (const VsgGpuAsset& asset : manifest.assets) {
    if (asset.asset_id.empty() || asset.cache_key.asset_key.empty() ||
        asset.cache_key.content_key.empty()) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "vsg_cache_asset_key",
                                   "VSG GPU cache asset is missing stable key.",
                                   asset.provenance_refs));
      ok = false;
    }
    if (!asset_ids.insert(asset.asset_id).second) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "vsg_cache_duplicate_asset",
                                   "VSG GPU cache asset id is duplicated.",
                                   asset.provenance_refs));
      ok = false;
    }
    if (ContainsForbiddenPolicyWord(asset.usage)) {
      out.push_back(MakeDiagnostic(
          DiagnosticSeverity::kError, "vsg_cache_policy_leak",
          "VSG GPU cache asset usage names a chart-source or presentation "
          "policy.",
          asset.provenance_refs));
      ok = false;
    }
    if (asset.byte_size == 0) {
      out.push_back(MakeDiagnostic(DiagnosticSeverity::kError,
                                   "vsg_cache_empty_asset",
                                   "VSG GPU cache asset has zero byte size.",
                                   asset.provenance_refs));
      ok = false;
    }
    if (asset.residency == VsgGpuResidency::kMachineLocal) {
      saw_machine_asset = true;
      const bool descriptor_asset =
          asset.kind == VsgGpuAssetKind::kTexture ||
          asset.kind == VsgGpuAssetKind::kAtlas ||
          asset.kind == VsgGpuAssetKind::kUniformBlock;
      if (descriptor_asset && !asset.descriptor_ready) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "vsg_cache_descriptor",
            "Machine-local VSG asset must be descriptor-ready.",
            asset.provenance_refs));
        ok = false;
      }
      if (descriptor_asset && asset.resource_id.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "vsg_cache_resource_asset",
            "Machine-local VSG cache asset must identify a neutral resource.",
            asset.provenance_refs));
        ok = false;
      }
    }
    if (asset.kind == VsgGpuAssetKind::kVertexBuffer ||
        asset.kind == VsgGpuAssetKind::kIndexBuffer ||
        asset.kind == VsgGpuAssetKind::kInstanceBuffer) {
      saw_buffer_asset = true;
      if (asset.primitive_ids.empty()) {
        out.push_back(MakeDiagnostic(
            DiagnosticSeverity::kError, "vsg_cache_primitive_asset",
            "VSG buffer asset must identify source neutral primitives.",
            asset.provenance_refs));
        ok = false;
      }
    }
  }

  if (manifest.assets.empty() || !saw_machine_asset || !saw_buffer_asset ||
      manifest.stats.estimated_bytes == 0) {
    out.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "vsg_cache_incomplete",
        "VSG GPU cache manifest must include machine-local resources, "
        "primitive buffers, and byte estimates."));
    ok = false;
  }

  for (const Diagnostic& diagnostic : manifest.diagnostics) {
    if (diagnostic.severity == DiagnosticSeverity::kError) {
      out.push_back(diagnostic);
      ok = false;
    }
  }
  return ok;
}

}  // namespace ocpn::render::vsg
