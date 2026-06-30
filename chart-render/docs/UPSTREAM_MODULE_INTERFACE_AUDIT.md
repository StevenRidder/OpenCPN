# Upstream Module Interface Audit

Status: UPSTREAM-2 production adoption gate

This note audits the render-core seams against OpenCPN's `ocpn_plugin.h`
surface before the first upstream-facing production slice. The goal is not to
reuse the plugin system, expose a new plugin framework, or make a browser or
backend look like an OpenCPN plugin. The goal is to prove the new C++ seams are
boring, versioned, capability-aware, and shaped around data flows OpenCPN has
already had to support.

Reference surface: `include/ocpn_plugin.h` on the OpenCPN tree. The proof
branch keeps a sparse `chart-render/` checkout, so this audit treats that header
as upstream evidence rather than a local dependency.

## What `ocpn_plugin.h` Teaches

The current plugin API is broad because plugins historically needed to touch
many host services. The renderer slice should learn from that API without
copying it.

- Version and capability negotiation are explicit. `API_VERSION_MAJOR`,
  `API_VERSION_MINOR`, and the `WANTS_*` / `INSTALLS_*` flags let the host call
  only the surfaces a plugin declared.
- OpenCPN owns lifecycle and host objects. Plugin code gets `Init()`,
  `DeInit()`, optional late/shutdown hooks, canvas callbacks, and host APIs, but
  does not own the application.
- Viewport and display state are host-provided. `PlugIn_ViewPort`, color
  schemes, display categories, projection, scale, rotation, canvas size, and
  quilt state flow from OpenCPN into extension code.
- Chart metadata is separate from chart drawing. `PlugInChartBase` exposes chart
  family/type, projection, scale, extent, coverage, depth units, ready state,
  thumbnails, and region rendering.
- Navigation, AIS, routes, waypoints, and active-leg state are host services.
  Position fixes, AIS sentences/targets, active-leg info, route/waypoint APIs,
  and newer `HostApi121` helpers are large surfaces outside renderer ownership.
- Overlay drawing is callback and priority based. Standard DC and OpenGL
  overlay callbacks draw relative to viewport/canvas/priority. They do not make
  every overlay an official chart-source primitive.
- Messaging and configuration are opt-in host services. Plugin messaging,
  config reload/flush, preferences, context menus, toolbar items, mouse, and
  keyboard hooks are application integration surfaces, not renderer-core
  semantics.
- The presentation library helper surface exposes S-57/S-52 object operations
  because legacy extensions needed them. A new backend must not make those
  presentation decisions locally.

## Audit Matrix

| OpenCPN module need | `ocpn_plugin.h` evidence | Render-core seam | `UPSTREAM-1` gate |
| --- | --- | --- | --- |
| Versioned capability negotiation | API major/minor plus `WANTS_*` and `INSTALLS_*` bit flags | Schema versions on models/contracts, `DrawBackendCapabilities`, `OpenCpnFeatureFlags`, named fallback diagnostics | The upstream slice must expose one versioned adapter/contract path and visible unsupported-capability diagnostics. |
| Host lifecycle ownership | `Init()`, `DeInit()`, late init, preshutdown, resize, canvas callbacks | `OpenCpnCanvasLifecycle` and `ChooseOpenCpnRenderer()` keep wx canvas and swapchain ownership in OpenCPN | The feature flag may route only after OpenCPN provides a valid lifecycle and model; fallback stays legacy canvas. |
| Viewport and display state | `PlugIn_ViewPort`, `SetCurrentViewPort()`, color scheme and display category APIs | `RenderView`, `DisplayState`, and `ViewportTileSchedulerInput` | Adapter translation owns viewport/display/cache epoch setup; backends receive target uniforms, not host policy. |
| Chart source and metadata | `PlugInChartBase` chart type/family/projection/scale/extent/depth/coverage/readiness methods | `ChartSourceRef`, `ChartSourceProduct`, portable packages, `ChartCoverageMetadata` | Source formats stay in converter modules. VSG/WebGPU/server targets cannot parse source chart files. |
| S-52/S-101 presentation decisions | PLIB helpers, S-57 object info, display priority/category, depth and symbol state | `S52PresentationCompiler`, `NauticalRenderModel`, source-to-render inspection | Presentation decisions happen before backend handoff; a backend-local chart rule is a failed gate. |
| Overlay drawing and layering | `RenderOverlay*`, `RenderGLOverlay*`, canvas index, overlay priority constants | Tier 2/3 handles in `DrawBackendContract`, WebGPU artifact consumer, browser fixture | Overlays may compose with chart packets, but retain overlay/UI ownership and provenance. |
| Nav, AIS, and active leg data | `SetPositionFix()`, `SetPositionFixEx()`, `SetAISSentence()`, `PlugIn_AIS_Target`, `SetActiveLegInfo()` | Not owned by chart-render core; represented only as future Tier 2 overlay/artifact packets where needed | The first upstream slice must not implement nav/AIS/route semantics. It may preserve handles for later overlay integration. |
| Routes and waypoints | `Add/Update/DeleteSingleWaypoint`, route APIs, `HostApi121::Route`, active route helpers | Out of scope for renderer core; inspection can carry route/overlay provenance ids | If `UPSTREAM-1` needs route mutation or route editing, split a separate host-service adapter task first. |
| Messaging and config | `WANTS_PLUGIN_MESSAGING`, `SetPluginMessage()`, config flush/reload, preferences hooks | Diagnostics, feature flag names, and explicit adapter decisions | Do not add a plugin message bus to the renderer slice. Config scope is the feature flag and logged diagnostics. |
| UI/menu/input integration | Toolbar, toolbox, preferences, context menus, mouse and keyboard hooks | Out of scope for `chart-render/`; Helm/UI registries stay Tier 3 | The upstream slice should be reviewable without UI chrome, menu actions, or input hooks. |

## Coverage Verdict

The current render-core seams cover the renderer-relevant subset of
`ocpn_plugin.h`:

- viewport, projection, scale, rotation, pixel size, display category, palette,
  and safety-depth state;
- chart-source identity, chart family/type/projection/coverage, source object
  ids, provenance, diagnostics, and package readiness;
- presentation ownership for S-52/S-101 decisions before backend handoff;
- draw-only backend capability negotiation, fallback selection, offscreen/readback
  posture, and tier/provenance preservation;
- OpenCPN feature-flag routing with host lifecycle ownership and legacy fallback;
- Helm WebGPU/WebGL/server-raster consumer paths that do not own chart
  semantics.

The renderer slice intentionally does not cover:

- plugin loading, plugin manager metadata, or plugin ABI compatibility;
- toolbar, preferences, context menus, keyboard, mouse, or wxAUI integration;
- route/waypoint mutation, active-route control, or autopilot behavior;
- raw NMEA/SignalK/N2K message handling;
- AIS target list/control operations;
- plugin-to-plugin messaging;
- full PLIB/S-52 public helper replacement.

Those are valid OpenCPN module needs, but they are not prerequisites for a tiny
feature-flagged renderer slice.

## Upstream Slice Rule

`UPSTREAM-1` should present the new path as a small OpenCPN-native adapter:

```text
OpenCPN chart canvas and host lifecycle
  -> feature-flag decision
  -> adapter-provided RenderView + DisplayState + validated model
  -> draw-only backend contract
  -> VSG proof backend or visible legacy fallback
```

It should not present:

```text
OpenCPN plugin API replacement
  -> new module framework
  -> renderer-owned navigation, AIS, route, UI, config, or messaging surfaces
```

If the production slice cannot be reviewed without understanding route editing,
plugin messaging, toolbar/menu integration, AIS controls, or full chart-plugin
ABI behavior, it is too large for the first upstream PR.

## Reviewer Checklist

Before opening the upstream-facing slice:

- Confirm the adapter has one feature flag and one visible legacy fallback.
- Confirm OpenCPN owns the wx canvas, swapchain, native window, and host
  lifecycle.
- Confirm all S-52/S-101 display decisions enter the backend as compiled
  primitives or GPU artifacts, not as backend-local source parsing.
- Confirm unsupported backend/device/offscreen/readback capability failures emit
  named diagnostics.
- Confirm source-to-render inspection handles survive through the handoff.
- Confirm nav, AIS, route, waypoint, UI, config, and messaging surfaces are
  either absent or explicitly deferred in the PR description.
- Confirm the PR can be reviewed from `chart-render/` C++/CMake surfaces plus
  the existing proof docs, without private Helm runtime context.
