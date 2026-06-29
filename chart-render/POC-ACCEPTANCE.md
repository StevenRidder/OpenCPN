# Vulkan Renderer POC Acceptance

This POC is meant to answer one question: can OpenCPN and Helm share an
upstream-shaped C++ renderer core without turning the work into a Helm-only fork
or a separate sidecar renderer?

## Must Prove

- A C++ command stream and neutral nautical render model can represent source
  chart objects, S-52/S-101 presentation output, raster sheet policy,
  provenance, and target viewport state without depending on Helm HTTP routes,
  MapLibre, VSG, or OpenCPN wx canvas globals.
- Chart-source inputs can be normalized before rendering, including S-57/SENC,
  raster charts, MBTiles/PMTiles interchange packages, debug fixtures, and a
  path for future S-101 support.
- Chart 1 point, line, and area cases can be named, rendered, and compared as
  repeatable conformance cases.
- Wrong-location bugs can be debugged from source chart id, source object id,
  projection transform, generated geometry, target command, and applied S-52
  rule evidence.
- The same renderer core can support an OpenCPN interactive adapter and a Helm
  headless/offscreen tile adapter behind feature flags.
- VSG is proven as one output backend only; S-52/S-101 rules, chart-source
  parsing, chart quilting, source semantics, cache keys, and scheduling policy
  are owned before the backend handoff.
- Regression evidence can be reproduced from a local command with documented
  golden-image tolerances and fixture inputs.

## Non-Goals

- This POC is not full S-52 parity.
- This POC is not an ECDIS certification or safety approval.
- This POC is not a replacement for all OpenCPN chart rendering paths.
- This POC is not a standalone renderer repository extraction gate.
- This POC is not a Helm UI, MapLibre, route-planning, or HTTP cache project.
- This POC is not proof that MBTiles or PMTiles are the final renderer hot-path
  contract.
- This POC is not permission to touch the live Helm `:8080` runtime.

## Evidence By Stakeholder

OpenCPN-facing evidence:

- PRs are against the `vulkan/render-core-poc` branch and remain C++/CMake
  native.
- The renderer seam builds under the OpenCPN-compatible C++ baseline.
- The interactive adapter uses the shared command stream instead of a parallel
  demo-only scene.
- Chart 1 images document accepted differences from current OpenCPN baselines.

Helm-facing evidence:

- The headless adapter consumes a pinned OpenCPN renderer branch/commit.
- Helm-specific work stays at the tile target, PNG response, cache/ETag,
  diagnostics, and MapLibre composition boundary.
- The legacy chart path remains available as a feature-flag fallback.
- Tile output is deterministic for the same chart inputs, display state, and
  viewport.

QA-facing evidence:

- Fixture inputs and acceptance cases are committed with the code.
- Golden images include command-stream provenance and baseline metadata.
- Image diffs report tolerance, changed pixels, and fixture ids.
- Failures include enough provenance to explain wrong-location, wrong-symbol,
  wrong-depth, and missing-coverage regressions.

## POC Exit Gates

- `CHART-1`: committed Chart 1 point, line, and area acceptance catalog.
- `CHART-2`: rendered canonical Chart 1 point, line, and area outputs.
- `CHART-3`: comparison against OpenCPN baselines with documented tolerances.
- `DEPTH-4`: tile-scale contour performance smoke.
- `ADAPT-1`: OpenCPN feature-flag adapter sketch.
- `ADAPT-2`: Helm headless tile adapter sketch.
- `ADAPT-4`: overscan, prefetch, cache-invalidation, and zoom-blend scheduler
  policy.
- `ADAPT-5`: deferred Metal backend compatibility checkpoint and gap list.
- `CACHE-1`: machine-local GPU artifact cache contract with memory budget,
  invalidation, material/pipeline, and tier/provenance handles.
- `QA-2`: repeatable golden-image regression runner.
- `QA-3`: repeatable stakeholder demo script and evidence packet showing the
  shared renderer spine, OpenCPN interactive adapter, Helm headless/offscreen
  policy, feature inspection, regression evidence, and VSG draw/cache-only
  boundary.
- `SEAM-5`: backend-neutral nautical render model and draw-only backend handoff.
- `REPO-4`: Helm feature flag wired to the shared offscreen renderer.

The POC is credible only when the evidence above is present in code, fixtures,
or reproducible commands. Board comments alone are not acceptance evidence.

The initial local command for QA-2 is the CMake-built
`opencpn-golden-regression-smoke` target. It validates the Chart 1 acceptance
and baseline manifests, reports pending baseline captures, and includes the
depth performance smoke counters.

The initial local command for ADAPT-1 is the CMake-built
`opencpn-feature-flag-adapter-smoke` target. It validates that the OpenCPN
feature-flag decision consumes a neutral nautical render model, preserves the
legacy canvas fallback, and refuses shared-renderer routing when OpenCPN
wx/swapchain ownership or model validation is missing.

The initial local command for ADAPT-4 is the CMake-built
`opencpn-viewport-tile-scheduler-smoke` target. It validates visible tile
coverage, overscan rings, prefetch budget truncation, display/presentation
cache invalidation epochs, and adjacent zoom-level blend requests as adapter
scheduler policy rather than VSG backend semantics.

The local command for QA-3 is the CMake `opencpn-stakeholder-demo` target. It
builds the final stakeholder evidence packet from the local smoke binaries and
writes `stakeholder-demo/qa3-stakeholder-demo-summary.md` under the build
directory. Helm HTTP route evidence remains a separate private-port follow-up
because this OpenCPN branch must not start Helm or touch the live `:8080`
screen.

ADAPT-5 evidence is the committed
`docs/METAL_BACKEND_COMPATIBILITY.md` checkpoint. It records that the neutral
model remains compatible with a future Metal backend only if platform surface
binding, pipeline/material profiles, GPU cache keys, label/glyph ownership,
frame synchronization, and provenance/debug parity are handled before
implementation.

The local command for CACHE-1 is the CMake-built
`opencpn-gpu-artifact-cache-contract-smoke` target. It validates that the
machine-local cache emits rebuildable backend/device artifact records from the
neutral model, preserves tier/provenance handles, records invalidation domains
and memory budgets, and rejects backend-owned source or presentation semantics.
