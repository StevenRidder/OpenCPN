// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#include "vsg/vsg_backend.hpp"

#include "draw_backend_contract.hpp"
#include "gpu_artifact_cache_contract.hpp"
#include "vsg/vsg_gpu_cache.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ocpn::render::vsg {
namespace {

struct Rgba {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 255;
};

struct PixelPoint {
  int x = 0;
  int y = 0;
  bool valid = false;
};

Diagnostic MakeDiagnostic(DiagnosticSeverity severity, std::string code,
                          std::string message,
                          std::vector<std::string> provenance_refs = {}) {
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.provenance_refs = std::move(provenance_refs);
  diagnostic.suggested_action =
      "Keep the VSG backend draw/cache-only: validate neutral primitives, "
      "compiled GPU artifacts, and backend handoff metadata before live VSG "
      "object replay.";
  return diagnostic;
}

void AppendDiagnostics(std::vector<Diagnostic>* destination,
                       const std::vector<Diagnostic>& incoming) {
  destination->insert(destination->end(), incoming.begin(), incoming.end());
}

bool HasError(const std::vector<Diagnostic>& diagnostics) {
  for (const Diagnostic& diagnostic : diagnostics) {
    if (diagnostic.severity == DiagnosticSeverity::kError) return true;
  }
  return false;
}

std::uint32_t PrimitiveCount(const NauticalRenderModel& model) {
  std::uint32_t count = 0;
  for (const NauticalLayer& layer : model.layers) {
    count += static_cast<std::uint32_t>(layer.primitives.size());
  }
  return count;
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

std::uint64_t StableHash(const std::string& value) {
  return Fnva64String(1469598103934665603ULL, value);
}

std::uint64_t PixelHash(const PixelBuffer& pixels) {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = Fnva64Uint32(hash, pixels.pixel_size.width);
  hash = Fnva64Uint32(hash, pixels.pixel_size.height);
  if (!pixels.rgba8.empty()) {
    hash = Fnva64Bytes(hash, pixels.rgba8.data(), pixels.rgba8.size());
  }
  return hash;
}

std::string HexHash(std::uint64_t hash) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

Rgba BackgroundFor(Palette palette) {
  switch (palette) {
    case Palette::kDay:
      return {218, 231, 232, 255};
    case Palette::kDusk:
      return {74, 78, 92, 255};
    case Palette::kNight:
      return {12, 18, 26, 255};
  }
  return {218, 231, 232, 255};
}

Rgba ColorForPrimitive(const NauticalPrimitive& primitive,
                       std::uint32_t layer_index) {
  const std::uint64_t hash =
      StableHash(primitive.primitive_id + ":" + primitive.role + ":" +
                 primitive.cache_key.resource_key);
  const std::uint8_t jitter =
      static_cast<std::uint8_t>((hash + layer_index * 17U) % 32U);

  switch (primitive.type) {
    case NauticalPrimitiveType::kAreaFill:
      if (primitive.metadata.find("safety_class") != primitive.metadata.end() &&
          primitive.metadata.at("safety_class") == "shallow") {
        return {96, static_cast<std::uint8_t>(132 + jitter), 180, 190};
      }
      return {133, static_cast<std::uint8_t>(178 + jitter / 2U), 165, 176};
    case NauticalPrimitiveType::kRasterPatch:
      return {92, static_cast<std::uint8_t>(126 + jitter), 154, 185};
    case NauticalPrimitiveType::kContourLine:
      return primitive.metadata.find("safety_contour") !=
                     primitive.metadata.end() &&
                 primitive.metadata.at("safety_contour") == "true"
             ? Rgba{96, 42, 38, 245}
             : Rgba{48, 65, static_cast<std::uint8_t>(95 + jitter), 225};
    case NauticalPrimitiveType::kLineStroke:
      return {54, 67, static_cast<std::uint8_t>(86 + jitter), 225};
    case NauticalPrimitiveType::kSymbolInstance:
      return {215, static_cast<std::uint8_t>(58 + jitter), 48, 245};
    case NauticalPrimitiveType::kTextLabel:
      return {30, 33, static_cast<std::uint8_t>(42 + jitter / 2U), 235};
    case NauticalPrimitiveType::kSounding:
      return {33, 44, static_cast<std::uint8_t>(118 + jitter), 235};
    case NauticalPrimitiveType::kClipBoundary:
      return {20, 20, 20, 96};
  }
  return {32, 32, 32, 255};
}

void ResizePixels(const RenderTarget& target, PixelBuffer* pixels) {
  pixels->pixel_size = target.pixel_size;
  const std::size_t pixel_count =
      static_cast<std::size_t>(target.pixel_size.width) * target.pixel_size.height;
  pixels->rgba8.assign(pixel_count * 4U, 0);
}

std::size_t Offset(const PixelBuffer& pixels, int x, int y) {
  return (static_cast<std::size_t>(y) * pixels.pixel_size.width +
          static_cast<std::size_t>(x)) *
         4U;
}

void BlendPixel(PixelBuffer* pixels, int x, int y, Rgba color) {
  if (x < 0 || y < 0 ||
      x >= static_cast<int>(pixels->pixel_size.width) ||
      y >= static_cast<int>(pixels->pixel_size.height)) {
    return;
  }
  const std::size_t offset = Offset(*pixels, x, y);
  const std::uint32_t alpha = color.a;
  const std::uint32_t inv_alpha = 255U - alpha;
  pixels->rgba8[offset] = static_cast<std::uint8_t>(
      (color.r * alpha + pixels->rgba8[offset] * inv_alpha) / 255U);
  pixels->rgba8[offset + 1U] = static_cast<std::uint8_t>(
      (color.g * alpha + pixels->rgba8[offset + 1U] * inv_alpha) / 255U);
  pixels->rgba8[offset + 2U] = static_cast<std::uint8_t>(
      (color.b * alpha + pixels->rgba8[offset + 2U] * inv_alpha) / 255U);
  pixels->rgba8[offset + 3U] = 255U;
}

void FillBackground(Rgba color, PixelBuffer* pixels) {
  for (std::uint32_t y = 0; y < pixels->pixel_size.height; ++y) {
    for (std::uint32_t x = 0; x < pixels->pixel_size.width; ++x) {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * pixels->pixel_size.width + x) * 4U;
      pixels->rgba8[offset] = color.r;
      pixels->rgba8[offset + 1U] = color.g;
      pixels->rgba8[offset + 2U] = color.b;
      pixels->rgba8[offset + 3U] = color.a;
    }
  }
}

int ClampToPixel(double value, std::uint32_t limit) {
  if (limit == 0U) return 0;
  const double clamped =
      std::max(0.0, std::min(value, static_cast<double>(limit - 1U)));
  return static_cast<int>(std::round(clamped));
}

PixelPoint ProjectPoint(const Point2& point, CoordinateSpace coordinate_space,
                        const RenderView& view, PixelSize size) {
  if (size.width == 0U || size.height == 0U) return {};
  if (coordinate_space == CoordinateSpace::kGeographic) {
    const double lon_span = view.geographic_bbox.east - view.geographic_bbox.west;
    const double lat_span = view.geographic_bbox.north - view.geographic_bbox.south;
    if (std::abs(lon_span) <= std::numeric_limits<double>::epsilon() ||
        std::abs(lat_span) <= std::numeric_limits<double>::epsilon()) {
      return {};
    }
    PixelPoint projected;
    projected.x =
        ClampToPixel((point.x - view.geographic_bbox.west) / lon_span *
                         static_cast<double>(size.width - 1U),
                     size.width);
    projected.y =
        ClampToPixel((view.geographic_bbox.north - point.y) / lat_span *
                         static_cast<double>(size.height - 1U),
                     size.height);
    projected.valid = true;
    return projected;
  }

  const bool normalized =
      point.x >= 0.0 && point.x <= 1.0 && point.y >= 0.0 && point.y <= 1.0;
  PixelPoint projected;
  projected.x = ClampToPixel(normalized ? point.x * (size.width - 1U)
                                        : point.x,
                             size.width);
  projected.y = ClampToPixel(normalized ? point.y * (size.height - 1U)
                                        : point.y,
                             size.height);
  projected.valid = true;
  return projected;
}

void PaintMarker(PixelBuffer* pixels, PixelPoint center, Rgba color, int radius) {
  if (!center.valid) return;
  for (int y = center.y - radius; y <= center.y + radius; ++y) {
    for (int x = center.x - radius; x <= center.x + radius; ++x) {
      BlendPixel(pixels, x, y, color);
    }
  }
}

void DrawLine(PixelBuffer* pixels, PixelPoint a, PixelPoint b, Rgba color,
              int width) {
  if (!a.valid || !b.valid) return;
  int x0 = a.x;
  int y0 = a.y;
  const int x1 = b.x;
  const int y1 = b.y;
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    PaintMarker(pixels, {x0, y0, true}, color, width);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void FillBounds(PixelBuffer* pixels, PixelPoint min_pt, PixelPoint max_pt,
                Rgba color) {
  if (!min_pt.valid || !max_pt.valid) return;
  const int x0 = std::min(min_pt.x, max_pt.x);
  const int x1 = std::max(min_pt.x, max_pt.x);
  const int y0 = std::min(min_pt.y, max_pt.y);
  const int y1 = std::max(min_pt.y, max_pt.y);
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      BlendPixel(pixels, x, y, color);
    }
  }
}

std::vector<Point2> GeometryPoints(const Geometry& geometry) {
  std::vector<Point2> points = geometry.points;
  for (const std::vector<Point2>& ring : geometry.rings) {
    points.insert(points.end(), ring.begin(), ring.end());
  }
  return points;
}

void PaintGeometry(PixelBuffer* pixels, const RenderView& view,
                   const NauticalPrimitive& primitive, const Geometry& geometry,
                   Rgba color) {
  const std::vector<Point2> points = GeometryPoints(geometry);
  if (points.empty()) return;

  std::vector<PixelPoint> projected;
  projected.reserve(points.size());
  for (const Point2& point : points) {
    PixelPoint px =
        ProjectPoint(point, geometry.coordinate_space, view, pixels->pixel_size);
    if (px.valid) projected.push_back(px);
  }
  if (projected.empty()) return;

  if (primitive.type == NauticalPrimitiveType::kAreaFill ||
      primitive.type == NauticalPrimitiveType::kRasterPatch) {
    PixelPoint min_pt = projected.front();
    PixelPoint max_pt = projected.front();
    for (const PixelPoint& point : projected) {
      min_pt.x = std::min(min_pt.x, point.x);
      min_pt.y = std::min(min_pt.y, point.y);
      max_pt.x = std::max(max_pt.x, point.x);
      max_pt.y = std::max(max_pt.y, point.y);
    }
    FillBounds(pixels, min_pt, max_pt, color);
    return;
  }

  const int width = std::max(1, static_cast<int>(std::round(primitive.width_px)));
  if (projected.size() == 1U) {
    PaintMarker(pixels, projected.front(), color, width + 1);
    return;
  }
  for (std::size_t i = 1; i < projected.size(); ++i) {
    DrawLine(pixels, projected[i - 1U], projected[i], color, width);
  }
}

void PaintPrimitive(PixelBuffer* pixels, const RenderView& view,
                    const NauticalPrimitive& primitive,
                    std::uint32_t layer_index) {
  Rgba color = ColorForPrimitive(primitive, layer_index);
  for (const Geometry& geometry : primitive.geometries) {
    PaintGeometry(pixels, view, primitive, geometry, color);
  }

  if (primitive.type == NauticalPrimitiveType::kSymbolInstance ||
      primitive.type == NauticalPrimitiveType::kTextLabel ||
      primitive.type == NauticalPrimitiveType::kSounding ||
      primitive.geometries.empty()) {
    PixelPoint center =
        ProjectPoint(primitive.position, primitive.handoff.coordinate_space, view,
                     pixels->pixel_size);
    if (!center.valid || (primitive.position.x == 0.0 &&
                          primitive.position.y == 0.0)) {
      const std::uint64_t hash = StableHash(primitive.primitive_id);
      center.x = static_cast<int>(hash % std::max<std::uint32_t>(
                                             1U, pixels->pixel_size.width));
      center.y = static_cast<int>((hash >> 16U) % std::max<std::uint32_t>(
                                                   1U, pixels->pixel_size.height));
      center.valid = true;
    }
    const int radius =
        primitive.type == NauticalPrimitiveType::kTextLabel ? 2 : 3;
    PaintMarker(pixels, center, color, radius);
  }
}

void PaintProductionFixture(const NauticalRenderModel& model,
                            const GpuArtifactCacheManifest& artifacts,
                            const VsgGpuCacheManifest& vsg_cache,
                            const RenderTarget& target,
                            PixelBuffer* pixels) {
  ResizePixels(target, pixels);
  FillBackground(BackgroundFor(model.display_state.palette), pixels);

  std::uint32_t layer_index = 0;
  for (const NauticalLayer& layer : model.layers) {
    for (const NauticalPrimitive& primitive : layer.primitives) {
      PaintPrimitive(pixels, model.render_view, primitive, layer_index);
    }
    ++layer_index;
  }

  const std::uint8_t artifact_marker = static_cast<std::uint8_t>(
      std::min<std::size_t>(255U, artifacts.artifacts.size() * 7U));
  const std::uint8_t vsg_marker = static_cast<std::uint8_t>(
      std::min<std::size_t>(255U, vsg_cache.assets.size() * 5U));
  for (std::uint32_t x = 0; x < pixels->pixel_size.width; ++x) {
    BlendPixel(pixels, static_cast<int>(x), 0,
               {artifact_marker, vsg_marker, 36, 255});
  }
}

GpuArtifactCacheOptions ArtifactOptionsFor(const NauticalRenderModel& model) {
  GpuArtifactCacheOptions options;
  options.backend_target = "vsg";
  options.device_profile = "vulkan-proof-device";
  options.material_profile =
      model.metadata.find("package_id") == model.metadata.end()
          ? "neutral-material-v1"
          : "vsg-neutral-package-v1";
  options.cache_namespace = "opencpn-vsg-production-slice";
  options.memory_budget_bytes = 32ULL * 1024ULL * 1024ULL;
  options.invalidation_epoch = model.source_epoch + ":vsg-production-fixture";
  return options;
}

DrawBackendCapabilities VsgCapabilities(
    const GpuArtifactCacheOptions& options) {
  DrawBackendCapabilities capabilities;
  capabilities.backend_id = "vsg-native-production-slice";
  capabilities.target = DrawBackendTarget::kVsgVulkan;
  capabilities.device_profile = options.device_profile;
  capabilities.material_profile = options.material_profile;
  capabilities.accepts_neutral_model = true;
  capabilities.accepts_gpu_artifacts = true;
  capabilities.supports_swapchain = true;
  capabilities.supports_offscreen = true;
  capabilities.supports_readback = true;
  capabilities.fallback_backend_ids = {"server-raster"};
  return capabilities;
}

const char* TargetName(RenderTargetKind kind) {
  switch (kind) {
    case RenderTargetKind::kOffscreen:
      return "offscreen";
    case RenderTargetKind::kSwapchain:
      return "swapchain";
  }
  return "unknown";
}

}  // namespace

const char* VsgBackend::Name() const {
  return "vulkan-scenegraph-placeholder";
}

RenderResult VsgBackend::Render(const RenderScene& scene,
                                const RenderTarget& target) {
  return RenderModel(BuildNauticalRenderModel(scene), target);
}

RenderResult VsgBackend::RenderModel(const NauticalRenderModel& model,
                                     const RenderTarget& target) {
  RenderResult result;
  result.ok = false;
  result.pixels.pixel_size = target.pixel_size;

  if (target.pixel_size.width == 0U || target.pixel_size.height == 0U ||
      target.device_pixel_ratio <= 0.0) {
    result.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "backend.vsg_target_invalid",
        "VSG backend target is missing pixel size or device pixel ratio."));
    return result;
  }

  const GpuArtifactCacheOptions artifact_options = ArtifactOptionsFor(model);
  const GpuArtifactCacheManifest artifacts =
      BuildGpuArtifactCacheManifest(model, artifact_options);
  const DrawBackendContract contract = BuildDrawBackendContract(
      model, artifacts, VsgCapabilities(artifact_options));
  std::vector<Diagnostic> handoff_diagnostics;
  const bool handoff_ok =
      ValidateDrawBackendHandoff(model, artifacts, contract,
                                 &handoff_diagnostics);
  AppendDiagnostics(&result.diagnostics, handoff_diagnostics);

  const VsgGpuCacheManifest vsg_cache = BuildVsgGpuCacheManifest(model);
  std::vector<Diagnostic> cache_diagnostics;
  const bool cache_ok =
      ValidateVsgGpuCacheManifest(vsg_cache, &cache_diagnostics);
  AppendDiagnostics(&result.diagnostics, cache_diagnostics);

  if (!handoff_ok || !cache_ok || HasError(result.diagnostics)) {
    result.diagnostics.push_back(MakeDiagnostic(
        DiagnosticSeverity::kError, "backend.vsg_handoff_rejected",
        "VSG backend rejected the production fixture before rendering because "
        "the neutral model, GPU artifact manifest, or draw handoff contract "
        "failed validation."));
    return result;
  }

  PaintProductionFixture(model, artifacts, vsg_cache, target, &result.pixels);
  const std::uint64_t pixel_hash = PixelHash(result.pixels);

  std::ostringstream cache_message;
  cache_message << "VSG backend prepared " << vsg_cache.assets.size()
                << " VSG asset records and " << artifacts.artifacts.size()
                << " backend artifact records for " << PrimitiveCount(model)
                << " neutral primitives.";
  result.diagnostics.push_back(MakeDiagnostic(
      DiagnosticSeverity::kInfo, "backend.vsg_gpu_cache",
      cache_message.str()));

  std::ostringstream render_message;
  render_message << "Rendered deterministic " << TargetName(target.kind)
                 << " VSG production fixture from draw/cache handoff "
                 << contract.contract_id << " with pixel_hash="
                 << HexHash(pixel_hash) << ".";
  result.diagnostics.push_back(MakeDiagnostic(
      DiagnosticSeverity::kInfo, "backend.vsg_production_fixture",
      render_message.str()));

  result.diagnostics.push_back(MakeDiagnostic(
      DiagnosticSeverity::kWarning, "backend.vsg_artifact_fallback",
      "Live VSG object replay is not enabled in this slice; the backend used "
      "compiled artifact records and neutral primitive geometry to produce a "
      "deterministic fixture image."));
  result.ok = true;
  return result;
}

}  // namespace ocpn::render::vsg
