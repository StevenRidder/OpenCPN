# Chart Render Skeleton

This directory is the first upstream-shaped Vulkan renderer seam scaffold for
the `vulkan/render-core-poc` branch. It is intentionally independent of Helm
HTTP routes, MapLibre policy, OpenCPN wx canvas globals, and VulkanSceneGraph
headers.

The current shape mirrors the Vulkan board seam work:

- `include/render_view.hpp` defines target-independent viewport and display
  settings.
- `include/chart_source.hpp` defines the pluggable chart-source boundary before
  S-52 conversion.
- `include/render_scene.hpp` defines the backend-neutral render command stream.
- `include/nautical_render_model.hpp` defines the backend-neutral nautical
  render model consumed by VSG/OpenGL/Metal/WebGPU/Helm backends.
- `include/conversion_trace.hpp` defines source-to-command traceability for
  wrong-location debugging.
- `include/render_backend.hpp` defines the renderer backend interface and
  onscreen/offscreen target boundary.
- `source/` documents and implements the chart-source boundary for S-57/SENC,
  raster, MBTiles/PMTiles interchange, debug fixtures, and future S-101 input.
- `include/chart_interchange.hpp` classifies MBTiles/PMTiles as optional
  interchange/debug artifacts rather than the renderer hot-path contract.
- `include/chart1_acceptance.hpp` defines the first Chart 1 point, line, and
  area acceptance catalog.
- `include/chart1_conformance.hpp` builds and validates the catalog-backed
  Chart 1 command-stream scene.
- `include/chart1_baseline.hpp` defines the Chart 1 baseline-comparison
  manifest and tolerance contract.
- `include/depth_tessellation.hpp` prototypes shoreline, land, depth area, and
  area-pattern tessellation into render commands.
- `include/depth_safety.hpp` applies safety-depth classification, palette
  buckets, and zoom-aware contour visibility to depth commands.
- `include/depth_performance.hpp` measures the contour-heavy fixture through
  the offscreen backend interface for early timing and memory smoke checks.
- `include/golden_regression.hpp` wires Chart 1 and depth fixtures into a
  repeatable golden-regression smoke command.
- `POC-ACCEPTANCE.md` defines the POC acceptance rubric, non-goals, and
  stakeholder evidence.
- `s52/` is the placeholder for S-57/SENC plus S-52/S-101 presentation
  compilation to the command stream and neutral model.
- `vsg/` is the placeholder for the VulkanSceneGraph backend. It accepts the
  neutral nautical render model, prepares a VSG-only GPU asset cache manifest,
  and currently returns a structured diagnostic instead of drawing.
- `vsg/GPU_CACHE.md` documents the machine-local VSG GPU cache boundary:
  descriptor-ready textures/atlases, scene/frame buffers, stable keys, and
  provenance handles derived from the neutral model without backend-owned chart
  semantics.
- `tests/fixtures/chart-1/` contains the initial command-stream fixture and
  Chart 1 acceptance catalog for golden-image work.

The POC rule is branch-first, not repo-first. The semantic center is the neutral
nautical render model: chart-source parsing, S-52/S-101 presentation rules,
quilting, and scheduling policy stay before the backend boundary. Do not extract
this into a standalone renderer repository until OpenCPN and Helm both consume
the same model and golden image tests prove the shared semantics.
