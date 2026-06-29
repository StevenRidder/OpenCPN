# Replaceable Chart Converter Module API

Status: CONVERT-1 production architecture draft

This document defines the boundary for source-chart converters that produce the
portable nautical package:

```text
source chart
  -> converter module
  -> portable nautical package
  -> S-52/S-101 presentation compiler
  -> neutral nautical render model
  -> machine-local GPU artifact cache
  -> backend renderer
```

The renderer core must not parse S-57, S-101, KAP, CM93, OSM, MBTiles, PMTiles,
or future sonar/depth inputs directly. Source formats are owned by replaceable
converter modules. A converter may be implemented in-process for OpenCPN-shaped
C++ integration or hosted behind a small process runner for offline conversion,
but the production contract is the same: converters write validated portable
nautical packages, and downstream presentation/rendering reads only that
package contract.

## Goals

- Make source-chart conversion replaceable without changing the renderer.
- Preserve source identity, update metadata, object classes, attributes,
  geometry hashes, raster bounds, and provenance needed for wrong-location
  debugging.
- Keep renderer and backend code free of source-format parsing.
- Let S-57/S-101, raster/KAP, CM93, OSM/community, future sonar/depth, and
  MBTiles/PMTiles import/export/debug paths coexist without becoming one
  monolith.
- Keep production runtime work OpenCPN-compatible C++ and CMake shaped.

## Non-Goals

- Not a complete S-57, S-101, raster, CM93, OSM, or sonar implementation.
- Not a GPU cache, texture compiler, vertex-buffer layout, or renderer hot
  path.
- Not a place to evaluate S-52/S-101 presentation rules.
- Not an MBTiles/PMTiles renderer contract.
- Not a Helm HTTP, tile cache, ETag, MapLibre, or browser-composition API.
- Not permission to add a foreign-language renderer core.

## Boundary

Input:

- one or more source chart records;
- source file identities and content hashes;
- edition/update chain metadata;
- conversion profile, for example `s57-s52-poc` or future `s101`;
- target package schema version;
- optional fixture/debug policy;
- optional geographic or scale bounds for bounded conversion.

Output:

- a portable nautical package manifest;
- normalized source records;
- feature, geometry, raster, coverage, provenance, diagnostic, index, and
  checksum records;
- explicit unsupported-content diagnostics;
- conversion summary and reproducibility metadata.

The output must validate without renderer code. A backend should be able to
refuse any package that lacks source identity, provenance, or stable package
schema information before Vulkan, VSG, WebGPU, or other device-specific code is
involved.

## C++ Interface Shape

The first implementation should stay close to the existing `IChartSource`
surface while adding the package writer and validation boundary. The logical API
should look like this, even if `CONVERT-2` chooses slightly different names:

```cpp
class IChartConverter {
 public:
  virtual ~IChartConverter() = default;

  virtual ConverterIdentity Identity() const = 0;
  virtual ConverterCapabilities Capabilities() const = 0;

  virtual SourceInspection Inspect(const SourceSet& sources,
                                   const ConversionOptions& options) = 0;

  virtual ConversionResult Convert(const ConversionRequest& request,
                                   IPackageWriter& package,
                                   IDiagnosticSink& diagnostics) = 0;
};
```

`Inspect` is cheap and does not write a package. It identifies source kind,
edition/update chain, projection, scale, coverage, license/distribution class,
and unsupported high-level capabilities.

`Convert` writes deterministic package records and returns a summary. It must
not invoke presentation compilation or backend code.

## Process Boundary

The architecture supports two hosting modes:

- in-process C++ converter module for OpenCPN-native integration, tests, and
  small fixtures;
- out-of-process converter runner for offline preprocessing, crash isolation,
  or optional heavyweight import workflows.

Both modes use the same logical records and validation rules. The process
runner is not a semantic escape hatch. It must still emit the portable package
contract and must not hand backend-ready GPU assets to the renderer.

The process runner should exchange:

- request manifest;
- source descriptors or read-only source paths;
- output package path;
- converter identity and version;
- conversion diagnostics;
- summary JSON or equivalent structured status.

The process boundary may help operationally, but the renderer boundary remains
the package.

## Converter Identity

Every converter must declare:

- converter id;
- converter version;
- source kinds accepted;
- package schema versions emitted;
- presentation profiles supported;
- whether it emits vector features, raster sheets, coverage, or debug artifacts;
- whether it preserves source object ids;
- whether it can represent update chains;
- known unsupported object classes or source features;
- build/toolchain identity for reproducibility.

Converter identity becomes package metadata and is included in cache keys and
diagnostics. If two converter versions produce different geometry from the same
source, the package must make that visible.

## Source Ownership

Converters own source-format decoding and normalization. The table below is the
initial ownership plan:

| Source kind | Converter owns | Package must preserve |
|---|---|---|
| S-57/S-101 | Dataset metadata, feature/object ids, object classes, attributes, geometry, edition/update chain, native scale, SCAMIN inputs. | Source cell/dataset id, source object id, acronym/class, attributes, geometry hashes, update chain, provenance. |
| Raster/KAP | Georeferencing, image identity, chart bounds, visible bounds, collar/no-data masks, scale, projection, visible-outside-bounds behavior. | Sheet id, image hash/reference, pixel size, bounds, collar policy, no-data policy, quilt rank, provenance. |
| CM93 | Source identity, object extraction limits, scale family, coverage, diagnostics for unsupported/private details. | Source id, feature identity where available, coverage, scale hints, diagnostics, provenance. |
| OSM/community | Source snapshot, tags, attribution, confidence/provenance, conflict policy. | Source id, native feature id, tags/attributes, geometry hash, license class, provenance. |
| Future sonar/depth | Sensor/source epoch, sampling resolution, vertical datum, confidence, derived contour/depth metadata. | Source id, sample/mesh/contour ids, units, confidence, lineage, provenance. |
| MBTiles/PMTiles | Import/export/debug classification, tile metadata, source lineage when known. | Interchange artifact metadata and normalized feature/raster records, not a renderer hot-path dependency. |

Format-specific quirks stay in the converter. Presentation and backend code
should see package records, not parser internals.

## Update Metadata

Converters must make update state explicit:

- base source id;
- update source ids in applied order;
- edition/update labels;
- source epoch;
- superseded/deleted feature references;
- converter policy for missing or out-of-order updates;
- package content hash after updates are applied.

The package should allow a maintainer to answer which base cell and update
files produced a rendered feature. Silent "latest wins" behavior is not enough.

## Provenance Obligations

Every emitted feature, geometry, raster sheet, coverage polygon, and diagnostic
must be traceable to source records. At minimum, provenance should include:

- source id;
- source object id if available;
- source object class/acronym if available;
- source geometry hash;
- source coordinate samples;
- projection/normalization step ids;
- generated package record ids;
- converter id and version;
- diagnostic ids emitted during conversion.

Wrong-location reports should be debuggable along this chain:

```text
source file/cell
  -> source object id
  -> source geometry hash
  -> converter step
  -> package feature/geometry
  -> presentation primitive
  -> backend/cache handle
  -> final draw item or pixel query
```

If a source format does not provide stable object ids, the converter must say so
and emit deterministic synthetic ids based on source path, source hash, feature
class, and geometry hash.

## Diagnostics

Converters must report problems as structured diagnostics, not comments in log
files. Diagnostics should include:

- diagnostic id;
- severity: fatal, error, warning, unsupported, deferred, or info;
- source id;
- source object id if known;
- package record id if generated;
- human-readable message;
- machine-readable code;
- recommended owner, such as converter, package validator, presentation
  compiler, or source data;
- whether downstream presentation/rendering may continue.

Fatal diagnostics stop package acceptance. Unsupported/deferred diagnostics may
allow partial package acceptance when the limitation is explicit and testable.

## Validation Gates

Before a package reaches presentation compilation:

1. the converter validates source identities and hashes;
2. the package writer validates record references while writing;
3. the package validator checks schema, checksums, geometry bounds, source
   references, provenance references, and raster/coverage consistency;
4. fixture tests verify deterministic output for the same source/update set;
5. diagnostics are addressable by source and package record id.

A converter must not hide validation failures by dropping records. If a feature
cannot be represented, it should become an explicit diagnostic tied to the
source object.

## Determinism

For the same source files, updates, options, converter version, and package
schema, conversion output should be byte-stable or logically stable after
canonicalization. This matters for regression tests, cache keys, public review,
and maintainer trust.

The first implementation may use canonical JSON fixtures. The API must not
depend on JSON as the permanent storage form.

## CONVERT-2 Vertical Slice

The first concrete slice is the C++ fixture converter in
`include/s57_portable_package_converter.hpp` and
`source/s57_portable_package_converter.cpp`. It builds one bounded synthetic
S-57-style cell into the portable nautical package contract. The fixture
preserves dataset identity, edition/update chain, source feature ids, object
classes, attributes, normalized WGS84 geometry, coverage polygon, source and
generated geometry hashes, package checksums, and provenance records. It also
emits a traceable diagnostic for one unsupported object class so the converter
boundary proves explicit fixture limits instead of silently dropping content.

This is not full S-57 coverage and not a source-file parser. A production
converter can replace the fixture input side as long as it emits the same
portable package records and validation evidence before presentation
compilation.

## Module Ownership

| Layer | Owns | Must not own |
|---|---|---|
| Converter module | Source parsing, projection/normalization, update application, package record creation, source diagnostics. | S-52/S-101 symbol decisions, backend resources, GPU caches, adapter scheduling. |
| Package validator | Schema, references, hashes, bounds, provenance, completeness diagnostics. | Source parsing, display choices, GPU layout. |
| Presentation compiler | Display category, SCAMIN, palette, safety contour/depth, object-class presentation, text/soundings semantics. | Source file parsing, update-chain decoding, backend draw resources. |
| Backend renderer | Drawing already-compiled neutral primitives and consuming machine-local GPU artifacts. | Source semantics, S-52/S-101 rules, package validation policy. |
| OpenCPN/Helm adapters | Host lifecycle, viewport/tile scheduling, offscreen/onscreen output, cache policy, inspection UI. | Source parsing or presentation semantics. |

## MBTiles and PMTiles

MBTiles and PMTiles remain useful as:

- import inputs;
- export/debug artifacts;
- fixture containers;
- transport/interchange formats.

They are not the final renderer contract. A converter may read them, classify
their payload, and emit package records. The renderer must not repeatedly decode
container tiles as its hot path, and MapLibre/web-map assumptions must not leak
into the shared render core.

## Initial CONVERT-2 Slice

`CONVERT-2` should prove the boundary with one bounded fixture:

1. one redistributable S-57/S-101-style source fixture or synthetic equivalent;
2. source inspection summary;
3. package manifest/source records;
4. point, line, area, and depth-related feature records;
5. geometry records with source hashes;
6. update/source metadata, even if the first fixture has no updates;
7. provenance records for every emitted feature and geometry;
8. explicit diagnostics for unsupported source content;
9. a C++17-compatible smoke test proving convert -> validate -> inspect.

The slice should not implement full S-57 coverage, full S-101, raster quilting,
or any GPU artifact output.

## Review Questions

1. Is the converter/package boundary the right place to isolate source-format
   complexity?
2. Which source fields are mandatory for wrong-location reports?
3. Should the first runner be in-process only, or should the process boundary be
   exercised in the first vertical slice?
4. Which raster collar and visible-outside-bounds cases should be first-class
   fixtures?
5. What source/update metadata would make OpenCPN maintainers comfortable
   reviewing conversion output without stepping through backend code?
