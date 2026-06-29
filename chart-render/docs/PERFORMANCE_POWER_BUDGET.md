# Performance And Power Budget

Status: PERF-1 production architecture and C++ contract

PERF-1 defines the first measurable budget for the production renderer slice.
The contract is intentionally small: it names the metrics that must be measured,
the profile-specific thresholds, and the owners for each stage. PERF-2 should
attach real measurements to this contract; PERF-1 does not claim production
timings from fixture smokes.

The C++ surface lives in `include/performance_budget_contract.hpp` and
`source/performance_budget_contract.cpp`.

## Pipeline Scope

The budget covers the first production slice:

```text
portable package load
  -> S-52/S-101 presentation compile
  -> GPU artifact compile/cache hit
  -> draw-only backend render
  -> viewport scheduler
  -> memory, disk cache, and power envelope
```

The contract references the existing boundaries:

- portable package: `portable-nautical-package`;
- presentation output: `backend-neutral-nautical-render-model`;
- GPU artifacts: `machine-local-gpu-artifacts`;
- backend: `draw-only-backend`;
- scheduler: `adapter-viewport-cache-v1`.

Backends still do not parse source charts, own presentation semantics, or decide
quilting/scheduler policy.

## Device Profiles

| Profile | Target | Frame Budget | Prefetch Policy | Power Policy |
| --- | --- | ---: | --- | --- |
| `desktop_reference` | Desktop VSG/WebGPU reference | 60 Hz / 16.7 ms render | up to 64 prefetch tiles | active <= 45 W, idle <= 12 W |
| `boat_low_power` | Fanless boat-class GPU | 15 Hz / 66.7 ms render | up to 16 prefetch tiles, 4 in power-save | active <= 10 W, idle <= 3 W |

The boat profile requires low-power mode and cache hits for sustained pan/zoom.
If cache hits are not available, adapters should reduce prefetch and preserve
visible-tile priority rather than hiding the miss with optimistic frame claims.

## Required Metrics

Every profile must have hard-gate targets for:

- package load time (`package_load_ms`);
- presentation compile time (`presentation_compile_ms`);
- GPU artifact compile time (`gpu_artifact_compile_ms`);
- GPU artifact cache-hit time (`gpu_artifact_cache_hit_ms`);
- backend render time (`backend_render_ms`);
- resident memory (`resident_memory_bytes`);
- disk cache footprint (`disk_cache_bytes`);
- active power (`active_power_watts`);
- idle power (`idle_power_watts`);
- viewport scheduling time (`viewport_schedule_ms`).

## Budget Table

| Metric | Desktop Reference | Boat Low Power |
| --- | ---: | ---: |
| Package load | 50 ms | 150 ms |
| Presentation compile | 30 ms | 120 ms |
| GPU artifact compile | 50 ms | 250 ms |
| GPU artifact cache hit | 5 ms | 20 ms |
| Backend render | 16.7 ms | 66.7 ms |
| Resident memory | 512 MiB | 256 MiB |
| Disk cache | 256 MiB | 128 MiB |
| Active power | 45 W | 10 W |
| Idle power | 12 W | 3 W |
| Viewport scheduling | 5 ms | 15 ms |

These are first-slice gates, not final product limits. They are deliberately
strict enough to prevent hidden regressions and broad enough to work before the
real VSG/WebGPU production paths are tuned.

## Validation

`ValidatePerformanceBudget` rejects a budget when:

- the budget identity or named pipeline contracts are missing;
- desktop or boat-class profiles are absent;
- a profile has invalid frame-rate or prefetch settings;
- boat-class mode does not require low-power behavior, bounded prefetch, and
  cache hits for sustained pan/zoom;
- any required metric target is missing;
- a target uses the wrong unit;
- boat memory, disk, or power budgets exceed desktop budgets.

`EvaluatePerformanceBudget` rejects a measurement set when:

- a hard-gate measurement is missing;
- a value uses the wrong unit or has no evidence id;
- a metric exceeds its target.

## Smoke Test

```bash
cmake -S chart-render -B /tmp/helm-vulkan-perf1-build
cmake --build /tmp/helm-vulkan-perf1-build
/tmp/helm-vulkan-perf1-build/opencpn-performance-budget-contract-smoke
```

The smoke builds current fixture package/model/cache/backend/scheduler objects,
evaluates desktop and boat-class fixture measurements against the contract, and
checks negative cases for missing cache-hit budget, active-power overrun, and
missing render-time measurement. Real timing and power evidence belongs in
PERF-2.
