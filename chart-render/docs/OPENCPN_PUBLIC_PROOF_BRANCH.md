# OpenCPN Public Proof Branch

Status: Vulkan board `PUB-2`

This note defines the public-facing OpenCPN review branch for the Vulkan
renderer POC. It is meant to make the proof branch reviewable without requiring
internal board context, local worktree paths, private Helm runtime details, or
generated demo artifacts.

## Review Target

- Branch under review: `vulkan/render-core-poc`.
- Sanitized proof base: `0730391eb59376df86e7e2515ebcb2aa2f7704db`.
- Public posture: upstream-shaped C++/CMake renderer seam for architecture
  review.
- Non-posture: not a replacement ultimatum, not a Helm-only fork, not a
  production renderer claim, and not a safety certification claim.

The proof branch should be read as an incremental OpenCPN-native renderer path:
chart-source normalization, S-52/S-101 presentation, neutral nautical render
model, adapter scheduler policy, and backend handoff stay visibly separated.

## Included Evidence

- `README.md`: branch map and evidence summary.
- `docs/RFC_RENDER_CORE_POC.md`: RFC package entry point for architecture
  review.
- `docs/OPENCPN_COMMUNITY_RFC_POST_DRAFT.md`: draft community post asking for
  architecture review and seam feedback.
- `POC-ACCEPTANCE.md`: acceptance rubric, non-goals, and stakeholder evidence.
- `docs/STAKEHOLDER_DEMO.md`: reproducible stakeholder demo command and talk
  track.
- `docs/PUBLIC_RELEASE_HYGIENE.md`: PUB-1 hygiene audit and publication gates.
- `docs/HELM_WEB_RENDER_TARGET.md`: Helm consumer contract with WebGPU-first
  client direction, WebGL/MapLibre fallback, and server-raster fallback.
- `docs/MAINTAINER_RESPONSE_MATRIX.md`: maintainer concerns mapped to public
  response posture, current evidence, limits, and follow-up acceptance.
- `include/`, `source/`, `s52/`, and `vsg/`: reviewable C++ core surfaces.
- `tests/fixtures/`: JSON/chart-render fixtures required by the branch-local
  smoke targets.
- `scripts/stakeholder_demo.sh`: support script only; it does not define
  runtime renderer behavior.

Representative public evidence PRs already merged into the review branch:

- #16 `SEAM-5`: neutral nautical render model.
- #18 `SYM-5`: S-52 presentation compiler into the neutral model.
- #20 `CHART-4`: Chart 1 debug inspection app.
- #21 `ADAPT-4`: viewport tile scheduler policy.
- #23 `ADAPT-6`: Helm WebGPU-first render target contract.
- #24 `QA-3`: stakeholder demo target.
- #25 `PUB-1`: public release hygiene audit.
- #26 `PUB-2`: sanitized public OpenCPN proof branch guide.
- #27 `PUB-7`: maintainer response matrix.
- #28 `PUB-4`: RFC proof package.

## Reviewer Path In 10 Minutes

For a skeptical first pass, start from a fresh clone and run the branch-local
evidence. The commands below do not require Helm, private chart packs, private
runtime paths, or generated artifacts checked into Git:

```sh
git clone https://github.com/StevenRidder/OpenCPN.git opencpn-vulkan-proof
cd opencpn-vulkan-proof
git switch vulkan/render-core-poc

cmake -S chart-render -B /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build --target opencpn-stakeholder-demo
```

Then read the proof package in this order:

1. `chart-render/docs/RFC_RENDER_CORE_POC.md` - the architecture ask,
   boundaries, evidence, limitations, and open questions.
2. `chart-render/POC-ACCEPTANCE.md` - acceptance rubric and non-goals.
3. `chart-render/docs/STAKEHOLDER_DEMO.md` - what the demo target proves and
   what it does not claim.
4. `chart-render/docs/MAINTAINER_RESPONSE_MATRIX.md` - known maintainer
   concerns mapped to evidence and follow-up work.

## Reproduce The Local Evidence

From a checkout of `vulkan/render-core-poc`:

```sh
cmake -S chart-render -B /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build
cmake --build /tmp/opencpn-vulkan-proof-build --target opencpn-stakeholder-demo
```

The demo target runs the branch-local smoke binaries and writes a disposable
summary under the build directory:

```text
/tmp/opencpn-vulkan-proof-build/stakeholder-demo/qa3-stakeholder-demo-summary.md
/tmp/opencpn-vulkan-proof-build/stakeholder-demo/qa3-stakeholder-demo.log
```

These `/tmp` paths are examples, not required machine layout. Do not commit the
build tree, generated logs, generated images, private chart cells, generated
SENC caches, S-63 permits, oeSENC data, or private voyage/runtime data.

## Public Branch Boundaries

- OpenCPN proof scope: C++17-shaped renderer seam, neutral model, presentation
  compiler, scheduler policy, inspection provenance, fixtures, and CMake smoke
  evidence.
- Helm scope: consumer/product context only. Helm shares the semantic pipeline
  and should prioritize WebGPU browser artifacts, WebGL/MapLibre fallback, and
  server-raster fallback. Helm does not consume VSG as the client renderer.
- VSG scope: native proof backend only. VSG consumes neutral primitives and GPU
  cache manifests; it does not own chart-source parsing, S-52/S-101 rules,
  quilting, scheduler policy, cache epochs, or safety semantics.
- Support-script scope: scripts may build and summarize evidence, but renderer
  runtime implementation remains C++/CMake/OpenCPN-native.

## Explicit Exclusions

- ADAPT-5 / Metal compatibility: PR #22 is still open and dirty at PUB-2 time.
  Do not present Metal compatibility notes as landed evidence until that PR is
  rebased and merged, or until the RFC explicitly excludes Metal compatibility
  from the proof package.
- Full WebGPU renderer implementation: WebGPU is Helm's preferred client target,
  but this branch defines the server artifact contract and gap list only.
- Full S-52 parity, ECDIS certification, production safety approval, and
  primary-navigation readiness.
- Standalone renderer repository extraction. The current rule remains
  branch-first until OpenCPN and Helm both consume the shared model with
  accepted golden evidence.
- Whole-repo Helm publication. PUB-3 must curate the Helm/WebGPU explanation
  separately instead of publishing Helm planning history as Vulkan evidence.

## RFC Handoff

The RFC package starts at `docs/RFC_RENDER_CORE_POC.md` and builds the OpenCPN
side from this note, the acceptance rubric, the maintainer response matrix, the
stakeholder demo, and the hygiene checklist. The community ask should be
architecture review of the seam and evidence, not approval of a wholesale
renderer replacement.
