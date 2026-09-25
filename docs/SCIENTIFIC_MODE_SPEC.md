# Scientific Mode Specification (Not Enabled by Default)

This document defines the next scientific phase without changing the legacy baseline.

## Why it is separate

Several corrections would alter trajectories and rendered pixels. They must therefore live behind a distinct mode rather than be silently substituted into the current shader.

## Required components

1. **Independent double-precision CPU reference**
   - Schwarzschild metric in a clearly documented coordinate system.
   - Deterministic ray initial conditions.
   - RK4 or an adaptive embedded Runge-Kutta method.
   - Conservation diagnostics for null condition, energy and angular momentum.

2. **Corrected GPU integrator**
   - separate shader/program from `geodesic.comp`;
   - no reuse of the historical `legacyEulerStep` name/path;
   - explicit error/tolerance strategy if adaptive stepping is introduced.

3. **Coordinate robustness**
   - clamp inverse-trigonometric inputs defensively;
   - avoid or explicitly handle spherical-coordinate pole singularities;
   - define horizon/near-horizon behavior.

4. **Continuous hit testing**
   - segment-plane disk crossing;
   - segment-sphere object/horizon tests where appropriate;
   - no point-sampling-only assumptions for thin geometry.

5. **Normalized units**
   - strongly consider geometric/nondimensional units to keep working values near order one while retaining a mapping to SI units.

6. **Safe escape criterion**
   - replace the current effectively unreachable radius only after deriving a criterion that proves a ray cannot return to visible scene geometry.

## Validation gates

A scientific mode is not complete until it has tests for:

- radial null trajectories;
- weak-field deflection;
- near-critical capture/escape behavior;
- null constraint drift;
- conserved quantity drift;
- pole-adjacent camera/ray states;
- CPU-vs-GPU trajectory agreement with documented tolerances.

## Future optional capabilities

After the corrected Schwarzschild path is validated:

- Kerr metric / spin;
- relativistic accretion-disk emission and Doppler/gravitational shifts;
- celestial-sphere/HDRI background for lensing visualization;
- higher-resolution or adaptive rendering;
- Blender adapter as a separate frontend, not as the owner of the physics core.
