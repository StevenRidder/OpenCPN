// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "chart_source.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ocpn::render {

inline constexpr std::uint32_t kPortableNauticalPackageSchemaVersion = 1;

struct PortablePackageManifest {
  std::uint32_t schema_version = kPortableNauticalPackageSchemaVersion;
  std::string package_id;
  std::string profile;
  std::string producer;
  std::string producer_version;
  std::string converter_id;
  std::string converter_version;
  std::string created_at;
  std::string source_epoch;
  std::string content_hash;
  std::string coordinate_reference = "EPSG:4326";
  std::string units = "degrees";
  bool fixture_package = false;
  std::map<std::string, std::string> metadata;
};

struct PortableCoverageRecord {
  std::string coverage_id;
  std::string source_id;
  Geometry geometry;
  double min_scale_denom = 0.0;
  double max_scale_denom = 0.0;
  int priority = 0;
  std::string chart_family;
  std::string boundary_policy;
  std::string update_epoch;
  std::vector<std::string> provenance_refs;
  std::map<std::string, std::string> metadata;
};

struct PortablePackageChecksums {
  std::map<std::string, std::string> record_hashes;
  std::string package_hash;
};

struct PortableNauticalPackage {
  PortablePackageManifest manifest;
  ChartSourceProduct product;
  std::vector<PortableCoverageRecord> coverage;
  PortablePackageChecksums checksums;
};

PortableNauticalPackage BuildPortablePackageFixture();

PortablePackageChecksums ComputePortablePackageChecksums(
    const PortableNauticalPackage& package);

std::string WritePortableNauticalPackage(
    const PortableNauticalPackage& package);

bool ReadPortableNauticalPackage(const std::string& encoded,
                                 PortableNauticalPackage* package,
                                 std::vector<Diagnostic>* diagnostics);

bool ValidatePortableNauticalPackage(
    const PortableNauticalPackage& package,
    std::vector<Diagnostic>* diagnostics);

bool RoundTripPortablePackageFixture(PortableNauticalPackage* round_tripped,
                                     std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
