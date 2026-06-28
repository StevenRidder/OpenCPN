# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(opencpn_chart_render_skeleton LANGUAGES CXX)

add_library(opencpn-chart-render-skeleton STATIC
  source/depth_safety.cpp
  source/depth_tessellation.cpp
  source/chart1_baseline.cpp
  source/chart1_conformance.cpp
  source/chart1_acceptance.cpp
  source/conversion_trace.cpp
  source/chart_interchange.cpp
  source/chart_source.cpp
  s52/s52_command_builder.cpp
  vsg/vsg_backend.cpp
)

target_compile_features(opencpn-chart-render-skeleton PUBLIC cxx_std_17)
target_include_directories(opencpn-chart-render-skeleton
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(opencpn-chart1-conformance-smoke
  tests/chart1_conformance_smoke.cpp
)
target_link_libraries(opencpn-chart1-conformance-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-chart1-baseline-smoke
  tests/chart1_baseline_smoke.cpp
)
target_link_libraries(opencpn-chart1-baseline-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-depth-tessellation-smoke
  tests/depth_tessellation_smoke.cpp
)
target_link_libraries(opencpn-depth-tessellation-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-depth-safety-smoke
  tests/depth_safety_smoke.cpp
)
target_link_libraries(opencpn-depth-safety-smoke
  PRIVATE opencpn-chart-render-skeleton
)
