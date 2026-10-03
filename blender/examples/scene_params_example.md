# `scene_params_example.json` — campos

Arquivo de comentários para `blender/examples/scene_params_example.json`, já
que JSON puro não aceita comentários. Schema **`black_hole.scene_params/v1`**,
o mesmo que o addon exporta (`json_io.build_params_dict`) e que os binários C++
leem com `--scene`.

```bash
./build/scientific/bh_render_cpu --scene blender/examples/scene_params_example.json --out out.png
./build/gl/BlackHole3D --scene blender/examples/scene_params_example.json
```

Legenda: **C++** = lido por `bh::load_scene_params_json` (`BlackHole3D --scene`,
`bh_render_cpu --scene`); **Import** = lido pelo **Import Scene Params JSON**
do addon; **Export** = escrito pelo addon.

| Campo | Valor no exemplo | C++ | Import | Export | Observação |
|-------|------------------|:---:|:------:|:------:|------------|
| `schema` | `black_hole.scene_params/v1` | ✔ (validado) | — | ✔ | |
| `notes` | texto | — | — | ✔ | |
| `gravity` | `false` | ✔ | — | — | flag Gravity legada (tecla G) |
| `black_hole.name` | `Sagittarius A*` | ✔ | — | ✔ | |
| `black_hole.mass_kg` | 8.54e36 | ✔ | ✔ | ✔ | |
| `black_hole.r_s_m` | 1.269e10 | ✔ | — | ✔ | o addon grava 2GM/c² ≈ 1.26839e10 |
| `black_hole.position_m` | [0, 0, 0] | ✔ | — | ✔ | |
| `disk.inner_factor_rs` / `outer_factor_rs` | **2.2 / 5.2** | ✔ | ✔ | ✔ | fatores de rs; 2.2 rs fica dentro da ISCO (3 rs) |
| `disk.thickness_m` | 1e9 | ✔ | ✔ | ✔ | ≈ 0.079 rs; no Blender vira o Solidify do disco |
| `disk.disk_num` | 2.0 | ✔ | ✔ | ✔ | |
| `disk.inner_radius_m` / `outer_radius_m` | 2.7918e10 / 6.5988e10 | aceito, ignorado | — | ✔ | os fatores têm precedência |
| `disk.spin_turns` / `turbulence` | 1.0 / 0.0 | — | ✔ | ✔ | spin do UV; turbulência 0 = desligada |
| `guides.*` | 1.5 / 2.598… / 3.0 rs | — | — | ✔ | esfera de fótons, b_c, ISCO (`schwarzschild.hpp`); informativo |
| `camera.radius_m` | 6.34194e10 | ✔ | ✔ | ✔ | ≈ 4.997 rs, padrão travado |
| `camera.radius_rs` | 4.9976 | — | ✔ | ✔ | sem ele, o import usa `radius_m / rs` |
| `camera.azimuth_rad` | 0 | ✔ | ✔ | ✔ | |
| `camera.elevation_rad` | π/2 | ✔ | ✔ | ✔ | ângulo polar a partir de +Y (C++); π/2 = no plano do disco |
| `camera.fov_y_deg` | 60 | ✔ | ✔ | ✔ | `BlackHole3D` e `bh_render_cpu` |
| `camera.target_m` | [0, 0, 0] | ✔ | — | ✔ | |
| `objects[]` | 2 esferas (amarela, vermelha) + buraco negro | ✔ (até 16) | — | — | ausente → o C++ usa os 3 objetos legados; `[]` → nenhum objeto |
| `animation.mode` | `turntable` | — | ✔ | ✔ | `turntable` / `elevation_sweep` / `dolly` / `spiral` (`turntable_azimuth` da 0.7.x também é aceito) |
| `animation.easing` | `linear` | — | ✔ | ✔ | `linear` / `sine` |
| `animation.fps` / `duration_s` | 24 / 8.0 | — | ✔ | ✔ | 192 frames |
| `animation.elev_start_rad` / `elev_end_rad` | 0.35 / π − 0.35 | — | ✔ | ✔ | `ELEVATION_SWEEP` e `SPIRAL` |
| `animation.radius_end_rs` | 12.0 | — | ✔ | ✔ | `DOLLY` |
| `animation.azimuth_start_rad` / `azimuth_end_rad` | 0 / 2π | — | — | ✔ | informativo |
| `render.width` / `height` | 800 / 600 | — | ✔ | ✔ | painel **Render C++ → Blender** |
| `render.mode` | `legacy` | — | ✔ | ✔ | `legacy` / `relativistic` / `blackbody` |
| `render.integrator` | `rk4` | — | ✔ | ✔ | `rk4` / `rk45` |
| `render.frames` / `supersample` | 1 / 1 | — | ✔ | ✔ | |
| `render.tool` | `bh_render_cpu` | — | — | ✔ | |
| `render_baseline.*` | window [800, 600], compute [200, 150], `legacyEulerStep` | ✔ (informativo) | — | ✔ | não muda o render |
| `style_lock` | `docs/ESTILO_VISUAL_SIMULACAO.md` | — | — | ✔ | |

Chaves desconhecidas são ignoradas dos dois lados. Campos ausentes mantêm os
padrões de cada lado.

## Notas

- **A câmera do exemplo é a pose padrão do C++** (4.997 rs, elevação π/2),
  que fica dentro do disco. Com ela, `BlackHole3D` mostra a metade de cima do
  primeiro frame amarela e o Blender avisa "camera is inside the accretion
  disk". Para um render limpo, troque `camera.elevation_rad` para `1.25`. Para
  a vista icônica com lensing, use `examples/scene_relativistic_showcase.json`
  (disco 3–12 rs, câmera a 18 rs, elevação 1.40, FOV 38°).
- `camera.*` está no referencial **C++** (Y-up). O addon converte para o
  Blender (Z-up) com `cpp_to_blender` só na hora de posicionar a câmera, então
  o JSON nunca carrega coordenadas do Blender.
- No painel **Render C++ → Blender**, as flags `--azimuth`, `--elevation`,
  `--radius-rs` e `--fov-y-deg` (lidas da câmera animada em cada frame) são
  passadas explicitamente e têm precedência sobre `camera.*` do JSON.
- Exportador mínimo sem Blender: `python3 blender/scripts/export_scene_params.py --out x.json`
  (só `black_hole`, `disk`, `camera`, `render_baseline`).
