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
  render model consumed by the VSG proof backend, Helm WebGPU/browser
  artifacts, and future OpenGL/Metal compatibility work.
- `docs/PORTABLE_NAUTICAL_PACKAGE.md` defines the production package layer that
  sits between replaceable chart-source converters and the presentation
  compiler. It is durable chart truth, not a GPU cache or backend format.
- `docs/CHART_CONVERTER_MODULE_API.md` defines the replaceable converter module
  boundary for S-57/S-101, raster/KAP, CM93, OSM/community, future sonar/depth,
  and MBTiles/PMTiles import/export/debug paths.
- `docs/PRESENTATION_COMPILER_BOUNDARY.md` defines the production S-52/S-101
  compiler boundary from portable package plus display state to neutral
  nautical render primitives.
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
- `include/gpu_artifact_cache_contract.hpp` defines the CACHE-1
  machine-local GPU artifact cache contract for rebuildable backend/device
  records, memory budgets, invalidation domains, and tier/provenance handles.
- `include/draw_backend_contract.hpp` defines the BACKEND-1 draw-only backend
  contract for VSG/Vulkan, Helm WebGPU, WebGL/MapLibre fallback, and server
  raster targets without letting backends own chart semantics or visual tiers.
- `include/helm_webgpu_artifact_consumer.hpp` defines the HELMWEBGPU-1 browser
  artifact consumer contract. Helm WebGPU consumes compiled primitive packets,
  inspection packets, server-raster fallback records, and Helm overlay/UI
  registry assets without importing VSG/OpenCPN internals or owning
  S-52/S-101 semantics.
- `include/helm_webgpu_browser_fixture.hpp` defines the HELMWEBGPU-2 minimal
  browser consumer fixture. It proves feature detection, WebGPU/WebGL/server
  raster fallback selection, Tier 1 chart artifact consumption, Tier 2/3
  overlay/UI composition, and safety-relevant inspection handles without
  browser-owned chart semantics.
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
- `docs/RFC_RENDER_CORE_POC.md` is the public RFC package entry point for
  architecture review of the shared OpenCPN/Helm render-core proof.
- `docs/MACHINE_LOCAL_GPU_ARTIFACT_CACHE.md` records the production cache
  contract above backend-specific VSG/WebGPU/WebGL/Metal-compatible artifact
  implementations.
- `docs/DRAW_ONLY_BACKEND_CONTRACT.md` records the production backend handoff:
  renderers consume neutral primitives or compiled GPU artifacts, preserve
  Tier 1/Tier 2/Tier 3 provenance, and remain draw/cache-only.
- `docs/HELM_WEBGPU_ARTIFACT_CONSUMER.md` records the first Helm browser
  consumer slice over the same package, presentation, cache, backend, and
  inspection contracts.
- `docs/HELM_WEBGPU_BROWSER_FIXTURE.md` records the HELMWEBGPU-2 fixture that
  composes official chart packets with Helm registry assets and validates
  feature-detected fallback behavior.
- `docs/OPENCPN_COMMUNITY_RFC_POST_DRAFT.md` drafts the public community post
  asking for architecture review and maintainer seam feedback.
- `docs/STAKEHOLDER_DEMO.md` defines the QA-3 dual-adapter stakeholder demo
  runbook and talk track.
- `docs/PUBLIC_RELEASE_HYGIENE.md` records the PUB-1 publication hygiene audit,
  public branch/PR matrix, and sanitization gates before public sharing.
- `docs/OPENCPN_PUBLIC_PROOF_BRANCH.md` defines the PUB-2 sanitized public
  review target, evidence commands, and exclusions for the OpenCPN proof
  branch.
- `docs/UPSTREAM_MODULE_INTERFACE_AUDIT.md` audits the renderer/module seams
  against OpenCPN's `ocpn_plugin.h` before the first upstream-facing production
  slice. It maps viewport, chart metadata, presentation, overlays, lifecycle,
  capabilities, nav/AIS, routes, messaging, and config to explicit accepted or
  deferred boundaries.
- `docs/UPSTREAM_PRODUCTION_SLICE.md` defines the first tiny feature-flagged
  OpenCPN production slice for upstream review: one bounded S-57 fixture through
  converter, portable package, presentation compiler, GPU cache/backend, golden
  regression, inspection trace, and performance evidence.
- `docs/MAINTAINER_RESPONSE_MATRIX.md` maps maintainer concerns to public RFC
  response posture, current evidence, limitations, and follow-up acceptance
  criteria.
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
- `docs/METAL_BACKEND_COMPATIBILITY.md` records the ADAPT-5 deferred native
  Metal compatibility checkpoint and gap list without implementing a Metal
  renderer.
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

ADAPT-5 evidence is `docs/METAL_BACKEND_COMPATIBILITY.md`. It confirms the
neutral model and adapter seam leave room for a future native Apple backend,
then names the platform surface, pipeline, GPU-cache, text, synchronization,
and debug gaps that must close before any Metal renderer work starts.

CACHE-1 evidence is the CMake-built
`opencpn-gpu-artifact-cache-contract-smoke` target. It proves the generic
machine-local cache contract can derive rebuildable artifact records from the
neutral model, preserve Tier 1/provenance handles, report memory-budget
pressure, and reject source/presentation policy leaks in backend artifact keys.

BACKEND-1 evidence is the CMake-built `opencpn-draw-backend-contract-smoke`
target. It proves VSG and WebGPU targets can consume the same draw-only
model/artifact boundary, preserves official-chart versus overlay/UI tier
handles, and rejects backend-owned chart semantics or Tier 2/3 masquerading as
S-52/S-101 chart truth.

HELMWEBGPU-1 evidence is the `helm_webgpu_artifact_consumer` C++ contract and
smoke. It proves Helm's WebGPU-first client target consumes the shared neutral
model, GPU artifact cache, draw-only backend, and source-to-render inspection
contracts while preserving Tier 1 official chart truth separately from Helm
Tier 2 overlays and Tier 3 UI registry assets.

HELMWEBGPU-2 evidence is the CMake-built
`opencpn-helm-webgpu-browser-fixture-smoke` target. It builds a minimal Helm
browser artifact consumer fixture from the HELMWEBGPU-1 contract, validates
WebGPU/WebGL/server-raster feature detection, composes Tier 1 chart artifacts
with Tier 2/3 Helm assets, and keeps safety-relevant inspection/query handles
available for rendered and server-declared hidden/simplified chart states.

HELMWEBGPU-4 evidence is the CMake-built
`opencpn-helm-webgpu-playwright-fixture` target plus the Playwright spec at
`tests/web/helm_webgpu_fixture.spec.js`. The C++ target exports deterministic
fixture JSON and a static harness; Playwright loads it in Chromium and checks
WebGPU, WebGL/MapLibre, and server-raster selection without adding browser
chart-semantics code.

UPSTREAM-2 evidence is `docs/UPSTREAM_MODULE_INTERFACE_AUDIT.md`. It checks the
new renderer seams against OpenCPN's `ocpn_plugin.h` surface, then names the
small interface subset that belongs in the first feature-flagged upstream slice
and the plugin/module surfaces that remain explicit non-goals.

UPSTREAM-1 evidence is the CMake-built
`opencpn-upstream-production-slice-smoke` target and
`docs/UPSTREAM_PRODUCTION_SLICE.md`. The smoke target verifies the feature flag
keeps legacy canvas fallback by default, then runs the bounded production
fixture through golden corpus, source-to-render inspection, VSG draw/cache
handoff, and performance evidence without adding plugin/UI/navigation scope.
