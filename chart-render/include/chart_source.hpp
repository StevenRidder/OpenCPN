// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "render_scene.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kChartSourceSchemaVersion = 1;

enum class ChartSourceKind {
  kS57Cell,
  kSencCell,
  kRasterChart,
  kMbtilesPackage,
  kPmtilesPackage,
  kS101Dataset,
  kDebugFixture
};

enum class ChartSourceRole {
  kPrimary,
  kUpdate,
  kOverview,
  kOverlay,
  kDebug
};

enum class NormalizedGeometryKind {
  kUnknown,
  kPoint,
  kLine,
  kArea,
  kMultiPoint,
  kRasterSheetReference
};

enum class RasterSheetKind {
  kChartImage,
  kCollarMask,
  kNoDataMask,
  kCoverageMask,
  kDebugOverlay
};

enum class DebugArtifactKind {
  kSourceMetadata,
  kFeatureDump,
  kGeometryTrace,
  kRuleTrace,
  kRasterFootprint,
  kInterchangePackage
};

struct ChartSourceRef {
  std::string source_id;
  ChartSourceKind kind = ChartSourceKind::kS57Cell;
  ChartSourceRole role = ChartSourceRole::kPrimary;
  std::string native_name;
  std::string edition;
  std::string update;
  std::string content_hash;
  std::string native_projection;
  double native_scale_denom = 0.0;
  GeoBounds geographic_bbox;
  std::map<std::string, std::string> metadata;
};

struct ChartSourceCapabilities {
  std::vector<ChartSourceKind> source_kinds;
  bool emits_normalized_objects = true;
  bool emits_raster_sheets = false;
  bool emits_debug_artifacts = false;
  bool preserves_source_object_ids = true;
};

struct ChartSourceRequest {
  std::vector<ChartSourceRef> sources;
  RenderView view;
  std::string profile = "s52";
  bool include_debug_artifacts = false;
  bool allow_interchange_inputs = true;
};

struct ChartAttribute {
  std::string acronym;
  std::string value;
  std::string display_value;
};

struct NormalizedChartObject {
  std::string object_id;
  std::string object_class;
  NormalizedGeometryKind geometry_kind = NormalizedGeometryKind::kUnknown;
  Geometry geometry;
  std::vector<ChartAttribute> attributes;
  double min_scale_denom = 0.0;
  double max_scale_denom = 0.0;
  int source_priority = 0;
  std::vector<std::string> provenance_refs;
  std::map<std::string, std::string> metadata;
};

struct RasterSheet {
  std::string sheet_id;
  RasterSheetKind kind = RasterSheetKind::kChartImage;
  std::string source_id;
  GeoBounds geographic_bbox;
  PixelSize pixel_size;
  CoordinateSpace coordinate_space = CoordinateSpace::kRaster;
  std::string content_hash;
  std::string color_model = "rgba8";
  std::string no_data_policy;
  std::string collar_policy;
  std::vector<std::string> provenance_refs;
  std::map<std::string, std::string> metadata;
};

struct DebugArtifact {
  std::string artifact_id;
  DebugArtifactKind kind = DebugArtifactKind::kSourceMetadata;
  std::string source_id;
  std::string content_hash;
  std::string media_type;
  std::string producer;
  std::vector<std::string> provenance_refs;
  std::map<std::string, std::string> metadata;
};

struct ChartSourceProduct {
  std::uint32_t schema_version = kChartSourceSchemaVersion;
  std::string product_id;
  std::vector<ChartSourceRef> sources;
  std::vector<NormalizedChartObject> objects;
  std::vector<RasterSheet> raster_sheets;
  std::vector<DebugArtifact> debug_artifacts;
  std::vector<ProvenanceRecord> provenance_table;
  std::vector<Diagnostic> diagnostics;
  std::map<std::string, std::string> metadata;
};

class IChartSource {
 public:
  virtual ~IChartSource() = default;

  virtual const char* Name() const = 0;
  virtual ChartSourceCapabilities Capabilities() const = 0;
  virtual ChartSourceProduct Load(const ChartSourceRequest& request) = 0;
};

const char* ToString(ChartSourceKind kind);
const char* ToString(DebugArtifactKind kind);

bool ValidateChartSourceProduct(const ChartSourceProduct& product,
                                std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
