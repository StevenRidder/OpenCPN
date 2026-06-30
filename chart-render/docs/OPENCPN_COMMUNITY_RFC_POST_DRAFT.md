# Draft OpenCPN Community RFC Post

Status: draft for human review before posting

Suggested subject:

```text
RFC: small C++ render-core seam for OpenCPN/Helm architecture review
```

Draft:

```text
Hi all,

I would like to ask for architecture review on a Vulkan renderer proof-of-concept
branch before it grows any further.

This is not a request to replace OpenCPN's renderer, and not a proposal to merge
a large plotter rewrite. The current legacy renderer remains the default. The
POC is feature-flagged and scoped to a small C++ render-core seam under
chart-render/ on the vulkan/render-core-poc branch.

The review package starts here:

- chart-render/docs/RFC_RENDER_CORE_POC.md
- chart-render/docs/OPENCPN_PUBLIC_PROOF_BRANCH.md
- chart-render/docs/MAINTAINER_RESPONSE_MATRIX.md
- chart-render/docs/PRODUCTION_HARDENING_MAP.md
- chart-render/docs/STAKEHOLDER_DEMO.md

The current proof branch head for this package is:

- vulkan/render-core-poc at 0730391eb59376df86e7e2515ebcb2aa2f7704db

What I am trying to prove
-------------------------

The question is whether OpenCPN and a headless Helm adapter can share an
upstream-shaped C++ rendering core and neutral nautical render model without
creating a Helm-first fork, a sidecar rewrite, or a monolithic AI-generated
plotter.

The proposed split is:

- chart-source conversion and provenance before rendering;
- S-52/S-101 presentation decisions before backend handoff;
- a neutral NauticalRenderModel with source trace handles, resource metadata,
  coverage metadata, LOD hints, diagnostics, and cache keys;
- an OpenCPN interactive adapter that preserves OpenCPN canvas/swapchain
  ownership and legacy fallback;
- VSG/Vulkan as one native proof backend, draw/cache only;
- a Helm headless/offscreen adapter and WebGPU-first client contract that
  consume artifacts from the same model without moving chart semantics into the
  browser.

Why I am asking early
---------------------

I know the obvious concerns, including what people sometimes call a
vibe-coded monolith:

- a large AI-assisted code drop can be unmaintainable;
- a chartplotter can become a monolith if the module boundaries are wrong;
- conversion, presentation, and rendering can get blurred;
- performance claims can hide real bottlenecks across hardware;
- a backend can accidentally become the owner of chart semantics;
- parallel experiments can become impossible to collaborate on if the interfaces
  are not small and replaceable.

Those are the concerns this RFC is trying to make reviewable. The maintainer
response matrix is included so the POC can be judged against those risks, not
around them.

What works in the current proof
-------------------------------

The branch builds with CMake and has a repeatable stakeholder demo target:

cmake -S chart-render -B /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build --target opencpn-stakeholder-demo

That target runs local smoke evidence for:

- S-52/S-101 presentation compiler feeding the neutral model;
- neutral nautical render model validation;
- OpenCPN feature-flag adapter behavior and legacy fallback;
- viewport tile scheduler policy for visible tiles, overscan, prefetch, cache
  epochs, and zoom blending;
- Chart 1 source-to-render object inspection;
- golden-regression smoke with honest pending-baseline reporting;
- VSG GPU cache/backend staying draw/cache-only and neutral-model-fed.

What is placeholder or out of scope
-----------------------------------

- This is not full S-52 parity.
- This is not ECDIS certification or a safety approval.
- This is not a production performance benchmark.
- This is not a production WebGPU renderer.
- This is not a production Metal backend.
- This is not a standalone repository extraction decision.
- The current VSG path is proof backend/cache evidence, not a claim that VSG
  owns chart semantics.
- Helm/WebGPU is a consumer target contract, not the source of OpenCPN renderer
  truth.

What feedback I am asking for
-----------------------------

I would especially value feedback on:

1. Are the chart-source, S-52 presentation, neutral model, adapter, and backend
   seams the right units of review?
2. Does the NauticalRenderModel carry the right metadata for wrong-location
   debugging and GPU handoff?
3. Which Chart 1, depth, raster, label, text, safety, and quilting cases should
   be next in the fixture corpus?
4. Which S-52/S-101 rules should be reviewed first to avoid building on the
   wrong abstraction?
5. What diagnostics would make a VSG backend reviewable without letting that
   backend own chart semantics?
6. What performance counters and target scenes would be meaningful across
   real OpenCPN hardware diversity?
7. Is the standalone extraction gate strong enough, or should more upstream
   review criteria be added before any repository split is considered?

Production hardening, if this seam is worth pursuing
----------------------------------------------------

If maintainers think the seam is directionally sane, the next work should be
hardening rather than broadening:

- expand fixture coverage with maintainer-selected Chart 1/S-52/depth/raster
  cases;
- replace placeholder backend diagnostics with real VSG/Vulkan draw evidence;
- add real image baselines and stricter golden regression checks;
- add contour-heavy, label-heavy, raster, and mixed-quilt performance scenes;
- keep OpenCPN adapter work feature-flagged with legacy fallback as default;
- keep Helm as a consumer/offscreen adapter and browser artifact target;
- preserve C++/CMake/OpenCPN-native implementation for renderer runtime code;
- revisit repository extraction only after both OpenCPN and Helm consume the
  same command stream with accepted evidence.

The proposed hardening map and decision log are in
chart-render/docs/PRODUCTION_HARDENING_MAP.md. That map is meant to be revised
from maintainer feedback before the first tiny feature-flagged production-slice
PR.

If the module boundaries are wrong, I would rather learn that now while the POC
is still small.

Thanks for looking, and especially for any blunt feedback on maintainability,
module boundaries, fixture choices, and where this should look more like native
OpenCPN work.
```

## Posting Checklist

- Replace branch SHA if a newer proof package lands before posting.
- Link to the GitHub branch and PR/RFC package URL that reviewers should read.
- Include the production hardening map if asking what should happen after RFC
  feedback.
- Do not include generated `/tmp` summaries or local worktree paths.
- Do not include private chart data, private screenshots, S-63 permits, oeSENC
  output, or runtime vessel data.
- Keep the ask to architecture review and seam feedback.
- Have a human do final tone review before posting.
