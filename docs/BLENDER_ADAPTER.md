# Blender adapter (opcional) — arquitetura 0.8.0

## Posição arquitetural

**Blender não é o dono da física.** A pasta [`blender/`](../blender/) é um
*frontend* em volta do núcleo C++/OpenGL e da biblioteca científica
`bh_scientific`. O adaptador funciona nas **duas direções**:

1. **Blender → C++:** o addon exporta `black_hole.scene_params/v1` (JSON), que
   `BlackHole3D --scene` e `bh_render_cpu --scene` carregam.
2. **C++ → Blender:** o addon roda `bh_render_cpu` e importa os PNG/BMP (still
   ou sequência) como background da câmera ou como plano emissivo.

O build padrão **não** exige Blender. `bh_scientific`, `bh_render_cpu`,
`BlackHole2D`/`BlackHole3D` e todos os testes CTest, inclusive os do addon,
rodam sem ele.

![Cena do addon no Blender 4.0.2 (Cycles, elevação 1.25)](images/blender_el125_cycles.png)

*`images/blender_el125_cycles.png`: o que o lado Blender produz sozinho
(geometria + paleta travada, sem lensing). Compare com a mesma pose calculada
pelo C++ em [`images/scientific_el125.png`](images/scientific_el125.png) e
[`images/relativistic_el125.png`](images/relativistic_el125.png).*

## Diagrama

```
                 ┌───────────────────────── C++ (física) ─────────────────────────┐
                 │  bh_scientific: geodésicas Schwarzschild (RK4/RK45, plano       │
                 │  orbital), cruzamento contínuo do disco, escape por prova,      │
                 │  redshift/Doppler, Page–Thorne                                  │
                 │  BlackHole3D   (GL; padrão = baseline legado intocado)          │
                 │  bh_render_cpu (CPU headless; legacy | relativistic | blackbody)│
                 └───────▲───────────────────────────────────────────┬────────────┘
  scene_params/v1 JSON   │ --scene file.json                         │ PNG/BMP, stem_NNNN.png
  (câmera no referencial │ + flags --azimuth/--elevation/            │ + 1 linha JSON no stdout
   C++, Y-up)            │   --radius-rs/--fov-y-deg                 ▼
                 ┌───────┴──────────────── Blender addon 0.8.0 ─────────────────────┐
                 │ json_io.py ── export/import ──┐   render_bridge.py ── subprocess │
                 │ constants.py: espelho dos headers, cpp_to_blender (x,y,z)→(x,−z,y)│
                 │ scene_builder / materials / disk_mesh / grid_mesh / horizon /     │
                 │ guides / camera_orbit: cena de referência com paleta travada      │
                 └──────────────────────────────────────────────────────────────────┘
```

## Módulos do addon (`blender/addons/black_hole_bridge/`)

| Módulo | Papel | Puro (sem `bpy`)? |
|--------|-------|-------------------|
| `constants.py` | espelho de `black_hole.cpp` / `geodesic.comp` / `grid.frag` / `schwarzschild.hpp`; `cpp_to_blender`, `blender_to_cpp`, `camera_position`, `camera_inside_disk`, `grid_warp_y`, `disk_color_rgb` | sim |
| `json_io.py` | `build_params_dict`, `params_from_settings`, `apply_params_to_settings`, leitura e escrita | sim |
| `render_bridge.py` | `build_render_command`, `parse_render_summary`, `expected_frame_paths`, `find_sequence_files`, `find_renderer`, `plane_size_for_fov`, `orbit_params_from_cpp_position`, `camera_fov_y`; operadores Render / Import background / Import as plane | helpers sim |
| `camera_orbit.py` | `ease_t`, `camera_path_sample`, `make_path_fn` (presets); rig com Track To, FOV vertical, `bake_camera_path` | helpers sim |
| `disk_mesh.py`, `grid_mesh.py`, `guides.py` | geometria gerada no referencial C++ (testada) e convertida ao escrever no Blender; Solidify no disco, Wireframe na grade, curvas com bevel nos anéis | helpers sim |
| `materials.py` | disco Emission `(1, r, 0.2)·r` + spin/turbulência, grade/guias cinza translúcidos, horizonte preto, `setup_color_parity` (Raw), mundo preto | não |
| `scene_builder.py` | Build Full Scene: coleções `BH_*`, Cycles, Raw, aviso de câmera no disco | não |
| `export_ops.py`, `__init__.py` | operadores, `BHBridgeSettings`, sub-painéis, `register` | não |

Todo módulo importa em `python3` puro (`bpy` em `try/except`). Por isso
`tests/test_blender_addon_parity.py` testa os helpers contra os headers C++
sem Blender.

## Contratos entre os lados

| Contrato | Regra |
|----------|-------|
| Eixos | C++ Y-up (disco em XZ) → Blender Z-up (disco em XY) **só** via `cpp_to_blender` (x, y, z) → (x, −z, y), rotação própria de +90° em X (det +1, preserva produto vetorial). A câmera Blender (Track To, up = +Z) enquadra igual à C++ (up = +Y). Helpers puros e JSON ficam no referencial C++. |
| Unidades | Blender: 1 unidade = rs. JSON: metros (`radius_m`, `r_s_m`) + fatores de rs (`inner_factor_rs`, …). CLI: `--radius-rs`. |
| Cor | Disco = pixel OpenGL `vec4(1, r, 0.2, r)` misturado sobre preto = `(1, r, 0.2)·r`, com os fatores reais do disco; grade 0.5·0.7 = 0.35; view transform **Raw**. `disk_glow` 1.0 = paridade. |
| Pose padrão | 4.997 rs, elevação π/2: dentro da laje do disco. O Blender avisa e recomenda Elevação 1.25; no OpenGL, a metade de cima do frame inicial sai amarela ([`images/legacy_default_gpu.png`](images/legacy_default_gpu.png)), porque `cos(float(π/2))` ≈ −4.37e−8 deixa a câmera ≈ 2.8 km abaixo do plano. Baseline documentado, não alterado. |
| JSON lido pelo C++ | `black_hole`, `camera` (`radius_m`, `azimuth_rad`, `elevation_rad`, `fov_y_deg`, `target_m`), `disk` (fatores, `thickness_m`, `disk_num`), `gravity`, `objects[]`, `render_baseline` (informativo). O resto é ignorado. `BlackHole3D` aplica câmera (inclusive FOV e alvo), massa, disco e objetos (`[]` = nenhum); janela 800×600 e compute 200×150 continuam travados. |
| JSON lido pelo addon | massa, fatores/espessura/spin/turbulência do disco, câmera, `grid_size`, `animation.*`, `render.*`. `objects`/`gravity` ignorados; `turntable_azimuth` (0.7.x) → `TURNTABLE`. |
| CLI do renderer | `bh_render_cpu [--scene] --out [--width] [--height] [--mode legacy\|relativistic\|blackbody] [--integrator rk4\|rk45] [--azimuth] [--elevation] [--radius-rs] [--fov-y-deg] [--frames] [--azimuth-turns] [--elevation-end] [--threads] [--supersample] [--exposure] [--gamma] [--mdot-edd] [--spin-sign ±1] [--stars] [--max-steps] [--quiet] [--help]`. O addon usa os três modos, RK4/RK45, supersample e `--stars`, sempre com `--frames 1` e a câmera explícita. |
| Sequências | Addon: uma chamada por frame da cena (`frame_start … frame_start + N − 1`) com a pose animada da câmera → `stem_0000 … stem_{N−1}`. CLI: `--frames N` → mesma nomenclatura, azimute az₀ + 2π·T·k/N. O import vira `SEQUENCE` começando no `frame_start` da cena, com `frame_end = frame_start + N − 1`. |
| Retorno | exit 0 + uma linha JSON (`frames`, `width`, `height`, `mode`, `integrator`, `shadow_fraction`, `disk_fraction`, `mean_steps`, `seconds`, …) → `Scene["bh_last_render_summary"]`. |

## Papel de cada parte

| Responsabilidade | Onde vive |
|------------------|-----------|
| Integração geodésica, métrica, validação | C++/GLSL (`bh_scientific`, `geodesic_scientific.comp`) |
| Baseline visual em tempo real (padrão intocado: Euler legado, 60000 passos) | `BlackHole3D` sem flags |
| Imagens de referência headless e determinísticas | `bh_render_cpu` |
| Editar massa, disco, câmera e animação; exportar JSON | addon |
| Cena de referência no Blender (paleta travada, sem lensing) | addon |
| Trazer as imagens do C++ para dentro do Blender | addon (`render_bridge.py`) |

## Verificação

| Onde | O quê |
|------|-------|
| CTest `blender_addon_compile` | `py_compile` de todos os módulos e scripts |
| CTest `blender_addon_parity` | constantes e fórmulas vs headers, `cpp_to_blender`, JSON ida e volta, CLI do renderer, sequências, presets/easing, zips determinísticos |
| CTest `render_cli_smoke`, `render_cli_relativistic_sequence` | o renderer com `--scene` e com `--frames` |
| CTest `render_cli_contract` | todas as flags do `bh_render_cpu` (`tests/test_render_cli.py`) |
| CTest `gpu_cpu_agreement` | `BlackHole3D --capture` × `bh_render_cpu` (6 cenas × 2 modos, inclusive FOV/alvo, meia massa e `"objects": []`; Mesa llvmpipe; código 77 = pulado sem GL) |
| CI `blender-headless` | Blender 4.0.x do `apt` roda `build_scene_headless.py` e gera o .blend |
| CI `scientific-headless` | zips do addon em dia (`package_addon.py` + `git diff --exit-code`) |
| Manual (Blender 4.0.2, Cycles) | Build Full Scene a elevação 1.25 → [`images/blender_el125_cycles.png`](images/blender_el125_cycles.png); JSON exportado pelo addon carregado por `BlackHole3D --scene … --capture` e por `bh_render_cpu --scene` |

Comandos: `make package-addon`, `make blender-scene` (precisa de `blender` no
`PATH`), `make test`.

## O que não fazer

- Não mover o integrador para Geometry Nodes nem para shaders do Blender.
- Não afirmar que um `.blend` ou um render Cycles "valida" Schwarzschild.
  O Blender não curva a luz.
- Não afirmar simulação Kerr: o projeto só tem raios analíticos de Kerr
  (`kerr_analytic.hpp`), nenhuma geodésica de Kerr.
- Não converter eixos fora de `cpp_to_blender`, nem gravar coordenadas do
  Blender no JSON.
- Não trocar a paleta: disco `(1, r, 0.2)·r`, grade/guias cinza, fundo preto
  ([`ESTILO_VISUAL_SIMULACAO.md`](ESTILO_VISUAL_SIMULACAO.md)).
- Não misturar fluidos/partículas do Blender com afirmações científicas deste
  repositório.

## Documentos

- [`blender/README.md`](../blender/README.md): visão geral e galeria
- [`blender/INSTALAR_E_RODAR.md`](../blender/INSTALAR_E_RODAR.md): guia canônico
- [`blender/docs/PIPELINE_C++_BLENDER.md`](../blender/docs/PIPELINE_C++_BLENDER.md): as duas direções, passo a passo
- [`blender/docs/ANIMACAO.md`](../blender/docs/ANIMACAO.md): presets, easing, spin, sequências
- [`blender/examples/scene_params_example.md`](../blender/examples/scene_params_example.md): campos do JSON
