# Gravity_Sim legacy snapshot

This directory contains earlier experiments, binaries and CUDA/OpenGL prototypes from the project's development history. It is intentionally preserved for reference and is **not** part of the top-level CMake build.

The maintained baseline targets are defined by the repository-root `CMakeLists.txt`:

- `BlackHole2D` from `2D_lensing.cpp`;
- `BlackHole3D` from `black_hole.cpp` + `geodesic.comp`;
- `bh_scientific` / `test_scientific_ref` (headless scientific reference, no OpenGL).

Do not assume the files under `Gravity_Sim/src/` implement the same equations, rendering path or dependency model as the maintained root targets. Changes to this directory should be treated as legacy/prototype work unless it is deliberately promoted into the root architecture with tests.

## Why keep it?

Historical prototypes (including CUDA experiments and older Windows binaries under `bin/`) document how the visual baseline evolved. Deleting them would lose that trail. Prefer documenting limitations here over “cleaning” by removal.
