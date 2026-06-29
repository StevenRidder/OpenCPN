# Chart Source Boundary

The chart-source module is the input side of the Vulkan renderer POC. It sits
before S-52 symbolization and before any VSG backend work:

```text
source charts
  -> chart-source module
  -> normalized chart objects, raster sheets, debug artifacts
  -> portable nautical package
  -> S-52/S-101 presentation compiler
  -> render command stream
  -> neutral nautical render model
  -> backend
```

The production converter module API is defined in
`docs/CHART_CONVERTER_MODULE_API.md`.

The boundary is deliberately source-pluggable. S-57 cells, SENC caches, raster
charts, MBTiles or PMTiles interchange packages, debug fixtures, and future
S-101 datasets should all enter through `IChartSource` and produce the same
`ChartSourceProduct` shape. Renderer backends should never need to know which
chart engine or interchange container produced a feature.

Backends consume the neutral nautical render model or compiled assets derived
from it. They do not own S-52/S-101 rules, chart-source parsing, chart quilting,
source semantics, cache-key policy, or scheduler policy.

Runtime rules for this POC:

- Keep production chart-source and renderer code in C++ with an OpenCPN-shaped
  build surface.
- Preserve source ids, object classes, geometry hashes, and transform decisions
  through `provenance_table`.
- Treat MBTiles and PMTiles as optional interchange or debug inputs, not the
  final renderer hot-path contract. See `INTERCHANGE.md`.
- Emit raster sheets with explicit no-data, collar, and coverage policies so
  quilting and tile composition decisions remain inspectable.
- Keep Helm-specific HTTP, cache, ETag, and MapLibre policy outside this module.
