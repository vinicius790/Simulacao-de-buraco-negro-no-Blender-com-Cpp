# Arquitetura do engine / Engine architecture

## Pipeline padrão (OpenGL 4.3 compute) — baseline

```
CPU camera / disk / objects
        │
        ▼
   UBOs (std140)  ──►  geodesic.comp (compute)
        │                    │
        │                    ▼
        │              imageStore → texture RGBA8 (200×150)
        │                    │
        │                    ▼  memory barrier
        │              fullscreen quad (grid.vert/frag sample)
        │                    +
        └──────────►  spacetime grid overlay (VAO)
                           │
                           ▼
                      framebuffer 800×600
```

1. **CPU** atualiza câmera (órbita/pan/zoom) e marca UBOs dirty.
2. **Camera / Disk / Objects** sobem via `glBufferSubData` (layouts std140 explícitos).
3. **Compute shader** `geodesic.comp` integra geodésicas nulas com **`legacyEulerStep`**.
4. Resultado em textura; barreira `IMAGE_ACCESS | TEXTURE_FETCH`.
5. Pass fullscreen + grid compositado com blending legado.

## Biblioteca científica (sem OpenGL)

```
include/black_hole/*.hpp
        ▲
src/scientific/*.cpp  →  lib bh_scientific
        ▲
tests/test_scientific_ref.cpp
examples/quickstart_scientific.cpp
```

Não participa do caminho de render default. Serve como referência CPU e porta de testes headless / CI.

## Separação de modos

| Modo | Entrada | Integrador | Status |
|------|---------|------------|--------|
| Baseline visual | `geodesic.comp` | Euler 1-stage | **Default** |
| Científico CPU | `bh_scientific` | RK4 4-stage | Testes / referência |
| Científico GPU | `shaders/geodesic_scientific.comp` | (stub) | Não ligado |

## Blender

`blender/` exporta JSON de parâmetros. **Não** executa a física.


## Scene JSON I/O (aditivo)

```
examples/scene_params_example.json
        ▲
include/black_hole/scene_params.hpp
src/scientific/scene_params.cpp   →  parser hand-rolled (schema restrito)
        ▲
tools/bh_scene_dump.cpp           →  imprime defaults / carrega JSON
```

**Não** está ligado ao loop de `black_hole.cpp`. Serve Blender, testes e ferramentas CLI.

Schema: `black_hole.scene_params/v1` — câmera, disco (fatores rs), gravity, objects[], metadados 800×600 / 200×150.

## Targets CMake

| Target | Deps GL | Descrição |
|--------|---------|-----------|
| `bh_scientific` | Não | Lib de referência |
| `test_scientific_ref` | Não | Testes headless |
| `quickstart_scientific` | Não | Exemplo CLI |
| `bh_scene_dump` | Não | Dump JSON de cena |
| `BlackHole2D` | Sim | Lensing 2D legado |
| `BlackHole3D` | Sim | Visual 3D default |
| `validate_shaders` | glslang | Opcional |

Flag: `BLACK_HOLE_BUILD_GL` (ON por padrão). CI científico usa `OFF`.

## Makefile

- `make test` / `make test-all` — invariantes + style-check + ctest
- `make style-check` — `tests/test_style_contract.py`
- `make build` — configure científico + build

## Sincronização CPU ↔ GPU (std140)

Layouts host explícitos em `black_hole.cpp`:

- `CameraUBOData` 80 bytes
- `DiskUBOData` 16 bytes
- `ObjectsUBOData` 784 bytes (`vec4 mass[16]` stride)

Qualquer drift quebra o visual; `static_assert` e invariantes protegem o baseline.
