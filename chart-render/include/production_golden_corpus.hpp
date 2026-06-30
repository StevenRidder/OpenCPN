// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "gpu_artifact_cache_contract.hpp"
#include "portable_nautical_package.hpp"
#include "source_to_render_inspection.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kProductionGoldenCorpusSchemaVersion = 1;

struct ProductionGoldenPrimitive {
  std::string primitive_id;
  std::string source_object_id;
  std::string object_class;
  std::string presentation_rule_id;
  std::string primitive_type;
  std::string stable_hash;
};

struct ProductionGoldenCorpusExpected {
  std::uint32_t schema_version = kProductionGoldenCorpusSchemaVersion;
  std::string corpus_id = "qa5-production-first-slice";
  std::string source_id = "s57:US5CONVERT2";
  std::string source_edition = "4";
  std::string source_update = "2";
  std::string distribution_class = "redistributable_synthetic";
  std::string package_id = "s57:US5CONVERT2:package";
  std::string package_profile = "s57-s52-poc";
  std::string package_hash;
  std::string model_id = "s57:US5CONVERT2:package:presentation";
  std::string backend_name = "vulkan-scenegraph-placeholder";
  std::string golden_image_hash = "009410097424697d";
  std::uint64_t golden_image_bytes = 256ULL * 256ULL * 4ULL;
  std::size_t primitive_count = 3;
  std::size_t artifact_count = 13;
  std::size_t scene_artifact_count = 2;
  std::size_t inspection_row_count = 3;
  std::uint32_t buoy_sample_x = 125;
  std::uint32_t buoy_sample_y = 134;
  std::string buoy_sample_rgba8 = "d34735ff";
  std::vector<ProductionGoldenPrimitive> primitives;
  std::vector<std::string> required_limitations;
};

struct ProductionGoldenCorpusSnapshot {
  std::uint32_t schema_version = kProductionGoldenCorpusSchemaVersion;
  std::string corpus_id = "qa5-production-first-slice";
  std::string source_id;
  std::string source_edition;
  std::string source_update;
  std::string distribution_class;
  std::string package_id;
  std::string package_profile;
  std::string package_hash;
  std::string model_id;
  std::string model_epoch;
  std::vector<ProductionGoldenPrimitive> primitives;
  GpuArtifactCacheManifest cache_manifest;
  std::string backend_name;
  std::string golden_image_hash;
  std::uint64_t golden_image_bytes = 0;
  SourceToRenderInspectionReport inspection_report;
  std::vector<std::string> known_limitations;
  std::vector<Diagnostic> diagnostics;
  bool ok = false;
};

ProductionGoldenCorpusExpected BuildDefaultProductionGoldenCorpusExpected();

ProductionGoldenCorpusSnapshot BuildProductionGoldenCorpusSnapshot(
    const ProductionGoldenCorpusExpected& expected =
        BuildDefaultProductionGoldenCorpusExpected());

bool ValidateProductionGoldenCorpusSnapshot(
    const ProductionGoldenCorpusSnapshot& snapshot,
    const ProductionGoldenCorpusExpected& expected,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
