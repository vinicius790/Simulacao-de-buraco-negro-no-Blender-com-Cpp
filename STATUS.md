# STATUS — 0.8.0 verification (relativistic optics, CPU reference, GPU scientific mode, Blender render bridge)

**Time zone:** America/Sao_Paulo (BRT). **Branch:** `claude/sharp-cerf-oqkp6n`. **Previous freeze:** 0.6.0 (scientific foundation) → 0.7.0-grand (scene I/O + style bible).

**Environment of record:** Linux container, 4 CPUs, Mesa **llvmpipe** OpenGL 4.5 (software rasteriser) via `xvfb-run`, Blender 4.0.2 (`/usr/bin/blender`), GCC with `-Wall -Wextra -Wpedantic` (warning-free). No hardware GPU was available: every GPU statement below refers to llvmpipe only.

---

## 0.8.0 verification gates

| Gate | Result |
|------|--------|
| `python3 tests/validate_source_invariants.py --root .` | **PASS** — root `geodesic.comp` unchanged (`legacyEulerStep` with 1 RHS evaluation, `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 steps, `ESCAPE_R = 1e30`), 800×600 / 200×150, disk 2.2–5.2 rs |
| `BlackHole3D` with no flags | **Default = historical baseline** (legacy compute program, legacy constants); the 0.8.0 CLI is additive and the engine is built after CLI parsing |
| CTest, full tree (`build/gl`, `BLACK_HOLE_BUILD_GL=ON`) | **PASS 15/15** — `bh3d_cli`, `source_invariants`, `style_contract`, `blender_addon_compile`, `blender_addon_parity`, `tools_selftest`, `shader_contract`, `scientific_ref`, `cpu_render`, `render_cli_smoke`, `render_cli_contract`, `render_cli_rejects_bad_spin`, `render_cli_relativistic_sequence`, `gpu_cpu_agreement`, `legacy_golden` |
| CTest, scientific tree (`build/scientific`, no OpenGL) | **PASS 12/12** — same list minus `gpu_cpu_agreement`, `legacy_golden` and `bh3d_cli` (only registered when GL is built) |
| `legacy_golden`: `BlackHole3D` with no flags vs `docs/images/legacy_default_gpu.png` | **byte-identical** on the recorded driver (llvmpipe, Mesa 25.2.8, `docs/images/legacy_default_gpu.json`); `--scene examples/scene_params_example.json` gives exactly the same frame as no flags |
| Build warnings (`-Wall -Wextra -Wpedantic`) | **0** (4 unused-parameter warnings in `black_hole.cpp` fixed) |
| `validate_shaders` (glslangValidator: `grid.vert`, `grid.frag`, `geodesic.comp`, `shaders/geodesic_scientific.comp`) | **PASS** |
| `shader_contract`: mirrors, UBO bindings, `SciParams` GLSL ↔ C++ (binding 4, 32 bytes, `static_assert`) | **PASS** |
| **GPU ↔ CPU agreement**, pose az 0 / el 1.25 (`gpu_cpu_agreement`, 200×150) | `--scientific`: max error **1/255**, mean 0.058/255 · `--relativistic`: max error **1/255**, mean 0.00001/255 (one channel of one pixel; byte-identical in the run recorded before the review fixes) |
| GPU ↔ CPU agreement, the other 5 scenes (az 0.7 / el 0.60 / 8e10 m; az 2.1 / el 2.00; FOV 40° with off-origin target; half mass; `"objects": []` with a 3–12 rs disk) | at most 2 boundary pixels of 30 000 out of tolerance in this run (bad fraction ≤ **6.7e-5**; ≤ 4 pixels / 1.3e-4 in earlier runs; gate 2e-3); mean abs error **< 0.08/255** (gate 0.5) — per-scene table in `docs/VALIDATION_STATUS.md` |
| Null-constraint drift after 100 steps | Euler **8.8e-3** vs RK4 **2.4e-9** |
| Weak-field deflection, b = 50 rs | numeric (planar RK45) **0.0412157 rad** vs 1st order 0.04 vs 2nd order 0.0411781 → numeric within ≈ 0.09 % of 2nd order (test gate 1 %) |
| RK45 vs fine RK4 | same end state with **120 steps vs 3000** |
| Capture / escape around b_c = 2.598 rs | b = 2.0, 2.5 rs captured; b = 2.7, 3.2 rs escaped — as predicted |
| Page–Thorne closed form vs numeric quadrature | agree within **2e-3**; F(ISCO) = 0 |
| Rendered shadow area vs analytic b_c (`cpu_render`) | within **8 %** |
| `bh_render_cpu` determinism | bit-identical for any `--threads` |
| `bh_render_cpu` 400×300 `legacy`, 4 threads | ≈ 0.5–1.2 s depending on pose and machine load; mean ≈ 77–152 RK4 steps per ray |
| Sgr A\* numbers (M = 8.54e36 kg) | L_Edd = 5.40e37 W; at 1 % Eddington Ṁ = 1.05e20 kg/s; Page–Thorne T_eff peak ≈ 86 700 K at r ≈ 4.78 rs (≈ 9.55 M); shadow at default camera (4.997 rs) = 0.4836 rad = 27.7°; v_loc = 0.645 c at 2.2 rs (unstable), 0.5 c at ISCO, 0.345 c at 5.2 rs |
| Kerr (analytic only), a\* = 0.998 prograde | ISCO 1.237 M, r₊ 1.063 M, η 32.1 % — limits a\* = 0, ±1 checked in `scientific_ref` |
| Blender 4.0.2 headless (`build_scene_headless.py`, Cycles) | **PASS** — scene builds and renders (`docs/images/blender_el125_cycles.png`): amber parity palette, warped grey grid, guide rings, black horizon |
| Blender add-on on Blender **4.0.2, 5.0.1** (full operator sweep, Cycles render, save, re-register) | **PASS** — 0 failures, 0 deprecation warnings; Blender 5 removed `Action.fcurves`, handled by `compat.py` (slotted-action channel bags) |
| Windows build (MinGW cross-compile, run under Wine) | **PASS** — 0 warnings; `test_scientific_ref` + `test_cpu_render` pass; renders byte-identical to Linux |
| Blender addon zips | deterministic (`package_addon.py`); CI fails if out of date |

### Discovery (documented, **not** changed)

At the locked default elevation `M_PI / 2.0f`, `cos(π/2f) ≈ −4.37e-8`, so the camera sits **≈ 2.8 km below** the disk plane, and the default camera radius 6.34194e10 m ≈ **4.997 rs** lies **inside** the 2.2–5.2 rs annulus. With the legacy point-sampled disk test, every upward ray "crosses the disk" on its first step: the top half of the initial frame renders solid yellow until the user orbits. Verified on llvmpipe with `BlackHole3D --capture` (`docs/images/legacy_default_gpu.png`). The scientific GPU mode and the CPU renderer handle a camera on the disk plane correctly (the first segment is not counted as a crossing). The legacy disk inner edge 2.2 rs is also inside the ISCO (3 rs). The Blender addon warns when the camera is inside the disk slab and recommends elevation 1.25.

---

## Delivered in 0.8.0

- **`bh_scientific`** — `orbits.hpp`, `redshift.hpp`, `disk_emission.hpp`, `hit_testing.hpp`, `escape.hpp`, `integrator_rk45.*`, `planar_geodesic.*`, `kerr_analytic.hpp` (analytic only), `cpu_renderer.*`, `image_io.*`; 2nd-order weak-field deflection; RK4 RHS freezes acceleration inside the horizon (no NaN on overshooting stages).
- **Tools** — `bh_render_cpu` (CLI renderer, JSON summary line, frame sequences with `--azimuth-turns` / `--elevation-end`), `tools/image_diff.py` (stdlib PNG/PPM/BMP comparer with heat map and `--max-bad-fraction` gate), `tools/frames_to_gif.py` (stdlib animated GIF from a frame sequence).
- **`BlackHole3D`** — `--scene`, `--scientific`, `--relativistic`, `--capture`, `--help`; no flags = exact baseline.
- **`shaders/geodesic_scientific.comp`** — rewritten and wired: planar RK4 with corrected `r·f` term, continuous hits, proof-based escape, optional Doppler shading.
- **Tests** — new `shader_contract`, `tools_selftest`, `cpu_render`, `render_cli_contract` and other CLI tests, `gpu_cpu_agreement`, `legacy_golden`; `scientific_ref` extended.
- **Blender addon 0.8.0** — C++ Y-up → Blender Z-up coordinate layer, disk thickness, visible grid/rings in Cycles/EEVEE, exact colour parity under the "Raw" view transform, guide rings, render bridge, animation presets, deterministic zips.
- **Engineering** — tracked root `Makefile`, 4-job CI (scientific-headless, scientific-portable on Windows/macOS, linux-build with llvmpipe, blender-headless), example `examples/scene_relativistic_showcase.json`, gallery in `docs/images/`.

---

## Remaining gaps (P2) — re-evaluated for 0.8.0

| # | Gap (from 0.6.0) | 0.8.0 state |
|---|------------------|-------------|
| 1 | Blender grand frontend | ✔ **DONE** — render bridge (`bh_render_cpu` → camera background / emissive plane), guide rings, animation presets, colour parity, JSON round-trip. Blender itself still does no lensing (by design). |
| 2 | Wire scientific GPU | ✔ **DONE** behind explicit flags `--scientific` / `--relativistic`; CPU↔GPU tolerance enforced by `gpu_cpu_agreement`. |
| 3 | Continuous hit-testing | ✔ **DONE in the new paths** (scientific shader, CPU renderer). Legacy point sampling remains in the baseline by contract. |
| 4 | Pole-safe spherical coordinates | ✔ **DONE** via the planar (orbital-plane) chart, exact by spherical symmetry; legacy shader keeps its global chart. |
| 5 | Escape criterion | ✔ **DONE (new paths only)** — derived proof-based test (outgoing, beyond photon sphere, beyond scene bound) replaces `ESCAPE_R = 1e30` there. |
| 6 | Relativistic disk appearance | ✔ **DONE** — Doppler + gravitational redshift, g⁴ beaming, Page–Thorne blackbody (CPU `relativistic` / `blackbody`; GPU `--relativistic` Doppler shading). |
| 7 | Kerr | ✘ **Still analytic-only** (`kerr_analytic.hpp`); no Kerr metric, geodesics or frame dragging. Do not claim. |
| 8 | Golden framebuffer regression | ✔ **DONE on Mesa llvmpipe** via GPU↔CPU agreement (6 scenes × 2 modes) and the exact legacy golden image (`legacy_golden`, 200×150 compute texture). Not yet run on hardware GPU drivers. |

### What remains

1. **Kerr ray tracing** — Kerr metric geodesics (Carter constant / Boyer–Lindquist or Kerr–Schild), frame dragging, spin-dependent shadow; only then upgrade the docs.
2. **Hardware-GPU validation** — run `gpu_cpu_agreement` on NVIDIA / AMD / Intel drivers; record real timings (none are claimed today).
3. **Radiative transfer** — optically thin emission, absorption, spectra, polarisation, GRMHD inputs (out of scope today).
4. ~~Blackbody mode on the GPU~~ — ✔ **DONE**: `BlackHole3D --blackbody` (Page–Thorne, g-shifted) agrees with `bh_render_cpu --mode blackbody` to 1/255 in `gpu_cpu_agreement`.
5. **Optional opt-in fix for the legacy default-view artifact** (e.g. a flag that nudges the camera off the disk plane) — must never change the default baseline.
6. ~~Runtime scientific controls in `BlackHole3D`~~ — ✔ **DONE**: `--exposure`, `--spin-sign`, `--max-steps`, `--mdot-edd` (tested by `bh3d_cli`).
7. **Real LICENSE and authors** — still `TBD` / placeholders (`LICENSE-NOTES.md`, `CITATION.cff`).

---

## Non-negotiables for follow-on work

- Keep default animation/visual design: disk **2.2–5.2 rs**, Sag A\* scale, spacetime grid, orbital camera (radius 6.34194e10 m, azimuth 0, elevation π/2, FOV 60°), window 800×600, compute 200×150, disk colour `vec3(1, r_norm, 0.2)` with alpha `r_norm`, grid grey `vec4(0.5, 0.5, 0.5, 0.7)`.
- Do **not** rewrite `geodesic.comp` physics; `BlackHole3D` with no flags must stay the historical baseline. New physics goes behind explicit flags or into `bh_scientific` / `bh_render_cpu`.
- Do **not** claim Kerr simulation, radiative transfer, GRMHD, or GPU timings beyond what was measured (Mesa llvmpipe only).
- Re-run invariants + the full CTest suite (`make test`, and `make test-gl` where OpenGL is available) after edits.
