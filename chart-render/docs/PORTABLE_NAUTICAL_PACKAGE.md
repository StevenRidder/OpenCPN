# Portable Nautical Package Architecture

Status: FORMAT-1 production architecture draft

This document defines the durable, source-neutral chart package between
chart-source converters and presentation compilation:

```text
source chart
  -> converter module
  -> portable nautical package
  -> S-52/S-101 presentation compiler
  -> neutral nautical render model
  -> machine-local GPU artifact cache
  -> backend renderer
```

The package is the production answer to the "new chart format" concern raised
during the POC: the renderer should not decode S-57, raster, CM93, PMTiles, or
future sonar formats directly, and backend code should not contain chart
semantics. Converters produce a stable nautical package; presentation compiles
that package into backend-neutral primitives; each machine then builds its own
GPU artifacts.

## What This Package Is

The portable nautical package is durable chart truth in an OpenCPN-reviewable
shape. It is:

- source-neutral: S-57/S-101, raster/KAP, CM93, OSM/community data, future
  sonar/depth sources, and debug fixtures can all compile into the same package
  family;
- presentation-ready: it carries enough normalized chart facts for S-52/S-101
  presentation decisions, but does not pre-decide display state;
- provenance-rich: every feature, raster sheet, and derived geometry can be
  traced back to source chart, source object, transform, and converter version;
- portable: it can be copied between machines and checked into small fixtures
  when the source data is redistributable;
- backend-independent: it contains no Vulkan, VSG, WebGPU, Metal, OpenGL,
  MapLibre, GPU buffer, shader, or device-memory fields.

## What This Package Is Not

The package is not:

- a GPU cache;
- a renderer hot-path buffer layout;
- a texture atlas;
- a WebGPU, Vulkan, Metal, or OpenGL resource format;
- a substitute for S-52/S-101 presentation rules;
- an MBTiles/PMTiles replacement for web distribution;
- a place for OpenCPN wx canvas state, Helm HTTP/ETag policy, scheduler policy,
  or route/AIS overlay behavior.

Those concerns live in later layers. In particular, the machine-local GPU
artifact cache is rebuildable and backend-specific. The portable package is the
stable input to presentation and debugging.

## Required Package Records

The first production package should be a directory or archive with an explicit
manifest and content-addressed records. A binary form can come later, but the
logical records should be stable first.

```text
manifest
sources
features
geometries
rasters
coverage
provenance
diagnostics
indexes
checksums
```

### Manifest

The manifest identifies the package and the schema used to read it:

- package id;
- package schema version;
- profile, for example `s57-s52-poc` or future `s101`;
- producer name and version;
- converter module id and version;
- creation time;
- source epoch;
- content hash over all package records;
- declared coordinate reference for normalized geometry;
- declared units;
- fixture/debug flags;
- required optional capabilities.

### Sources

Source records preserve chart lineage:

- source id;
- source kind: S-57 cell, S-101 dataset, SENC cache, raster chart, CM93 source,
  PMTiles/MBTiles interchange input, community overlay, sonar/depth source, or
  debug fixture;
- native name and edition/update chain;
- source scale;
- native projection;
- geographic bounds;
- source content hash;
- licensing/distribution class;
- update relationships, including base cell and update files when applicable.

### Features

Feature records are source-neutral chart objects:

- stable feature id inside the package;
- source chart id;
- source object id;
- object class/acronym;
- geometry reference ids;
- attribute list with native acronym, normalized value, and display value;
- min/max scale denominators and SCAMIN inputs;
- source priority and update priority;
- relationship ids, for collections, master/slave features, or grouped objects;
- provenance references;
- diagnostics references.

Feature records carry chart facts only. They do not contain resolved S-52 symbol
ids, palette choices, display category filters, draw order, glyph placement, or
backend resource ids.

### Geometries

Geometry records hold normalized coordinates and geometry identity:

- geometry id;
- geometry kind: point, multipoint, line, area, raster footprint, or coverage
  polygon;
- coordinate reference used by the package;
- normalized coordinate arrays;
- source geometry hash;
- topology hints, such as ring winding, holes, shared edges, and simplification
  lineage;
- valid scale range hints;
- source sample points for inspection.

The first implementation may use simple C++ and JSON fixtures, but the contract
must not depend on JSON. `FORMAT-2` can choose a minimal fixture encoding as
long as these logical fields survive round trip.

### Raster Sheets

Raster records describe chart images and masks without becoming GPU textures:

- sheet id;
- source id;
- image content hash or external source reference;
- pixel size;
- native color model;
- geographic bounds;
- chart bounds;
- visible bounds;
- collar bounds;
- no-data policy;
- collar policy;
- boundary policy;
- quilt policy and rank;
- whether visible information may exist outside chart bounds;
- provenance references.

Compressed GPU texture variants are not stored here. A later cache/compiler may
derive BC/ETC/ASTC/other device-specific forms on the rendering machine.

### Coverage

Coverage records make quilting and chart selection inspectable:

- coverage id;
- source id or group id;
- geographic polygon;
- valid scale range;
- priority/rank;
- chart family;
- boundary behavior;
- overlap policy;
- update epoch.

Coverage is package data, but runtime selection, overscan, prefetch, adjacent
zoom blending, and cache epoch policy remain adapter/scheduler concerns.

### Provenance

Provenance records are mandatory for wrong-location debugging:

- provenance id;
- source id;
- source object id;
- source object class;
- source geometry hash;
- converter step ids;
- transform chain;
- coordinate samples before and after projection/normalization;
- generated feature/geometry/raster ids;
- diagnostics emitted by each step.

The inspection chain should be able to answer:

```text
source chart/object
  -> converter record
  -> package feature
  -> presentation rule
  -> neutral primitive
  -> cache key
  -> backend resource
  -> final draw item or pixel query
```

## Validation Rules

A package is invalid if:

- the manifest schema version is unsupported;
- package content hashes do not match;
- a feature references missing geometry, source, or provenance records;
- a raster sheet lacks bounds or content identity;
- source object ids are discarded when the source format provides them;
- geometry bounds are inconsistent with coverage/source bounds;
- diagnostics are present but not addressable by record id;
- renderer/backend fields are embedded in portable records.

A package may be valid but incomplete if a converter emits explicit diagnostics
for unsupported object classes, missing private chart data, or presentation
rules intentionally deferred to later slices.

## Converter Responsibilities

The replaceable converter API is defined in
`docs/CHART_CONVERTER_MODULE_API.md`. At the package boundary, each converter
module must:

- produce a package that validates without renderer code;
- preserve source feature ids and source geometry hashes wherever available;
- attach provenance to every derived feature, geometry, raster, and coverage
  record;
- emit deterministic output for the same source/update set and converter
  version;
- report unsupported content as diagnostics rather than silently dropping it;
- keep format-specific decisions inside the converter, not in presentation or
  backend code.

S-57/S-101, raster/KAP, CM93, MBTiles/PMTiles, OSM/community, and future sonar
converters can be implemented, replaced, or rewritten independently as long as
they produce this package contract.

## Presentation Compiler Responsibilities

The production compiler boundary is defined in
`docs/PRESENTATION_COMPILER_BOUNDARY.md`. The S-52/S-101 presentation compiler
consumes:

- portable package features/geometries/rasters/coverage;
- display state;
- safety contour/depth settings;
- scale/view state;
- palette and display-category settings.

It emits `NauticalRenderModel` primitives such as area fills, line strokes,
symbols, text labels, soundings, raster patches, and contours. It owns S-52 and
S-101 decisions. The package does not.

## GPU Artifact Cache Responsibilities

The production cache contract is defined in
`docs/MACHINE_LOCAL_GPU_ARTIFACT_CACHE.md` and represented in C++ by
`include/gpu_artifact_cache_contract.hpp`.

The machine-local GPU artifact cache consumes neutral render primitives or
presentation assets and emits backend/device-specific artifacts:

- vertex and index buffers;
- compressed raster textures;
- symbol, pattern, line, and glyph atlases;
- material and pipeline keys;
- viewport/tile cache entries;
- memory estimates and eviction metadata.

This cache is disposable and may differ for VSG/Vulkan, WebGPU, WebGL, Metal,
or future backends. It can store provenance handles but must not become the
owner of chart semantics.

## Initial FORMAT-2 Slice

`FORMAT-2` should implement the smallest useful round trip:

1. one redistributable Chart 1-style fixture package;
2. one source record;
3. at least one point, line, area, raster/coverage, and depth-related feature;
4. normalized geometry records;
5. provenance records for each feature;
6. manifest and checksums;
7. validator diagnostics;
8. a C++17-compatible smoke test proving read -> validate -> write -> validate
   preserves ids, bounds, hashes, scale metadata, and provenance.

No GPU fields are allowed in that slice.

## Open Questions

- Should the first persisted form be a directory of canonical JSON records, a
  compact binary table set, SQLite, FlatBuffers, or another container?
- Which NOAA/S-57 fixture cell should become the first redistributable
  non-synthetic package?
- How should update chains be represented for mixed base/update packages?
- Which coverage/quilt rules belong in the package versus the runtime adapter?
- How much geometry simplification lineage must be retained for maintainers to
  debug wrong-location reports?
- What minimum performance target should `FORMAT-2` meet for package load and
  validation on boat-class hardware?
