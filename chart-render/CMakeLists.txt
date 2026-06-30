# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(opencpn_chart_render_skeleton LANGUAGES CXX)

add_library(opencpn-chart-render-skeleton STATIC
  source/draw_backend_contract.cpp
  source/helm_webgpu_artifact_consumer.cpp
  source/performance_budget_contract.cpp
  source/opencpn_feature_flag_adapter.cpp
  source/viewport_tile_scheduler.cpp
  source/source_to_render_inspection.cpp
  source/gpu_artifact_cache_contract.cpp
  source/nautical_render_model.cpp
  source/golden_regression.cpp
  source/depth_performance.cpp
  source/depth_quilting.cpp
  source/depth_safety.cpp
  source/depth_tessellation.cpp
  source/chart1_baseline.cpp
  source/chart1_conformance.cpp
  source/chart1_debug_app.cpp
  source/chart1_acceptance.cpp
  source/conversion_trace.cpp
  source/chart_interchange.cpp
  source/chart_source.cpp
  source/portable_nautical_package.cpp
  source/s57_portable_package_converter.cpp
  source/s52_presentation_compiler.cpp
  s52/s52_command_builder.cpp
  vsg/vsg_backend.cpp
  vsg/vsg_gpu_cache.cpp
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

add_executable(opencpn-chart1-debug-app-smoke
  tests/chart1_debug_app_smoke.cpp
)
target_link_libraries(opencpn-chart1-debug-app-smoke
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

add_executable(opencpn-depth-performance-smoke
  tests/depth_performance_smoke.cpp
)
target_link_libraries(opencpn-depth-performance-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-performance-budget-contract-smoke
  tests/performance_budget_contract_smoke.cpp
)
target_link_libraries(opencpn-performance-budget-contract-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-golden-regression-smoke
  tests/golden_regression_smoke.cpp
)
target_link_libraries(opencpn-golden-regression-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-depth-quilting-smoke
  tests/depth_quilting_smoke.cpp
)
target_link_libraries(opencpn-depth-quilting-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-neutral-model-smoke
  tests/neutral_model_smoke.cpp
)
target_link_libraries(opencpn-neutral-model-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-vsg-gpu-cache-smoke
  tests/vsg_gpu_cache_smoke.cpp
)
target_link_libraries(opencpn-vsg-gpu-cache-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-vsg-backend-production-fixture-smoke
  tests/vsg_backend_production_fixture_smoke.cpp
)
target_link_libraries(opencpn-vsg-backend-production-fixture-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-gpu-artifact-cache-contract-smoke
  tests/gpu_artifact_cache_contract_smoke.cpp
)
target_link_libraries(opencpn-gpu-artifact-cache-contract-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-source-to-render-inspection-smoke
  tests/source_to_render_inspection_smoke.cpp
)
target_link_libraries(opencpn-source-to-render-inspection-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-source-to-render-production-fixture-smoke
  tests/source_to_render_production_fixture_smoke.cpp
)
target_link_libraries(opencpn-source-to-render-production-fixture-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-draw-backend-contract-smoke
  tests/draw_backend_contract_smoke.cpp
)
target_link_libraries(opencpn-draw-backend-contract-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-helm-webgpu-artifact-consumer-smoke
  tests/helm_webgpu_artifact_consumer_smoke.cpp
)
target_link_libraries(opencpn-helm-webgpu-artifact-consumer-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-s52-presentation-compiler-smoke
  tests/s52_presentation_compiler_smoke.cpp
)
target_link_libraries(opencpn-s52-presentation-compiler-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-portable-package-roundtrip-smoke
  tests/portable_package_roundtrip_smoke.cpp
)
target_link_libraries(opencpn-portable-package-roundtrip-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-s57-portable-package-converter-smoke
  tests/s57_portable_package_converter_smoke.cpp
)
target_link_libraries(opencpn-s57-portable-package-converter-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-feature-flag-adapter-smoke
  tests/opencpn_feature_flag_adapter_smoke.cpp
)
target_link_libraries(opencpn-feature-flag-adapter-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_executable(opencpn-viewport-tile-scheduler-smoke
  tests/viewport_tile_scheduler_smoke.cpp
)
target_link_libraries(opencpn-viewport-tile-scheduler-smoke
  PRIVATE opencpn-chart-render-skeleton
)

add_custom_target(opencpn-stakeholder-demo
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/stakeholder_demo.sh
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/stakeholder-demo
  DEPENDS
    opencpn-s52-presentation-compiler-smoke
    opencpn-neutral-model-smoke
    opencpn-feature-flag-adapter-smoke
    opencpn-viewport-tile-scheduler-smoke
    opencpn-chart1-debug-app-smoke
    opencpn-golden-regression-smoke
    opencpn-performance-budget-contract-smoke
    opencpn-gpu-artifact-cache-contract-smoke
    opencpn-s57-portable-package-converter-smoke
    opencpn-source-to-render-inspection-smoke
    opencpn-source-to-render-production-fixture-smoke
    opencpn-draw-backend-contract-smoke
    opencpn-helm-webgpu-artifact-consumer-smoke
    opencpn-vsg-gpu-cache-smoke
    opencpn-vsg-backend-production-fixture-smoke
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Run the QA-3 dual-adapter stakeholder demo evidence script"
)
