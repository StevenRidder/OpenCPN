# Helm WebGPU Artifact Consumer Contract

Status: HELMWEBGPU-1 production architecture and C++ contract

This document defines the first Helm browser production slice that consumes the
same portable-package, presentation, GPU-cache, draw-backend, and inspection
contracts as the OpenCPN/VSG proof path:

```text
portable nautical package
  -> S-52/S-101 presentation compiler
  -> neutral nautical render model
  -> machine-local GPU artifact cache
  -> draw-only WebGPU backend contract
  -> Helm browser artifact consumer
```

The Helm browser is an artifact consumer. It composes chart artifacts with
overlays and UI assets; it does not parse charts, run S-52/S-101 rules, choose
display categories, apply SCAMIN, pick safety contours, quilt sources, or infer
official symbols from Helm registries.

## Inputs

The consumer contract is built from:

- a validated `NauticalRenderModel`;
- a validated `GpuArtifactCacheManifest` whose backend target is `webgpu`;
- a `DrawBackendContract` whose target is WebGPU and whose capabilities include
  overlay/UI composition;
- a `SourceToRenderInspectionReport` with object, pixel, primitive, artifact,
  and final web asset handles;
- Helm `TOOLS-9`/`TOOLS-10` registry assets for Tier 2 overlays and Tier 3 UI
  controls.

The browser does not consume VSG objects, OpenCPN wx canvas state, raw ENC/SENC
or S-101 datasets, PMTiles/MBTiles internals, presentation catalogs, or
backend-specific cache policy.

## Visual Tier Rules

The contract keeps the Helm visual taxonomy explicit:

| Tier | Owner | Browser role |
|---|---|---|
| Tier 1 official chart | `presentation_compiler` | Draw compiled chart packets and expose source-to-render inspection |
| Tier 2 Helm overlay | `helm_overlay_registry` | Compose AIS, route, track, weather, and environmental overlays |
| Tier 3 UI asset | `helm_ui_registry` | Compose controls, handles, hit affordances, and panel assets |

Tier 1 artifacts must carry source standard, primitive, provenance, cache, and
inspection handles. Tier 2/3 assets must carry Helm registry ids and must not
claim `s52-compatible`, `s101-compatible`, or `presentation_compiler`
ownership.

## C++ Contract

The C++ surface in `include/helm_webgpu_artifact_consumer.hpp` provides:

- `HelmWebgpuConsumerOptions`: client id, route prefix, packet schema, primary
  target, fallback inclusion, and Helm registry assets;
- `HelmWebgpuArtifactSlice`: compiled primitive packet, inspection packet,
  server-raster fallback tile, offline pack, and Helm registry asset records;
- `HelmWebgpuFallbackRoute`: visible WebGPU to WebGL/MapLibre and server-raster
  fallback declarations;
- `HelmWebgpuInspectionHook`: browser object/pixel query handles tied back to
  source chart/object, presentation rule, primitive id, artifact id, and final
  web asset id;
- `HelmWebgpuConsumerContract`: the normalized browser artifact consumer
  contract tying model, cache, backend, inspection, fallbacks, and visual tiers
  together;
- `ValidateHelmWebgpuConsumerHandoff`: end-to-end validation across the model,
  cache manifest, draw backend contract, inspection report, and browser
  consumer contract.

## Required Artifact Families

A valid first slice includes:

- compiled primitive packet for WebGPU;
- inspection packet for object/pixel/debug queries;
- server-raster fallback tile for unsupported clients and verification runs;
- optional offline pack metadata derived from the same model/cache epochs;
- at least one Helm-owned Tier 2 or Tier 3 registry asset so the consumer proves
  the taxonomy split instead of treating all visuals as chart truth.

The compiled primitive packet and inspection packet may be serialized later by
HELMWEBGPU-2. HELMWEBGPU-1 only defines the contract and validation surface.

## Fallback Rules

Fallback is valid only when it is visible and semantic-preserving:

- WebGPU is the primary target.
- WebGL/MapLibre may compose overlays and fallback chart packets where useful.
- Server raster provides the deterministic chart-pixel fallback.
- Fallbacks must preserve the server-side presentation output and must emit a
  diagnostic or route reason; they cannot silently re-style chart semantics.

## Inspection Rules

Each Tier 1 browser artifact must preserve a query path:

```text
source chart/object
  -> converter/package
  -> S-52/S-101 presentation rule
  -> neutral primitive
  -> GPU artifact/cache record
  -> WebGPU draw item / final web asset
  -> object query or pixel query
```

Wrong-location reports on Tier 1 route to converter/projection ownership.
Wrong-symbol reports on Tier 1 route to converter, presentation, or backend
handoff ownership. Tier 2/3 overlay/UI reports route to the Helm registry or UI
layer.

## Non-Goals

- No full WebGPU renderer implementation.
- No TypeScript, JavaScript, or browser runtime production code in this slice.
- No VSG dependency in Helm web code.
- No WebGL/MapLibre S-52 style reimplementation.
- No raw chart-source parsing in the browser.
- No live Helm `:8080` change.
