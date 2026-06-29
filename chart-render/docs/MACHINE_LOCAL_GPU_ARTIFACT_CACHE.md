# Machine-Local GPU Artifact Cache Contract

Status: CACHE-1 production architecture and C++ contract, CACHE-2 VSG slice

This document defines the rebuildable cache layer between the neutral nautical
render model and any backend renderer:

```text
neutral nautical render model
  -> machine-local GPU artifact cache
  -> VSG / WebGPU / WebGL / Metal-compatible / server-raster backend
```

The cache compiles backend-neutral primitives and resource records into
device/backend-specific artifact records. It is not portable chart truth, not a
presentation compiler, and not a source-chart parser.

## Inputs

The cache consumes:

- `NauticalRenderModel` layers, primitives, LOD hints, coverage metadata, and
  resource records;
- presentation output cache keys;
- source trace/provenance handles carried by neutral primitives;
- backend target name, device profile, material profile, cache namespace, and
  invalidation epoch;
- memory budget for the current machine/backend profile.

It does not consume S-57, S-101, KAP, CM93, MBTiles, PMTiles, OpenCPN canvas
state, Helm HTTP state, MapLibre layers, or UI icon registry semantics.

## Outputs

The C++ contract in `include/gpu_artifact_cache_contract.hpp` emits
`GpuArtifactCacheManifest` records:

- vertex and index buffer artifacts;
- uniform/frame data artifacts;
- compressed raster texture records when the backend/device profile supports
  them;
- symbol, pattern, glyph, and line-pattern atlas records;
- material and pipeline key records;
- viewport/tile artifact entries keyed by the adapter cache epoch;
- byte estimates, memory budget, and over-budget diagnostics;
- tier, semantic-owner, primitive-id, and provenance handles for inspection.

Artifact records are disposable. If the device, material profile, atlas packing,
texture compression, display state, or source epoch changes, the cache may be
deleted and rebuilt from the neutral model and resource table.

## Ownership Rules

The cache owns:

- device/backend artifact keys;
- buffer and texture/atlas residency classes;
- material and pipeline profile keys;
- compressed/uncompressed artifact choice;
- memory estimates, priorities, and budget pressure diagnostics;
- invalidation domains for resource content, presentation epoch, material
  profile, frame state, and viewport cache epoch.

The cache must not own:

- S-52/S-101 object-class decisions;
- display category, SCAMIN, palette, safety-depth, or sounding semantics;
- source chart parsing or package conversion;
- raster quilting policy;
- overscan, prefetch, or adjacent zoom selection;
- Helm HTTP/cache header policy;
- Tier 2/3 Helm overlay or UI icon meaning.

If an artifact record needs to preserve where a symbol, glyph, texture, or
pipeline came from, it stores handles such as `semantic_tier`,
`semantic_owner`, `source_standard`, neutral primitive ids, and provenance refs.
Those handles are for inspection and cache invalidation only; they are not
permission for backend code to reinterpret chart meaning.

DEBUG-1 consumes the same handles in
`SourceToRenderInspectionReport`. That report links neutral primitives to cache
artifacts, backend draw items, and pixel/object query ids so wrong-location and
wrong-symbol debugging can cross the cache boundary without giving the cache or
backend ownership of chart semantics.

## Tier and Provenance Handles

`GpuArtifactTierHandle` keeps Tier 1 official chart artifacts distinct from
later Helm overlay/UI artifacts:

- `semantic_tier`, for example `tier1_official_chart`;
- `semantic_owner`, normally `presentation_compiler`;
- `source_standard`, for example `s52-compatible`;
- `provenance_refs`;
- `primitive_ids`.

This lets a future combined backend draw chart artifacts and overlays together
without collapsing their meaning in the cache. Official chart symbols remain
owned by the presentation compiler. Helm overlay and UI icon registries remain
outside CACHE-1 until a later combined overlay fixture defines that contract.

## Validation Gates

`ValidateGpuArtifactCacheManifest` rejects manifests when:

- identity, backend target, device profile, material profile, namespace, or
  memory budget is missing;
- `input_contract` is not `backend-neutral-nautical-render-model`;
- `cache_owner` is not `runtime_gpu_artifact_cache`;
- `semantic_owner` is `backend`;
- artifact ids or cache keys are missing or duplicated;
- records are not rebuildable and device-specific;
- records have zero byte size;
- usage, material keys, or pipeline keys contain source/presentation policy
  words such as S-52, S-101, chart-source, MBTiles, PMTiles, quilting,
  scheduler, prefetch, display-category, or SCAMIN;
- tier or semantic-owner handles are missing;
- required coverage is absent: machine-local artifacts, material records,
  viewport entries, and provenance/tier handles.

Over-budget manifests are valid but produce a `gpu_artifact_cache_budget`
warning. Budget pressure is expected on small boat-class devices; it should
drive eviction or profile changes, not silent semantic shortcuts.

## Relationship to VSG Cache

`vsg/vsg_gpu_cache.hpp` remains the VSG proof backend's cache manifestation.
It can be implemented in terms of this generic contract later, but it should
stay backend-specific: VSG/Vulkan handles, descriptor layouts, and real GPU
objects belong there.

CACHE-1 sits above that backend-specific layer. Its job is to keep artifact
identity, rebuildability, memory budgets, invalidation, and tier/provenance
rules common across VSG and future WebGPU/WebGL/Metal-compatible/server-raster
targets.

## Initial CACHE-2 VSG Slice

`CACHE-2` compiles the first package-derived neutral fixture into
VSG/Vulkan-targeted generic artifact records. The fixture path is:

```text
CONVERT-2 S-57 portable package
  -> PRESENT-2 S-52/S-101 package presentation model
  -> BuildGpuArtifactCacheManifest(backend_target="vsg")
```

The resulting manifest includes vertex/index buffers, view uniforms, symbol
atlas records, line-pattern records, material/pipeline keys, viewport entries,
invalidation metadata, byte estimates, and deterministic cache ids. Resource
artifacts inherit `source_standard` from the neutral primitives that reference
them, so the S-57 package provenance reaches symbol/line/palette artifacts
without asking the cache to reinterpret S-52/S-101 meaning.

This slice is official chart portrayal only. The CACHE-2 smoke rejects Helm
Tier 2/3 overlay or UI icon registry policy words in VSG package artifact
usage/material/pipeline keys. A later combined overlay fixture can define how
different tiers are drawn together downstream, but the chart-semantics cache
must not collapse those meanings here.

## Smoke Test

```bash
cmake -S chart-render -B /tmp/helm-vulkan-cache1-build
cmake --build /tmp/helm-vulkan-cache1-build
/tmp/helm-vulkan-cache1-build/opencpn-gpu-artifact-cache-contract-smoke
```

The smoke test asserts that:

- Chart 1-style fixture primitives produce buffers, atlases, material records,
  viewport entries, tier handles, provenance handles, and invalidation epochs;
- artifact ids are deterministic;
- small memory budgets produce a visible budget diagnostic;
- backend-owned semantics and presentation-policy words in artifact usage are
  rejected;
- raster quilting fixtures produce raster texture artifacts without moving
  quilting policy into the cache.
- the CONVERT-2/PRESENT-2 S-57 package fixture produces VSG-targeted artifact
  records with Tier 1 S-57 provenance, package-keyed invalidation, material and
  pipeline keys, resource artifacts, deterministic ids, memory evidence, and no
  Helm Tier 2/3 policy leakage.
