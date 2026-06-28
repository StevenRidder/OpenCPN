# Golden Regression Smoke

`golden_regression.hpp` is the repeatable local/CI command for the Vulkan POC
acceptance chain. It currently validates:

- Chart 1 acceptance catalog coverage.
- Chart 1 command-stream conformance.
- Chart 1 baseline comparison metadata and tolerances.
- Pending OpenCPN baseline capture slots.
- Depth safety/performance smoke counters.

The runner does not fabricate PNG baselines. Until real OpenCPN captures exist,
pending baseline slots are reported as pending evidence rather than failed image
diffs. Once QA-2 grows real image assets, the same command can become strict
about pixel comparison.
