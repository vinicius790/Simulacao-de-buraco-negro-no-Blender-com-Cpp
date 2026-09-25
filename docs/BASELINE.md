# Legacy Baseline Contract

This repository keeps the historical render/physics path as the reference baseline. Engineering changes in the default path must preserve its observable behavior unless a change is explicitly classified as a correction that fixes undefined/incorrect API behavior.

## Original archive

- Source archive: `black_hole-main.rar`
- SHA-256: `5f283b7dc4531123d0f20b54caf5ee1dc1308b069a944e1b58449a6b3a2ee5ee`

## Original key-file hashes

```text
d5f39a36322ed110f8674e3eb4168ae1dce5ec8bf98386892b9a6d8ad75e6c52  black_hole.cpp
9924ebbcaa42a667a429eee73fd9d438843ace32718af5d0118ba5893e23d4fe  geodesic.comp
260053e384c4bcf6d19fbcf361d3687d725b63e1de2897d846372dc1f4dcb990  CMakeLists.txt
2eaba452e1fd9c747fcdcf043f06066682f6a9c8bb1b4b58994488af4043c3d7  2D_lensing.cpp
82d8ba198b26644b0171e8d9515172ef5ab4a23e81cf9d41847716b7d6f811c5  CPU-geodesic.cpp
a9891e8dff1f1f50f05255e1ea508c56b20952fcb72181c71e03f90da74e925b  ray_tracing.cpp
76fde24d01fe2d607ba97b25a82d2884c0abd03d40cf4329bf5b4787e8f85e21  README.md
37abd591e48e432daa747ea7983db2419e860a2c628dfb658f12c9a2e264ba73  vcpkg.json
```

## Locked default behavior

The static regression guard in `tests/validate_source_invariants.py` protects these historical defaults:

- window: 800x600;
- compute image: 200x150;
- `SagA_rs = 1.269e10` in the compute shader;
- `D_LAMBDA = 1e7`;
- 60,000 integration steps;
- `ESCAPE_R = 1e30`;
- disk radii `2.2 * r_s` and `5.2 * r_s`;
- legacy fullscreen `GL_TRIANGLE_STRIP` behavior;
- legacy alpha blending/compositing behavior;
- the single-stage Euler integrator historically mislabeled as RK4.

The test does **not** claim these choices are scientifically ideal. It exists to prevent a refactor from silently changing the current visual/physics baseline.

## Required validation for future changes

1. Run `python tests/validate_source_invariants.py --root .`.
2. Build both CMake targets.
3. Run the GLSL validation target when `glslangValidator` is available.
4. Capture deterministic reference frames on the same GPU/driver and compare the raw compute texture and final framebuffer.
5. For scientific-mode changes, compare trajectories against an independent double-precision CPU reference rather than requiring equality with the legacy render.
