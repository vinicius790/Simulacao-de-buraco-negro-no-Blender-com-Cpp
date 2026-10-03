# Status de validação / Validation status (0.8.0)

Atualizado para o pacote **0.8.0** (anterior: 0.7.0-grand). Registra **o que foi
executado e medido**, em que ambiente, e o que **ainda não** foi validado. Números
sem fonte executável não entram aqui.

## Ambiente de registro

| Item | Valor |
|------|-------|
| Sistema | Container Linux (Ubuntu 24.04), 4 CPUs |
| Compilador / build | GCC 13.3.0, CMake 3.28.3, `-Wall -Wextra -Wpedantic` → **0 avisos** |
| Python | 3.11 (só biblioteca padrão nos testes e ferramentas) |
| OpenGL | **Mesa llvmpipe** (renderizador por software, LLVM 20.1.2), **OpenGL 4.5 Core**, Mesa **25.2.8**, via `xvfb-run` |
| Blender | **4.0.2** (`/usr/bin/blender`, pacote do Ubuntu), Cycles headless |
| GPU física | **Nenhuma.** Toda afirmação de GPU abaixo se refere ao llvmpipe |

## Validado agora

### Portões automáticos

| Portão | Comando | Resultado |
|--------|---------|-----------|
| Invariantes do baseline | `python3 tests/validate_source_invariants.py --root .` | PASS — `geodesic.comp` inalterado, constantes travadas |
| Contrato de estilo | `python3 tests/test_style_contract.py --root .` | PASS |
| Contrato de shaders | `python3 tests/test_shader_contract.py --root .` | PASS — espelhos idênticos, bindings, `SciParams` GLSL ↔ C++ (binding 4, 32 bytes) |
| Addon (compilação / paridade) | `test_blender_addon_compile.py`, `test_blender_addon_parity.py` | PASS (sem `bpy`) |
| Ferramentas stdlib | `python3 tests/test_tools.py --root .` | PASS — `image_diff` (PNG filtros 0–4, PPM, BMP), `frames_to_gif` (LZW/GIF89a) |
| Física de referência | `test_scientific_ref` | PASS — 157 asserções |
| Renderer CPU | `test_cpu_render` | PASS |
| CLI do renderer | `render_cli_smoke`, `render_cli_contract` (todas as flags, `tests/test_render_cli.py`), `render_cli_rejects_bad_spin`, `render_cli_relativistic_sequence` | PASS |
| GPU ↔ CPU | `tests/test_gpu_cpu_agreement.py` (Mesa llvmpipe) | PASS — 6 cenas × 2 modos (tabela abaixo) |
| Golden do baseline | `tests/test_legacy_golden.py` (`legacy_golden`, Mesa llvmpipe) | PASS — `BlackHole3D` sem flags idêntico byte a byte a `images/legacy_default_gpu.png` (driver gravado em `images/legacy_default_gpu.json`); `--scene examples/scene_params_example.json` = sem flags |
| GLSL | `validate_shaders` (glslangValidator nos 4 shaders) | PASS |
| `BlackHole3D` sem flags | inspeção + invariantes | baseline histórico; a CLI 0.8.0 é aditiva e o `Engine` é construído depois dela |
| Blender headless | `blender --background --python blender/scripts/build_scene_headless.py` | PASS — cena montada e renderizada em Cycles ([`images/blender_el125_cycles.png`](images/blender_el125_cycles.png)) |
| Zips do addon | `blender/scripts/package_addon.py` | determinísticos; o CI falha se estiverem desatualizados |

A árvore GL registra os mesmos 12 testes CTest da árvore científica mais
`gpu_cpu_agreement` e `legacy_golden` (só existem com OpenGL): 14 no total. Cada
teste da tabela foi executado e passou neste ambiente; `gpu_cpu_agreement` e
`legacy_golden` rodam de fato (não SKIP) com `xvfb-run` + llvmpipe.

### GPU ↔ CPU (`gpu_cpu_agreement`, 200×150, Mesa llvmpipe)

`BlackHole3D --scene S --scientific|--relativistic --capture` (float32) contra
`bh_render_cpu --scene S --mode legacy|relativistic` (double). Gate:
`bad_fraction ≤ 0,002` (pixel ruim = diferença > 24/255 em algum canal) e erro
médio ≤ 0,5/255.

| Cena | Modo | Erro médio (/255) | Erro máx. | `bad_fraction` |
|------|------|-------------------|-----------|----------------|
| az 0, el 1,25, 4,997 rs | `--scientific` | 0,058 | 1 | 0 |
| az 0, el 1,25, 4,997 rs | `--relativistic` | 0,00001 | 1 | 0 |
| az 0,7, el 0,60, 8e10 m | `--scientific` | 0,070 | 3 | 0 |
| az 0,7, el 0,60, 8e10 m | `--relativistic` | 0,0001 | 1 | 0 |
| az 2,1, el 2,00, 4,997 rs | `--scientific` | 0,062 | 246 | 6,7e-5 (2 de 30 000 pixels) |
| az 2,1, el 2,00, 4,997 rs | `--relativistic` | 0,006 | 173 | 6,7e-5 (2 de 30 000 pixels) |
| az 0,3, el 1,30, 9e10 m, FOV 40°, alvo (0, 5e9, 0) m | `--scientific` | 0,060 | 117 | 3,3e-5 (1 pixel) |
| az 0,3, el 1,30, 9e10 m, FOV 40°, alvo (0, 5e9, 0) m | `--relativistic` | 0,005 | 179 | 3,3e-5 (1 pixel) |
| az 0, el 1,25, metade da massa (r_s = 6,345e9 m), objetos default | `--scientific` | 0,035 | 1 | 0 |
| az 0, el 1,25, metade da massa (r_s = 6,345e9 m), objetos default | `--relativistic` | 0,00002 | 1 | 0 |
| az ≈ π, el 1,40, 2,2842e11 m (18 rs), disco 3–12 rs, `"objects": []` | `--scientific` | 0,042 | 1 | 0 |
| az ≈ π, el 1,40, 2,2842e11 m (18 rs), disco 3–12 rs, `"objects": []` | `--relativistic` | 0,00001 | 1 | 0 |

Execução anterior (antes das correções de revisão da 0.8.0): na pose az 0/el 1,25, erro máx.
1/255 (científico) e bytes idênticos (relativístico); nas outras duas poses, 2–4
pixels de borda em 30 000 (fração ≤ 1,3e-4), erro médio < 0,08/255. Os pixels
divergentes ficam em bordas sombra/disco, onde float32 e double decidem de lados
diferentes.

### Números medidos

| Grandeza | Valor | Fonte |
|----------|-------|-------|
| Deriva do vínculo nulo após 100 passos | Euler 8,8e-3 · RK4 2,4e-9 | `scientific_ref` |
| Deflexão fraca, b = 50 rs | numérica (RK45 planar) 0,0412157 rad · 1.ª ordem 0,04 · 2.ª ordem 0,0411781 → numérica a ≈ 0,09 % da 2.ª ordem (gate 1 %) | `scientific_ref`, `quickstart_scientific` |
| RK45 vs RK4 fino | mesmo estado final com 120 passos vs 3000 | `scientific_ref` |
| Captura/escape em torno de b_c = 2,598 rs | b = 2,0 e 2,5 rs capturados; 2,7 e 3,2 rs escapam | `scientific_ref` |
| Page–Thorne forma fechada vs quadratura | concordam a ≤ 2e-3; F(ISCO) = 0 | `scientific_ref` |
| Área da sombra renderizada vs b_c analítico | 0,131484 vs 0,131327 (gate 8 %) | `cpu_render` |
| Borda da sombra traçada vs Synge (sin α = b_c √f/r) | coincide em r_cam = 1,3 / 3 / 4,997 / 10 / 60 rs | `cpu_render` |
| Determinismo do `bh_render_cpu` | idêntico bit a bit para 1 e 4 threads (`image_diff` → erro 0) | `cpu_render`, `image_diff.py` |
| `bh_render_cpu` 400×300 `legacy`, 4 threads | 0,76 s (el 1,25, 111 passos/raio) a 1,31–1,41 s (pose default, 184 passos/raio); registro anterior ≈ 0,5–1,2 s, 77–152 passos | linha JSON — ver [RENDER_CPU.md §9](RENDER_CPU.md#9-desempenho-medido) |
| Sgr A\* (M = 8,54e36 kg) | L_Edd = 5,40e37 W; 1 % Eddington → Ṁ = 1,05e20 kg/s; pico de T_eff Page–Thorne ≈ 86 700 K em r ≈ 4,78 rs (≈ 9,55 M) | `quickstart_scientific` |
| Sombra na câmera default (4,997 rs) | 0,4836 rad = 27,7° | `quickstart_scientific` |
| Velocidade orbital local | 0,645 c a 2,2 rs (dentro da ISCO, instável) · 0,5 c na ISCO · 0,345 c a 5,2 rs | `quickstart_scientific` |
| Kerr a\* = 0,998 progrado (**só analítico**) | ISCO 1,237 M · r₊ 1,063 M · η 32,1 %; limites a\* = 0, ±1 conferidos | `scientific_ref`, `quickstart_scientific` |

### Descoberta registrada (não corrigida)

A vista inicial do `BlackHole3D` sem flags renderiza a metade superior amarela
sólida: a elevação default `π/2` em float deixa a câmera ≈ 2,8 km abaixo do plano
do disco e o raio default (≈ 4,997 rs) fica dentro do anel 2,2–5,2 rs. Verificado
com `BlackHole3D --capture` em llvmpipe
([`images/legacy_default_gpu.png`](images/legacy_default_gpu.png)). Documentado
em [BASELINE.md](BASELINE.md); o baseline **não** foi alterado.

## Ainda não validado

| Item | Situação |
|------|----------|
| GPUs físicas e drivers proprietários (NVIDIA, AMD, Intel) | **Não executado.** `gpu_cpu_agreement` só rodou em Mesa llvmpipe. Diferenças de float/driver em hardware real não foram medidas |
| Desempenho de GPU | **Nenhum número de GPU é afirmado.** llvmpipe é software; não representa hardware |
| Windows (MSVC, drivers Windows) e macOS | **Sem resultado registrado.** O job de CI `scientific-portable` (Windows/MSVC, macOS/Apple Clang; só a árvore científica, sem OpenGL) está configurado, mas não foi executado neste ambiente. `BlackHole3D` em Windows não foi testado; macOS não oferece OpenGL 4.3 compute |
| Blender ≠ 4.0.2 (4.1+, builds oficiais com OpenImageDenoise, EEVEE) | Não testado; o addon declara mínimo 4.0.0 |
| Golden do framebuffer completo 800×600 (grade + composição) | Não há; o `--capture` compara só a textura de compute 200×150 |
| Corpo negro na GPU | Não existe: `blackbody` só no `bh_render_cpu` |
| Kerr (métrica, geodésicas, arrasto de referencial, sombra com spin) | **Não implementado**; só raios analíticos |
| Transferência radiativa, plasma, GRMHD, polarização | Fora do escopo |
| Precisão absoluta da cor do corpo negro vs espectros reais | Não validada; a conversão usa o locus planckiano (Kim et al. 2002) e Reinhard |

## Como reexecutar

```bash
make style-check   # contratos estáticos (segundos, sem build)
make test          # árvore científica, sem OpenGL
make test-gl       # árvore completa; usa xvfb-run + Mesa se não houver display
make shaders       # glslangValidator
make blender-scene # precisa de `blender` no PATH
```

Ou: `cmake --preset scientific && cmake --build --preset scientific && ctest --preset scientific`.
Checklist completo: [CHECKLIST_VALIDACAO.md](CHECKLIST_VALIDACAO.md).

## EN (short)

0.8.0 was validated on a 4-CPU Linux container with **Mesa llvmpipe 25.2.8
(OpenGL 4.5, software)** and **Blender 4.0.2**. All CTest gates pass, including a
real GPU↔CPU golden comparison on llvmpipe. Not validated: hardware GPUs and
their drivers, Windows/macOS, GPU performance, Kerr (analytic radii only), and any
radiative transfer.
