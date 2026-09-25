# **black**_**hole**

Black hole simulation project

Here is the black hole raw code, everything will be inside a src bin incase you want to copy the files

I'm writing this as I'm beginning this project (hopefully I complete it ;D) here is what I plan to do:

1. Ray-tracing : add ray tracing to the gravity simulation to simulate gravitational lensing

2. Accretion disk : simulate accreciate disk using the ray tracing + the halos

3. Spacetime curvature : demonstrate visually the "trapdoor in spacetime" that is black holes using spacetime grid

4. [optional] try to make it run realtime ;D

I hope it works :/

Edit: After completion of project -

## **Building Requirements:**

1. C++ Compiler supporting C++ 17 or newer

2. [Cmake](https://cmake.org/)

3. [Vcpkg](https://vcpkg.io/en/) *(only needed for the OpenGL apps)*

4. [Git](https://git-scm.com/)

## **Build Instructions:**

### A) Scientific library only (no OpenGL — CI / headless)

```bash
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON
cmake --build build/scientific
ctest --test-dir build/scientific --output-on-failure
./build/scientific/quickstart_scientific
```

Or use the Makefile:

```bash
make test
```

### B) Full OpenGL visualization (legacy baseline)

1. Clone the repository:
	-  `git clone https://github.com/kavan010/black_hole.git`
2. CD into the newly cloned directory
	- `cd ./black_hole` 
3. Install dependencies with Vcpkg
	- `vcpkg install`
4. Get the vcpkg cmake toolchain file path
	- `vcpkg integrate install`
	- This will output something like : `CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"`
5. Create a build directory
	- `mkdir build`
6. Configure project with CMake
	-  `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DBLACK_HOLE_BUILD_GL=ON`
	- Use the vcpkg cmake toolchain path from above
7. Build the project
	- `cmake --build build`
8. Run the program
	- The executables will be located in the build folder

### Alternative: Debian/Ubuntu apt workaround

If you don't want to use vcpkg, or you just need a quick way to install the native development packages on Debian/Ubuntu, install these packages and then run the normal CMake steps above:

```bash
sudo apt update
sudo apt install build-essential cmake \
	libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev
```

This provides the GLEW, GLFW, GLM and OpenGL development files so `find_package(...)` calls in `CMakeLists.txt` can locate the libraries. After installing, run the `cmake -B build -S . -DBLACK_HOLE_BUILD_GL=ON` and `cmake --build build` commands as shown in the Build Instructions.

## **How the code works:**
for 2D: simple, just run 2D_lensing.cpp with the nessesary dependencies installed.

for 3D: black_hole.cpp and geodesic.comp work together to run the simuation faster using GPU, essentially it sends over a UBO and geodesic.comp runs heavy calculations using that data.

should work with nessesary dependencies installed, however I have only run it on windows with my GPU so am not sure!

LMK if you would like an in-depth explanation of how the code works aswell :)

---

## Package map (0.6.x)

| Area | Location | Notes |
|------|----------|-------|
| Default visual baseline | `black_hole.cpp` + `geodesic.comp` | Historical **Euler** integrator |
| Scientific CPU library | `include/black_hole/`, `src/scientific/` | RK4 + diagnostics, no GL |
| Tests | `tests/` | Invariants + `test_scientific_ref` |
| Docs | `docs/INDEX.md` | Full map (PT/EN) |
| Blender bridge | `blender/` | Optional frontend only |
| Legacy archive | `Gravity_Sim/` | Kept; not in root CMake |

Kerr / spin is **not** implemented. Do not treat GPU timings or experimental measurements as part of this tree unless you add and validate them yourself.

## Engineering baseline upgrade

This package includes a conservative engineering pass that keeps the historical simulation path as the default baseline while removing avoidable driver/CPU work and adding regression infrastructure.

Important files:

- `docs/BASELINE.md` — exact baseline contract and original hashes;
- `docs/ENGINEERING_UPGRADE.md` — implemented changes and intentionally deferred behavior changes;
- `docs/SCIENTIFIC_MODE_SPEC.md` — specification for a future corrected scientific mode;
- `docs/MODO_CIENTIFICO.md` — how to use the new scientific library;
- `tests/validate_source_invariants.py` — guards the historical render/physics constants and critical OpenGL corrections.

Run the source regression guard directly:

```bash
python3 tests/validate_source_invariants.py --root .
```

Or through CTest after configuring the project:

```bash
cmake --preset scientific   # or: make configure && make test
```

If `glslangValidator` is installed, CMake exposes an additional offline shader check (GL build only):

```bash
cmake --build build/default --target validate_shaders
```

The archive does **not** require Blender, CUDA, Vulkan or an AI/agent runtime for the default simulation. Blender under `blender/` is an optional parameter/export frontend; physics remains in C++.
