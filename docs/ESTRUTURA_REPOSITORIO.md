# Estrutura do repositório / Repository structure (0.8.0)

Árvore versionada após a 0.8.0. `build/` é gerado (ignorado pelo git); o CMake
raiz não compila `Gravity_Sim/`. Itens marcados **(novo 0.8.0)** ou
**(alterado 0.8.0)** entraram ou mudaram nesta versão.

```text
black_hole/
├── README.md / ENTREGA.md / STATUS.md / CHANGELOG.md
├── CITATION.cff / LICENSE-NOTES.md          # licença real ainda TBD (não inventada)
├── BASELINE_SHA256.txt                      # hashes do arquivo original
├── CMakeLists.txt                           # (alterado 0.8.0) versão 0.8.0, include(CTest) no topo, Threads
├── CMakePresets.json                        # default, scientific, vcpkg-release, debug, ci-linux, ci-scientific
├── Makefile                                 # (novo 0.8.0, versionado) atalhos sobre CMake/CTest
├── vcpkg.json
├── .editorconfig / .clang-format / .clang-tidy / .gitattributes
├── .gitignore                               # (alterado 0.8.0) exceção para o Makefile da raiz
├── .github/workflows/ci.yml                 # (alterado 0.8.0) scientific-headless, scientific-portable (Windows/macOS), linux-build, blender-headless
│
├── black_hole.cpp                           # (alterado 0.8.0) BlackHole3D: baseline + CLI --scene/--scientific/--relativistic/--capture/--help
├── geodesic.comp                            # Baseline GPU — legacyEulerStep (INALTERADO, protegido por invariantes)
├── grid.vert / grid.frag                    # grade de spacetime (canônicos)
├── 2D_lensing.cpp                           # BlackHole2D
├── CPU-geodesic.cpp                         # protótipo CPU histórico (não é o default)
├── ray_tracing.cpp                          # experimento histórico
│
├── include/black_hole/                      # API científica (headers)
│   ├── units.hpp · schwarzschild.hpp · ray_state.hpp · conserved.hpp
│   ├── integrator_rk4.hpp · camera_model.hpp · disk_model.hpp · scene_params.hpp
│   ├── weak_field.hpp                       # (alterado 0.8.0) deflexão de 2.ª ordem
│   ├── orbits.hpp                           # (novo) órbitas circulares tipo-tempo
│   ├── redshift.hpp                         # (novo) Doppler + redshift gravitacional, g⁴
│   ├── disk_emission.hpp                    # (novo) Page–Thorne, Eddington, corpo negro → sRGB
│   ├── hit_testing.hpp                      # (novo) colisões contínuas segmento–plano/esfera
│   ├── escape.hpp                           # (novo) b_c, escape por prova, raio da sombra
│   ├── integrator_rk45.hpp                  # (novo) Dormand–Prince 5(4)
│   ├── planar_geodesic.hpp                  # (novo) integração no plano orbital
│   ├── kerr_analytic.hpp                    # (novo) raios analíticos de Kerr (NÃO simula Kerr)
│   ├── cpu_renderer.hpp                     # (novo) renderer CPU headless
│   └── image_io.hpp                         # (novo) PNG/BMP/PPM sem dependências
├── src/scientific/                          # implementação → libbh_scientific
│   ├── schwarzschild.cpp · ray_state.cpp · conserved.cpp · scene_params.cpp
│   ├── integrator_rk4.cpp                   # (alterado 0.8.0) RHS congela dentro do horizonte (sem NaN)
│   ├── integrator_rk45.cpp                  # (novo)
│   ├── planar_geodesic.cpp                  # (novo)
│   ├── cpu_renderer.cpp                     # (novo)
│   └── image_io.cpp                         # (novo)
├── shaders/
│   ├── README.md
│   ├── geodesic.comp · grid.vert · grid.frag   # espelhos byte a byte da raiz (shader_contract)
│   └── geodesic_scientific.comp             # (reescrito 0.8.0, ligado) RK4 planar via --scientific
├── tools/
│   ├── bh_render_cpu.cpp                    # (novo) CLI do renderer CPU
│   ├── bh_scene_dump.cpp                    # dump/regravação do JSON de cena
│   ├── image_diff.py                        # (novo) comparador PNG/PPM/BMP (stdlib)
│   └── frames_to_gif.py                     # (novo) sequência → GIF animado (stdlib)
├── examples/
│   ├── README.md
│   ├── quickstart_scientific.cpp / .py      # (alterados 0.8.0) tabela de números de Sgr A*
│   ├── scene_params_example.json            # cena default (espelho em blender/examples/)
│   └── scene_relativistic_showcase.json     # (novo) disco 3–12 rs, câmera 18 rs, el 1,40, FOV 38°
├── tests/
│   ├── validate_source_invariants.py        # baseline travado
│   ├── test_style_contract.py               # bíblia visual
│   ├── test_shader_contract.py              # (novo) espelhos, bindings, SciParams GLSL ↔ C++
│   ├── test_blender_addon_compile.py        # (novo) py_compile do addon
│   ├── test_blender_addon_parity.py         # (novo) paridade addon ↔ headers, CLI, zips
│   ├── test_tools.py                        # (novo) autoteste de image_diff / frames_to_gif
│   ├── test_scientific_ref.cpp              # (estendido) física headless
│   ├── test_cpu_render.cpp                  # (novo) renderer CPU
│   ├── test_render_cli.py                   # (novo) contrato completo da CLI bh_render_cpu
│   ├── test_gpu_cpu_agreement.py            # (novo) BlackHole3D --capture × bh_render_cpu
│   └── test_legacy_golden.py                # (novo) BlackHole3D sem flags × docs/images/legacy_default_gpu.png
├── scripts/
│   ├── hash_tree.py
│   └── apply_baseline_check.sh
├── docs/                                    # índice: INDEX.md
│   ├── RENDER_CPU.md                        # (novo) manual do bh_render_cpu
│   ├── … (ver INDEX.md)
│   └── images/                              # (novo) galeria gerada neste repositório
│       ├── showcase_relativistic.png · showcase_blackbody.png · showcase_elevation_sweep.gif
│       ├── legacy_default_gpu.png · legacy_el125_gpu.png          # BlackHole3D --capture (llvmpipe)
│       ├── legacy_default_gpu.json                                 # renderer/versão GL da captura golden (legacy_golden)
│       ├── scientific_el125.png · relativistic_el125.png · blackbody_el100.png   # bh_render_cpu
│       └── blender_el125_cycles.png                                # Blender 4.0.2 Cycles
├── blender/                                 # frontend opcional Blender 4.x
│   ├── README.md · INSTALAR_E_RODAR.md
│   ├── black_hole_bridge.zip                # zip determinístico (cópia)
│   ├── addons/
│   │   ├── black_hole_bridge.zip            # zip determinístico instalável
│   │   └── black_hole_bridge/
│   │       ├── __init__.py · blender_manifest.toml       # (alterados) 0.8.0, sub-painéis
│   │       ├── constants.py                 # (alterado) cpp_to_blender, camera_inside_disk, render/anim
│   │       ├── json_io.py · export_ops.py · scene_builder.py · materials.py
│   │       ├── camera_orbit.py              # (alterado) presets TURNTABLE/ELEVATION_SWEEP/DOLLY/SPIRAL
│   │       ├── disk_mesh.py · grid_mesh.py · horizon.py
│   │       ├── guides.py                    # (novo) anéis 1,5 rs / 2,598 rs / 3 rs
│   │       └── render_bridge.py             # (novo) roda bh_render_cpu e importa o resultado
│   ├── docs/ (INSTALACAO_ADDON.md · ANIMACAO.md · PIPELINE_C++_BLENDER.md)
│   ├── examples/ (scene_params_example.json/.md · animation_style_guide.md · output/)
│   └── scripts/
│       ├── build_scene_headless.py          # (alterado) blender --background → .blend
│       ├── export_scene_params.py
│       └── package_addon.py                 # (novo) zips determinísticos (--check)
├── vs_code/                                 # configs auxiliares do VS Code
└── Gravity_Sim/                             # snapshot LEGACY — inventário em README_LEGACY.md
```

## Targets CMake

| Target | GL? | Descrição |
|--------|-----|-----------|
| `BlackHole2D` / `BlackHole3D` | Sim (`BLACK_HOLE_BUILD_GL=ON`) | Visualização; `BlackHole3D` sem flags = baseline |
| `bh_scientific` | Não | Biblioteca científica (+ `Threads`) |
| `bh_render_cpu` | Não | Renderer CPU (CLI) — [RENDER_CPU.md](RENDER_CPU.md) |
| `bh_scene_dump` | Não | Dump do JSON de cena |
| `quickstart_scientific` | Não | Números de Sgr A\* |
| `test_scientific_ref` / `test_cpu_render` | Não | Executáveis de teste |
| `validate_shaders` | glslang | Os 4 shaders GLSL (opcional, se `glslangValidator` existir) |

## Testes CTest registrados

| Teste | Tipo | Árvore |
|-------|------|--------|
| `source_invariants`, `style_contract`, `shader_contract` | Python, contratos estáticos | ambas |
| `blender_addon_compile`, `blender_addon_parity` | Python, sem `bpy` | ambas |
| `tools_selftest` | Python, ferramentas stdlib | ambas |
| `scientific_ref`, `cpu_render` | C++ | ambas |
| `render_cli_smoke`, `render_cli_rejects_bad_spin`, `render_cli_relativistic_sequence` | CLI `bh_render_cpu` | ambas |
| `render_cli_contract` | Python + CLI `bh_render_cpu` (todas as flags) | ambas |
| `gpu_cpu_agreement` | Python + `BlackHole3D` + `bh_render_cpu` | só GL (SKIP = 77 sem contexto) |
| `legacy_golden` | Python + `BlackHole3D` (sem flags) vs imagem de referência | só GL (SKIP = 77 sem contexto) |

A árvore GL tem exatamente os mesmos 12 testes da científica mais `gpu_cpu_agreement` e `legacy_golden` (14).
A lista autoritativa é `ctest -N` na árvore de build.

## O que não é o build default

- `Gravity_Sim/**` — protótipos/binários históricos.
- Blender como dependência de runtime do simulador (o addon é opcional).
- Substituição silenciosa de `geodesic.comp` pelo shader científico: o científico
  só roda com `--scientific` / `--relativistic`.

Ver [INDEX.md](INDEX.md), [ARQUITETURA.md](ARQUITETURA.md), [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md),
[COMPARATIVO_BASELINE_VS_CIENTIFICO.md](COMPARATIVO_BASELINE_VS_CIENTIFICO.md).
