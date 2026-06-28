# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(opencpn_chart_render_skeleton LANGUAGES CXX)

add_library(opencpn-chart-render-skeleton STATIC
  s52/s52_command_builder.cpp
  vsg/vsg_backend.cpp
)

target_compile_features(opencpn-chart-render-skeleton PUBLIC cxx_std_17)
target_include_directories(opencpn-chart-render-skeleton
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
