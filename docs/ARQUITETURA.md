# Arquitetura do engine / Engine architecture (0.8.0)

Três executáveis de render compartilham a mesma descrição de cena
(`black_hole.scene_params/v1`) e as mesmas convenções (Y-up, disco no plano XZ,
linha 0 da imagem = topo):

| Executável | Onde roda | Física | Papel |
|------------|-----------|--------|-------|
| `BlackHole3D` sem flags | GPU (OpenGL 4.3 compute) | `geodesic.comp`, Euler legado | **Default**, baseline travado |
| `BlackHole3D --scientific` / `--relativistic` | GPU | `shaders/geodesic_scientific.comp`, RK4 planar | Modo científico opt-in |
| `bh_render_cpu` | CPU, headless, multithread | `bh_scientific` (RK4/RK45 planar, `double`) | Referência, testes golden, ponte Blender |

## 1. Visão geral

```
                          ┌──────────── Blender addon 0.8.0 (frontend, sem física) ────────────┐
                          │ json_io.py ── export / import ──┐      render_bridge.py ── subprocess │
                          └──────────────┬──────────────────┼───────────────────▲───────────────┘
              scene_params/v1 JSON       │                  │ argv              │ PNG/BMP (stem_NNNN)
              (referencial C++, Y-up)    ▼                  ▼                   │ + 1 linha JSON stdout
 examples/*.json ─────────────► bh::load_scene_params_json (src/scientific/scene_params.cpp)
                                         │                  │                   │
                 ┌───────────────────────┘                  └──────────┐        │
                 ▼                                                     ▼        │
   BlackHole3D (black_hole.cpp, GL)                       bh_render_cpu (tools/bh_render_cpu.cpp)
   GPU: geodesic.comp | geodesic_scientific.comp          CPU: bh::render (cpu_renderer.cpp) ─────┘
                 │                                                     │
                 └──── --capture out.png ───► tools/image_diff.py ◄────┘   (gpu_cpu_agreement)
```

## 2. `BlackHole3D`: CLI → escolha do shader

```
argv ──► parseArgs() ──► RuntimeConfig g_config { scientific, relativistic, sceneLoaded, capturePath, scene }
           │   (nenhuma flag)   → tudo falso: baseline histórico exato
           │   --scene f.json   → load_scene_params_json → applySceneToGlobals()
           │                      (SagA massa/posição, câmera raio/azimute/elevação/FOV/alvo, objects[], Gravity)
           │   --scientific     → scientific = true
           │   --relativistic   → scientific = true, relativistic = true
           │   --capture out    → capturePath = out
           │   --help / -h      → uso, código 0 · flag desconhecida ou --scene inválido → código 2
           ▼
   Engine engine;   // construído DEPOIS da CLI, para o programa de compute casar com o modo
           │
           ├── scientific == false ─► CreateComputeProgram("geodesic.comp")
           │                           legacyEulerStep · metros · SagA_rs 1.269e10 · D_LAMBDA 1e7
           │                           60 000 passos · ESCAPE_R 1e30 · teste de disco pontual
           │                           UBOs: 1 Camera (80 B) · 2 Disk (16 B) · 3 Objects (784 B)
           │
           └── scientific == true ──► CreateComputeProgram("geodesic_scientific.comp")
                                       RK4 planar (θ ≡ π/2) · rs = 1 (unitScale = 1 / r_s_m)
                                       colisões contínuas · escape por prova · semente na câmera estática
                                       UBOs 1, 2, 3 (posições divididas por rs; disco = fatores da cena;
                                       Objects SEM o marcador do BH — bh::is_black_hole_marker)
                                       + 4 SciParams (32 B, static_assert) ← uploadSciUBO()
```

Os shaders são carregados do diretório do executável: o `POST_BUILD` do CMake copia
`geodesic.comp`, `grid.vert`, `grid.frag` (raiz) e `shaders/geodesic_scientific.comp`
para junto do `BlackHole3D`.

### UBO `SciParams` (binding 4, só no modo científico)

Layout std140 de 32 bytes, espelhado em `SciParamsUBOData` (`black_hole.cpp`) e
conferido campo a campo pelo teste `shader_contract`:

| Offset | Campo GLSL | Tipo | Valor enviado pelo host | Significado |
|--------|------------|------|-------------------------|-------------|
| 0 | `colorMode` | `int` | 0 (`--scientific`) / 1 (`--relativistic`) | 0 = paleta legada `vec4(1, r, 0.2, r)`; 1 = paleta legada × Doppler/redshift (g⁴, Reinhard) |
| 4 | `maxSteps` | `int` | 4000 | Limite de passos RK4 por raio |
| 8 | `stepK` | `float` | 0.02 | Passo geométrico `dλ = clamp(stepK·r, stepMin, stepMax)` |
| 12 | `stepMin` | `float` | 0.005 | rs |
| 16 | `stepMax` | `float` | 2.0 | rs |
| 20 | `sceneBound` | `float` | `max(r_ext, \|c_i\| + R_i)·1,05 + 0,5` | Raio (rs) além do qual nada pode ser atingido (escape) |
| 24 | `exposure` | `float` | 2.0 | Exposição do Reinhard (`colorMode` 1) |
| 28 | `spinSign` | `float` | +1 | Sentido de rotação do disco em torno de +Y |

Esses valores são fixos no código do `BlackHole3D`; só o `bh_render_cpu` os expõe
como flags (`--max-steps`, `--exposure`, `--spin-sign`).

### Frame loop (os dois modos)

```
glClear(preto) ─► física N-corpos legada (só com Gravity ON) ─► regenera grade se objetos moveram
   ─► drawGrid(viewProj)                       GL_LINES, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA
   ─► dispatchCompute(camera)
        uploadCameraUBO (todo frame) · Disk/Objects só quando "dirty" · SciParams junto com Objects
        glBindImageTexture(0, RGBA8 200×150) · glDispatchCompute(⌈200/16⌉, ⌈150/16⌉, 1)
        glMemoryBarrier(SHADER_IMAGE_ACCESS | TEXTURE_FETCH)
   ─► drawFullScreenQuad()                     GL_TRIANGLE_STRIP, 6 vértices, blending herdado:
                                               onde a textura tem alpha 0 (fundo), a grade aparece
   ─► --capture? glGetTexImage → vira linhas (linha 0 = topo) → RGB·alpha sobre preto
                 → write_png/bmp/ppm → sai do loop (código 0; falha de gravação → 1)
   ─► glfwSwapBuffers / glfwPollEvents
```

A captura lê **só a textura de compute** (200×150), não o framebuffer 800×600:
a grade não aparece nela. É a imagem comparada com o `bh_render_cpu`.

Sem `--scene`, a câmera usa os defaults travados (6,34194e10 m, azimute 0,
elevação π/2, FOV 60°, alvo na origem). Com `--scene`, FOV e alvo também vêm do
JSON (`fov_y_deg`, `target_m`), tanto no `uploadCameraUBO` quanto na projeção da
grade.

O marcador do buraco negro (a esfera preta default na origem) só é enviado ao
shader legado. O modo científico e o `bh_render_cpu` o descartam com o mesmo
predicado (`bh::is_black_hole_marker`: centrado no buraco e com raio a 5 % de rs
ou massa ≥ metade da do buraco), porque o teste de horizonte desenha o buraco de
forma exata.

## 3. `bh_render_cpu`: pipeline do renderer CPU

```
argv ─► Options { width 200, height 150, mode, integrator, threads, supersample, exposure,
                  gamma, mdot_edd, spin_sign, stars, max_steps, scene }
   ├─ --scene → load_scene_params_json ; flags --azimuth/--elevation/--radius-rs/--fov-y-deg sobrescrevem
   ▼
para cada frame f (--frames N: azimute += 2π·T·f/N ; elevação → --elevation-end):
   make_geometric_scene   metros → rs; esferas (marcador do BH fora: is_black_hole_marker); scene_bound
   make_camera_frame      base right/up/forward legada (Y-up); polo exato → up alternativo
   render()               threads puxam linhas de um contador atômico (determinístico)
     └─ por pixel, S×S sub-amostras:
          primary_ray_direction ─► trace_ray:
             make_planar_ray       semente no referencial da câmera estática; plano orbital
             loop ≤ max_steps:     rk4 (dλ = clamp(0,02·r, 0,005, teto)) | rk45 (Dormand–Prince, tol 1e-8)
                                   teto = 2 rs até 1,05·r_ext (paridade GPU); além disso cresce com r
               segmento prev→cur:  horizonte (segmento–esfera) · disco (segmento–plano interpolado,
                                   anel [r_int, r_ext]; 1º segmento ignorado se |y| < 1e-6·r na câmera)
                                   · esferas · o mais próximo vence
               disco atingido     → g = doppler_gravitational_factor_at(ρ, E, L_y, r_cam, spin)
               will_escape_scene  → escapou (dr > 0, r > 1,5 rs, r > scene_bound)
          shade: legacy | relativistic | blackbody  (horizonte/limite = preto; estrelas opcionais)
   image_io::write_image  PNG (stored deflate) | BMP | PPM, gamma opcional
stdout ◄─ 1 linha JSON (frações, mean_steps, min_g/max_g, seconds, out)
```

Manual completo: [RENDER_CPU.md](RENDER_CPU.md).

## 4. Biblioteca científica `bh_scientific` (sem OpenGL)

```
include/black_hole/*.hpp  ◄── API (unidades, métrica, órbitas, redshift, emissão, colisões, escape,
        ▲                         integradores, plano orbital, Kerr analítico, câmera, disco, cena,
        │                         renderer CPU, I/O de imagem)
src/scientific/*.cpp  ──► libbh_scientific.a (+ Threads)
        ▲
        ├── tests/test_scientific_ref.cpp, tests/test_cpu_render.cpp
        ├── tools/bh_render_cpu.cpp, tools/bh_scene_dump.cpp, examples/quickstart_scientific.cpp
        └── black_hole.cpp (scene_params + image_io, para --scene e --capture)
```

| Header | Conteúdo |
|--------|----------|
| `units.hpp`, `schwarzschild.hpp` | Constantes SI, Sgr A\*, rs, esfera de fótons, ISCO |
| `orbits.hpp` | Órbitas circulares tipo-tempo: Ω, u^t, Ẽ, L̃, v_loc, η = 1 − √(8/9) |
| `redshift.hpp` | g do disco (Doppler + gravitacional), g estático, I ∝ g⁴, T_obs = g·T_emit |
| `disk_emission.hpp` | Fluxo Page–Thorne em forma fechada, Eddington, corpo negro → sRGB linear, Reinhard |
| `hit_testing.hpp` | Colisões contínuas segmento–plano / segmento–esfera |
| `escape.hpp` | b_c = (3√3/2) rs, escape por prova, raio angular da sombra |
| `integrator_rk4.hpp`, `integrator_rk45.hpp` | RK4 clássico (RHS congela dentro do horizonte) e Dormand–Prince 5(4) |
| `planar_geodesic.hpp` | Integração no plano orbital (sem singularidade polar) |
| `weak_field.hpp` | Deflexão de 1.ª e 2.ª ordem |
| `kerr_analytic.hpp` | Raios **analíticos** de Kerr (BPT) — Kerr não é simulado |
| `camera_model.hpp`, `disk_model.hpp`, `scene_params.hpp` | Espelho da câmera/disco legados e JSON v1 |
| `cpu_renderer.hpp`, `image_io.hpp` | Renderer headless e gravadores PNG/BMP/PPM |

## 5. Ponte com o Blender (duas direções)

```
 Blender → C++                                         C++ → Blender
 ─────────────                                         ─────────────
 painel do addon (câmera, disco, animação, render)     bh_render_cpu grava PNG/BMP (ou stem_NNNN)
   │ json_io.params_from_settings                         │ stdout: 1 linha JSON
   ▼                                                      ▼
 scene_params/v1 (JSON, metros + fatores de rs,        render_bridge.import_render_background
   referencial C++ Y-up)                                 → fundo da BH_OrbitCamera (sequência → SEQUENCE)
   │                                                   render_bridge.import_render_as_plane
   ├──► BlackHole3D --scene f.json [--scientific]        → plano emissivo do tamanho do FOV
   └──► bh_render_cpu --scene f.json --frames 1 …     Scene["bh_last_render_summary"] ← JSON + command
        (operador "Render via C++ (CPU)": um processo
         por frame, com a pose real da câmera animada)
```

- Conversão de eixos só em `constants.cpp_to_blender`: (x, y, z) C++ → (x, −z, y)
  Blender (rotação própria de +90° em X). Os helpers puros e o JSON ficam no
  referencial C++.
- O Blender **não** integra geodésicas nem curva a luz: monta a geometria com a
  paleta travada (disco `(1, r, 0.2)·r`, grade 0,35, guias, horizonte preto) e
  importa as imagens calculadas pelo C++.
- Detalhes: [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md).

## 6. Separação de modos

| Modo | Entrada | Integrador | Colisões / escape | Status |
|------|---------|------------|-------------------|--------|
| Baseline visual | `BlackHole3D` sem flags → `geodesic.comp` | Euler 1 estágio (`legacyEulerStep`) | pontual / `ESCAPE_R = 1e30` | **Default**, travado |
| Científico GPU | `BlackHole3D --scientific` | RK4 planar, `float` | contínuas / por prova | Opt-in |
| Relativístico GPU | `BlackHole3D --relativistic` | idem + Doppler/redshift | idem | Opt-in |
| Científico CPU | `bh_render_cpu --mode legacy\|relativistic\|blackbody` | RK4 geométrico ou RK45, `double` | idem | Referência / testes |
| Kerr | — | — | — | **Não implementado** (só raios analíticos) |

O modo `blackbody` (Page–Thorne) existe só no CPU.

## 7. Sincronização CPU ↔ GPU (std140)

Layouts host explícitos em `black_hole.cpp`, todos com `static_assert`:

| Struct | Binding | Bytes | Shader |
|--------|---------|-------|--------|
| `CameraUBOData` | 1 | 80 | os dois |
| `DiskUBOData` | 2 | 16 | os dois |
| `ObjectsUBOData` (`vec4 mass[16]` stride) | 3 | 784 | os dois |
| `SciParamsUBOData` | 4 | 32 | só `geodesic_scientific.comp` |

`shader_contract` verifica que todo binding usado pelo host existe no shader
carregado e que o shader legado **não** declara `SciParams`/binding 4.
`source_invariants` protege as constantes e a forma do shader legado.

## 8. Targets CMake

| Target | Deps GL | Descrição |
|--------|---------|-----------|
| `bh_scientific` | Não | Lib de referência (+ `Threads`) |
| `test_scientific_ref` | Não | Testes headless da física |
| `test_cpu_render` | Não | Testes do renderer CPU |
| `bh_render_cpu` | Não | Renderer CPU (CLI) |
| `quickstart_scientific` | Não | Tabela de números de Sgr A\* |
| `bh_scene_dump` | Não | Dump/regravação do JSON de cena |
| `BlackHole2D` | Sim | Lensing 2D legado |
| `BlackHole3D` | Sim | Visual 3D (default = baseline; flags opt-in) |
| `validate_shaders` | glslang | `grid.vert`, `grid.frag`, `geodesic.comp`, `shaders/geodesic_scientific.comp` |

Flag: `BLACK_HOLE_BUILD_GL` (ON por padrão). Os jobs de CI `scientific-headless` e
`scientific-portable` (Windows/macOS) usam `OFF`; o `linux-build` usa `ON` com Mesa llvmpipe + Xvfb. `include(CTest)` fica no
topo do `CMakeLists.txt` para que `gpu_cpu_agreement` (registrado dentro do bloco
GL) seja de fato adicionado.

## 9. Makefile

Atalhos opcionais sobre CMake/CTest — ver [ROTEIRO_BUILD.md](ROTEIRO_BUILD.md#atalhos-do-makefile).
`make test` (árvore científica), `make test-gl` (árvore completa, inclui
`gpu_cpu_agreement`), `make render`, `make package-addon`, `make style-check`.
