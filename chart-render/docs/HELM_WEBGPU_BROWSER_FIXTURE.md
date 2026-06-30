# Helm WebGPU Browser Fixture

Status: HELMWEBGPU-2 production vertical slice fixture

`include/helm_webgpu_browser_fixture.hpp` and
`source/helm_webgpu_browser_fixture.cpp` build the first minimal Helm-side
browser artifact consumer fixture. It is still C++ contract/test code, not a
browser renderer implementation. The fixture proves that a Helm client can
consume model-derived packets, inspection metadata, and Helm registry assets
without importing OpenCPN/VSG internals or moving chart-source semantics into
browser code.

## What It Consumes

The fixture starts from the HELMWEBGPU-1 consumer contract:

- Tier 1 official chart artifacts from the presentation/compiler/cache path;
- source-to-render inspection hooks for object and pixel queries;
- server-raster fallback routes declared by the draw backend contract;
- Tier 2 Helm overlay/environmental packets from the Helm registry;
- Tier 3 Helm UI assets from the Helm UI registry.

Tier 1 items are always marked `compose_server_chart_artifact_only`. The
fixture requires source standard, provenance, primitive/query handles, and
`presentation_compiler` ownership for those chart packets.

Tier 2/3 items remain Helm-owned. AIS, route, weather/environment, pins, and UI
handles can be composed by the browser, but they cannot masquerade as official
S-52/S-101 chart truth.

## Feature Detection

`HelmWebgpuBrowserFeatureProfile` describes the client capability check:

- WebGPU available with required storage buffers and texture arrays;
- WebGL/MapLibre fallback available;
- server-raster fallback available;
- offline cache availability.

`BuildHelmWebgpuBrowserConsumerFixture` selects:

1. WebGPU when the profile is usable;
2. WebGL/MapLibre when WebGPU is unavailable and that fallback is declared;
3. server raster when browser GPU paths are unavailable.

Every fallback must reference a visible, semantic-preserving route from the
HELMWEBGPU-1 contract. A fallback cannot silently restyle chart semantics.

## Safety And Inspection

The fixture carries safety-relevant traces from the inspection report:

```text
source chart/object
  -> presentation rule
  -> primitive/artifact
  -> final web asset
  -> object query / pixel query
```

It also includes a server-declared hidden/simplified low-zoom trace. This is a
fixture proof that the browser can warn, query, or explain a safety-relevant
state while still treating visibility, SCAMIN, display category, safety-contour,
and symbol decisions as server-side presentation output.

## Non-Goals

- No TypeScript, JavaScript, or browser production runtime.
- No WebGPU shader implementation.
- No browser-side S-52/S-101/S-100 portrayal logic.
- No VSG or OpenCPN internal object leakage into Helm web code.
- No live Helm `:8080` change.
