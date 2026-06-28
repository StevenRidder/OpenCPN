# QA-3 Stakeholder Demo

This demo script is the final Vulkan POC walk-through for stakeholders. It is
designed to show one upstream-shaped renderer core serving the OpenCPN
interactive adapter and the Helm headless/offscreen tile path without turning the
work into a Helm-only fork or a backend-owned chart semantics layer.

## Run It

From the OpenCPN checkout on `vulkan/render-core-poc` or a QA branch:

```sh
cmake -S chart-render -B /tmp/helm-vulkan-qa3-build
cmake --build /tmp/helm-vulkan-qa3-build
cmake --build /tmp/helm-vulkan-qa3-build --target opencpn-stakeholder-demo
```

The target writes:

```text
/tmp/helm-vulkan-qa3-build/stakeholder-demo/qa3-stakeholder-demo-summary.md
/tmp/helm-vulkan-qa3-build/stakeholder-demo/qa3-stakeholder-demo.log
```

The summary is the short artifact to share after the demo. The per-step logs
show the exact smoke binary output.

## Demo Spine

1. Chart-source normalization and S-52/S-101 presentation compile into the
   backend-neutral nautical render model.
2. The neutral model stays the semantic center for OpenCPN, Helm, VSG, and
   future Metal/WebGPU targets.
3. OpenCPN selects the shared renderer through a feature-flag adapter only after
   it has a validated neutral model and OpenCPN-owned canvas/swapchain context.
4. Helm/offscreen tile behavior is represented as adapter scheduler policy:
   visible tiles, overscan, prefetch, cache epochs, and adjacent zoom blending.
5. Feature inspection traces wrong-location bugs from source chart/object through
   projection, command, cache key, backend primitive, and final asset ids.
6. Golden regression evidence exercises Chart 1 and depth fixtures with honest
   pending-baseline reporting.
7. VSG remains draw/cache-only and consumes neutral primitives without owning
   chart-source, S-52, quilting, cache-key, or scheduler semantics.

## OpenCPN Interactive Evidence

The OpenCPN portion of the demo is local to this branch:

- `opencpn-feature-flag-adapter-smoke` proves the feature flag routes only a
  validated neutral model to the shared renderer and keeps legacy fallback.
- `opencpn-s52-presentation-compiler-smoke` proves S-52/S-101 presentation
  decisions compile before backend handoff.
- `opencpn-neutral-model-smoke` proves the neutral model is the shared contract.
- `opencpn-chart1-debug-app-smoke` proves the inspection path for
  wrong-location debugging.

## Helm Headless Evidence

The Helm portion is shown in two layers:

- This branch's `opencpn-viewport-tile-scheduler-smoke` proves the shared
  adapter scheduler policy for tile coverage, overscan, cache invalidation, and
  zoom blending before VSG backend handoff.
- Helm REPO-4 proves the HTTP tile route can opt into the shared offscreen
  renderer path with `HELM_CHART_RENDERER=vulkan`, deterministic renderer cache
  keys, renderer headers, and explicit fallback behavior.

Do not use the live Helm `:8080` screen for the stakeholder demo. Run any Helm
route smoke on a private port and record the branch, head SHA, port, and chart
fixture or BYO chart data used.

## Presenter Checklist

- Start with the architecture boundary: chart normalization, presentation
  compiler, neutral render model, GPU asset/cache/scheduler, VSG backend.
- Name the safety boundary: this is a POC, not ECDIS certification or full S-52
  parity.
- Show OpenCPN first, because the branch is upstream-shaped and C++/CMake
  native.
- Show Helm second, as a thin headless/offscreen adapter consuming the same
  renderer path behind a feature flag.
- When asked about incorrect pixels, use the Chart 1 debug app evidence and walk
  from source feature id to final backend asset id.
- When asked about future backends, say Metal and WebGPU are compatibility
  targets fed by the same neutral model, not current implementations.
- When asked about MBTiles or PMTiles, say they are import/export/debug fixture
  artifacts, not the renderer hot-path contract.

## Pass Criteria

The demo is ready when:

- `opencpn-stakeholder-demo` completes successfully.
- The generated summary lists PASS for all seven local evidence steps.
- Helm route evidence is captured separately from a private Helm port when the
  live HTTP path is shown.
- The presenter can explain why VSG is an output backend only and why scheduler,
  quilting, cache policy, and presentation semantics remain before backend
  handoff.
