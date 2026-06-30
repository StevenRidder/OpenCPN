# Production Hardening Map And RFC Decision Log

Status: DOCS-1 post-RFC planning gate for `UPSTREAM-1`

This document turns the RFC package, maintainer concern matrix, compatibility
matrix, golden corpus, and performance fixture into a public hardening plan. It
does not widen the renderer scope. The next implementation milestone remains a
small feature-flagged OpenCPN production-slice PR, not a broad renderer
replacement.

## Purpose

`UPSTREAM-1` should be reviewable by OpenCPN maintainers as a bounded C++ seam.
Before that PR, the project needs one place that states:

- which seams are accepted for the next slice;
- which approaches are rejected;
- how the feature flag and fallback rules work;
- how fixtures grow from the first production slice;
- which compatibility and performance claims are allowed;
- how Helm WebGPU remains a consumer boundary;
- what remains deferred until later evidence exists.

## Accepted Seams For The Next Slice

| Seam | Accepted ownership | Current evidence | Hardening rule |
| --- | --- | --- | --- |
| Chart source and converter | C++ converter modules normalize source records into portable packages. | `CHART_CONVERTER_MODULE_API.md`, S-57 converter smoke | Add chart-source formats behind converter modules, not in render backends. |
| Portable nautical package | Durable chart truth between converters and presentation. | `PORTABLE_NAUTICAL_PACKAGE.md`, package roundtrip smoke | Keep GPU/device fields out of portable records. |
| Presentation compiler | S-52/S-101 display decisions before backend handoff. | `S52_PRESENTATION_COMPILER.md`, presentation compiler smoke | Add rule coverage here, not in shaders or browsers. |
| Neutral render model | Backend-neutral primitives, resources, provenance, LOD, cache keys. | `nautical_render_model.hpp`, neutral model smoke | Backend APIs consume this model instead of chart sources. |
| GPU artifact cache | Rebuildable machine-local buffers, textures, metadata, memory budgets. | `MACHINE_LOCAL_GPU_ARTIFACT_CACHE.md`, cache smokes | Cache records are disposable device artifacts, not chart truth. |
| Draw backend | VSG/WebGPU/server targets draw decided primitives and artifacts. | `DRAW_ONLY_BACKEND_CONTRACT.md`, VSG backend smokes | Reject backend-local chart-source or S-52 ownership. |
| Inspection | Source-to-pixel trace for wrong-location and wrong-symbol debugging. | `SOURCE_TO_RENDER_INSPECTION.md`, DEBUG-2 fixture smoke | Every new production fixture keeps trace rows and pixel/query handles. |
| Golden QA | Semantic, artifact, trace, limitation, and pixel gates for fixtures. | `PRODUCTION_GOLDEN_CORPUS.md`, QA-5 smoke | Fail on semantic drift before treating pixels as enough. |
| Performance | Stage timings, memory, disk, cache-hit timing, and power telemetry status. | `PERFORMANCE_POWER_BUDGET.md`, PERF-2 smoke | Do not claim viability without measured evidence and explicit power status. |
| Compatibility | Platform/toolchain, VSG/Vulkan, WebGPU, server-raster, Metal posture. | `OPENCPN_VSG_WEBGPU_COMPATIBILITY_MATRIX.md` | Support claims require recorded environment and unavailable-target diagnostics. |
| Upstream module interface | `ocpn_plugin.h` lessons applied as a narrow C++ adapter audit. | `UPSTREAM_MODULE_INTERFACE_AUDIT.md` | Keep `UPSTREAM-1` to feature flag, lifecycle, viewport/display, validated model, backend capability, diagnostics, and fallback. |

## Rejected Approaches

These are non-starters for the production hardening path:

- browser, WebGPU, WebGL, MapLibre, or shader code deciding S-52/S-101 chart
  semantics;
- VSG/Vulkan code parsing S-57, SENC, S-101, KAP, CM93, PMTiles, MBTiles, or
  other chart-source formats directly;
- Helm HTTP/cache or UI needs becoming OpenCPN renderer truth;
- feature-flag paths silently falling back without diagnostics;
- performance claims based only on synthetic fixture budgets;
- power claims when host watt telemetry is unavailable;
- Metal presented as implemented or as Helm product priority;
- a standalone renderer repository split before OpenCPN and Helm consume the
  same accepted command stream with golden/debug evidence;
- public docs that imply automation replaces chart-renderer expert review.

## Feature-Flag Rules

The OpenCPN production slice must keep the legacy renderer as the default.

Required behavior:

- the shared renderer path is opt-in and feature-flagged;
- the adapter accepts only a validated neutral render model;
- OpenCPN keeps native window, canvas, swapchain, and host lifecycle ownership;
- legacy fallback remains available and visible;
- failed model validation, missing backend capability, missing offscreen
  support, or unavailable device/runtime emits a named diagnostic;
- the feature flag cannot bypass presentation compiler, QA, compatibility, or
  inspection gates.

The feature flag is a review boundary, not a product readiness claim.

## Fixture Growth Plan

The first production gate covers a redistributable S-57 fixture with `DEPARE`,
`DEPCNT`, and `BOYLAT`, plus diagnostic-only `LIGHTS` handling. The next slices
should grow by maintainer-selected risk, not by adding broad feature volume.

Recommended order:

1. Add Chart 1 cases for text, soundings, labels, and symbol variants that
   exercise presentation decisions.
2. Add depth and contour-heavy cases that stress fills, safety contour, SCAMIN,
   and line styling.
3. Add raster and mixed vector/raster fixtures with source provenance and
   fallback evidence.
4. Add label collision/declutter fixtures only after text shaping ownership is
   settled.
5. Add quilting/source-selection fixtures after adapter policy is reviewable.
6. Add real-chart failure fixtures only when licensing and redistribution allow
   the evidence to be shared or reproduced.

Every fixture must include package identity, neutral primitive hashes, artifact
metadata, source-to-render rows, known limitations, and deterministic pass/fail
diagnostics.

## Compatibility Decisions

Compatibility is evidence-based:

- C++17-shaped OpenCPN/CMake runtime code is the baseline.
- VSG/Vulkan is the native OpenCPN proof backend, not Helm client architecture.
- Helm WebGPU is the browser/client direction and consumes server artifacts.
- WebGL/MapLibre and server raster are fallback/composition surfaces.
- Metal remains deferred compatibility only.
- Unsupported targets must report target family, platform/compiler identity,
  failed capability, selected fallback, and source/model/artifact handles when
  backend handoff was reached.

Do not claim platform support from a single machine. Each support claim needs a
recorded compiler/toolchain, runtime, driver/browser profile, offscreen/readback
status, and diagnostic posture.

## Performance Gates

The first performance gate is the PERF-1 budget plus PERF-2 production fixture
measurements.

Allowed claims:

- the first production fixture records package load, presentation compile, GPU
  artifact compile, cache-hit timing, VSG backend render, viewport scheduling,
  memory, and disk-cache evidence;
- non-power measurements are compared against desktop reference budgets;
- power telemetry is explicit: measured when available, otherwise recorded as
  unavailable with a warning diagnostic.

Disallowed claims:

- production performance readiness;
- sustained boat-class viability without power telemetry and longer-running
  interaction scenes;
- broad device support without the compatibility matrix evidence;
- hiding cache misses or fallback paths behind optimistic frame claims.

## Helm WebGPU Boundary

Helm is a consumer of the shared C++ semantic pipeline, not the source of
OpenCPN renderer truth.

Accepted:

- Helm can drive artifact contract requirements for WebGPU, WebGL/MapLibre,
  server-raster fallback, overlays, inspection, cache/prefetch, and offline use.
- Helm Tier 2/3 overlays and UI assets keep Helm registry/provenance ownership.
- Environmental fields can use source-declared scalar/vector packets, no-data
  masks, time slices, LOD fallback, legends, and provenance handles.

Rejected:

- VSG becoming Helm client architecture;
- browser code deciding official chart portrayal;
- Helm overlay registries masquerading as S-52/S-101 chart standards;
- live Helm `:8080` or private runtime details in public proof commands.

## Decision Log

| Decision | Status | Evidence | Consequence |
| --- | --- | --- | --- |
| Keep renderer/runtime implementation C++17-shaped and CMake/OpenCPN-native. | Accepted | Working agreement, CMake targets, public proof docs | No non-C++ renderer core or sidecar runtime in the POC. |
| Keep chart semantics before backend handoff. | Accepted | presentation compiler, neutral model, backend contract | Backends draw decided primitives and artifacts only. |
| Use VSG/Vulkan as native proof backend only. | Accepted | VSG cache/backend smokes | VSG does not parse charts, own presentation, or define Helm client UX. |
| Make Helm WebGPU a consumer artifact contract. | Accepted | Helm WebGPU docs and contract smokes | Browser targets consume packets and inspection metadata without owning chart semantics. |
| Require source-to-pixel inspection. | Accepted | DEBUG-1/DEBUG-2 docs and smokes | Wrong-location and wrong-symbol reports need trace handles before renderer fixes. |
| Gate first production slice with QA-5. | Accepted | production golden corpus smoke | Semantic drift, trace loss, limitation loss, and pixel drift fail loudly. |
| Treat PERF-2 as fixture evidence, not product viability. | Accepted | production performance fixture smoke | Timing evidence exists; broad performance and power claims remain deferred. |
| Treat Metal as deferred compatibility. | Accepted | Metal compatibility note and compatibility matrix | No Metal renderer or Helm Metal priority claim. |
| Defer standalone repo extraction. | Accepted | public proof and release hygiene docs | Branch-first until shared evidence is accepted by both OpenCPN and Helm paths. |
| Audit the upstream module seam against `ocpn_plugin.h`. | Accepted | `UPSTREAM_MODULE_INTERFACE_AUDIT.md` | The first upstream PR must not copy plugin ABI, UI, route, AIS, messaging, or config scope into the renderer slice. |
| Prepare `UPSTREAM-1` as a tiny feature-flagged PR. | Pending implementation | this map plus existing gates | The next upstream PR must be small, reviewable, and explicit about non-goals. |

## Deferred Work

- Full S-52/S-101 parity.
- ECDIS certification or safety approval.
- Production WebGPU renderer implementation.
- Production Metal backend implementation.
- Real-world power telemetry and long-running boat-class interaction tests.
- Full label shaping, glyph atlas, and declutter ownership.
- Broad real-chart corpus, including licensing-safe public fixtures.
- Standalone renderer repository extraction.
- Public Helm product documentation beyond the curated WebGPU consumer boundary.

## `UPSTREAM-1` Entry Criteria

`UPSTREAM-1` may start when:

- this hardening map is merged;
- QA-5 and PERF-2 evidence are present on `vulkan/render-core-poc`;
- compatibility rules are linked from the public proof package;
- the `ocpn_plugin.h` module-interface audit is merged;
- the upstream PR scope is limited to one bounded feature-flagged production
  slice;
- public text names non-goals and deferred work before presenting evidence;
- no live Helm runtime, private chart data, generated build output, or board
  chatter is required for review.

The desired upstream result is a small reviewable OpenCPN-native PR. If the PR
cannot be reviewed without accepting the whole architecture, it is too large.
