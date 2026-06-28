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
- `include/render_backend.hpp` defines the renderer backend interface and
  onscreen/offscreen target boundary.
- `source/` documents and implements the chart-source boundary for S-57/SENC,
  raster, MBTiles/PMTiles interchange, debug fixtures, and future S-101 input.
- `include/chart_interchange.hpp` classifies MBTiles/PMTiles as optional
  interchange/debug artifacts rather than the renderer hot-path contract.
- `s52/` is the placeholder for S-57/SENC plus S-52 rules to command-stream
  conversion.
- `vsg/` is the placeholder for the VulkanSceneGraph backend. It currently
  returns a structured diagnostic instead of drawing.
- `tests/fixtures/chart-1/scene.commands.json` is the initial text fixture for
  command-stream and golden-image work.

The POC rule is branch-first, not repo-first. Do not extract this into a
standalone renderer repository until OpenCPN and Helm both consume the same
command stream and golden image tests prove the shared semantics.
