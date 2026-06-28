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
- `include/opencpn_feature_flag_adapter.hpp` sketches the ADAPT-1 OpenCPN
  feature-flag decision point for choosing the legacy canvas or shared renderer
  from a validated neutral model while OpenCPN keeps wx/swapchain ownership.
- `include/viewport_tile_scheduler.hpp` defines the ADAPT-4 adapter scheduler
  policy for visible tiles, overscan margins, prefetch rings, adjacent
  zoom-level blending, and cache invalidation epochs before backend handoff.
- `source/` documents and implements the chart-source boundary for S-57/SENC,
  raster, MBTiles/PMTiles interchange, debug fixtures, and future S-101 input.
- `include/chart_interchange.hpp` classifies MBTiles/PMTiles as optional
  interchange/debug artifacts rather than the renderer hot-path contract.
- `include/chart1_acceptance.hpp` defines the first Chart 1 point, line, and
  area acceptance catalog.
- `include/chart1_conformance.hpp` builds and validates the catalog-backed
  Chart 1 command-stream scene.
- `include/chart1_debug_app.hpp` exposes source-to-render object and layer
  inspection for the Chart 1 debug app.
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
- `docs/STAKEHOLDER_DEMO.md` defines the QA-3 dual-adapter stakeholder demo
  runbook and talk track.
- `scripts/stakeholder_demo.sh` runs the branch-local QA-3 evidence path and
  writes a stakeholder summary from the built smoke binaries.
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

ADAPT-1 evidence is the CMake-built `opencpn-feature-flag-adapter-smoke`
target. It proves the OpenCPN adapter can route a validated neutral model to the
shared renderer under a feature flag, preserve legacy fallback, and reject
missing lifecycle/model contracts without giving VSG ownership of chart
semantics.

ADAPT-4 evidence is the CMake-built `opencpn-viewport-tile-scheduler-smoke`
target. It proves the adapter scheduler owns visible/overscan/prefetch tile
selection, cache epoch invalidation, and fractional zoom blending without
moving scheduler policy into VSG or any other renderer backend.

QA-3 evidence is the CMake `opencpn-stakeholder-demo` target. It runs the
branch-local demo script over the existing smoke binaries in stakeholder order:
presentation compiler, neutral model, OpenCPN feature-flag adapter, Helm/offscreen
scheduler policy, Chart 1 debug inspection, golden regression, and VSG GPU cache.
The generated summary is suitable as the demo evidence packet; live Helm route
evidence must still be captured separately on a private port, never on Helm
`:8080`.
