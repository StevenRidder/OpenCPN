# Vulkan Renderer POC Acceptance

This POC is meant to answer one question: can OpenCPN and Helm share an
upstream-shaped C++ renderer core without turning the work into a Helm-only fork
or a separate sidecar renderer?

## Must Prove

- A C++ command stream can represent source chart objects, S-52 display rules,
  raster sheet policy, provenance, and target viewport state without depending
  on Helm HTTP routes, MapLibre, or OpenCPN wx canvas globals.
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
- `QA-2`: repeatable golden-image regression runner.
- `REPO-4`: Helm feature flag wired to the shared offscreen renderer.

The POC is credible only when the evidence above is present in code, fixtures,
or reproducible commands. Board comments alone are not acceptance evidence.
