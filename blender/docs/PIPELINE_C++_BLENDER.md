# Pipeline C++ ↔ JSON ↔ Blender

```
black_hole.cpp / geodesic.comp   ← física + lensing (dono)
        ↑ campos DiskUBO / Camera
scene_params JSON  (schema black_hole.scene_params/v1)
        ↑↓ add-on Black Hole Bridge
Blender cena estilo-travado (preview artístico)
```

| Responsabilidade | Onde |
|------------------|------|
| Integração geodésica, captura, cor GPU disco | C++ / `geodesic.comp` |
| Preview: horizonte, disco âmbar, grade, órbita | Blender addon |
| Troca de parâmetros | JSON export/import no painel |

Unidades Blender: `rs_geo=1`; metros = unidades × `r_s_m`.
Estilo: `docs/ESTILO_VISUAL_SIMULACAO.md`.
O binário C++ **não** carrega o JSON automaticamente (ainda).
