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
- `POC-ACCEPTANCE.md` defines the POC acceptance rubric, non-goals, and
  stakeholder evidence.
- `s52/` is the placeholder for S-57/SENC plus S-52 rules to command-stream
  conversion.
- `vsg/` is the placeholder for the VulkanSceneGraph backend. It currently
  returns a structured diagnostic instead of drawing.
- `tests/fixtures/chart-1/` contains the initial command-stream fixture and
  Chart 1 acceptance catalog for golden-image work.

The POC rule is branch-first, not repo-first. Do not extract this into a
standalone renderer repository until OpenCPN and Helm both consume the same
command stream and golden image tests prove the shared semantics.
