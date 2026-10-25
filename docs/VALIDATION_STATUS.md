# Validation status

Last updated with package 0.6.0 scientific expansion.

## Passing by construction / local gates

| Gate | Command | Claim |
|------|---------|-------|
| Baseline source invariants | `python3 tests/validate_source_invariants.py --root .` | Legacy constants + Euler path + std140/barrier intact |
| Scientific unit tests | `ctest -R scientific_ref` (BUILD_GL OFF OK) | Photon sphere, weak-field 1/b, E/L, RK4 vs Euler null drift |
| Docs honesty | manual | Kerr not claimed; GPU scientific stubbed |

## Not yet validated

- Deterministic GPU framebuffer golden images across drivers.
- CPU↔GPU scientific trajectory agreement (GPU path not enabled).
- Continuous hit-testing correctness.
- Pole-adjacent numerical stability.
- Any Kerr metric quantities.

## How to re-run

```bash
make test
# or
cmake --preset scientific && cmake --build --preset scientific && ctest --preset scientific
```
