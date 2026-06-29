# VSG GPU Asset Cache

VSG owns only the runtime GPU asset cache for the proof backend. Inputs are the
backend-neutral nautical render model and already-compiled resource records.

The cache does not parse charts, run S-52/S-101 presentation rules, decide
quilting, classify MBTiles/PMTiles as runtime contracts, schedule tile
prefetch, or define Helm HTTP/cache artifacts. Those decisions happen before
the backend boundary and arrive as neutral primitive fields, resource records,
LOD hints, coverage metadata, cache keys, and provenance handles.

The generic CACHE-1 contract is documented in
`docs/MACHINE_LOCAL_GPU_ARTIFACT_CACHE.md` and represented by
`GpuArtifactCacheManifest`. The current VSG-specific C++17 prototype emits a
deterministic `VsgGpuCacheManifest`:

- machine-local descriptor-ready texture/atlas/uniform records for resources
  used by neutral primitives;
- machine-local vertex/index buffer records for area, line, contour, raster,
  and clip geometry;
- frame-local instance buffer records for symbols, labels, and soundings;
- stable cache keys scoped by namespace, device profile, shader profile, model
  key, asset key, and content key;
- provenance refs and neutral primitive ids for wrong-location debugging;
- byte estimates and residency classes for later replacement by real VSG/Vulkan
  objects.

`opencpn-vsg-gpu-cache-smoke` builds manifests from the Chart 1 fixture and the
raster quilting fixture, verifies deterministic asset ids, checks texture,
buffer, instance, and uniform coverage, and rejects a manifest that assigns
semantic ownership to the backend.
