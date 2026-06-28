# Conversion Provenance

Every rendered object should be traceable without reading backend-specific
renderer code. The debug trail is:

```text
source chart/cell/object id
  -> source geometry hash
  -> projection and transform chain
  -> generated geometry id
  -> target tile/screen placement
  -> applied S-52 rule
  -> render command id
```

`conversion_trace.hpp` captures that trail and provides two guardrails:

- `AttachConversionTrace` writes trace ids and applied-rule metadata onto a
  `RenderCommand`.
- `ValidateRenderSceneTraceability` reports commands missing provenance,
  generated geometry ids, conversion trace ids, or S-52 rule metadata.

Wrong-location reports should attach the conversion trace and source
provenance. A buoy drawn inland, for example, should produce a diagnostic that
names the chart, object id, generated geometry id, target command id, target
bounds, and S-52 rule before anyone changes renderer math.
