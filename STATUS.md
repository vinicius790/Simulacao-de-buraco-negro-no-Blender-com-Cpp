# STATUS — scientific verification freeze (0.6.0)

**Time zone:** America/Sao_Paulo (BRT). Verification completed successfully; hand off for Blender/animation packaging.

## Proven green

| Gate | Result |
|------|--------|
| `python3 tests/validate_source_invariants.py --root .` | **PASS** — legacy Euler path, SagA_rs, disk 2.2/5.2 rs, resolutions untouched |
| `cmake -DBLACK_HOLE_BUILD_GL=OFF` + `bh_scientific` + `test_scientific_ref` | **PASS** (CTest 2/2) |
| Photon sphere = 1.5 rs; ISCO = 3 rs (= 6 M); weak-field α ∝ 1/b | **PASS** |
| Corrected null seed + Christoffel `r*f` RHS; RK4 \|gkk\| ≪ Euler | **PASS** (e.g. ~2e-9 vs ~9e-3) |
| `geodesic.comp` physics | **Unchanged** (default render still `legacyEulerStep`) |
| `Gravity_Sim/` | **Preserved** |
| Full GL configure + `BlackHole3D` link (when apt GLEW/GLFW/GLM present) | **Builds** (unused-param warnings only) |

## Delivered this phase (scientific lever)

- `include/black_hole/` + `src/scientific/` → lib `bh_scientific`
- `tests/test_scientific_ref.cpp`, Makefile, `BLACK_HOLE_BUILD_GL` option
- Docs: INDEX, FISICA_SCHWARZSCHILD, ARQUITETURA, ESTRUTURA, CHECKLIST, MODO_CIENTIFICO, …
- Stub `shaders/geodesic_scientific.comp` (not wired)
- Minimal Blender bridge scaffold under `blender/` (export JSON / grid mesh; **not** required to build C++)

## Remaining gaps (P2 — for other agents / later)

1. **Blender grand frontend** — deeper addon, animation timeline, import JSON into engine, preserve original disk/grid/orbit camera look.
2. **Wire scientific GPU** — finish `geodesic_scientific.comp`, explicit mode flag; CPU↔GPU trajectory tolerances.
3. **Continuous hit-testing** — segment–plane disk / segment–sphere horizon (point sampling remains baseline).
4. **Pole-safe spherical coords** — clamp / chart switch near θ∈{0,π}.
5. **Escape criterion** — replace legacy `ESCAPE_R=1e30` only behind a flag after derivation.
6. **Relativistic disk appearance** — Doppler / gravitational redshift / emission (visual baseline colors stay until then).
7. **Kerr** — not implemented; do not claim.
8. **Golden framebuffer regression** — deterministic GPU image compare across drivers.

## Non-negotiables for follow-on work

- Keep default animation/visual design: disk **2.2–5.2 rs**, Sag A scale, spacetime grid, orbital camera.
- Do **not** rewrite `geodesic.comp` physics.
- Re-run invariants + `test_scientific_ref` after edits.
