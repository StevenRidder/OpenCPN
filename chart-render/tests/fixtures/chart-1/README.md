# Chart 1 Fixture

This fixture is the first conformance target for the Vulkan renderer POC. It is
small by design: one area, one line, and one point symbol case. Later CHART
tasks should render the cases in `acceptance.catalog.json` through the shared
command stream and compare them against OpenCPN baselines.

Current acceptance cases:

- `chart1-area-depth`: `DEPARE` area fill via `cmd-depth-area`.
- `chart1-line-depth-contour`: `DEPCNT` line style via `cmd-depth-contour`.
- `chart1-point-buoy`: `BOYLAT` point symbol via `cmd-buoy`.

The fixture intentionally also contains text, sounding, and coverage commands
so traceability and raster policies remain visible, but those are not required
for CHART-1 acceptance.

`chart1_conformance.hpp` builds a conformance scene by filtering this fixture
to the accepted point, line, and area commands and validating command type,
resource type, S-52 rule metadata, provenance, and conversion trace refs.
