# scene_params_example.json — campos

Sidecar de comentários (JSON puro não tem comentários).

- `schema`: `black_hole.scene_params/v1`
- `units`: mapeamento `rs_geo=1` ↔ `r_s_m`
- `black_hole.mass_kg` / `r_s_m` — Sag A* baseline
- `disk.inner_factor_rs` / `outer_factor_rs` — **2.2 / 5.2**
- `disk.color_model` — `vec3(1.0, r, 0.2)` (geodesic.comp)
- `grid.color_rgba` — `(0.5,0.5,0.5,0.7)`; warp = generateGrid
- `camera.*` — radius / azimuth / elevation (fórmula C++)
- `animation` — turntable azimuth
- `render_baseline` — window/compute do OpenGL
- `style_lock` — aponta a `docs/ESTILO_VISUAL_SIMULACAO.md`
