# Source-To-Render Inspection Contract

Status: DEBUG-1 production architecture and C++ contract

The source-to-render inspection contract is the debug spine for wrong-location
and wrong-symbol failures. It makes one C++ row per neutral primitive and keeps
that row linked from source chart truth through cache/backend draw handles and
future object/pixel queries.

```text
source chart/object
  -> converter output / portable package feature
  -> normalized geometry and projection transform
  -> S-52/S-101 presentation rule
  -> neutral primitive and cache key
  -> GPU artifact/cache record
  -> backend resource and draw item
  -> pixel/object query handle
```

The contract lives in `include/source_to_render_inspection.hpp` and
`source/source_to_render_inspection.cpp`. It consumes a
`NauticalRenderModel`, an optional `GpuArtifactCacheManifest`, render-target
metadata, and a backend name. It does not parse S-57/S-101/KAP data and it does
not give backend code authority over chart semantics.

## C++ Surfaces

- `SourceToRenderInspectionReport`: report identity, input model id/epoch,
  cache manifest id, backend name, render target, primitive rows, scene-wide
  artifact handles, diagnostics, and `ok`.
- `SourceToRenderInspectionRow`: the row tying one neutral primitive to source,
  converter, presentation, cache, backend, artifact, and query handles.
- `BuildSourceToRenderInspectionReport`: builds the report from the neutral
  model and GPU artifact manifest.
- `ValidateSourceToRenderInspectionReport`: rejects missing source/projection,
  presentation, cache, artifact, backend, query, tier, and ownership handles.
- `FindInspectionByPrimitiveId` and `FindInspectionsBySourceObjectId`: lookup
  helpers for tests and future UI inspection panels.

## Required Row Handles

Each row records:

- source chart id, edition/update when present, source object id/class, and
  provenance refs;
- converter id, source product id, portable package id, converter output id,
  normalized feature id, conversion stage, normalized geometry id, geometry
  hashes, and projection transform chain;
- presentation rule id, display category, layer id, presentation layer, draw
  order, primitive id, primitive type, and primitive role;
- scene, primitive, resource, and tile cache keys;
- GPU artifact manifest id, artifact id, kind, residency, backend resource id,
  material/pipeline keys, invalidation domain, primitive ids, and provenance
  refs;
- draw-only backend name/target, backend resource id, final draw item id, final
  GPU/web asset ids, coordinate space, and accepted backend targets;
- object query id, pixel query id, hit-test index id, view id, and target pixel
  size.

## Visual Tier Ownership

Rows carry `InspectionTierHandle` so Helm overlays can share a renderer without
collapsing ownership:

- Tier 1 official chart rows use `tier1_official_chart`; wrong-location bugs
  route to converter/projection owners, and wrong-symbol bugs route to
  converter, presentation, or backend owners.
- Tier 2 Helm overlay rows must route both wrong-location and wrong-symbol bugs
  to the Helm overlay registry.
- Tier 3 Helm UI/control rows must route both bug classes to the Helm UI layer.

Validation rejects Tier 1 rows that point at Helm owners and rejects Tier 2/3
rows that route symbol/location bugs back into the presentation compiler.

## Chart 1 Debug Integration

`chart1::DebugReport` now includes `source_to_render`, built from the Chart 1
normalized source product, S-52 presentation compiler output, GPU artifact
cache manifest, draw-only VSG placeholder name, and offscreen render target.
The older `ObjectInspection` rows remain for fixture-specific UI checks; the
new report is the production DEBUG-1 contract that DEBUG-2 can exercise on the
first real source-to-pixel vertical slice.

## Validation

`ValidateSourceToRenderInspectionReport` fails closed when:

- report identity or neutral-model input contract is missing;
- a row lacks source chart/object provenance;
- converter output, normalized geometry, or projection transform is missing;
- presentation rule or layer handles are missing;
- neutral cache keys are missing;
- a cache-backed report has a primitive row with no GPU artifact link;
- backend resource, draw item, or final asset handles are missing;
- a backend is marked as semantic owner;
- object/pixel query handles are missing;
- visual tier ownership does not route bugs to the correct owner.

## Smoke Test

```bash
cmake -S chart-render -B /tmp/helm-vulkan-debug1-build
cmake --build /tmp/helm-vulkan-debug1-build
/tmp/helm-vulkan-debug1-build/opencpn-source-to-render-inspection-smoke
```

The smoke test validates Chart 1 area/line/symbol rows, negative ownership and
artifact cases, and a raster quilting fixture that preserves raster texture
artifact traceability.
