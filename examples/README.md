# Examples

| Example | Requires OpenGL | Description |
|---------|-----------------|-------------|
| `quickstart_scientific.cpp` | No | Prints Sag A* rs, photon sphere, ISCO, weak-field α |
| Root `BlackHole2D` / `BlackHole3D` | Yes | Legacy visual simulation |

Build scientific example (no GLEW/GLFW needed):

```bash
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF
cmake --build build/scientific --target quickstart_scientific
./build/scientific/quickstart_scientific
```

Or: `make build && ./build/scientific/quickstart_scientific`
