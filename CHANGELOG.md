# Changelog

## 0.7.0-grand — Scene I/O, style bible, scientific density (baseline locked)

- **Scene JSON I/O (additive):** `include/black_hole/scene_params.hpp` +
  `src/scientific/scene_params.cpp` — hand-rolled parser for restricted schema
  `black_hole.scene_params/v1` (camera, disk factors, gravity, objects).
- **CLI:** `tools/bh_scene_dump.cpp` linked to `bh_scientific` (defaults / load / save).
- **Examples:** `examples/scene_params_example.json` synced with `blender/examples/`.
- **Scientific shader body:** `shaders/geodesic_scientific.comp` — real RK4 loop
  (geometric `rs=1`), clearly **not** wired to `BlackHole3D`.
- **Library:** `disk_model.hpp` (geometric annulus 2.2–5.2; Novikov–Thorne NOT
  implemented), `camera_model.hpp` (orbit + grid warp parity).
- **Tests:** expanded `test_scientific_ref.cpp` (camera orbit + disk factors +
  scene roundtrip); new `tests/test_style_contract.py`.
- **Docs (PT):** `ESTILO_VISUAL_SIMULACAO.md`, `ANIMACAO_PIPELINE.md`; expanded
  `FISICA_SCHWARZSCHILD.md` and `ARQUITETURA.md`.
- **Makefile:** `style-check`, `test-all`.
- **CMake:** `bh_scene_dump`; glfw3/glm find fallbacks for distro packages;
  `style_contract` CTest.
- **Unchanged:** window 800×600, compute 200×150, disk 2.2–5.2 rs, `SagA_rs`,
  spacetime grid + fullscreen compose, `geodesic.comp` integrator math.

## 0.6.0 — Scientific foundation (baseline preserved)

- Added `bh_scientific` library under `include/black_hole/` + `src/scientific/`.
- Headless `tests/test_scientific_ref.cpp` and CTest `scientific_ref`.
- CMake `BLACK_HOLE_BUILD_GL`; Makefile convenience targets.
- Docs INDEX / FISICA / ARQUITETURA / Blender bridge / examples.
- Default render path remains historical Euler `geodesic.comp`.

## Engineering baseline upgrade (pre-0.6)

- Compute-image → texture-fetch barrier; std140 UBO layouts; texture reuse;
  cached uniforms; renamed `legacyEulerStep` without changing arithmetic;
  static invariants + optional shader validation.
