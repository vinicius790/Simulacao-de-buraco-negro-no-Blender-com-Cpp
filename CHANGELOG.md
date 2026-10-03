# Changelog

## 0.8.0 — Relativistic optics, CPU reference renderer, GPU scientific mode, Blender render bridge

The default render path is unchanged: root `geodesic.comp` (`legacyEulerStep`,
`SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 steps, `ESCAPE_R = 1e30`) is
still what `BlackHole3D` runs with no flags. Everything below is additive.

### Adversarial review fixes (all with regression tests)

- **Physics**: camera rays now seeded in the static observer's orthonormal frame (shadow edge matches Synge to 6 digits from 1.3 rs to 60 rs; was ~10 % too large); disk redshift measured by the camera at finite radius (`g_cam = g_∞/√f`); Synge branch inside the photon sphere; Kerr efficiency finite at a*=1 and |a*|-symmetric; RK45 rejects non-finite states; Page–Thorne asymptote documented correctly.
- **Renderer**: on-plane tolerance absorbs float32 elevations (Blender π/2), Y-up kept near the poles, black-hole marker recognised for any mass, starfield looked up with the lensed asymptotic direction, per-render blackbody normalisation (no thread-local cache), object alpha, step growth beyond the disk, strict CLI parsing with messages, JSON-escaped summary, thread-safe CRC, strict PPM reader.
- **GPU / BlackHole3D**: legacy no-flag render byte-identical to the original again (tan 30° rounded like the historical constant fold) and guarded by `legacy_golden`; scene FOV/target honoured; `"objects": []` removes defaults; spec-conformant capture barrier; `--help` anywhere; validation before any GL work; new `--blackbody`, `--exposure`, `--spin-sign`, `--max-steps`, `--mdot-edd`.
- **Blender add-on**: Blender **5.0** support (`compat.py`: slotted-action channel bags, `use_nodes` only before 5.0) — validated on 4.0.2 and 5.0.1; C++ frames imported as Non-Color under Raw (exact pixels) and re-read on re-render; render bridge always drives BH_OrbitCamera and syncs every frame with the animated camera; startup Cube/Light hidden; seamless turntable/spiral/spin loops; Blender 4.2+ manifest validation; ZIP_STORED for zlib-independent zips.
- **Tools / tests / CI**: `tools/frames_to_gif.py` (stdlib GIF with Bayer dithering), `--azimuth-turns` / `--elevation-end` sweeps, tests `render_cli_contract`, `tools_selftest`, `legacy_golden`, `bh3d_cli`; Windows/macOS CI job; 15 CTest tests.

### Scientific library (`bh_scientific`, `include/black_hole/`, `src/scientific/`)

- **New `orbits.hpp`:** circular timelike Schwarzschild orbits — Ω = √(M/r³),
  u^t = 1/√(1 − 3M/r), specific Ẽ and L̃, local speed √(M/(r − 2M)) (0.5 c at
  the ISCO), thin-disk efficiency η = 1 − √(8/9) ≈ 5.72 %.
- **New `redshift.hpp`:** g = ν_obs/ν_emit = 1 / (u^t (1 + Ω·L_axis/E)) for disk
  matter, backward-traced ray conventions documented; static g = √(1 − rs/r);
  I_obs = g⁴ I_emit; T_obs = g · T_emit.
- **New `disk_emission.hpp`:** Page–Thorne (1974) zero-torque thin-disk flux in
  closed form F = 3GMṀ/(8πr³) · R(x), x = √(r/M); Eddington luminosity and
  accretion rate; blackbody → linear sRGB (Kim et al. 2002 Planckian locus);
  Reinhard tonemap. No radiative transfer.
- **New `hit_testing.hpp`:** continuous segment–plane (interpolated crossing) and
  segment–sphere tests, replacing point sampling in the new paths.
- **New `escape.hpp`:** b_c = (3√3/2) rs ≈ 2.598 rs; proof-based escape (outgoing,
  beyond the photon sphere, beyond the scene bound) replaces `ESCAPE_R = 1e30`
  in the new paths; shadow angular radius sin α = b_c √f(r_o) / r_o.
- **New `integrator_rk45.hpp/.cpp`:** adaptive Dormand–Prince 5(4).
- **New `planar_geodesic.hpp/.cpp`:** pole-safe integration in each ray's orbital
  plane (θ ≡ π/2), exact by spherical symmetry.
- **New `kerr_analytic.hpp`:** Bardeen–Press–Teukolsky ISCO, horizons, ergosurface,
  photon orbits, efficiency — **analytic radii only, Kerr is not simulated**.
- **New `cpu_renderer.hpp/.cpp`:** deterministic multithreaded headless renderer;
  colour modes `legacy` (vec3(1, r, 0.2) · r), `relativistic` (legacy palette ×
  Doppler/gravitational redshift, g⁴ beaming, Reinhard) and `blackbody`
  (Page–Thorne T_eff shifted by g, zero emission inside the ISCO); integrators
  `rk4` (geometric step) / `rk45`; supersampling; optional procedural starfield;
  camera on the disk plane handled (first segment is not a crossing).
- **New `image_io.hpp/.cpp`:** dependency-free PNG (stored deflate), BMP and PPM
  writers with CRC32 / Adler32.
- **Changed `weak_field.hpp`:** added 2nd-order deflection 2 rs/b + (15π/16)(rs/b)².
- **Changed `integrator_rk4.cpp`:** `geodesic_rhs` freezes the acceleration when
  f ≤ 0 (inside the horizon) so overshooting RK stages no longer produce NaN.

### Tools

- **New `tools/bh_render_cpu.cpp`:** `bh_render_cpu [--scene] --out [--width]
  [--height] [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]
  [--azimuth] [--elevation] [--radius-rs] [--fov-y-deg] [--frames]
  [--azimuth-turns] [--elevation-end] [--threads] [--supersample] [--exposure]
  [--gamma] [--mdot-edd] [--spin-sign ±1] [--stars] [--max-steps] [--quiet]
  [--help]`; `--frames N` writes `stem_0000.png…` sweeping the azimuth by
  2π·T (`--azimuth-turns T`, default 1; the last frame does not repeat the
  first) and optionally the elevation up to `--elevation-end`; prints one JSON
  summary line; invalid values exit 2 naming the flag; `--spin-sign` accepts
  only ±1.
- **New `tools/image_diff.py`:** stdlib-only PNG (any filter, 8-bit RGB/RGBA) /
  PPM / BMP comparer — mean abs error, max error, bad-pixel fraction, PPM heat
  map, `--max-bad-fraction` gate.
- **New `tools/frames_to_gif.py`:** stdlib-only animated GIF89a writer for a
  frame sequence (`--fps`, `--pingpong`); used for
  `docs/images/showcase_elevation_sweep.gif`.

### OpenGL application (`black_hole.cpp`, `shaders/`)

- **CLI (additive):** `--scene file.json` (loads `black_hole.scene_params/v1`:
  mass / rs, camera incl. FOV and target, disk factors, objects — `"objects": []`
  means none — and Gravity), `--scientific`, `--relativistic`,
  `--capture out.png` (render one frame, read back the 200×150 compute texture
  flipped to on-screen orientation, save, exit), `--help` (exit 0 anywhere on
  the command line). Unknown flags, missing values and unsupported capture
  extensions exit 2 before any window is created. No flags = exact historical
  baseline (byte-identical golden capture, see `legacy_golden`).
- **Engine** is now constructed in `main()` after CLI parsing so the compute
  program matches the selected mode.
- **`SciParams` UBO** (binding 4, 32 bytes, `static_assert` on the C++ side).
- **`shaders/geodesic_scientific.comp` rewritten and wired:** planar RK4 with the
  corrected `r·f` term, continuous hits, proof-based escape, optional Doppler
  shading (`--relativistic`); rs = 1 units.
- Fixed 4 unused-parameter warnings: the build is warning-free with
  `-Wall -Wextra -Wpedantic`.

### Build, tests and CI

- **CMake:** project version 0.8.0; `include(CTest)` moved to the top; `Threads`;
  new targets `bh_render_cpu` and `test_cpu_render`; the scientific shader is
  copied next to `BlackHole3D` and validated by glslangValidator
  (`validate_shaders`).
- **Tests (14 in the full tree, 12 without OpenGL, all passing):**
  `source_invariants`, `style_contract`, `blender_addon_compile`,
  `blender_addon_parity`, **new** `tools_selftest` (`image_diff` PNG filters
  0–4 / PPM / BMP readers, `frames_to_gif` LZW / GIF89a), **new** `shader_contract` (mirrors identical, bindings, `SciParams` GLSL ↔ C++
  field by field), **extended** `scientific_ref` (orbits, redshift, Page–Thorne
  closed form vs numeric quadrature within 2e-3, hit tests, capture/escape around
  b_c for b = 2.0 / 2.5 / 2.7 / 3.2 rs, numeric weak-field deflection, RK45 vs RK4,
  planar vs 3-D chart, pole safety, Kerr limits), **new** `cpu_render`
  (structure, amber model, mirror symmetry, thread-count determinism, Doppler
  asymmetry + spin flip, blackbody, near-pole / on-axis camera, rendered shadow
  area vs analytic b_c within 8 %, PNG/CRC/Adler), `render_cli_smoke`, **new**
  `render_cli_contract` (every `bh_render_cpu` flag, invalid values, JSON
  escaping, sequence naming, `--help` lists every flag),
  `render_cli_rejects_bad_spin`, `render_cli_relativistic_sequence`, **new**
  `gpu_cpu_agreement` (`BlackHole3D --capture` under `xvfb-run` / Mesa llvmpipe vs
  `bh_render_cpu`, 6 scenes × 2 modes incl. scene FOV/target, half mass and
  `"objects": []`; SKIP code 77 without GL), **new** `legacy_golden`
  (`BlackHole3D` with no flags vs `docs/images/legacy_default_gpu.png`, exact on
  the driver recorded in `legacy_default_gpu.json`; `--scene` with the documented
  defaults == no flags; SKIP code 77 without GL).
- **Root `Makefile` now tracked** (un-ignored in `.gitignore`): `configure`,
  `build`, `test`, `configure-gl`, `build-gl`, `test-gl`, `test-all`, `shaders`,
  `render`, `package-addon`, `blender-scene`, `style-check`, `clean`.
- **CI (`.github/workflows/ci.yml`), 4 jobs:** `scientific-headless` (CTest;
  addon zips must be up to date; uploads CPU renders), `scientific-portable`
  (scientific tree + CTest on Windows/MSVC and macOS/Apple Clang, no OpenGL),
  `linux-build` (Mesa llvmpipe + Xvfb → real OpenGL 4.5 so `gpu_cpu_agreement`
  and `legacy_golden` run;
  `validate_shaders`; uploads GPU captures), `blender-headless` (apt Blender 4.0.x
  builds the `.blend` with `build_scene_headless.py`).

### Blender addon 0.8.0 (`blender/addons/black_hole_bridge/`)

- **New `guides.py`:** guide rings — photon sphere 1.5 rs, ISCO 3 rs, critical
  impact parameter 2.598 rs.
- **New `render_bridge.py`:** runs `bh_render_cpu` (modes `legacy` /
  `relativistic` / `blackbody`, RK4 / RK45, optional stars) once per frame with
  `--frames 1` and the `BH_OrbitCamera`'s actual animated pose, writing
  `stem_0000…`; imports the PNG/BMP or the image sequence (starting at the
  scene's `frame_start`) as camera background or as a camera-facing emissive
  plane, as Non-Color data under the Raw view (exact pixels).
- **Animation:** presets `TURNTABLE` / `ELEVATION_SWEEP` / `DOLLY` / `SPIRAL` with
  `LINEAR` / `SINE` easing; seamless loops (the azimuth of frame k is
  az₀ + 2π·k/N, like `bh_render_cpu --frames`, and the disk spin's last frame is
  turns·(N−1)/N); disk spin keyframes; optional turbulence (brightness only,
  off by default).
- **Fix — coordinates:** single layer `constants.cpp_to_blender`, C++ (x, y, z)
  Y-up → Blender (x, −z, y) Z-up, a proper +90° rotation about X (det +1), so the
  Blender camera frames exactly like the C++ camera (before: vertical disk and a
  90° roll). Pure helpers keep the C++ frame (parity tests).
- **Fix — disk thickness:** Solidify modifier with the locked DiskUBO thickness
  1e9 m ≈ 0.079 rs (before: zero thickness, invisible edge-on).
- **Fix — visibility:** grid built as quads + Wireframe modifier and guide rings
  as beveled curves (loose edges never appeared in Cycles/EEVEE renders).
- **Fix — colour parity:** disk material is pure emission (1, r, 0.2) · r — the
  OpenGL pixel `vec4(1, r, 0.2, r)` SRC_ALPHA-blended over black — using the
  actual inner/outer factors; new `disk_glow` (default 1.0 = parity); grid/guide
  emission 1.0 → grey 0.5 · 0.7 = 0.35 like `grid.frag` over black.
- **Fix — colour management / engine:** "Raw" view transform
  (`materials.setup_color_parity`; Blender 4.0's AgX washed the amber disk out)
  and Cycles selected by assignment (the old `"CYCLES" in dir(bpy.types)` check
  was always false).
- **Camera warning:** Build Full Scene reports a WARNING when the camera is inside
  the disk slab (`constants.camera_inside_disk`) — true for the locked C++
  default; elevation 1.25 recommended for clean renders.
- JSON extended (guides, render, animation); panel reorganised into sub-panels;
  `blender/scripts/package_addon.py` builds deterministic zips.
- Verified in real Blender 4.0.2 headless (Cycles). Ubuntu's Blender package lacks
  OpenImageDenoise: disable denoising for headless Cycles renders.

### Examples, docs and gallery

- **New `examples/scene_relativistic_showcase.json`:** disk 3–12 rs (from the
  ISCO), camera 18 rs, elevation 1.40 rad, FOV 38°, no extra objects.
- **New gallery `docs/images/`:** `legacy_default_gpu`, `legacy_el125_gpu`,
  `scientific_el125`, `relativistic_el125`, `blackbody_el100`,
  `showcase_relativistic`, `showcase_blackbody`, `blender_el125_cycles`,
  `showcase_elevation_sweep.gif`; `legacy_default_gpu.json` records the
  renderer / GL version of the golden capture.
- README rewritten (pt-BR + English summary: feature matrix, gallery, CLI
  reference, tests, measured physics, limitations); `ENTREGA.md`, `STATUS.md`
  (0.8.0 gates, P2 gaps re-evaluated), `CITATION.cff` (version/abstract).

### Measured (4 CPUs, Mesa llvmpipe OpenGL 4.5, Blender 4.0.2)

- GPU vs CPU: max error 1/255 (`--scientific` and `--relativistic`) at az 0 /
  el 1.25; other scenes: at most 4 boundary pixels of 30 000 out of tolerance
  (≤ 1.3e-4), mean abs error < 0.08/255. Legacy no-flag capture byte-identical
  to the golden image on llvmpipe.
- Null-constraint drift after 100 steps: Euler 8.8e-3 vs RK4 2.4e-9. RK45 reaches
  the fine-RK4 state with 120 steps vs 3000.
- Sgr A\*: L_Edd = 5.40e37 W; at 1 % Eddington Ṁ = 1.05e20 kg/s and Page–Thorne
  T_eff peak ≈ 86 700 K; shadow at the default camera 0.4836 rad = 27.7°.

### Documented, not changed

- **Legacy default-view artifact:** at elevation `M_PI / 2.0f`,
  cos(π/2f) ≈ −4.37e-8 puts the camera ≈ 2.8 km below the disk plane, and the
  default radius 4.997 rs is inside the 2.2–5.2 rs annulus, so every upward ray
  "hits the disk" on its first step and the top half of the initial frame is
  solid yellow until the user orbits. The legacy disk inner edge 2.2 rs is also
  inside the ISCO (3 rs). Both are part of the locked baseline.

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
