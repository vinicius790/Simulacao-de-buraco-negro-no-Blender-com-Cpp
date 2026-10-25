# Shaders layout

## Canonical baseline (do not break invariants)

The **source of truth** for the default render path remains the repository-root copies:

- `../geodesic.comp` — legacy Euler null-geodesic compute shader
- `../grid.vert` / `../grid.frag` — spacetime grid overlay

`tests/validate_source_invariants.py` reads those root files. Do not delete or
silently diverge them.

## This directory

Copies under `shaders/` are for future packaging / clearer layout. They are kept
in sync as convenience mirrors; root remains canonical for baseline tests.

## Scientific (optional, not default)

- `geodesic_scientific.comp` — stub / future RK4 scientific GPU path.

It is **not** loaded by `black_hole.cpp`. To switch later:

1. Implement and validate the CPU reference in `bh_scientific`.
2. Complete the scientific compute shader.
3. Add an explicit runtime/compile flag (e.g. `BH_SCIENTIFIC_MODE`) that loads
   `geodesic_scientific.comp` instead of `geodesic.comp`.
4. Keep `geodesic.comp` as the default so visual regression stays intact.
