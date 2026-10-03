# Pipeline de animação — OpenGL em tempo real, sequências CPU e bake no Blender (0.8.0)

Há três jeitos de produzir movimento. Só o primeiro é o default; os outros dois são
**opt-in** e não mudam o estilo travado ([ESTILO_VISUAL_SIMULACAO.md](ESTILO_VISUAL_SIMULACAO.md)).

| Caminho | Física | Saída | Quando usar |
|---------|--------|-------|-------------|
| `BlackHole3D` (tempo real) | `geodesic.comp` legado (default) ou `geodesic_scientific.comp` (`--scientific`/`--relativistic`) | janela 800×600 | exploração interativa |
| `bh_render_cpu --frames N` | RK4/RK45 planar em `double` | `stem_0000.png …` | sequências determinísticas, offline, sem GPU |
| Addon Blender | nenhuma (geometria + paleta) | keyframes + render Cycles/EEVEE | layout, câmera, composição; fundo calculado pelo C++ |

## 1. OpenGL (tempo real) — `black_hole.cpp`

```
glfwPollEvents → Camera (órbita arrastando com botão esquerdo/do meio, zoom no scroll; Gravity: tecla G ou botão direito pressionado)
  → física N-corpos legada dos objetos (só com Gravity ON) → regenera a grade se algo moveu
  → drawGrid (GL_LINES, warp em Y, blending SRC_ALPHA)
  → uploadCameraUBO (todo frame) · Disk / Objects (/ SciParams) só quando "dirty"
  → glDispatchCompute(programa escolhido pela CLI)   // 200×150, grupos 16×16
  → glMemoryBarrier(IMAGE_ACCESS | TEXTURE_FETCH)
  → drawFullScreenQuad (amostra a textura; blending herdado sobre a grade)
  → glfwSwapBuffers
```

| Item | Sem flags (default) | `--scientific` / `--relativistic` (opt-in) |
|------|---------------------|---------------------------------------------|
| Shader | `geodesic.comp` | `shaders/geodesic_scientific.comp` |
| Integrador | `legacyEulerStep` (1 estágio; o nome histórico "RK4" era incorreto) | RK4 clássico no plano orbital |
| Orçamento | 60 000 passos por pixel por frame, `D_LAMBDA = 1e7` m | até 4000 passos, `dλ = clamp(0,02·r, 0,005, 2)` rs |
| Disco / escape | pontual / `ESCAPE_R = 1e30` | contínuo / por prova |
| Cor do disco | `vec4(1, r, 0.2, r)` | `--scientific`: igual; `--relativistic`: × Doppler/redshift g⁴, Reinhard |

- A resolução de compute é fixa em 200×150 mesmo com a câmera parada (o ternário
  `moving` do shader é no-op).
- Otimizações de engenharia não alteram a aritmética do baseline
  ([ENGINEERING_UPGRADE.md](ENGINEERING_UPGRADE.md)).
- `--capture out.png` renderiza **um** frame e sai: serve para regressão, não para
  animação. Para capturar várias poses no GPU, gere uma cena JSON por pose
  (`--scene`), como faz `tests/test_gpu_cpu_agreement.py`.
- A vista inicial sem flags tem o artefato documentado da metade de cima amarela
  ([BASELINE.md](BASELINE.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)); comece
  animações a partir de outra elevação.

## 2. Sequências offline — `bh_render_cpu --frames N`

```bash
B=build/scientific/bh_render_cpu
mkdir -p build/renders

# Turntable: azimute az₀ + 2π·f/N (frame N repetiria o 0 → loop sem emenda)
$B --out build/renders/turn.png --width 320 --height 240 --elevation 1.25 --frames 120 --quiet

# Varredura de elevação sem girar (azimute fixo), cena showcase, modo relativístico
$B --scene examples/scene_relativistic_showcase.json --mode relativistic --stars \
   --width 320 --height 180 --frames 30 --azimuth-turns 0 --elevation-end 1.00 \
   --out build/renders/sweep.png --quiet

# Montar um GIF (stdlib, paleta global, ida e volta)
python3 tools/frames_to_gif.py "build/renders/sweep_*.png" -o build/renders/sweep.gif --fps 20 --pingpong
```

- Nomes: `stem_0000.ext … stem_{N−1}.ext` (4 dígitos), formato que o Blender
  reconhece como *image sequence*.
- `--azimuth-turns T` (padrão 1) define as voltas; `--elevation-end` interpola a
  elevação linearmente até o frame N−1.
- Cada frame é determinístico (idêntico para qualquer `--threads`).
- A física é a mesma do modo científico GPU, em `double`. Manual completo:
  [RENDER_CPU.md](RENDER_CPU.md).

## 3. Blender (bake / offline) — frontend, não motor de física

Blender monta a cena de referência e anima a câmera; **não** integra geodésicas
nem curva a luz.

1. **Build Full Scene** (coleções `BH_*`, Cycles, view transform **Raw** para
   paridade de cor). Com a pose default (4,997 rs, elevação π/2) o addon avisa que
   a câmera está **dentro da laje do disco**; use **Elevação 1,25**.
2. **Guias** (opcional): anéis em 1,5 rs (esfera de fótons), 2,598 rs (parâmetro de
   impacto crítico b_c, borda da sombra vista de longe) e 3 rs (ISCO), cinza da
   grade, como curvas com bevel (visíveis em Cycles/EEVEE).
3. **Câmera / animação → Preset + Easing → Bake Camera Animation:**

   | Preset | Raio | Azimute | Elevação |
   |--------|------|---------|----------|
   | `TURNTABLE` (padrão, igual à 0.7.x) | r | az₀ + 2π·t | el |
   | `ELEVATION_SWEEP` | r | az₀ | el_ini → el_fim |
   | `DOLLY` | r → r_fim (padrão 12 rs) | az₀ | el |
   | `SPIRAL` | r | az₀ + 2π·t | el_ini → el_fim |

   Easing `LINEAR` (t) ou `SINE` (0,5 − 0,5·cos πt), aplicado antes do preset.
   Padrões: 24 fps, 8 s, elevação 0,35 → π − 0,35.
4. **Disco (spin)** (opcional): keyframes de rotação da textura (`Voltas do disco`)
   e **turbulência** que modula só o **brilho**, nunca a cor — **desligada por
   padrão** (estilo travado).
5. **Render via C++ (CPU)** (opcional): o addon exporta o JSON e, para cada frame
   da cena, roda `bh_render_cpu --frames 1` com a pose **real** da câmera animada
   naquele frame (raio, azimute, elevação e FOV). O resultado (`stem_NNNN.png`) é
   importado como fundo da câmera ou plano emissivo, como *image sequence*
   alinhada quadro a quadro com qualquer preset/easing.
6. **Render Animation.** No Ubuntu, desligue o denoising do Cycles (o pacote da
   distro vem sem OpenImageDenoise).

A varredura interna da CLI (`--frames N`: `azimute = az₀ + 2π·T·f/N`, elevação
linear opcional) é independente do Blender e serve para sequências feitas só no
C++. O bake do Blender amostra os presets em `t = (frame − 1)/(N − 1)` com easing;
por isso o addon **não** usa `--frames N` e chama o renderer frame a frame com a
pose da câmera.

Detalhes: [`blender/docs/ANIMACAO.md`](../blender/docs/ANIMACAO.md),
[BLENDER_ADAPTER.md](BLENDER_ADAPTER.md).

### Fluxo típico de "filme"

| Etapa | Ferramenta | Nota |
|-------|------------|------|
| Layout de cena e câmera | addon Blender | paleta travada, guias opcionais |
| Export | JSON `black_hole.scene_params/v1` | referencial C++ Y-up |
| Frames com lente gravitacional | `bh_render_cpu --frames N` (ou pelo painel do addon) | `legacy` = estilo travado; `relativistic` / `blackbody` = opt-in |
| Composição | Blender (fundo da câmera / plano emissivo) | sem lensing no Blender |
| Visual em tempo real do baseline | `BlackHole3D` | Euler legado |
| GIF rápido | `tools/frames_to_gif.py` | stdlib |

## Paridade de teste

- `camera_model.hpp` ↔ `Camera` C++ ↔ campos JSON `camera.*` ↔ `constants.camera_position` no addon
- `disk_model.hpp` ↔ `uploadDiskUBO` ↔ `disk.inner_factor_rs` / `outer_factor_rs`
- `test_style_contract.py` trava 2,2 / 5,2 / `SagA_rs` / warp / cor
- `test_blender_addon_parity.py` confere os endpoints dos presets, o easing e o contrato da CLI do renderer
- `test_render_cli.py` confere que `--elevation-end` / `--azimuth-turns` reproduzem exatamente os renders de frame único nas pontas da varredura

## Honestidade

- Não há bake de geodésicas dentro do Blender; o Blender não curva a luz.
- Não há export Alembic/USD de raios.
- `gravity` no JSON espelha o flag legado de N-corpos dos objetos; não ativa GRMHD
  nem muda o integrador.
- Kerr não é simulado em nenhum caminho.
