# Upstream Production Slice

Status: UPSTREAM-1 production adoption slice

This is the first upstream-facing production slice for the render-core proof.
It is intentionally small: one redistributable S-57 fixture moves through the
C++ chart-render pipeline while the legacy OpenCPN renderer remains the default
unless a feature flag selects the shared renderer.

The slice is not a request to replace the OpenCPN renderer. It is a maintainer
review package for one bounded path.

## Slice Path

```text
OpenCPN chart canvas and host lifecycle
  -> opt-in feature flag
  -> S-57 converter fixture
  -> portable nautical package
  -> S-52 presentation compiler
  -> neutral nautical render model
  -> GPU artifact cache contract
  -> draw-only VSG proof backend
  -> golden corpus + source-to-render inspection + performance evidence
```

The legacy renderer remains the default when the flag is disabled, when OpenCPN
does not own the wx canvas/swapchain lifecycle, or when the neutral model is
missing or invalid.

## Bounded Fixture

The fixture is the QA-5/PERF-2 production slice:

- source id: `s57:US5CONVERT2`;
- package id: `s57:US5CONVERT2:package`;
- package hash: `fnv1a64:e2c66b175a87d42b`;
- render target: 256 x 256 offscreen image;
- golden image hash: `009410097424697d`;
- chart objects: `DEPARE.1001`, `DEPCNT.2001`, `BOYLAT.3001`;
- inspection rows: 3 source-to-pixel rows;
- GPU artifacts: 13 machine-local artifact records.

## Evidence Target

`opencpn-upstream-production-slice-smoke` is the UPSTREAM-1 gate. It verifies:

- the feature flag selects the shared renderer only for a valid neutral model;
- the disabled flag keeps the legacy renderer route;
- OpenCPN wx canvas/swapchain lifecycle ownership is required;
- the golden corpus still matches the package, primitive, artifact, image, and
  inspection evidence;
- semantic drift fails before pixel-only evidence can hide it;
- performance evidence measures the same package/image/inspection slice;
- power telemetry remains explicit when unavailable instead of fabricated.

Run the slice from a checkout of `vulkan/render-core-poc`:

```sh
cmake -S chart-render -B /tmp/opencpn-vulkan-upstream-slice-build
cmake --build /tmp/opencpn-vulkan-upstream-slice-build \
  --target opencpn-upstream-production-slice-smoke
/tmp/opencpn-vulkan-upstream-slice-build/opencpn-upstream-production-slice-smoke
```

For the broader evidence packet:

```sh
cmake --build /tmp/opencpn-vulkan-upstream-slice-build \
  --target opencpn-stakeholder-demo
```

## Review Boundary

This slice includes:

- C++17-shaped OpenCPN-native code under `chart-render/`;
- one feature-flag route and visible legacy fallback;
- a validated neutral model before backend handoff;
- draw-only backend ownership;
- source-to-render inspection rows;
- golden regression and performance evidence.

This slice does not include:

- full S-52/S-101 parity;
- production renderer readiness;
- ECDIS certification or safety approval;
- plugin ABI replacement;
- toolbar, menu, preference, keyboard, mouse, or wxAUI integration;
- route/waypoint mutation, AIS controls, autopilot behavior, or plugin
  messaging;
- Helm HTTP runtime, Helm live `:8080`, MapLibre policy, WebGPU renderer
  implementation, or Metal renderer implementation.

If a future upstream PR needs any of those excluded surfaces, it should split
that work into a separate reviewable task before broadening this slice.

