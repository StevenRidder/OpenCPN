# Maintainer Response Matrix

Status: public RFC support note

This matrix turns maintainer concerns into acceptance criteria for the Vulkan
renderer POC. It is not a rebuttal. It is a checklist for making the public RFC
reviewable by OpenCPN maintainers and chart-rendering experts before any broader
community post.

## How To Use This Matrix

- Lead with the concern, not the implementation.
- Point reviewers at small, reviewable C++ surfaces and reproducible commands.
- Name limitations honestly.
- Treat maintainer feedback as design input for the next slice.
- Avoid any claim that automation, generated code, or AI can replace expert
  chart-renderer review.
- Use `docs/PRODUCTION_HARDENING_MAP.md` as the post-RFC plan once reviewers
  agree which seams, fixtures, and gates should harden next.

## Response Matrix

| Concern | Public response posture | Current evidence | Limit / follow-up |
|---|---|---|---|
| This looks like a monolithic generated plotter rewrite. | Validate the risk. Present the POC as a scoped renderer seam, not a full plotter replacement. | `chart-render/` is the proof package; `POC-ACCEPTANCE.md` says the question is whether OpenCPN and Helm can share an upstream-shaped C++ renderer core without becoming a Helm-only fork. | Keep the RFC focused on `chart-render/` plus the curated Helm/WebGPU bridge doc. Do not publish board chatter, internal branch churn, or the whole Helm planning corpus as proof. |
| Humans cannot maintain a giant opaque change. | Keep the core small, named, and expert-reviewable. Make each boundary visible in file names and docs. | `include/`, `source/`, `s52/`, `vsg/`, and `tests/` separate command stream, chart-source boundary, S-52 compiler, neutral model, backend cache, and fixtures. | RFC reviewers should be able to critique one module without accepting the whole direction. If a module cannot be reviewed independently, split it before asking for broader feedback. |
| Chart-source conversion and rendering are being blurred. | State that conversion is a separate input boundary before S-52 presentation and backend handoff. | `source/README.md` defines `IChartSource` input and `ChartSourceProduct`; `INTERCHANGE.md` treats MBTiles/PMTiles as interchange/debug artifacts, not the hot path. | Future converters must emit the same normalized product/provenance shape. Do not let VSG, Helm HTTP, or browser code parse chart sources to fill gaps. |
| The render model may be the wrong foundation. | Ask for critique of the neutral model directly. Make it clear the model is the object under review. | `include/nautical_render_model.hpp` includes source trace handles, LOD hints, cache keys, coverage metadata, backend handoff contracts, layers, resources, provenance, and diagnostics. | The model is a POC schema, not a final standard. Rename, split, or replace fields if maintainers identify a better OpenCPN-shaped contract. |
| A GPU backend could accidentally own chart semantics. | Keep VSG/Vulkan as draw/cache-only proof backend. Chart semantics stay before backend handoff. | `vsg/GPU_CACHE.md` says VSG does not parse charts, run S-52/S-101, decide quilting, classify PMTiles/MBTiles, schedule prefetch, or own Helm artifacts; `opencpn-vsg-gpu-cache-smoke` rejects semantic ownership by the backend. | Treat any backend-local S-52 decision as a POC failure. Add regression checks when more backend code lands. |
| S-52/S-101 rules need expert review, not browser or shader improvisation. | Present the presentation compiler as the semantic owner, then ask maintainers to review that seam. | `docs/S52_PRESENTATION_COMPILER.md` maps normalized features into `NauticalRenderModel` primitives and owns display categories, SCAMIN, palette, symbols, soundings, text, and safety decisions. | Current coverage is Chart 1-style smoke coverage, not full S-52 parity. Expand through accepted fixtures and maintainer-selected cases. |
| Wrong-location bugs must be explainable. | Make source-to-render inspection a first-class acceptance criterion. | `docs/CHART1_DEBUG_APP.md` records source chart/object, geometry hashes, projection transform, S-52 rule, display state, tile/cache key, backend primitive, and final asset ids. | The debug app is fixture-scale. Future real-chart failures should attach the same trace fields before changing renderer math. |
| Performance claims are easy to overstate. | Do not claim production performance. Show the counters that exist and what they do not prove. | `source/DEPTH.md` describes `depth_performance.hpp`, which records elapsed time, command count, diagnostic count, and RGBA byte count through the offscreen backend interface. `vsg/GPU_CACHE.md` records byte estimates and residency classes. | This is not a production renderer benchmark. Add contour-heavy, label-heavy, and real-chart fixtures before making performance claims in public posts. |
| Correctness evidence should be reproducible. | Use local CMake targets and smoke binaries, not screenshots or board comments. | `POC-ACCEPTANCE.md` names `opencpn-golden-regression-smoke`, `opencpn-feature-flag-adapter-smoke`, `opencpn-viewport-tile-scheduler-smoke`, and `opencpn-stakeholder-demo`. `source/QA.md` reports pending baselines honestly. | Pending image baselines remain pending evidence, not pass/fail pixel parity. The RFC should say exactly which commands were run and which evidence is still missing. |
| OpenCPN maintainers should not have to review a foreign-language renderer core. | Keep runtime renderer implementation C++/CMake/OpenCPN-native. Scripts and JSON fixtures are support artifacts. | `chart-render/CMakeLists.txt` builds the proof targets; implementation surfaces are C++ headers/sources. `scripts/stakeholder_demo.sh` only summarizes smoke output. | If a future helper grows into runtime renderer behavior, move that behavior into C++ or mark it out of scope before public review. |
| Helm-specific product code could leak into OpenCPN renderer design. | Keep Helm as consumer/product context only. The shared core must not depend on Helm HTTP, MapLibre, cache headers, or UI state. | `docs/HELM_WEB_RENDER_TARGET.md` and the Helm `docs/VULKAN-HELM-WEBGPU-PROOF.md` describe Helm WebGPU/browser artifacts as consumer targets fed by the server-side model. | Helm product UX can drive artifact requirements, but not chart semantics. Any Helm-specific need that changes pixels belongs in renderer input/model review. |
| Future contributors need replaceable modules, not a locked architecture. | Document what can be reworked independently. | Source conversion, presentation compiler, neutral model, adapter scheduler, backend cache, and Helm/OpenCPN adapters are separate modules with named docs and smoke targets. | Standalone repository extraction remains gated until both adapters consume the same command stream with accepted golden evidence. |
| "AI built it" could be used to dismiss maintainability concerns. | Do not argue with that concern. Agree that reviewability, provenance, reproducibility, and expert critique are the bar. | The public proof package should preserve code lineage, exact PRs, smoke commands, and limitations. It should avoid process-centered language. | Public messaging must not imply AI can replace maintainers. The ask is architecture review and concrete seam feedback from domain experts. |

## Suggested RFC Language

Use language like this:

```text
This is an upstream-shaped C++ renderer-seam proof, not a request to accept a
large plotter rewrite. The ask is architecture review: are the chart-source
boundary, S-52/S-101 presentation compiler, neutral render model, inspection
provenance, adapter split, and backend handoff the right seams for maintainable
OpenCPN work?
```

Avoid language like this:

```text
AI produced a replacement renderer, and the current OpenCPN renderer is
obsolete.
```

## Evidence Checklist Before Posting

- Link the OpenCPN proof branch and exact commit.
- Link the curated Helm/WebGPU proof note, not the whole Helm planning history.
- Run and record the CMake stakeholder demo command.
- Include the WebGPU-first Helm framing and deferred-Metal limitation.
- State no full S-52 parity, no ECDIS certification, no primary-navigation
  readiness, no production WebGPU renderer, and no production Metal backend.
- Invite maintainers to challenge the module boundaries and fixture choices.
- Link the production hardening map as a draft plan, not as a fixed roadmap.
