# Pipeline de animação — OpenGL frame loop vs bake Blender

## OpenGL (tempo real) — `black_hole.cpp`

```
glfw poll → Camera.update (órbita/zoom)
         → uploadCameraUBO / Disk / Objects (std140)
         → glDispatchCompute(geodesic.comp)   // 200×150, legacyEulerStep
         → memoryBarrier(IMAGE | TEXTURE)
         → draw fullscreen quad (amostra textura)
         → generateGrid (warp Y) + drawGrid (GL_LINES)
         → swap buffers
```

- Integrador GPU default: **`legacyEulerStep`** (1 estágio; nome histórico “RK4” era incorreto).
- Resolução de compute fixa 200×150 mesmo com câmera parada (ternário no-op no shader).
- 60 000 passos por pixel por frame — custo alto; otimizações de engenharia não alteram a aritmética.
- `shaders/geodesic_scientific.comp` tem RK4 real com `rs=1` geométrico — **NÃO** ligado ao `BlackHole3D`.

## Blender (bake / offline)

Blender é **frontend de cena**, não motor de física:

1. Usuário ajusta câmera / disco / objetos no add-on.
2. Export JSON (`black_hole.scene_params/v1`) via `blender/scripts/export_scene_params.py` ou o painel.
3. O binário C++ (ou `bh_scene_dump`) é a fonte de verdade dos defaults.
4. Animações Blender (keyframes de azimuth/elevation/radius) **não** substituem a integração geodésica.

Fluxo típico de “filme”:

| Etapa | Ferramenta | Nota |
|-------|------------|------|
| Layout | Blender add-on | Casa com ESTILO_VISUAL |
| Export | JSON v1 | jq-friendly |
| Render científico futuro | CPU `bh_scientific` / shader científico | Ainda não é o default |
| Render visual baseline | `BlackHole3D` | Euler legado |

## Paridade de teste

- `camera_model.hpp` ↔ `Camera` C++ ↔ campos JSON `camera.*`
- `disk_model.hpp` ↔ `uploadDiskUBO` ↔ `disk.inner_factor_rs` / `outer_factor_rs`
- `test_style_contract.py` trava 2.2 / 5.2 / `SagA_rs` / warp / cor

## Honestidade

- Não há bake de geodésicas dentro do Blender neste pacote.
- Não há export Alembic/USD de raios.
- Gravity no JSON espelha o flag legado; não ativa GRMHD.
