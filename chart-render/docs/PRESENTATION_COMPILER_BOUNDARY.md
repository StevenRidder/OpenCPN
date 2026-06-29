# Production Presentation Compiler Boundary

Status: PRESENT-1 production architecture draft

This document defines the production boundary for S-52/S-101 presentation:

```text
portable nautical package + display state
  -> S-52/S-101 presentation compiler
  -> neutral nautical render model
  -> machine-local GPU artifact cache
  -> backend renderer
```

The compiler is the semantic owner for nautical presentation. It decides how
source-neutral package facts become draw-ready neutral primitives. Vulkan, VSG,
WebGPU, WebGL, Metal, OpenGL, MapLibre, and server-raster targets must not own
S-52/S-101 object-class decisions.

## Goals

- Keep chart semantics out of renderer backends.
- Make S-52/S-101 decisions reviewable in one C++ module.
- Compile the portable nautical package plus display state into
  backend-neutral `NauticalRenderModel` primitives.
- Preserve traceability from source feature to presentation rule to final
  primitive.
- Support OpenCPN native and Helm/WebGPU consumers from the same semantic
  output.

## Non-Goals

- Not a full S-52 or S-101 implementation in this first production plan.
- Not a GPU resource compiler.
- Not a texture atlas, glyph atlas, or shader-material system.
- Not a tile scheduler, HTTP cache, MapLibre layer, or wx canvas adapter.
- Not a place to parse S-57/S-101/KAP/CM93/OSM source files.
- Not an ECDIS certification claim.

## Input Contract

The compiler consumes:

- portable nautical package manifest and records;
- feature, geometry, raster, coverage, provenance, diagnostic, and index
  records;
- presentation catalog profile, for example S-52 POC, future full S-52, or
  future S-101;
- display state;
- view/scale state needed for SCAMIN and scale-dependent choices;
- palette state, including day/dusk/night and color table;
- display category and viewing group state;
- safety contour, safety depth, shallow/deep contour settings, and units;
- text, sounding, and simplified-symbol toggles;
- backend-neutral presentation asset identifiers.

The compiler should not read source chart files. If a needed source fact is
missing from the package, the compiler emits a diagnostic that names the missing
package field.

## Output Contract

The compiler emits `NauticalRenderModel` records:

- layers and stable draw order;
- backend-neutral primitives such as area fills, line strokes, contour lines,
  symbol instances, text labels, soundings, raster patches, and clip
  boundaries;
- backend-neutral resource identifiers for symbols, line styles, fills,
  patterns, fonts, and raster references;
- LOD hints and scale/display-category visibility decisions;
- coverage and quilt metadata needed by adapters;
- source trace handles on every primitive;
- presentation diagnostics;
- cache keys that identify semantic output, not GPU resources.

The model may be consumed by VSG/Vulkan, WebGPU, WebGL/MapLibre fallback,
OpenGL, Metal compatibility checks, or server-raster output. Those targets
consume primitives and assets; they do not re-run presentation rules.

## Semantic Ownership

The compiler owns:

- S-52/S-101 object-class and attribute interpretation;
- display category and viewing group filtering;
- SCAMIN and scale-dependent filtering;
- draw priority, display plane, and layer ordering;
- palette and color-table selection;
- safety contour and safety depth behavior;
- shallow/deep/safety area classification;
- symbol selection;
- line style and pattern selection;
- text and sounding selection, formatting, priority, and suppression rules;
- unsupported-rule diagnostics;
- rule-version and catalog-version metadata.

The backend owns:

- device-specific buffers;
- texture and atlas upload;
- command submission;
- shader/material binding;
- viewport draw/readback;
- backend performance counters.

The backend must not:

- inspect S-52/S-101 object classes to choose symbols;
- apply SCAMIN;
- select safety-contour styling;
- decide display category visibility;
- format soundings;
- parse source chart attributes;
- decide nautical draw order from source semantics.

## Compilation Pipeline

The production compiler should be structured as explicit passes:

```text
package validation snapshot
  -> feature classification
  -> display-category/viewing-group filter
  -> SCAMIN/scale filter
  -> safety/depth classification
  -> rule lookup and asset resolution
  -> text/sounding candidate selection
  -> draw-order/layer assignment
  -> neutral primitive emission
  -> model validation and diagnostics
```

Each pass should be deterministic and should attach diagnostics to source
feature ids and package record ids.

## Presentation Catalogs

Presentation catalogs should be versioned data or C++ tables, not backend
logic. A catalog profile identifies:

- rule-set name and version;
- supported object classes and attributes;
- symbol, line, pattern, fill, and font asset ids;
- display category mapping;
- draw priority/layer rules;
- safety/depth classification rules;
- text and sounding rules;
- known unsupported rules.

The first POC catalog can remain small and Chart 1-style. The production
boundary should allow later full S-52/S-101 catalogs without changing backend
interfaces.

## Display State

Display state is part of the compiler input and semantic cache key. It should
include:

- scale denominator and viewport transform used for SCAMIN decisions;
- display category;
- viewing group masks;
- palette name and color table version;
- safety contour and safety depth;
- shallow/deep contour settings;
- units and sounding formatting policy;
- text labels on/off;
- soundings on/off;
- simplified symbols on/off;
- isolated danger and important-text options when supported.

Changing display state invalidates presentation output, but it should not
invalidate the portable package.

## Text and Soundings

The compiler owns text and sounding semantics:

- which objects may produce labels;
- which attributes become label text;
- sounding formatting, precision, units, and safety class;
- label priority and suppression eligibility;
- source trace handles for labels and soundings;
- diagnostics for unsupported or missing label inputs.

Advanced collision placement may be a submodule, but it remains part of the
presentation layer contract. A backend may place glyphs from already-selected
label primitives or label candidates; it must not decide nautical label content
or object eligibility from source attributes.

## Raster and Coverage Presentation

Raster sheets enter through the portable package as chart facts. The compiler
may emit neutral `RasterPatch` or clip/coverage primitives with:

- visible bounds;
- chart bounds;
- collar/no-data policy;
- quilt rank;
- scale range;
- source trace handles;
- diagnostics for missing masks or ambiguous bounds.

The compiler does not create GPU textures. The GPU artifact cache may compress
or tile raster data later for the local device.

## Diagnostics

Presentation diagnostics should include:

- diagnostic id;
- severity;
- package feature/geometry/raster id;
- source trace handle;
- presentation rule id if known;
- display state key;
- message and machine-readable code;
- whether the feature was filtered, emitted with fallback, or dropped.

Expected diagnostics include:

- unsupported object class;
- missing required attribute;
- unsupported attribute value;
- display-category filtered;
- SCAMIN filtered;
- missing symbol/line/fill asset;
- malformed geometry for a rule;
- text or sounding suppressed by display state.

Filtering by display category or SCAMIN is not an error, but it should be
observable in debug builds and fixtures.

## Cache Keys

The compiler output cache key should include:

- portable package content hash;
- package schema version;
- presentation catalog id and version;
- display state hash;
- relevant scale/view state;
- compiler id and version.

It should not include GPU vendor, device id, swapchain, texture compression
format, shader layout, or backend memory decisions. Those belong to the
machine-local GPU artifact cache defined in
`docs/MACHINE_LOCAL_GPU_ARTIFACT_CACHE.md`.

## Backend Handoff

Before handoff, every emitted primitive should have:

- primitive id;
- layer id and draw order;
- geometry or position;
- backend-neutral asset references;
- opacity/rotation/scale values when relevant;
- LOD/display hints;
- source trace handle;
- cache key;
- backend handoff contract stating draw-only semantics.

A model is invalid if a primitive requires a backend to evaluate nautical
semantics before drawing.

## Validation Gates

Before backend handoff:

1. input package validates;
2. display state validates;
3. presentation catalog version is known;
4. emitted primitives have stable ids and trace handles;
5. every unsupported object creates a diagnostic;
6. every resource reference is backend-neutral and declared;
7. model validation confirms backend draw-only ownership.

These gates are designed to make OpenCPN review concrete: maintainers can
inspect rule decisions and provenance without reading VSG or WebGPU code.

## Initial PRESENT-2 Slice

`PRESENT-2` should compile one portable package fixture into neutral primitives:

1. package fixture from `FORMAT-2` and `CONVERT-2`;
2. display state with display category, palette, safety contour/depth,
   soundings, text, and scale;
3. at least one area fill, line/contour, symbol, text label, sounding, and
   filtered diagnostic;
4. source trace handles on every emitted primitive;
5. rule ids and catalog version in diagnostics/cache keys;
6. a C++17-compatible smoke test proving package -> compile -> validate;
7. fixture-golden output for review.

The slice should not implement full S-52/S-101 parity or any backend-specific
GPU resource creation.

## Review Questions

1. Are display-category, SCAMIN, palette, safety, text, and sounding decisions
   correctly owned by the compiler rather than the backend?
2. Which S-52/S-101 rules should become required fixtures first?
3. Does the trace handle carry enough information to debug wrong symbols and
   wrong locations?
4. Should label collision be fully inside presentation, or should the model
   allow backend-neutral label candidates for a later placement pass?
5. What diagnostics would make maintainers comfortable reviewing a new backend
   without fearing that it owns hidden chart semantics?
