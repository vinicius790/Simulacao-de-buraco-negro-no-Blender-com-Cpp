# Exemplos / Examples

| Exemplo | Precisa de OpenGL? | Descrição |
|---------|--------------------|-----------|
| `quickstart_scientific.cpp` (alvo `quickstart_scientific`) | Não | Tabela de Sgr A\*: rs, esfera de fótons, ISCO, b_c, órbitas circulares, redshift na ISCO, Page–Thorne a 1 % de Eddington, raio da sombra, deflexão traçada vs teoria, raios analíticos de Kerr |
| `quickstart_scientific.py` | Não | Subconjunto das mesmas fórmulas em Python puro (stdlib) |
| `scene_params_example.json` | Não | Cena default `black_hole.scene_params/v1` (espelho dos defaults travados: disco 2,2–5,2 rs, câmera 4,997 rs / az 0 / el π/2 / FOV 60°, 3 objetos). Espelhada em `blender/examples/` |
| `scene_relativistic_showcase.json` | Não | Cena showcase: disco 3–12 rs (a partir da ISCO), câmera a 18 rs, elevação 1,40 rad, FOV 38°, sem objetos |
| `BlackHole2D` / `BlackHole3D` (raiz) | Sim | Simulação visual legada (`BlackHole3D` sem flags = baseline) |

## Build (sem GLEW/GLFW)

```bash
make build                                   # build/scientific/
./build/scientific/quickstart_scientific
python3 examples/quickstart_scientific.py
```

Ou com CMake puro:

```bash
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF
cmake --build build/scientific --target quickstart_scientific bh_render_cpu
```

## `bh_render_cpu` com `scene_params_example.json`

O renderer CPU carrega a cena com `--scene`; flags de câmera sobrescrevem o JSON.
O diretório de saída precisa existir. Manual completo:
[`../docs/RENDER_CPU.md`](../docs/RENDER_CPU.md).

```bash
B=build/scientific/bh_render_cpu
S=examples/scene_params_example.json
mkdir -p build/renders

# 1. Pose default travada, 200×150 (a mesma resolução do compute legado)
$B --scene $S --out build/renders/ex_default.png

# 2. Elevação 1,25 (fora do plano do disco), 640×480, paleta legada
$B --scene $S --elevation 1.25 --width 640 --height 480 --out build/renders/ex_el125.png

# 3. Doppler + redshift gravitacional + beaming g⁴, integrador adaptativo RK45
$B --scene $S --elevation 1.25 --mode relativistic --integrator rk45 \
   --width 640 --height 480 --out build/renders/ex_rel.png

# 4. Câmera mais afastada e FOV menor, corpo negro Page–Thorne, estrelas
$B --scene $S --elevation 1.0 --radius-rs 12 --fov-y-deg 45 --mode blackbody --stars \
   --out build/renders/ex_bb.png

# 5. Turntable de 8 frames: orbit_0000.png … orbit_0007.png
$B --scene $S --elevation 1.25 --frames 8 --width 160 --height 120 --out build/renders/orbit.png
```

Cada execução imprime uma linha JSON. Saída real do exemplo 1 neste repositório
(container de 4 CPUs):

```json
{"frames":1,"width":200,"height":150,"mode":"legacy","integrator":"rk4","shadow_fraction":0.482933,"disk_fraction":0.232133,"object_fraction":0.00573333,"escaped_fraction":0.2792,"step_limit_rays":0,"mean_steps":183.433,"min_g":0.393837,"max_g":1.58306,"seconds":0.39,"out":"build/renders/ex_default.png"}
```

Observações sobre esses exemplos:

- O exemplo 1 usa a pose default (câmera no plano do disco, dentro do anel). O
  renderer CPU trata essa pose corretamente; no `BlackHole3D` sem flags a mesma
  pose mostra o artefato documentado da metade de cima amarela
  ([`../docs/BASELINE.md`](../docs/BASELINE.md)).
- No exemplo 4, o disco legado 2,2–5,2 rs começa dentro da ISCO (3 rs); o modo
  `blackbody` não emite entre 2,2 e 3 rs. Para um disco fisicamente consistente use
  `scene_relativistic_showcase.json`.
- Com a pose do exemplo 2, o RK45 do exemplo 3 usou ≈ 32 passos por raio contra
  ≈ 111 do RK4 (campo `mean_steps`).

## Cena showcase

```bash
$B --scene examples/scene_relativistic_showcase.json --mode relativistic \
   --width 640 --height 360 --supersample 2 --stars --out build/renders/showcase_relativistic.png
$B --scene examples/scene_relativistic_showcase.json --mode blackbody \
   --width 640 --height 360 --supersample 2 --stars --out build/renders/showcase_blackbody.png
```

É o que `make render` gera (junto com `legacy_el125.png`). Resultado:
[`../docs/images/showcase_relativistic.png`](../docs/images/showcase_relativistic.png),
[`../docs/images/showcase_blackbody.png`](../docs/images/showcase_blackbody.png).

## Mesma cena na GPU

```bash
cd build/gl
./BlackHole3D --scene ../../examples/scene_params_example.json --scientific
xvfb-run -a ./BlackHole3D --scene ../../examples/scene_relativistic_showcase.json --relativistic \
    --capture showcase_gpu.png      # sem monitor: Mesa llvmpipe
python3 ../../tools/image_diff.py showcase_gpu.png <render_cpu_200x150>.png
```

## Inspecionar / regravar a cena

```bash
./build/scientific/bh_scene_dump --load examples/scene_params_example.json
./build/scientific/bh_scene_dump --out /tmp/defaults.json    # defaults travados
```
