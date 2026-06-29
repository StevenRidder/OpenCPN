# S-52 Presentation Compiler

`SYM-5` moves chart presentation decisions into a dedicated C++ compiler that
emits the backend-neutral nautical render model defined by `SEAM-5`.

For the production boundary plan that follows the portable nautical package,
see `docs/PRESENTATION_COMPILER_BOUNDARY.md`.

## Boundary

Input:

- normalized nautical features from chart-source conversion;
- provenance and conversion trace handles;
- display state, including palette, display category, scale, text/sounding
  toggles, safety depth, and safety contour;
- presentation asset identifiers for fills, line styles, symbols, and fonts.

Output:

- `NauticalRenderModel` layers and primitives: `AreaFill`, `ContourLine`,
  `SymbolInstance`, `TextLabel`, and `Sounding` for the initial smoke slice;
- resolved resource identifiers and cache keys;
- stable layer ordering;
- provenance trace handles on every primitive;
- diagnostics for objects filtered before backend handoff.

The compiler owns S-52/S-101 object-class decisions, display category handling,
SCAMIN filtering, palette selection, safety-contour styling, safety-depth
classification, symbol selection, text-label selection, and sounding formatting.

The VSG, OpenGL, Metal, WebGPU, and Helm targets consume only compiled neutral
primitives or assets derived from them. They must not parse chart sources,
evaluate S-52 object classes, apply SCAMIN, choose safety-contour styling, run
quilting policy, or own runtime scheduling decisions.

## Initial Mapping

The first C++ fixture covers the core Chart 1-style cases:

| Normalized feature | Compiler decision | Neutral primitive |
|---|---|---|
| `DEPARE` | palette and safety-depth fill class | `AreaFill` |
| `DEPCNT` | safety-contour line style when `VALDCO` matches display state | `ContourLine` |
| `BOYLAT` | lateral buoy symbol asset | `SymbolInstance` |
| `SOUNDG` | formatted depth and safety class | `Sounding` |
| `LNDARE` | object-name text label | `TextLabel` |
| display-category mismatch | filtered with diagnostic | none |
| SCAMIN mismatch | filtered with diagnostic | none |

Raw object-class strings remain in `SourceTraceHandle` for wrong-location
debugging. They are not placed in primitive metadata or backend contracts, so a
backend can draw without knowing S-52.

## Smoke Test

```bash
cmake -S chart-render -B /tmp/helm-vulkan-sym5-build
cmake --build /tmp/helm-vulkan-sym5-build
/tmp/helm-vulkan-sym5-build/opencpn-s52-presentation-compiler-smoke
```

The smoke asserts that:

- normalized features compile into `NauticalRenderModel` primitives;
- display category and SCAMIN filtering happen before backend handoff;
- safety contour styling is resolved before backend handoff;
- emitted primitives keep provenance trace handles;
- VSG accepts the compiled model through `RenderModel` without owning S-52
  semantic decisions.
