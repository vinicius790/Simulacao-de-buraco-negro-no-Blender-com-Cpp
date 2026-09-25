# Modo científico / Scientific mode

Ver também a especificação: [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md).

## PT

A biblioteca `bh_scientific` implementa a **fase CPU** do modo científico:

- headers em `include/black_hole/`
- fontes em `src/scientific/`
- testes em `tests/test_scientific_ref.cpp` (sem GLEW/GLFW)

### Build só científico

```bash
make configure   # ou cmake -DBLACK_HOLE_BUILD_GL=OFF
make build
make test-scientific
./build/scientific/quickstart_scientific
```

### API rápida

```cpp
#include "black_hole/units.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/weak_field.hpp"
#include "black_hole/integrator_rk4.hpp"

double rs = bh::units::sagittarius_a_rs_si();
double r_ph = bh::photon_sphere_radius(rs);   // 1.5 rs
double r_isco = bh::isco_radius(rs);          // 3 rs = 6 M
double alpha = bh::weak_field::deflection_angle(rs, /*b=*/100*rs);

bh::RayState ray = bh::make_ray_from_cartesian(x,y,z, dx,dy,dz, rs);
bh::rk4_step(ray, d_lambda, rs);
```

### GPU científico

`shaders/geodesic_scientific.comp` é um **stub**. Não é usado por `black_hole.cpp`.
Para ligar no futuro: flag explícita + validação CPU↔GPU (ver SCIENTIFIC_MODE_SPEC).

## EN

Scientific CPU reference is always buildable with `BLACK_HOLE_BUILD_GL=OFF`.
Default on-screen path remains legacy Euler. Kerr is out of scope until implemented and tested.
