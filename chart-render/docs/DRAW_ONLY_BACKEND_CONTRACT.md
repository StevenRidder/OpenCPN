# Draw-Only Backend Contract

Status: BACKEND-1 production architecture and C++ contract

This document defines the backend contract after portable packages,
presentation compilation, scheduler policy, and machine-local GPU artifact
construction:

```text
portable nautical package
  -> S-52/S-101 presentation compiler
  -> neutral nautical render model
  -> machine-local GPU artifact cache
  -> draw-only backend
```

The backend is an output target. It draws already-decided primitives or
machine-local artifacts. It does not parse charts, choose nautical symbols,
evaluate S-52/S-101 rules, quilt charts, schedule tiles, or reinterpret visual
tiers.

## Targets

The first contract covers these target families:

- VSG/Vulkan: the OpenCPN-native proof backend and first production VSG slice.
- Helm WebGPU: the preferred Helm browser/client render target.
- WebGL/MapLibre: useful fallback/composition surface where WebGPU is
  unavailable.
- Server raster: safety and verification fallback for unsupported clients and
  deterministic golden evidence.

All targets share the same semantic input boundary. A faster target can improve
interaction, compositing, upload strategy, and readback. It cannot change chart
truth.

## Input Contract

A draw backend may consume:

- a validated `NauticalRenderModel`;
- a validated `GpuArtifactCacheManifest`;
- backend-neutral resource ids, atlas metadata, cache epochs, material keys,
  draw order, and source trace handles;
- target/view uniforms derived from `RenderView` and adapter scheduler output;
- tier/provenance handles for inspection.

A draw backend must not consume:

- S-57, SENC, S-101, KAP, CM93, MBTiles, PMTiles, or other source chart files;
- portable package records directly unless a future adapter explicitly
  converts them into neutral primitives first;
- presentation catalogs or S-52/S-101 rule tables;
- Helm overlay registries as if they were official chart standards;
- OpenCPN wx canvas ownership or Helm HTTP/cache policy.

## Ownership

The layers before the backend own:

- converter modules: source decoding and portable package production;
- presentation compiler: S-52/S-101 semantics, display categories, SCAMIN,
  safety depth/contours, symbol/line/pattern/text/sounding selection, layer
  order, and diagnostics;
- adapter scheduler: visible tiles, overscan, prefetch, zoom-level blending,
  cache invalidation epochs, and viewport policy;
- GPU artifact cache: machine-local buffers, textures, atlases, material keys,
  memory budgets, and rebuildable device/backend artifact ids.

The backend owns:

- device/surface setup;
- swapchain or offscreen target binding;
- buffer, texture, atlas, and material binding;
- draw submission order supplied by the model/artifact manifest;
- readback or frame presentation;
- backend performance counters and explicit fallback diagnostics.

The backend must reject input that asks it to own chart-source, presentation,
quilting, scheduler, or visual-tier policy.

## Visual Tiers

Backends can draw multiple visual tiers in one frame, but they must preserve
the source of meaning:

| Tier | Owner | Examples |
|---|---|---|
| Tier 1 official chart | `presentation_compiler` | S-52/S-101 chart primitives, official symbols, raster chart patches |
| Tier 2 Helm overlay | overlay registry | AIS, route, track, weather, environmental fields |
| Tier 3 UI asset | UI registry | handles, cursors, selection halos, panel affordances |

Tier 2/3 assets may be drawn by WebGPU, WebGL/MapLibre, VSG, or server raster
fallbacks, but they need their own registry/provenance handles. They must not
claim `presentation_compiler` ownership or `s52-compatible`/`s101` source
standards.

## C++ Contract

The C++ contract in `include/draw_backend_contract.hpp` provides:

- `DrawBackendCapabilities`: target type, device/material profile, neutral
  model/artifact inputs, surfaces, readback, overlay composition, and fallback
  ids;
- `DrawInputTierHandle`: semantic tier, owner, source standard, registry id,
  provenance refs, and primitive ids;
- `DrawBackendContract`: the normalized draw-only contract tying one backend
  target to one model and GPU artifact manifest;
- `ValidateDrawBackendContract`: identity, ownership, capability, policy-leak,
  and tier-split validation;
- `ValidateDrawBackendHandoff`: model, artifact manifest, target acceptance,
  and primitive draw-only handoff validation.

The smoke target `opencpn-draw-backend-contract-smoke` proves:

- VSG and WebGPU targets can consume the same model/artifact boundary;
- Tier 1 official chart handles survive into backend contracts;
- Tier 2/3 Helm overlay/UI tiers can be present without masquerading as chart
  truth;
- backend-owned semantics, policy words in material profiles, and invalid
  primitive handoffs are rejected.

## Fallbacks

Fallback is allowed only when it is visible and semantic-preserving:

- WebGPU can fall back to WebGL/MapLibre or server raster;
- VSG can fall back to the legacy OpenCPN renderer while the feature flag is
  off or while verification explicitly requests fallback;
- server raster can provide deterministic PNG evidence for tests and
  unsupported clients.

Fallbacks must not silently change display category, SCAMIN, safety contour,
palette, chart-source, quilting, or scheduler decisions. They must carry enough
diagnostics to explain which backend rendered the frame and why.

## Non-Goals

- No full VSG production renderer in this task.
- No full Helm WebGPU renderer in this task.
- No WebGL/MapLibre S-52 style implementation.
- No source chart parsing in backend code.
- No tile scheduler, HTTP/ETag, or live Helm `:8080` change.
- No claim of ECDIS or full S-52/S-101 parity.
