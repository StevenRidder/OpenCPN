# Depth And Shoreline Tessellation

`depth_tessellation.hpp` is the first depth/shoreline prototype. It converts
small normalized geometries into render commands without depending on the
DEPTH-1 fixture-cell selection work.

Current scope:

- Shoreline polylines become `kStrokeLine` commands.
- Land, depth, and area-pattern polygons become `kFillArea` commands.
- Polygon rings carry simple triangle-fan metadata so batching and buffer
  ownership work can see a mesh-like shape before broad tessellation lands.
- Every command carries provenance, conversion trace refs, and an applied S-52
  rule id.

Non-goals for this slice:

- Robust polygon triangulation with holes.
- Complete S-57 object coverage.
- Safety-contour rendering.
- Tile-scale performance claims.

`depth_safety.hpp` layers the first safety-depth behavior on this prototype:
depth areas are classified against `DisplayState` thresholds, palette buckets
are attached to commands, and the safety contour is emitted only when the
viewport scale says it should be visible.
