# Modo científico / Scientific mode

Guia de uso dos caminhos científicos da versão **0.8.0**. Teoria: [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md)
(geodésicas, escape, sombra, deflexão, integradores, Kerr analítico) e
[DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md) (órbitas, redshift, beaming, Page–Thorne). Estado de cada item da
especificação: [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md).

> **O default não muda.** `BlackHole3D` sem flags roda o `geodesic.comp` legado (`legacyEulerStep`, constantes
> travadas), inclusive o artefato da vista inicial (metade de cima amarela; ver
> [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)).
> Kerr **não** é simulado.

## PT

### Três portas de entrada

| Entrada | O que é | Precisa de OpenGL? |
|---------|---------|--------------------|
| Biblioteca `bh_scientific` (`include/black_hole/`, `src/scientific/`) | Referência C++17 em `double`: geodésicas, escape, sombra, disco, renderer CPU, escrita de imagens | Não |
| `bh_render_cpu` (`tools/bh_render_cpu.cpp`) | Renderer headless, determinístico e multithread; PNG/BMP/PPM; sequências de quadros; uma linha JSON de resumo | Não |
| `BlackHole3D --scientific` / `--relativistic` | Gêmeo GPU: `shaders/geodesic_scientific.comp` (carta plana, RK4, hits contínuos, escape provado) | Sim (4.3+; Mesa llvmpipe serve) |

### Build

```bash
make configure && make build      # build/scientific, sem OpenGL (bh_scientific, bh_render_cpu, testes)
make test                         # CTest headless
make configure-gl && make build-gl  # build/gl: BlackHole2D/3D + tudo acima
make test-gl                      # inclui gpu_cpu_agreement (xvfb-run/Mesa se não houver display)
make shaders                      # glslangValidator nos shaders (árvore GL)
make render                       # PNGs de referência em build/renders/ (inclui a cena showcase)
```

Equivalente em CMake puro:

```bash
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON
cmake --build build/scientific
ctest --test-dir build/scientific --output-on-failure
./build/scientific/quickstart_scientific     # tour numérico de todos os módulos
python3 examples/quickstart_scientific.py    # mesmas fórmulas em Python puro (stdlib)
```

---

### `BlackHole3D` (GPU)

```text
BlackHole3D [--scene file.json] [--scientific] [--relativistic] [--capture out.png] [--help]
```

| Flag | Efeito |
|------|--------|
| *(nenhuma)* | Baseline histórico exato |
| `--scene file.json` | Carrega `black_hole.scene_params/v1`: câmera (raio, azimute, elevação, `fov_y_deg`, `target_m`), fatores do disco, objetos, `Gravity`. Sem `--scene`, a câmera é a do baseline (FOV 60°, alvo na origem) |
| `--scientific` | Usa `shaders/geodesic_scientific.comp`; unidades rs = 1; paleta legada (`colorMode 0`) |
| `--relativistic` | `--scientific` + Doppler/redshift gravitacional e beaming \(g^4\) no disco (`colorMode 1`) |
| `--capture out.png` | Renderiza um quadro, lê a textura de compute 200×150, compõe alpha sobre preto, desvira para a orientação da tela, salva (`.png`/`.bmp`/`.ppm`) e sai |

Rode a partir do diretório do binário (os shaders são lidos do diretório corrente; o CMake os copia para lá):

```bash
cd build/gl
./BlackHole3D --scientific
./BlackHole3D --scene ../../examples/scene_relativistic_showcase.json --relativistic
# headless (CI / container sem display)
xvfb-run -a -s "-screen 0 1024x768x24" ./BlackHole3D --relativistic --capture rel.png
```

O que o modo científico faz no host (`black_hole.cpp`):

- divide posição da câmera, raios de objetos e espessura do disco por `SceneParams::r_s_m` (rs = 1); o anel do disco
  vai direto como fatores (2.2–5.2 por default);
- liga o UBO `SciParams` (binding 4, 32 bytes, `static_assert` no C++, conferido campo a campo pelo teste
  `shader_contract`), com valores fixos no código:

| Campo | Valor |
|-------|-------|
| `colorMode` | 0 (`--scientific`) / 1 (`--relativistic`) |
| `maxSteps` | 4000 |
| `stepK`, `stepMin`, `stepMax` | 0.02, 0.005, 2.0 (\(d\lambda=\mathrm{clamp}(k\,r,\min,\max)\), em rs) |
| `sceneBound` | \(1.05\cdot\max(r_{\rm out},\,|c|+R)+0.5\) |
| `exposure` | 2.0 |
| `spinSign` | +1 |

Limitações atuais da GPU científica: sem modo corpo negro e sem estrelas, exposure/spin/orçamento de passos não
ajustáveis em runtime, resolução de compute 200×150, precisão `float`. No modo científico o host omite a esfera
marcadora do buraco negro (`bh::is_black_hole_marker`, mesmo predicado do `bh_render_cpu`): a sombra vem do teste
exato do horizonte.

Cena mínima (é o que `tests/test_gpu_cpu_agreement.py` gera):

```json
{
  "schema": "black_hole.scene_params/v1",
  "camera": {"radius_m": 6.34194e10, "azimuth_rad": 0.0, "elevation_rad": 1.25, "fov_y_deg": 60.0}
}
```

Elevação 1.25 tira a câmera do plano do disco e dá uma vista limpa do anel legado.

---

### `bh_render_cpu` (CPU)

Referência completa do renderer: [RENDER_CPU.md](RENDER_CPU.md).

```text
bh_render_cpu [--scene scene.json] --out image.png [--width W] [--height H]
              [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]
              [--azimuth RAD] [--elevation RAD] [--radius-rs X] [--fov-y-deg D]
              [--frames N] [--azimuth-turns T] [--elevation-end RAD]
              [--threads T] [--supersample S]
              [--exposure E] [--gamma G] [--mdot-edd F] [--spin-sign +1|-1]
              [--stars] [--max-steps N] [--quiet] [--help]
```

| Flag | Default | Notas |
|------|---------|-------|
| `--width`, `--height` | 200, 150 | resolução de compute do legado |
| `--mode` | `legacy` | `legacy` = \((1,r,0.2)\cdot r\); `relativistic` = paleta × \(g^4\) + Reinhard; `blackbody` = Page–Thorne (só CPU) |
| `--integrator` | `rk4` | `rk4` com passo geométrico; `rk45` Dormand–Prince (tolerância \(10^{-8}\)) |
| `--azimuth`, `--elevation`, `--radius-rs`, `--fov-y-deg` | da cena | sobrescrevem a câmera do JSON |
| `--frames N` | 1 | escreve `stem_0000.png …`; azimute varre `--azimuth-turns` voltas (default 1); `--elevation-end` interpola a elevação |
| `--threads` | 0 = todos os núcleos | resultado **bit a bit idêntico** para qualquer número de threads |
| `--supersample S` | 1 | grade regular S×S por pixel |
| `--exposure` | 2.0 | antes do Reinhard (`relativistic`, `blackbody`) |
| `--gamma` | 1.0 | 1.0 = linear, como a textura legada |
| `--mdot-edd` | 0.01 | \(\dot M\) em frações de Eddington (`blackbody`) |
| `--spin-sign` | +1 | só aceita +1 ou −1 (o CTest `render_cli_rejects_bad_spin` garante) |
| `--stars` | desligado | campo de estrelas procedural para raios que escapam, consultado na direção assintótica (lenteado) |
| `--max-steps` | 20000 | orçamento por raio |

Saída: uma linha JSON, por exemplo (400×300, elevação 1.25, neste container; `seconds` e `out` abreviados):

```json
{"frames":1,"width":400,"height":300,"mode":"legacy","integrator":"rk4","shadow_fraction":0.351867,
 "disk_fraction":0.5593,"object_fraction":0.00110833,"escaped_fraction":0.087725,"step_limit_rays":0,
 "mean_steps":111.189,"min_g":0.392026,"max_g":1.6214,"seconds":0.83,"out":"..."}
```

`min_g`/`max_g` são os extremos do fator de redshift \(g\) nos pixels do disco (calculados em todos os modos).
Tempo medido neste container (4 CPUs): 400×300 `legacy` com 4 threads ≈ 0.5–1.4 s, conforme a pose e a carga da
máquina. Passos médios por raio (RK4): ≈ 111 na elevação 1.25 e ≈ 184 na pose default; com `--integrator rk45`,
≈ 32 na elevação 1.25.

Exemplos:

```bash
B=build/scientific/bh_render_cpu
$B --out legacy_el125.png --width 640 --height 480 --elevation 1.25
$B --out rel.png --mode relativistic --elevation 1.25 --integrator rk45
$B --scene examples/scene_relativistic_showcase.json --mode blackbody --stars \
   --width 640 --height 360 --supersample 2 --out showcase_bb.png
$B --out turn.png --frames 120 --mode relativistic --elevation 1.25   # turn_0000.png … turn_0119.png
python3 tools/image_diff.py a.png b.png --max-bad-fraction 0.002       # comparador stdlib
```

| Paleta legada | Relativístico | Corpo negro |
|---|---|---|
| ![](images/scientific_el125.png) | ![](images/relativistic_el125.png) | ![](images/blackbody_el100.png) |

O addon Blender usa o mesmo binário pela "render bridge" (`render_bridge.py`): roda `bh_render_cpu` e importa o
PNG (ou a sequência) como fundo de câmera ou plano emissivo. O Blender em si não faz lensing.

---

### API rápida (C++)

Todos os headers ficam em `include/black_hole/`, namespace `bh`. Linkar com `bh_scientific`.

#### Raios característicos, deflexão, escape e sombra

```cpp
#include "black_hole/units.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/weak_field.hpp"
#include "black_hole/escape.hpp"

const double rs = bh::units::sagittarius_a_rs_si();          // 1.268388e10 m
const double r_ph = bh::photon_sphere_radius(rs);            // 1.5 rs
const double r_isco = bh::isco_radius(rs);                   // 3 rs = 6 M
const double b_c = bh::escape::critical_impact_parameter(rs); // (3√3/2) rs ≈ 2.598 rs

double a1 = bh::weak_field::deflection_angle(1.0, 50.0);               // 2 rs/b = 0.04
double a2 = bh::weak_field::deflection_angle_second_order(1.0, 50.0);  // 0.0411781

double alpha = bh::escape::shadow_angular_radius(1.0, 4.997);          // 0.4836 rad (câmera default)
bool gone = bh::escape::will_escape_scene(r, dr_dlambda, 1.0, scene_bound);
```

#### Geodésica no plano orbital (sem polos) com RK4 ou RK45

```cpp
#include "black_hole/planar_geodesic.hpp"

// rs = 1; direção no referencial do observador estático (default)
bh::PlanarRay ray = bh::make_planar_ray({{20.0, 3.0, 0.0}}, {{-1.0, 0.0, 0.2}}, 1.0);
bh::Rk45Options opt;            // abs_tol = rel_tol = 1e-10, h_min = 1e-6, h_max = 1
double h = 0.01;
while (bh::planar_radius(ray) > 1.0 &&
       !bh::escape::will_escape_scene(bh::planar_radius(ray), bh::planar_dr(ray), 1.0, 30.0)) {
    const bh::Rk45Result r = bh::planar_rk45_step(ray, h, 1.0, opt);
    if (r.h_used == 0.0) break;   // estado inválido: o RK45 recusa o passo
    h = r.h_next;
    // ou: bh::planar_rk4_step(ray, bh::geometric_step(bh::planar_radius(ray), 1.0), 1.0);
}
bh::Vec3d p = bh::planar_position(ray);           // posição no mundo
double drift = bh::planar_null_constraint(ray, 1.0);
```

#### Disco: órbitas, redshift e Page–Thorne

```cpp
#include "black_hole/orbits.hpp"
#include "black_hole/redshift.hpp"
#include "black_hole/disk_emission.hpp"

const double r = 3.0;                                        // ISCO, rs = 1
double v   = bh::orbits::local_orbital_speed(r, 1.0);        // 0.5 c
double eta = bh::orbits::thin_disk_efficiency();             // 1 − sqrt(8/9) ≈ 0.0572

// E e L_axis vêm do raio traçado (PlanarRay::E, PlanarRay::L_vec[1] para disco no plano XZ)
double g_inf = bh::redshift::doppler_gravitational_factor(r, 1.0, E, L_axis, /*spin_sign=*/+1.0);
double g_cam = bh::redshift::doppler_gravitational_factor_at(r, 1.0, E, L_axis, r_cam, +1.0);
double boost = bh::redshift::intensity_boost(g_cam);         // g⁴

using namespace bh::disk_emission;
const double G = bh::units::G_SI, c = bh::units::C_SI, M = bh::units::SAGITTARIUS_A_MASS_KG;
double mdot = 0.01 * eddington_accretion_rate_si(M, G, c, eta);         // 1.05e20 kg/s
double F = page_thorne_flux_si(4.78 * rs, M, mdot, G, c);              // W/m², perto do pico
double T = effective_temperature(F);                                   // ≈ 8.7e4 K
double R, Gc, B;
blackbody_rgb(bh::redshift::observed_temperature(g_cam, T), R, Gc, B); // cromaticidade sRGB linear
```

#### Renderer CPU e escrita de imagem

```cpp
#include "black_hole/cpu_renderer.hpp"
#include "black_hole/image_io.hpp"
#include "black_hole/scene_params.hpp"

bh::render::Options opt;                        // 200×150, legacy, rk4, cena default
std::string err;
bh::load_scene_params_json("examples/scene_relativistic_showcase.json", opt.scene, err);
opt.width = 640; opt.height = 360;
opt.color_mode = bh::render::ColorMode::LegacyDoppler;   // "relativistic"
opt.integrator = bh::render::IntegratorKind::Rk45;
bh::render::Stats st;
bh::render::Image img = bh::render::render(opt, &st);    // determinístico
bh::image_io::write_image("out.png", img, /*gamma=*/1.0, err);
```

#### Kerr (só anotação)

```cpp
#include "black_hole/kerr_analytic.hpp"
double r_isco = bh::kerr::isco_radius(/*M=*/1.0, 0.998, /*prograde=*/true);   // 1.237 M
double eta_k  = bh::kerr::thin_disk_efficiency(1.0, 0.998, true);             // 0.321
// Raios analíticos apenas: nenhuma geodésica de Kerr é integrada em lugar nenhum.
```

---

### Testes que cobrem o modo científico

| CTest | Conteúdo |
|-------|----------|
| `scientific_ref` | órbitas, redshift, Page–Thorne vs quadratura, hit tests, captura/escape em torno de \(b_c\), deflexão numérica, RK45 vs RK4, carta plana vs 3-D, polos, sombra, Kerr analítico |
| `cpu_render` | estrutura, paleta âmbar, simetria espelhada, determinismo entre threads, assimetria Doppler + inversão do spin, corpo negro, câmera perto do/no polo, área da sombra (8 %), borda da sombra vs Synge, PNG/CRC/Adler |
| `render_cli_smoke`, `render_cli_rejects_bad_spin`, `render_cli_relativistic_sequence` | CLI do `bh_render_cpu` |
| `shader_contract` | espelhos idênticos, bindings, `SciParams` GLSL ↔ C++ |
| `gpu_cpu_agreement` | `BlackHole3D --capture` vs `bh_render_cpu` em todas as poses do script (hoje 5, inclusive FOV 40° com alvo deslocado e meia massa) × 2 modos (Mesa llvmpipe); código 77 = SKIP sem GL |
| `source_invariants`, `style_contract` | o baseline continua intacto |

## EN

Three entry points: the `bh_scientific` double-precision library, the headless `bh_render_cpu` CLI
(`legacy` / `relativistic` / `blackbody`, `rk4` / `rk45`, deterministic for any thread count) and the GPU twin
`BlackHole3D --scientific` / `--relativistic` (planar RK4 in the ray's orbital plane, continuous hits, proof-based
escape, rs = 1 units, `SciParams` UBO at binding 4). `BlackHole3D` with no flags is still the exact historical
Euler baseline. GPU statements refer to Mesa llvmpipe only. Kerr is analytic-only (`kerr_analytic.hpp`), never
simulated.
