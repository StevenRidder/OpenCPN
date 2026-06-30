# QA-5 Production Golden Corpus

The QA-5 corpus is the first production vertical-slice gate for the Vulkan
render-core proof branch. It runs entirely in `chart-render` and does not start
OpenCPN, Helm, or any network listener.

The gate composes the merged first-slice contracts:

- redistributable S-57 fixture input `s57:US5CONVERT2`, edition `4`, update `2`
- portable nautical package identity and stable package hash
- S-52 neutral primitive ids, rules, source object handles, and stable primitive
  hashes
- VSG GPU artifact-cache metadata, tier handles, provenance, and primitive links
- deterministic offscreen golden image hash `009410097424697d`
- source-to-render inspection rows, pixel samples, and human-readable trace
- explicit known limitations for the first slice

Run it from a configured chart-render build:

```sh
cmake --build <build-dir> --target opencpn-production-golden-corpus-smoke
<build-dir>/opencpn-production-golden-corpus-smoke
```

The smoke also mutates a valid snapshot to prove the gate rejects semantic drift
before relying on pixels alone: package hash changes, primitive/rule changes,
GPU artifact semantic-owner changes, image hash changes, missing inspection
trace evidence, and missing limitations all fail with explicit diagnostics.

## Known Limitations

- The fixture is synthetic and redistributable, not a real NOAA cell.
- The first slice covers `DEPARE`, `DEPCNT`, and `BOYLAT` only.
- `LIGHTS`/light sectors are diagnostic-only in this slice.
- The VSG backend path is a deterministic fixture renderer and artifact-backed
  fallback, not live VSG scene-object replay.
- Passing this gate is not full S-52 parity, ECDIS certification, or a safety
  approval.
