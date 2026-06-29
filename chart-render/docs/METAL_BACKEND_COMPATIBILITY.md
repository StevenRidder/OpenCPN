# Deferred Metal Backend Compatibility

`ADAPT-5` is a compatibility checkpoint, not a Metal renderer implementation.
The current POC remains viable for a future native Apple backend as long as
Metal stays behind the same draw-only backend boundary used by VSG and does not
take ownership of chart sources, S-52/S-101 presentation decisions, quilting,
viewport scheduling, or Helm tile policy.

## Reviewed Boundaries

- `IRenderBackend` already exposes a backend-neutral draw contract over
  `RenderScene` and `NauticalRenderModel` with onscreen swapchain and offscreen
  targets.
- `NauticalRenderModel` carries typed nautical primitives, resource ids, cache
  keys, LOD hints, display state, coverage metadata, provenance, and a
  `draw_only` handoff contract before any backend-specific work begins.
- The S-52 presentation compiler owns object-class decisions, display
  category, SCAMIN, palette, safety-depth styling, symbol selection, label
  selection, and sounding formatting before backend handoff.
- The OpenCPN feature-flag adapter keeps wx canvas and swapchain lifetime with
  OpenCPN, so a future Metal backend should bind to OpenCPN-owned platform
  surfaces instead of creating an independent UI lifecycle.
- The viewport tile scheduler owns visible, overscan, prefetch, cache-epoch,
  and adjacent zoom-blend policy outside the renderer backend.
- The VSG GPU cache manifest proves the intended ownership shape for backend
  runtime assets: deterministic device/profile-scoped records with primitive
  ids and provenance refs, but no chart semantics.

## Compatibility Verdict

The neutral render model and adapter contract leave room for a native Metal
backend. A Metal backend can consume the same compiled primitives and resource
records as VSG, build Metal-specific pipelines and GPU resources from them, and
return the same `RenderResult` diagnostics without moving nautical semantics
behind the backend boundary.

That viability depends on keeping the backend boundary narrow:

- platform surfaces remain adapter-owned;
- shader, material, and pipeline descriptors are derived from neutral primitive
  roles and resource records;
- textures, atlases, buffers, and frame resources are cache artifacts, not chart
  source truth;
- text shaping and glyph atlas ownership are explicit before rendering;
- synchronization is frame/runtime policy, not chart scheduling policy;
- diagnostics keep source provenance and generated backend asset ids together.

## Gap List

1. Platform surface binding:
   `RenderTarget` names `kSwapchain`, but it has no C++ carrier for
   OpenCPN-owned native surface handles, CAMetalLayer/MTKView binding,
   drawable size, color space, or Retina scale policy. Add a small
   adapter-owned surface descriptor before any Metal prototype.

2. Shader and pipeline profiles:
   The model has primitive roles and resource ids, but there is not yet an
   explicit cross-backend material or pipeline descriptor for blend mode,
   stencil/depth rules, symbol instancing layout, line joins, area-pattern
   sampling, or raster compositing. Metal should not infer these from S-52.

3. Texture and GPU cache keys:
   VSG cache keys include device and shader profiles. A Metal path needs the
   same deterministic split for device family, pixel format, sampler policy,
   atlas packing version, and resource content hash without making the cache a
   chart-source owner.

4. Labels and soundings:
   `TextLabel` and `Sounding` primitives are neutral, but glyph shaping,
   fallback fonts, glyph atlas format, signed-distance-field policy, and
   CoreText/HarfBuzz ownership are still unspecified. This must be settled
   before a native label renderer is credible.

5. Synchronization and frame lifecycle:
   `RenderResult` is enough for the current placeholder and offscreen smoke
   tests, but a live Metal backend needs explicit frame-in-flight, drawable,
   command-buffer, fence, and resize behavior tied to the OpenCPN adapter
   lifecycle.

6. Debug and provenance parity:
   VSG diagnostics currently report accepted neutral models and GPU cache
   manifests. A Metal path needs equivalent manifest/debug rows so Chart 1
   failures can cite source object ids, neutral primitive ids, Metal asset ids,
   and pipeline/profile names together.

## Follow-On Task Candidates

Create these only when the project chooses to start a Metal prototype:

- define an adapter-owned native surface descriptor for OpenCPN swapchain
  targets;
- add a backend-neutral material/pipeline descriptor fixture and smoke test;
- add a Metal-shaped GPU cache manifest smoke alongside the VSG cache smoke;
- define the glyph shaping and atlas contract for text labels and soundings;
- add Chart 1 debug-report parity requirements for any non-VSG backend.

Until then, Metal remains a deferred backend target. The implementation path is
compatible, but the POC should continue proving the shared C++ neutral model,
S-52 compiler, VSG backend, OpenCPN adapter, and golden/debug evidence first.
