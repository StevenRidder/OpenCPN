# OpenCPN/VSG/WebGPU Compatibility Matrix

Status: COMPAT-1 production architecture acceptance matrix

This matrix defines what the `vulkan/render-core-poc` branch may claim before
public hardening work begins. It is an acceptance contract, not a new renderer
implementation. Runtime renderer work remains C++17-shaped, CMake-based, and
OpenCPN-native.

## Compatibility Baseline

| Area | Supported baseline | Required evidence | Current posture |
| --- | --- | --- | --- |
| C++ language | OpenCPN-compatible C++17-shaped code | `target_compile_features(... cxx_std_17)`, branch-local smoke build | Required for runtime and renderer code |
| Build system | CMake 3.16+ subtree build under `chart-render/` | `cmake -S chart-render -B <build-dir>` and full target build | Required |
| Source conversion | Replaceable C++ converter modules emit portable packages | S-57 package roundtrip and converter diagnostics smokes | Supported for first fixture slice |
| Presentation | S-52/S-101 decisions occur before backend handoff | presentation compiler smoke and source-to-render traces | Required boundary |
| Neutral model | Backend-neutral primitives, resources, cache keys, provenance | neutral-model smoke and golden/debug evidence | Required boundary |
| VSG/Vulkan | Native OpenCPN proof backend consuming neutral artifacts | VSG cache/backend smokes and production fixture evidence | Proof backend only |
| Helm WebGPU | Browser/client artifact consumer contract | WebGPU consumer contract smoke and docs | Contract only, not full renderer |
| WebGL/MapLibre | Fallback/composition target for Helm clients | explicit fallback route in WebGPU contract | Fallback only |
| Server raster | Safety and verification fallback | golden corpus and deterministic offscreen hash evidence | Required fallback |
| Metal | Deferred native-backend compatibility note | `METAL_BACKEND_COMPATIBILITY.md` gap list | No active Metal renderer |

## Platform And Toolchain Matrix

| Platform | Compiler/toolchain expectation | Vulkan/VSG expectation | WebGPU expectation | Gate before public hardening |
| --- | --- | --- | --- | --- |
| macOS | AppleClang with C++17 and CMake 3.16+ | Vulkan through available VSG/Vulkan loader path or fixture-safe fallback; no Metal dependency | Safari/Chrome WebGPU remains Helm-client future work | Build all `chart-render` smokes; Metal remains deferred |
| Linux | GCC or Clang with C++17 and CMake 3.16+ | Native Vulkan loader, VSG runtime deps, and offscreen-capable driver | Chromium-family WebGPU where available for Helm client experiments | Build all smokes and report missing Vulkan/VSG deps as diagnostics |
| Windows | MSVC C++17 or clang-cl with CMake generator support | Vulkan SDK/VSG runtime deps and offscreen-capable driver | Edge/Chrome WebGPU where available for Helm client experiments | Build all smokes before claiming support; record unavailable deps explicitly |
| Headless CI | C++17 compiler and CMake only | Offscreen fixture path or explicit unavailable-backend diagnostic | No browser required | Smokes must fail loudly if a target is unavailable |

The POC may be reviewed on any platform that can build the C++ evidence. A
platform is not considered production-supported until its compiler, CMake,
VSG/Vulkan runtime, offscreen behavior, and diagnostics are recorded together.

## Device And Driver Assumptions

VSG/Vulkan proof runs require:

- a Vulkan loader and driver stack visible to the process;
- VSG runtime dependencies matching the compiled proof branch;
- an offscreen-capable render path for fixture and golden evidence;
- deterministic render target size, color format, and readback policy for
  golden/debug smokes;
- explicit diagnostics when device creation, required extensions, swapchain, or
  offscreen readback are unavailable.

Helm WebGPU client work requires:

- feature detection before selecting WebGPU;
- WebGL/MapLibre or server-raster fallback when WebGPU is absent, disabled, or
  below required feature profile;
- server-owned chart semantics and inspection handles in every fallback path;
- no browser-side S-52/S-101 or S-100 portrayal decisions to fill gaps.

## Headless And Offscreen Requirements

The public proof branch must keep headless/offscreen behavior separate from live
OpenCPN or Helm UI lifecycles:

- `RenderTarget`/backend code owns offscreen binding, readback, and result
  diagnostics only after the adapter has provided a validated model and target.
- OpenCPN owns native window, canvas, swapchain, and feature-flag selection.
- Helm owns HTTP/cache/browser composition outside this branch; public demos use
  private ports and never depend on Helm's live `:8080` screen.
- Golden image and source-to-pixel traces must include backend ids, artifact
  ids, source object ids, and pixel/query evidence.

## Unsupported Target Diagnostics

When a target cannot run, it must produce an auditable diagnostic rather than a
silent fallback. Minimum diagnostic fields:

- target family: `vsg_vulkan`, `helm_webgpu`, `webgl_maplibre`,
  `server_raster`, or `metal_deferred`;
- platform and compiler identity when known;
- device/profile id, driver/runtime id, or clear `unavailable` marker;
- failed capability: device creation, required extension, offscreen target,
  readback, shader/material profile, cache artifact, browser feature, or
  server-raster fallback;
- semantic-preserving fallback selected, or explicit `no_fallback_available`;
- source/model/artifact/provenance handles if a rendered fixture reached
  backend handoff.

Fallbacks may preserve availability. They must not make the proof look greener
than it is.

## Public Claim Rules

Allowed claims:

- The branch is an upstream-shaped C++/CMake proof of a renderer seam.
- VSG/Vulkan is the native OpenCPN proof backend for the first production
  fixture path.
- Helm WebGPU is the preferred client direction and artifact-consumer contract.
- WebGL/MapLibre and server raster are fallback/composition surfaces.
- Metal is compatible with the boundary in principle, but remains deferred.

Disallowed claims:

- production renderer readiness;
- full S-52/S-101 parity;
- ECDIS certification or primary-navigation approval;
- implemented production WebGPU renderer;
- implemented Metal renderer;
- support for every platform, GPU, browser, or driver without recorded evidence.

## Acceptance Checklist

Before `UPSTREAM-1` or public hardening claims depend on this matrix:

- all branch-local smoke targets build on the claiming platform;
- `opencpn-stakeholder-demo` completes and records disposable evidence;
- unsupported targets produce named diagnostics or are documented as deferred;
- PERF-2 records measured timings before performance viability is claimed;
- QA-5 golden/debug evidence remains current for the first production slice;
- docs link this matrix wherever compatibility, performance, fallback, or
  Metal posture is discussed.
