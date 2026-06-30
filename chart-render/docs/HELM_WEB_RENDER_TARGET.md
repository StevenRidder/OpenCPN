# Helm WebGPU-First Render Target Path

`ADAPT-6` defines the Helm browser render target for the shared nautical
pipeline. Helm should share chart-source normalization, S-52/S-101 presentation,
object inspection metadata, neutral nautical render-model semantics, scheduler
policy, provenance, and cache epochs. Helm should not consume the VSG renderer
or move chart semantics into browser code.

WebGPU is the preferred Helm client render target for smooth pan/zoom, zoom
blending, palette transitions, AIS/tracks, routes, weather, and other overlay
composition. This task does not implement a full WebGPU renderer; it defines the
contract a future implementation must consume.

## Render Contract

The renderer-of-record remains the C++ OpenCPN-shaped server pipeline until a
browser target proves parity:

```text
chart source / interchange package
  -> chart-source normalization
  -> S-52/S-101 presentation compiler
  -> NauticalRenderModel
  -> adapter viewport scheduler
  -> Helm browser/offline artifact
```

The handoff to Helm is an artifact boundary, not a VSG boundary. The server can
emit these artifact families from the same model:

- Compiled primitive packet: neutral primitives, layer order, resource ids,
  atlas metadata, cache epochs, and source trace handles for a WebGPU/WebGL
  client renderer.
- Inspection packet: Chart 1/source-to-render rows, object/layer indexes,
  projection transforms, rule ids, primitive ids, cache keys, and diagnostics.
- Raster fallback tile: RGBA8 offscreen output encoded as PNG and served through
  `GET /chart/{z}/{x}/{y}.png`.
- Offline pack: PMTiles or MBTiles containing raster tiles now, and compiled
  primitive packets once parity evidence exists, with chart edition, render
  date, renderer SHA, schema version, display state, zoom range, and coverage
  metadata.
- Environmental field packet: time-varying scalar and u/v texture components
  for weather, wave, current, and warning overlays, with no-data masks, time
  interpolation, LOD parent fallback, legends, cache/provenance handles,
  inspection traces, and server-raster fallback routes.

Raw ENC, SENC, S-101, or interchange container data is never the browser render
contract.

`docs/HELM_WEBGPU_ARTIFACT_CONSUMER.md` and the C++ surface in
`include/helm_webgpu_artifact_consumer.hpp` define the first validated consumer
slice for this handoff. That slice binds the compiled primitive packet,
inspection packet, server-raster fallback, optional offline pack metadata, and
Helm `TOOLS-9`/`TOOLS-10` registry assets into one WebGPU-first browser
contract.

`docs/HELM_WEBGPU_BROWSER_FIXTURE.md` and the C++ surface in
`include/helm_webgpu_browser_fixture.hpp` define the first browser-side fixture
over that contract. It validates target selection, fallback routing, official
chart packet consumption, Helm overlay/UI composition, and safety-relevant
inspection handles without implementing browser shaders or S-52/S-101/S-100
portrayal policy in the browser.

HELMWEBGPU-3 adds the environmental field portion of the same consumer
contract. The browser can draw advisory Open-Meteo/Open-Marine model bundles
and official S-100-family met-ocean products such as S-412, S-413, and S-414
only as source-declared Helm overlay packets. Source adapters and the server
own product identity, validity, normalized values, provenance, inspection, and
fallback rasters; the browser owns composition and animation.

## Feature Detection

Helm clients choose the highest available target at runtime:

1. WebGPU: primary path for nautical chart primitives and high-frequency
   interaction/compositing once parity gaps are closed.
2. WebGL/MapLibre: useful path for basemap composition, overlays, raster packs,
   and possible compiled-primitive experiments where WebGPU is unavailable.
3. Server raster plus Canvas/DOM: safety fallback for chart pixels, inspection
   overlays, low-power browsers, and verification runs.

The route shape can remain stable while the selected target changes. A client
may still request `/chart/{z}/{x}/{y}.png` for fallback or verification, but a
WebGPU-capable client should prefer model-derived packets when available.

## Server Owns

The server-side C++ pipeline owns:

- chart-source parsing and normalization;
- S-52/S-101 object-class decisions, display categories, SCAMIN, palette,
  symbol, pattern, line-style, text, sounding, and safety-depth decisions;
- chart quilting and source selection;
- neutral primitive generation and layer ordering;
- tile/view scheduler policy, overscan, prefetch plans, zoom-level blend
  requests, and cache invalidation epochs;
- object inspection provenance from source feature to browser draw record;
- deterministic raster fallback output and golden-regression evidence.

No browser renderer may make its own S-52 or chart-source decisions to fill a
gap in the artifact contract.

## Browser Owns

The Helm browser/client owns:

- feature detection and renderer selection;
- WebGPU/WebGL/MapLibre composition of chart artifacts with AIS, routes, tracks,
  weather, satellite, places, alarms, and UI overlays;
- camera movement, animation, smooth pan/zoom, and local zoom blending using
  server-provided scheduler hints;
- client cache admission, eviction, pack mounting, and prefetch execution;
- palette or display transitions only when the server artifact includes the
  required compiled resources and display-state provenance;
- object picking and inspection UI using source trace handles and object/layer
  indexes emitted by the server.

The browser can own interaction speed without owning chart semantics.

## Offline And Cache

Offline support starts with baked raster PMTiles/MBTiles packs and should evolve
toward compiled primitive packs when WebGPU parity is proven.

Every pack or cache entry must include:

- chart/source epoch and coverage;
- renderer branch/SHA and artifact schema;
- presentation, display, and scheduler epochs;
- palette, display category, safety contour/depth settings, text/sounding
  toggles, zoom range, and overzoom policy;
- generated date and staleness metadata;
- source trace/index metadata when object inspection is supported.

PMTiles and MBTiles are packaging artifacts. They are useful for offline and
debug distribution, but they must not make browser or backend code depend on
container-specific chart semantics.

## WebGPU Gap List

Before a full WebGPU chart renderer is accepted, the POC needs evidence for:

| Gap | Required evidence |
|---|---|
| Packet schema | A C++-emitted compiled primitive packet with stable ids, layer order, resources, epochs, and source trace handles. |
| Resource model | Browser-ready symbol, pattern, line, text, raster, and atlas metadata independent of VSG objects. |
| Pipeline mapping | WebGPU buffer, texture, bind-group, and shader inputs derived from neutral primitives without chart semantics in shaders. |
| Text and labels | Text shaping, label placement, collision/declutter, soundings, and orientation with accepted golden baselines. |
| Safety parity | Safety-depth fills, safety-contour emphasis, SCAMIN, display categories, and overzoom behavior match the C++ reference path. |
| Scheduler hints | Overscan, prefetch, adjacent zoom-level blending, and cache invalidation epochs are usable by the client. |
| Inspection | A browser draw record can be traced back to source object, S-52 rule, primitive id, cache key, and tile/view. |
| Fallback | WebGPU failure falls back to WebGL/MapLibre or server raster without silent semantic changes. |

## Acceptance Guardrails

- WebGPU is the Helm client direction, not a side note.
- The server remains the semantic authority until browser parity is proven.
- The WebGPU consumer contract must distinguish Tier 1 official chart artifacts
  from Tier 2/3 Helm overlay/UI registry assets, with query/provenance hooks for
  both paths.
- WebGL/MapLibre are useful composition and fallback surfaces, not S-52
  reimplementations.
- Server-rendered raster remains the safety and verification fallback.
- Environmental fields must preserve scalar/vector texture components,
  no-data masks, time interpolation, LOD parent fallback, legends, provenance,
  and inspection handles without embedding S-100 portrayal semantics in
  browser shaders.
- Golden-image and Chart 1 inspection evidence must stay valid across targets.
- Public chart routes may stay raster-compatible while new packet endpoints
  evolve behind feature detection.

## Non-Goals

- No full WebGPU renderer implementation in this task.
- No Helm UI or `web/style.json` changes.
- No VSG dependency in Helm.
- No MapLibre S-52 style reimplementation.
- No browser-side S-100 weather/current/wave portrayal rule implementation.
- No raw chart-source parsing in browser code.
- No change to the live Helm `:8080` runtime.
