# RFC: Shared OpenCPN/Helm Render Core POC

Status: public RFC package

This RFC package asks for architecture review of a small C++ renderer seam, not
approval of a wholesale plotter rewrite. The proof is centered on the
OpenCPN-facing `vulkan/render-core-poc` branch and its `chart-render/` subtree.
Helm is included as a consumer because it needs the same nautical model and
inspection contract for a WebGPU-first browser path.

## Problem Statement

OpenCPN and Helm both need chart rendering that is:

- maintainable by chart-rendering experts;
- modular enough to replace chart-source converters independently;
- explicit about S-52/S-101 presentation decisions;
- debuggable from final pixels back to source chart objects;
- capable of feeding both an OpenCPN interactive adapter and a Helm
  headless/offscreen adapter;
- honest about what is proven and what remains a POC.

The central question is:

```text
Can OpenCPN and Helm share an upstream-shaped C++ render core and neutral
nautical render model without creating a Helm-first fork, a sidecar rewrite, or
a monolithic AI-generated plotter?
```

## Non-Goals

- Not full S-52 parity.
- Not ECDIS certification or safety approval.
- Not a primary-navigation recommendation.
- Not a replacement for all existing OpenCPN rendering paths.
- Not a production WebGPU renderer.
- Not a production Metal backend.
- Not a standalone renderer repository extraction decision.
- Not permission to move OpenCPN-derived chart semantics into Helm web/mobile
  clients.

## Architecture Overview

The POC keeps chart semantics before backend handoff:

```text
source charts / debug interchange
        |
        v
IChartSource -> ChartSourceProduct + provenance
        |
        v
portable nautical package
        |
        v
S-52/S-101 presentation compiler
        |
        v
NauticalRenderModel
        |
        +--> OpenCPN interactive adapter -> VSG native proof backend
        |
        +--> Helm headless/offscreen adapter -> server artifact boundary
                                               |
                                               +--> WebGPU browser target
                                               +--> WebGL/MapLibre fallback
                                               +--> server-raster tile fallback
```

The shared core owns source normalization, presentation decisions, neutral
primitives, provenance, and render-model validation. Adapters own host
integration: OpenCPN owns wx/canvas/swapchain behavior; Helm owns tile math,
PNG/readback, HTTP/cache policy, diagnostics presentation, and browser
composition.

## Module Boundaries Under Review

| Module | Owns | Evidence |
|---|---|---|
| Chart-source/converter boundary | Source parsing/normalization into the portable nautical package, plus provenance, update metadata, diagnostics, and conversion trace handles. | `docs/CHART_CONVERTER_MODULE_API.md`, `source/README.md`, `source/INTERCHANGE.md`, `include/chart_source.hpp` |
| Portable nautical package | Durable source-neutral chart truth between replaceable converters and presentation compilation; not a GPU cache or backend format. | `docs/PORTABLE_NAUTICAL_PACKAGE.md` |
| S-52/S-101 presentation compiler | Display category, SCAMIN, palette, symbol, text, sounding, and safety-depth decisions before backend handoff. | `docs/PRESENTATION_COMPILER_BOUNDARY.md`, `docs/S52_PRESENTATION_COMPILER.md`, `include/s52_presentation_compiler.hpp` |
| Neutral nautical render model | Backend-neutral layers, primitives, resource table, LOD hints, coverage metadata, cache keys, diagnostics, and source traces. | `include/nautical_render_model.hpp`, `opencpn-neutral-model-smoke` |
| Adapter scheduler policy | Visible tiles, overscan, prefetch, adjacent zoom blending, and cache invalidation epochs before renderer backend handoff. | `include/viewport_tile_scheduler.hpp`, `opencpn-viewport-tile-scheduler-smoke` |
| VSG proof backend | Native draw/cache proof fed by neutral primitives; no chart-source or S-52 ownership. | `vsg/GPU_CACHE.md`, `opencpn-vsg-gpu-cache-smoke` |
| OpenCPN adapter | Feature-flagged route from validated neutral model into the shared renderer while preserving legacy fallback and host ownership. | `include/opencpn_feature_flag_adapter.hpp`, `opencpn-feature-flag-adapter-smoke` |
| Helm adapter/target contract | Consumer-side offscreen/server artifact path, WebGPU-first client direction, WebGL/MapLibre and server-raster fallbacks. | `docs/HELM_WEB_RENDER_TARGET.md`, Helm `docs/VULKAN-HELM-WEBGPU-PROOF.md` |
| Debug and regression evidence | Chart 1 inspection, source-to-render provenance, golden-regression smoke, and honest pending-baseline reporting. | `docs/CHART1_DEBUG_APP.md`, `source/QA.md`, `opencpn-golden-regression-smoke` |

## Maintainer Concerns Addressed

The companion matrix in `docs/MAINTAINER_RESPONSE_MATRIX.md` treats maintainer
concerns as acceptance criteria. The short version:

- The POC is scoped to `chart-render/`, not a giant plotter replacement.
- Runtime renderer implementation remains C++/CMake/OpenCPN-native.
- Support scripts and JSON fixtures are evidence helpers, not renderer runtime.
- Chart-source conversion, presentation, model, scheduler, backend, adapters,
  and fixtures are separable modules.
- Replaceable chart converters produce the portable nautical package; renderer
  backends do not parse source chart formats or own conversion quirks.
- The portable nautical package is the production path toward a GPU-friendly
  chart format while still separating durable chart truth from machine-local GPU
  artifacts.
- The presentation compiler is the semantic owner for S-52/S-101 display
  category, SCAMIN, palette, ordering, safety, text, and sounding decisions.
- VSG/Vulkan is draw/cache-only proof backend, not chart-semantics owner.
- Helm is a consumer/product target, not the source of OpenCPN renderer truth.
- Public messaging must not claim AI can replace maintainers or expert review.

## Source-To-Render Debugging

Wrong-location and wrong-symbol bugs must be explainable without reading
backend-specific rendering code. The intended trace is:

```text
source chart id
  -> source object id / class
  -> geometry hash
  -> projection transform
  -> presentation rule id
  -> neutral primitive id
  -> cache key / backend primitive id
  -> final asset or pixel output
```

`docs/CHART1_DEBUG_APP.md` and the Chart 1 debug smoke cover this at fixture
scale. Future real-chart bug reports should attach the same trace fields before
renderer math changes are made.

## Helm/WebGPU Contract

Helm's product direction is WebGPU-first, with WebGL/MapLibre and server-raster
fallbacks. That does not make VSG the Helm client architecture.

The server-side C++ pipeline remains the semantic authority until a browser
target proves parity. A future Helm WebGPU client should consume compiled
primitive packets, inspection packets, resource metadata, cache epochs, and
scheduler hints derived from `NauticalRenderModel`. The browser may own camera
motion, composition, cache admission, picking UI, and overlays; it must not
invent S-52/S-101 semantics to fill model gaps.

## Reproducible Evidence

From the OpenCPN proof branch:

```sh
cmake -S chart-render -B /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build --target opencpn-stakeholder-demo
```

The stakeholder demo target runs the current local evidence path:

- `opencpn-s52-presentation-compiler-smoke`
- `opencpn-neutral-model-smoke`
- `opencpn-feature-flag-adapter-smoke`
- `opencpn-viewport-tile-scheduler-smoke`
- `opencpn-chart1-debug-app-smoke`
- `opencpn-golden-regression-smoke`
- `opencpn-vsg-gpu-cache-smoke`

Generated summaries and logs are disposable build artifacts under `/tmp`; do not
commit them.

## Current Limitations

- Chart 1 and depth fixtures are early acceptance evidence, not full chart
  coverage.
- Golden image baselines still include pending slots; pending evidence is
  reported honestly rather than treated as parity.
- Performance evidence is smoke/counter-level only. It is not a production
  benchmark.
- The OpenCPN interactive adapter is still a POC feature-flag path with legacy
  fallback.
- Helm HTTP route evidence is separate from this OpenCPN branch and must run on
  a private Helm port.
- Metal compatibility remains deferred unless a later branch lands that evidence.
- Standalone repository extraction remains gated on dual-adapter consumption,
  golden evidence, GUI-free core shape, maintainer agreement, and documented
  GPL/upstream boundaries.

## Open Questions For Maintainers

1. Are the chart-source, presentation compiler, neutral model, adapter, and
   backend seams the right review units?
2. Does `NauticalRenderModel` carry the right trace, coverage, LOD, resource,
   and cache metadata for wrong-location debugging and GPU handoff?
3. Which Chart 1, depth, raster, label, text, safety, and quilting cases should
   become the next required fixtures?
4. Which S-52/S-101 rules should be reviewed first to avoid building on the
   wrong abstraction?
5. What diagnostics would make OpenCPN maintainers comfortable reviewing a VSG
   backend without letting that backend own semantics?
6. What performance counters or target scenes should be required before any
   public performance claim?
7. Is the standalone repository extraction gate sufficient, or should additional
   upstream review criteria be added?
8. Is the Helm WebGPU artifact boundary acceptable as a consumer target, or does
   it need different packet/resource/provenance fields?

## RFC Package Links

- `README.md` - branch map and evidence summary.
- `POC-ACCEPTANCE.md` - POC acceptance rubric and non-goals.
- `docs/OPENCPN_PUBLIC_PROOF_BRANCH.md` - sanitized public proof branch guide.
- `docs/PUBLIC_RELEASE_HYGIENE.md` - release hygiene audit and gates.
- `docs/MAINTAINER_RESPONSE_MATRIX.md` - concern/response matrix.
- `docs/PORTABLE_NAUTICAL_PACKAGE.md` - production chart package architecture.
- `docs/CHART_CONVERTER_MODULE_API.md` - replaceable source-chart converter
  module API.
- `docs/PRESENTATION_COMPILER_BOUNDARY.md` - production S-52/S-101 compiler
  boundary.
- `docs/STAKEHOLDER_DEMO.md` - stakeholder demo runbook.
- `docs/HELM_WEB_RENDER_TARGET.md` - Helm WebGPU-first target contract.
- Helm `docs/VULKAN-HELM-WEBGPU-PROOF.md` - Helm-side public proof note.

The intended public ask is narrow: please review the seams, evidence, and
fixture choices before this grows into broader production work.
