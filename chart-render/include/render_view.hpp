// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

#pragma once

#include <cstdint>
#include <string>

namespace ocpn::render {

enum class Projection {
  kUnknown,
  kMercator,
  kWebMercatorTile
};

enum class Palette {
  kDay,
  kDusk,
  kNight
};

enum class DisplayCategory {
  kBase,
  kStandard,
  kAll,
  kMariner
};

struct GeoPoint {
  double lon = 0.0;
  double lat = 0.0;
};

struct GeoBounds {
  double west = -180.0;
  double south = -90.0;
  double east = 180.0;
  double north = 90.0;
};

struct PixelSize {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct RenderView {
  std::string view_id;
  Projection projection = Projection::kUnknown;
  GeoBounds geographic_bbox;
  GeoPoint center;
  double scale_denom = 0.0;
  double rotation_deg = 0.0;
  PixelSize pixel_size;
  double device_pixel_ratio = 1.0;
  double overzoom = 1.0;
  std::uint32_t overscan_px = 0;
};

struct DisplayState {
  Palette palette = Palette::kDay;
  DisplayCategory display_category = DisplayCategory::kStandard;
  double safety_depth_m = 0.0;
  double shallow_contour_m = 0.0;
  double safety_contour_m = 0.0;
  double deep_contour_m = 0.0;
  bool show_text = true;
  bool show_soundings = true;
  bool show_lights = true;
  bool simplified_symbols = false;
  bool two_shade_depth = false;
  std::string language = "en";
  std::string units = "metric";
};

}  // namespace ocpn::render
