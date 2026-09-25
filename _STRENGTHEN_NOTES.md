# Strengthen notes — 0.6.0 scientific expansion

## What changed

- Introduced domain-modeled scientific library (`bh_scientific`) without rewriting
  the working OpenGL path (`laziness-protocol`).
- Split CMake so scientific tests run when GLEW/GLFW/GLM are absent
  (`BLACK_HOLE_BUILD_GL=OFF`).
- Added prove-it-works gates: invariants Python test + C++ assert binary + Makefile.
- Documented physics honesty: photon sphere / ISCO conventions, what is NOT modeled,
  Kerr explicitly unimplemented.
- Blender bridge is export/visualization only; no Blender dependency for C++ builds.
- Root `geodesic.comp` / `grid.*` remain canonical; `shaders/` holds mirrors + stub.

## What did NOT change

- Legacy constants and Euler integrator in `geodesic.comp`.
- Default disk factors 2.2 / 5.2 rs, resolution 800×600 / 200×150.
- `Gravity_Sim/` retained.
- No invented GPU timings or Kerr claims.

## Remaining P2 gaps

- Wire scientific GPU shader and CPU↔GPU trajectory agreement tests.
- Continuous hit-testing (segment–plane / segment–sphere).
- Pole-safe spherical handling / coordinate robustness.
- Scientifically derived escape criterion (replace `ESCAPE_R=1e30` only behind a flag).
- Relativistic disk emission / Doppler / gravitational redshift.
- Optional Kerr after Schwarzschild scientific path is fully validated.
- JSON scene loader into `black_hole.cpp` (export exists; import not hooked).
