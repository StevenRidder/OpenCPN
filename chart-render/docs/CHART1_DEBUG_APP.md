# Chart 1 Debug App

`CHART-4` turns the Chart 1 conformance harness into an inspectable debug app.
The app does not add chart semantics to VSG. It builds a Chart 1 normalized
source product, validates it against the conformance catalog, compiles it
through the S-52 presentation compiler into the neutral nautical render model,
runs the draw-only VSG placeholder, and records source-to-render rows for each
accepted case.

Each `ObjectInspection` links:

- source chart id and source feature id;
- original/provenance geometry hashes;
- normalized/target geometry id and coordinate space;
- projection transform chain;
- S-52 object class and rule id;
- display category and scale/overzoom state;
- tile/cache key;
- backend primitive id;
- final placeholder GPU and web asset ids.

Use `FindObjectBySourceFeatureId`, `FindObjectByBackendPrimitiveId`, and
`FindObjectsInLayer` for object and layer inspection in tests or future UI
adapters.
