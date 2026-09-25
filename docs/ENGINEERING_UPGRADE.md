# Engineering Upgrade Notes

## Scope

This upgrade intentionally improves engineering quality around the existing simulation without replacing the historical numerical model. The default render path remains the legacy baseline.

## Implemented changes

### 1. Correct OpenGL memory visibility

The compute shader writes the output texture with `imageStore`, then the fullscreen pass reads the same texture through `texture()`. The barrier now includes both `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT` and `GL_TEXTURE_FETCH_BARRIER_BIT` so the following texture fetch is synchronized with prior shader image writes.

Classification: **CORRECTION**.

### 2. Explicit std140 layouts

CPU-side Camera, Disk and Objects UBO layouts are now centralized and size-checked. The shader `moving` flag uses an integer rather than relying on C++ `bool` representation, and the object mass array uses a `vec4` stride to match std140 array rules.

Classification: **CORRECTION / RESTRUCTURING**.

### 3. Persistent compute texture allocation

The 200x150 RGBA8 compute texture is no longer reallocated every frame. It is recreated only if the compute resolution actually changes.

Classification: **OPTIMIZATION**.

### 4. Static UBO upload avoidance

Disk data and object data are uploaded only when dirty. The camera UBO remains per-frame because it depends on camera state.

Classification: **OPTIMIZATION**.

### 5. Grid topology reuse

The grid index topology and VAO/EBO setup are created once. Grid vertex data is regenerated only when legacy gravity actually moves scene objects.

Classification: **OPTIMIZATION / RESTRUCTURING**.

### 6. Uniform-location caching

`viewProj` and `screenTexture` uniform locations are queried once after program creation instead of every frame.

Classification: **OPTIMIZATION**.

### 7. Equatorial-plane early rejection

The expensive radial length computation for disk intersection is skipped unless a segment first crosses the equatorial plane. The predicate itself is unchanged.

Classification: **OPTIMIZATION**.

### 8. Honest integrator naming

The historical `rk4Step` function performs one explicit Euler stage. It is renamed to `legacyEulerStep` without changing the numerical operations. This prevents future maintenance from treating the baseline as a real fourth-order Runge-Kutta implementation.

Classification: **RESTRUCTURING / PRECISION DOCUMENTATION**.

### 9. Tooling and regression infrastructure

Added:

- `CMakePresets.json`;
- compiler warning configuration;
- optional `validate_shaders` CMake target;
- CTest integration for static source invariants;
- `.clang-format`;
- `.clang-tidy`;
- GitHub Actions Linux build/static/GLSL validation workflow.

Classification: **MODERNIZATION / RESTRUCTURING**.

## Deliberately not changed

These are known issues or scientific limitations, but changing them in the default path would change the observable baseline:

- the single-stage Euler integrator;
- the 3D geodesic equations currently implemented;
- the initial null-geodesic energy expression;
- the historical angular-momentum expression;
- the 60,000-step budget;
- the `1e30` escape radius;
- the fullscreen quad topology;
- blending state inherited by the fullscreen composition pass;
- the frame-rate-dependent legacy object-gravity update.

They should be addressed in an optional scientific/corrected mode after deterministic regression capture exists.

## Skill/workflow principles used

The upgrade follows the useful parts of the referenced development-skill workflows:

- inspect the repository before choosing architecture;
- separate bug/correctness review from feature work;
- create regression guards before risky refactors;
- keep changes reviewable and localized;
- validate with independent tools where possible;
- do not add technology merely because it is newer;
- document known limitations instead of hiding them behind more abstraction.

Agent-SDK-specific scaffolding and OpenAI API integration were intentionally not added because this is a C++/OpenGL numerical-rendering project, not an agent application.
