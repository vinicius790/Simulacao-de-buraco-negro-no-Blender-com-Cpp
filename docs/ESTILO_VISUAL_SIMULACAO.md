# Bíblia do estilo visual — baseline legado (travado)

Documento de contrato visual. **Blender e qualquer frontend devem espelhar estes números.**
Testes: `tests/validate_source_invariants.py` + `tests/test_style_contract.py`.

## Resolução (invariantes)

| Item | Valor | Onde |
|------|-------|------|
| Janela | **800 × 600** | `black_hole.cpp` `WIDTH`/`HEIGHT` |
| Compute | **200 × 150** | `COMPUTE_WIDTH`/`COMPUTE_HEIGHT` |
| Fullscreen | `GL_TRIANGLE_STRIP`, 6 verts | quad legado |
| Blend | `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` | compose grid + geodésica |

## Constantes físicas / shader

| Constante | Valor | Arquivo |
|-----------|-------|---------|
| `SagA_rs` | **1.269e10** m | `geodesic.comp` (literal histórico) |
| `D_LAMBDA` | 1e7 | `geodesic.comp` |
| `ESCAPE_R` | 1e30 | `geodesic.comp` |
| Passos | **60000** (parado e em movimento) | `geodesic.comp` |
| Massa Sag A* | 8.54e36 kg | `black_hole.cpp` / `units.hpp` |

## Disco de acreção (anulo geométrico)

**NÃO é Novikov–Thorne.** Só geometria + cor.

| Item | Valor |
|------|-------|
| Raio interno | `rs * 2.2` |
| Raio externo | `rs * 5.2` |
| Espessura UBO | `1e9` m |
| `disk_num` | `2.0` (upload CPU) |
| Hit | cruzamento do plano equatorial (Y muda de sinal) com ρ ∈ [r1, r2] |

### Cor do disco (`geodesic.comp`)

```glsl
float r = length(pos) / disk_r2;   // r_norm
vec3 diskColor = vec3(1.0, r, 0.2);
color = vec4(diskColor, r);        // alpha = r_norm
```

Helpers C++: `include/black_hole/disk_model.hpp` (`legacy_annulus`, `legacy_disk_rgb`).

## Câmera orbital

Espelhada em `include/black_hole/camera_model.hpp` e `black_hole.cpp::Camera`:

| Campo | Default |
|-------|---------|
| `radius` | 6.34194e10 m |
| `azimuth` | 0 |
| `elevation` | π/2 (vista equatorial) |
| FOV Y | 60° → `tanHalfFov = tan(30°)` |
| Aspect | 800/600 |
| Up | +Y; disco no plano XZ |
| Target | sempre (0,0,0) |
| Elevação | clamp (0.01, π−0.01) |
| Zoom | radius ∈ [1e10, 1e12] |

Posição:

```
x = r * sin(e) * cos(a)
y = r * cos(e)
z = r * sin(e) * sin(a)
```

## Grid de spacetime (overlay)

| Item | Valor |
|------|-------|
| Tamanho | 25 × 25 |
| Spacing | 1e10 m |
| Topologia | `GL_LINES` (índices estáticos) |
| Cor | `vec4(0.5, 0.5, 0.5, 0.7)` em `grid.frag` (comentário diz “azul”) |

### Fórmula de warp (`generateGrid`)

Para cada massa, `dist = sqrt(dx² + dz²)`:

- se `dist > rs`: `deltaY = 2 * sqrt(rs * (dist - rs))`; `y += deltaY - 3e10`
- senão (dentro/no horizonte): `y += 2 * rs - 3e10` (poço profundo)

É **estético**, não embedding isométrico. Helper: `bh::camera::grid_warp_delta_y`.

## Compose final

1. Compute escreve RGBA8 200×150 (`imageStore`)
2. Barreira `IMAGE_ACCESS | TEXTURE_FETCH`
3. Quad fullscreen amostra a textura
4. Grid warp desenhado por cima com blending legado
5. Clear color preto `(0,0,0,1)`

## Objetos de cena (defaults)

1. Esfera amarela `(4e11, 0, 0)`, raio 4e10, massa solar
2. Esfera vermelha `(0, 0, 4e11)`, raio 4e10, massa solar
3. BH em `(0,0,0)` com raio `rs`, cor preta

## Gravity flag

- Default **OFF**
- Tecla `G` / RMB liga; **não** muda o integrador geodésico default
- Usado para motion N-body de objetos no loop CPU

## O que Blender DEVE casar

- Fatores de disco 2.2 / 5.2
- Câmera orbital (radius/az/el/FOV)
- Cor do disco `vec3(1, r_norm, 0.2)`
- Warp do grid (fórmula acima)
- Janela 800×600 e compute 200×150 nos metadados de cena
- **Não** reivindicar Kerr, Novikov–Thorne ou RK4 no caminho default

JSON de referência: `examples/scene_params_example.json` (espelhado em `blender/examples/`).
