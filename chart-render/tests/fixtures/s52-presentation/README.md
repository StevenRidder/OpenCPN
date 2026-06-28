# S-52 Presentation Compiler Fixture

These fixtures describe the normalized-feature and display-state inputs used by
`opencpn-s52-presentation-compiler-smoke`. The smoke builds the same fixture in
C++ so the runtime compiler stays C++17-only.

Covered cases:

- `DEPARE` area fill with safety-depth classification;
- `DEPCNT` safety contour line;
- `BOYLAT` point symbol;
- `SOUNDG` sounding text;
- `LNDARE` object-name label;
- one display-category filtered object;
- one SCAMIN filtered object;
- provenance preservation for every emitted primitive.
