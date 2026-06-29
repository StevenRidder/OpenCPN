// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include "portable_nautical_package.hpp"

#include <map>
#include <string>
#include <vector>

namespace ocpn::render {

struct S57FixtureFeature {
  std::string source_feature_id;
  std::string object_class;
  NormalizedGeometryKind geometry_kind = NormalizedGeometryKind::kUnknown;
  Geometry geometry;
  std::vector<ChartAttribute> attributes;
  double min_scale_denom = 0.0;
  double max_scale_denom = 0.0;
  int source_priority = 0;
  std::string source_geometry_hash;
  bool supported = true;
  std::string unsupported_reason;
  std::map<std::string, std::string> metadata;
};

struct S57FixtureCell {
  std::string dataset_name;
  std::string edition;
  std::string update;
  std::string producer_code;
  std::string product_identifier = "INT.IHO.S-57.fixture";
  std::string product_edition = "3.1";
  std::string dataset_reference_date;
  std::string source_epoch;
  std::string content_hash;
  std::string native_projection = "WGS84";
  double native_scale_denom = 0.0;
  GeoBounds geographic_bbox;
  std::vector<S57FixtureFeature> features;
  std::map<std::string, std::string> metadata;
};

struct S57ConverterIdentity {
  std::string converter_id = "synthetic-s57-portable-converter";
  std::string converter_version = "convert-2";
  std::string emitted_package_schema = "portable-nautical-package-v1";
  bool preserves_source_object_ids = true;
  bool emits_diagnostics = true;
};

class S57PortablePackageConverter {
 public:
  S57ConverterIdentity Identity() const;
  PortableNauticalPackage Convert(const S57FixtureCell& cell) const;
};

S57FixtureCell BuildS57ConverterFixtureCell();

bool ValidateS57ConverterFixturePackage(
    const PortableNauticalPackage& package,
    std::vector<Diagnostic>* diagnostics);

}  // namespace ocpn::render
