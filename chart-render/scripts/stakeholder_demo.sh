#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: stakeholder_demo.sh <binary-dir> [output-dir]

Runs the QA-3 stakeholder demo evidence path from an already-built
chart-render CMake directory. The script does not start OpenCPN, Helm, or any
network listener; it only runs local smoke binaries and writes an evidence
summary.
USAGE
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 64
fi

binary_dir=$1
output_dir=${2:-"$binary_dir/stakeholder-demo"}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
chart_render_dir=$(cd -- "$script_dir/.." && pwd)
repo_root=$(cd -- "$chart_render_dir/.." && pwd)

if [[ ! -d "$binary_dir" ]]; then
  echo "Binary directory does not exist: $binary_dir" >&2
  exit 66
fi

mkdir -p "$output_dir"

summary_file="$output_dir/qa3-stakeholder-demo-summary.md"
log_file="$output_dir/qa3-stakeholder-demo.log"

branch=$(git -C "$repo_root" rev-parse --abbrev-ref HEAD 2>/dev/null || true)
head_sha=$(git -C "$repo_root" rev-parse HEAD 2>/dev/null || true)
generated_at=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

cat > "$summary_file" <<SUMMARY
# QA-3 Stakeholder Demo Evidence

Generated: $generated_at
Repository branch: ${branch:-unknown}
Repository head: ${head_sha:-unknown}

This evidence script runs the branch-local proof for the stakeholder demo:
chart normalization and presentation compile into the neutral nautical render
model; OpenCPN routes a validated model through its feature-flag adapter; Helm
tile policy is represented as adapter scheduler/offscreen target policy; feature
inspection preserves wrong-location provenance; golden regression and VSG cache
checks remain backend-neutral.

## Results

SUMMARY

cat > "$log_file" <<LOG
QA-3 stakeholder demo evidence
generated=$generated_at
branch=${branch:-unknown}
head=${head_sha:-unknown}

LOG

run_step() {
  local step_id=$1
  local title=$2
  local binary_name=$3
  local claim=$4
  local binary_path="$binary_dir/$binary_name"
  local step_log="$output_dir/$step_id-$binary_name.txt"

  if [[ ! -x "$binary_path" ]]; then
    {
      echo "Missing executable: $binary_path"
      echo "Build chart-render before running this demo."
    } > "$step_log"
    echo "- FAIL: $title" >> "$summary_file"
    echo "  - Binary: \`$binary_name\`" >> "$summary_file"
    echo "  - Evidence: \`$step_log\`" >> "$summary_file"
    echo "  - Claim: $claim" >> "$summary_file"
    cat "$step_log" >> "$log_file"
    exit 66
  fi

  {
    echo "== $step_id $title =="
    echo "binary=$binary_path"
    echo "claim=$claim"
    "$binary_path"
  } > "$step_log" 2>&1

  {
    echo "- PASS: $title"
    echo "  - Binary: \`$binary_name\`"
    echo "  - Evidence: \`$step_log\`"
    echo "  - Claim: $claim"
  } >> "$summary_file"

  cat "$step_log" >> "$log_file"
  echo >> "$log_file"
}

run_step \
  "01-presentation" \
  "Chart normalization and S-52 presentation compiler feed the neutral model" \
  "opencpn-s52-presentation-compiler-smoke" \
  "Chart-source and S-52/S-101 semantics stay before backend handoff."

run_step \
  "02-neutral-model" \
  "Neutral nautical render model is the shared semantic center" \
  "opencpn-neutral-model-smoke" \
  "OpenCPN, VSG, Helm WebGPU/browser artifacts, and future backend compatibility work consume neutral primitives."

run_step \
  "03-opencpn-adapter" \
  "OpenCPN interactive adapter routes a validated model under feature flag" \
  "opencpn-feature-flag-adapter-smoke" \
  "OpenCPN keeps wx canvas and swapchain ownership; legacy fallback remains."

run_step \
  "04-helm-headless-policy" \
  "Helm/offscreen tile policy is adapter scheduler policy" \
  "opencpn-viewport-tile-scheduler-smoke" \
  "Overscan, prefetch, cache epochs, and zoom blending stay out of VSG backend semantics."

run_step \
  "05-inspection" \
  "Feature inspection traces wrong-location bugs from source to backend asset" \
  "opencpn-chart1-debug-app-smoke" \
  "Stakeholders can inspect source feature, projection, command, cache, and final asset ids."

run_step \
  "06-regression" \
  "Golden regression command exercises Chart 1 and depth evidence" \
  "opencpn-golden-regression-smoke" \
  "Regression output is repeatable and reports pending baseline captures honestly."

run_step \
  "07-vsg-backend" \
  "VSG cache/backend remains draw/cache-only and neutral-model-fed" \
  "opencpn-vsg-gpu-cache-smoke" \
  "GPU assets are derived from neutral primitives without chart-source or scheduler semantics."

run_step \
  "08-production-golden-corpus" \
  "Production first-slice golden corpus fails on semantic and pixel drift" \
  "opencpn-production-golden-corpus-smoke" \
  "Redistributable fixture, portable package hash, neutral primitive hashes, GPU artifact metadata, golden image, inspection trace, and known limitations are gated together."

run_step \
  "09-production-performance-fixture" \
  "Production first-slice performance evidence measures the C++ render path" \
  "opencpn-production-performance-fixture-smoke" \
  "Converter/package load, presentation compile, GPU artifact compile/cache hit, VSG render, viewport scheduling, memory, disk, and repeat stability are measured or explicitly marked unavailable."

run_step \
  "10-upstream-production-slice" \
  "Tiny upstream production slice keeps legacy fallback and carries evidence" \
  "opencpn-upstream-production-slice-smoke" \
  "One bounded S-57 fixture is feature-flagged, golden-gated, inspection-traced, backend-rendered, and performance-measured without adding plugin/UI/navigation scope."

cat >> "$summary_file" <<'SUMMARY'

## Helm Private-Port Follow-Up

This OpenCPN-branch script does not start Helm or touch the live Helm `:8080`
screen. To complete the live demo, run the Helm REPO-4 route smoke in a private
Helm worktree/port with `HELM_CHART_RENDERER=vulkan` and the shared renderer
fixture or binary configured. The expected Helm evidence is:

- `/chart/{z}/{x}/{y}.png` returns `X-Helm-Renderer: vulkan`.
- Repeated renders are deterministic for the same chart inputs and view state.
- `If-None-Match` returns `304` with the Vulkan renderer ETag.
- Failures are explicit and no-store; legacy fallback only happens when requested.

## What This Demo Does Not Claim

- It is not full S-52 parity.
- It is not ECDIS certification or a safety approval.
- It is not a standalone repository extraction decision.
- It is not proof that MBTiles or PMTiles are the renderer hot path.
- It is not a Metal or WebGPU implementation.
SUMMARY

echo "QA-3 stakeholder demo evidence written to $output_dir"
echo "Summary: $summary_file"
