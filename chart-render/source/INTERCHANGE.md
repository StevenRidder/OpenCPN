# Interchange Packages

MBTiles and PMTiles are useful packaging formats for importing, exporting, and
debugging chart data. In this POC they are not the renderer contract.

Allowed uses:

- Source input to an `IChartSource` implementation.
- Fixture or regression-test packaging.
- Debug artifact attached to a `ChartSourceProduct`.
- Offline exchange format before normalization.

Disallowed use:

- Repeatedly decoding MBTiles or PMTiles tiles as the renderer hot path.
- Making renderer backends depend on container-specific addressing, metadata,
  tile encoding, or web-map assumptions.

The intended runtime path is:

```text
MBTiles or PMTiles package
  -> chart-source normalization
  -> ChartSourceProduct
  -> portable nautical package
  -> S-52 command stream
  -> machine-local normalized scene and GPU resource caches
  -> renderer backend
```

`chart_interchange.hpp` captures this policy in C++ so callers can classify a
package for source/debug use, build a debug artifact, and derive cache keys that
point at normalized scene data and GPU resources instead of the original
container.

The replaceable converter API in `docs/CHART_CONVERTER_MODULE_API.md` keeps
MBTiles and PMTiles on the import/export/debug side of the package boundary.
