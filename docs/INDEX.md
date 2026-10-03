# Índice da documentação / Package map (0.8.0)

Mapa do repositório após a 0.8.0 (óptica relativística, renderer CPU de
referência, modo científico na GPU, ponte de render com o Blender). Docs de
usuário em português (pt-BR); identificadores técnicos em inglês.

## Começar aqui

| Documento | Descrição |
|-----------|-----------|
| [../README.md](../README.md) | Visão geral, galeria, início rápido, referência de CLI, números medidos, limitações honestas |
| [../STATUS.md](../STATUS.md) | Portões de verificação da 0.8.0 e lacunas restantes |
| [../CHANGELOG.md](../CHANGELOG.md) | O que mudou em cada versão |
| [../ENTREGA.md](../ENTREGA.md) | Resumo da entrega |
| [ROTEIRO_BUILD.md](ROTEIRO_BUILD.md) | apt (incl. Mesa/Xvfb para GPU headless), vcpkg, presets, alvos do `Makefile`, Windows |
| [CHECKLIST_VALIDACAO.md](CHECKLIST_VALIDACAO.md) | Portões antes de aceitar mudanças |
| [VALIDATION_STATUS.md](VALIDATION_STATUS.md) | O que foi validado (llvmpipe, Blender 4.0.2) e o que não foi |

## Renderização e ferramentas

| Documento / ferramenta | Descrição |
|------------------------|-----------|
| [RENDER_CPU.md](RENDER_CPU.md) | **Manual do `bh_render_cpu`**: CLI completa, sequências, linha JSON, modos, desempenho, testes golden, ponte Blender, `image_diff.py` |
| [`../tools/bh_render_cpu.cpp`](../tools/bh_render_cpu.cpp) | Renderer CPU headless (legacy / relativistic / blackbody; RK4 / RK45) |
| [`../tools/image_diff.py`](../tools/image_diff.py) | Comparador PNG/PPM/BMP só com stdlib: erro médio/máx., fração de pixels ruins, mapa de calor, gate `--max-bad-fraction` |
| [`../tools/frames_to_gif.py`](../tools/frames_to_gif.py) | Sequência de frames → GIF animado (stdlib, paleta global, `--pingpong`) |
| [`../tools/bh_scene_dump.cpp`](../tools/bh_scene_dump.cpp) | Imprime/regrava o JSON `scene_params/v1` |
| [`../examples/`](../examples/) | `quickstart_scientific`, cenas JSON (`scene_params_example.json`, `scene_relativistic_showcase.json`) — ver [`../examples/README.md`](../examples/README.md) |
| [`../shaders/README.md`](../shaders/README.md) | Shaders: baseline canônico na raiz, espelhos, `geodesic_scientific.comp` e o UBO `SciParams` |
| [`../Makefile`](../Makefile) | Atalhos `build`, `test`, `test-gl`, `render`, `package-addon`, `blender-scene`, `style-check`, … |

## Contrato do baseline e arquitetura

| Documento | Descrição |
|-----------|-----------|
| [BASELINE.md](BASELINE.md) | Contrato legado (hashes, constantes travadas) + descoberta do artefato da vista inicial |
| [ESTILO_VISUAL_SIMULACAO.md](ESTILO_VISUAL_SIMULACAO.md) | Bíblia do estilo visual (travado); modos novos são opt-in |
| [ARQUITETURA.md](ARQUITETURA.md) | CLI → escolha do shader, UBOs (incl. `SciParams` binding 4), pipeline do renderer CPU, ponte Blender nas duas direções |
| [ESTRUTURA_REPOSITORIO.md](ESTRUTURA_REPOSITORIO.md) | Árvore completa e targets/testes CMake |
| [ENGINEERING_UPGRADE.md](ENGINEERING_UPGRADE.md) | Correções/otimizações sem mudar o modelo numérico default (inclui itens 0.8.0) |
| [ANIMACAO_PIPELINE.md](ANIMACAO_PIPELINE.md) | Frame loop OpenGL, sequências do `bh_render_cpu`, bake no Blender |
| [SKILLS_WORKFLOW.md](SKILLS_WORKFLOW.md) | Princípios de fluxo usados nos upgrades |

## Ciência

| Documento | Descrição |
|-----------|-----------|
| [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md) | O que a física modela / não modela |
| [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md) | Disco relativístico: órbitas, redshift/Doppler, Page–Thorne (referenciado por `disk_emission.hpp`) |
| [MODO_CIENTIFICO.md](MODO_CIENTIFICO.md) | Uso da biblioteca `bh_scientific` |
| [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md) | Especificação do modo científico (separado do default) |
| [COMPARATIVO_BASELINE_VS_CIENTIFICO.md](COMPARATIVO_BASELINE_VS_CIENTIFICO.md) | Baseline vs. científico lado a lado |
| [BRIEF_REVISAO_CIENTIFICA.md](BRIEF_REVISAO_CIENTIFICA.md) | Brief de revisão científica |
| [GLOSSARIO.md](GLOSSARIO.md) | rs, esfera de fótons, ISCO, UBO, compute shader, lensing, … |

## Blender (frontend opcional)

| Documento | Descrição |
|-----------|-----------|
| [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md) | Arquitetura do adaptador (Blender → C++ por JSON, C++ → Blender por imagens) |
| [`../blender/README.md`](../blender/README.md) | Visão geral do addon e galeria |
| [`../blender/INSTALAR_E_RODAR.md`](../blender/INSTALAR_E_RODAR.md) | Guia canônico de instalação e uso |
| [`../blender/docs/PIPELINE_C++_BLENDER.md`](../blender/docs/PIPELINE_C++_BLENDER.md) | As duas direções, passo a passo |
| [`../blender/docs/ANIMACAO.md`](../blender/docs/ANIMACAO.md) | Presets TURNTABLE/ELEVATION_SWEEP/DOLLY/SPIRAL, easing, spin do disco |
| [`../blender/docs/INSTALACAO_ADDON.md`](../blender/docs/INSTALACAO_ADDON.md) | Instalação do addon |
| [`../blender/examples/scene_params_example.md`](../blender/examples/scene_params_example.md) | Campos do JSON de cena |
| [`../blender/scripts/build_scene_headless.py`](../blender/scripts/build_scene_headless.py) | `blender --background` → `.blend` |
| [`../blender/scripts/package_addon.py`](../blender/scripts/package_addon.py) | Zips determinísticos do addon (`--check`) |

## Galeria (`docs/images/`)

Todas geradas neste repositório (CPU: `bh_render_cpu`; GPU: `BlackHole3D --capture`
em Mesa llvmpipe; Blender 4.0.2 Cycles headless).

| Imagem | Origem |
|--------|--------|
| [showcase_relativistic.png](images/showcase_relativistic.png) | `bh_render_cpu`, cena showcase, `relativistic`, estrelas |
| [showcase_blackbody.png](images/showcase_blackbody.png) | idem, `blackbody` (branco-azulado: pico ≈ 87 000 K, UV) |
| [showcase_elevation_sweep.gif](images/showcase_elevation_sweep.gif) | animação da cena showcase (sequência do `bh_render_cpu` montada com `frames_to_gif.py`) |
| [legacy_default_gpu.png](images/legacy_default_gpu.png) | `BlackHole3D` sem flags, vista inicial (artefato documentado) |
| [legacy_el125_gpu.png](images/legacy_el125_gpu.png) | baseline legado na GPU, elevação 1,25 |
| [scientific_el125.png](images/scientific_el125.png) | `bh_render_cpu`, paleta legada, 640×480 |
| [relativistic_el125.png](images/relativistic_el125.png) | `bh_render_cpu`, `relativistic` |
| [blackbody_el100.png](images/blackbody_el100.png) | `bh_render_cpu`, `blackbody` |
| [blender_el125_cycles.png](images/blender_el125_cycles.png) | cena do addon em Cycles (sem lensing, por design) |

## Núcleo científico

| Caminho | Papel |
|---------|-------|
| [`../include/black_hole/`](../include/black_hole/) | Headers: `units`, `schwarzschild`, `orbits`, `redshift`, `disk_emission`, `hit_testing`, `escape`, `integrator_rk4`, `integrator_rk45`, `planar_geodesic`, `weak_field`, `kerr_analytic` (só analítico), `camera_model`, `disk_model`, `scene_params`, `cpu_renderer`, `image_io`, … |
| [`../src/scientific/`](../src/scientific/) | Implementação → `libbh_scientific` |
| [`../shaders/geodesic_scientific.comp`](../shaders/geodesic_scientific.comp) | Gêmeo GPU do renderer CPU, ligado por `BlackHole3D --scientific` / `--relativistic` |
| CMake `bh_scientific` / `BLACK_HOLE_BUILD_GL` | Lib sem GL; GL continua opcional |

O **default** de visualização continua `BlackHole3D` sem flags = `geodesic.comp`
(`legacyEulerStep`). Nada científico substitui esse caminho em silêncio.

## Testes, scripts e regressão

| Artefato | Descrição |
|----------|-----------|
| [`../tests/validate_source_invariants.py`](../tests/validate_source_invariants.py) | Guarda estática do baseline |
| [`../tests/test_style_contract.py`](../tests/test_style_contract.py) | Números da bíblia visual |
| [`../tests/test_shader_contract.py`](../tests/test_shader_contract.py) | Espelhos, bindings, `SciParams` GLSL ↔ C++ |
| [`../tests/test_scientific_ref.cpp`](../tests/test_scientific_ref.cpp) | Física headless |
| [`../tests/test_cpu_render.cpp`](../tests/test_cpu_render.cpp) | Renderer CPU |
| [`../tests/test_render_cli.py`](../tests/test_render_cli.py) | Contrato completo da CLI `bh_render_cpu` |
| [`../tests/test_gpu_cpu_agreement.py`](../tests/test_gpu_cpu_agreement.py) | `BlackHole3D --capture` × `bh_render_cpu` (Mesa llvmpipe ou GPU) |
| [`../tests/test_blender_addon_compile.py`](../tests/test_blender_addon_compile.py), [`../tests/test_blender_addon_parity.py`](../tests/test_blender_addon_parity.py) | Addon sem `bpy` |
| [`../tests/test_tools.py`](../tests/test_tools.py) | Autoteste de `image_diff` / `frames_to_gif` |
| [`../scripts/apply_baseline_check.sh`](../scripts/apply_baseline_check.sh) | Wrapper do teste de invariantes |
| [`../scripts/hash_tree.py`](../scripts/hash_tree.py) | SHA-256 de fontes-chave |
| [`../BASELINE_SHA256.txt`](../BASELINE_SHA256.txt) | Hashes do arquivo original |
| [`../.github/workflows/ci.yml`](../.github/workflows/ci.yml) | CI: `scientific-headless`, `scientific-portable` (Windows/MSVC, macOS/Apple Clang, sem GL), `linux-build` (llvmpipe + Xvfb), `blender-headless` |

## Organização e legado

| Documento | Descrição |
|-----------|-----------|
| [../Gravity_Sim/README_LEGACY.md](../Gravity_Sim/README_LEGACY.md) | Inventário de `Gravity_Sim/src/` (não compilado) |

## Licença e citação

| Artefato | Descrição |
|----------|-----------|
| [`../LICENSE-NOTES.md`](../LICENSE-NOTES.md) | Origem upstream — **não** inventa LICENSE |
| [`../CITATION.cff`](../CITATION.cff) | Citação (autores placeholder) |
| Upstream | https://github.com/kavan010/black_hole |

## EN (short)

The default render path is still the **historical Euler** baseline in
`geodesic.comp` (`BlackHole3D` with no flags). New physics is opt-in:
`BlackHole3D --scientific/--relativistic` (GPU, planar RK4) and `bh_render_cpu`
(CPU reference, see [RENDER_CPU.md](RENDER_CPU.md)). Kerr is **not** simulated
(analytic radii only). Blender is an optional frontend that never bends light.
Not a GRMHD code. GPU results were measured on Mesa llvmpipe only.
