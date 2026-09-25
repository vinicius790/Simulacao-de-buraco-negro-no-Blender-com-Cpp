# Blender adapter (opcional)

## Posição arquitetural

**Blender não é o dono da física.** A pasta [`blender/`](../blender/) (se presente) é só um *frontend* / bridge de parâmetros de cena em torno do núcleo C++/OpenGL e da lib científica.

O build default **não** exige Blender para:

- `bh_scientific` / `test_scientific_ref`
- `BlackHole2D` / `BlackHole3D`

## Layout atual (se presente)

```text
blender/
  README.md
  addons/black_hole_bridge/   # add-on Blender 4.x
  scripts/export_scene_params.py
  examples/scene_params_example.json
```

Ver o README dentro de `blender/` para instalação do add-on e export JSON alinhado aos campos DiskUBO / Camera do baseline.

## Papel

| Responsabilidade | Onde vive |
|------------------|-----------|
| Integração geodésica, métrica, validação | C++/GLSL (+ `bh_scientific`) |
| Editar massa, raios de disco, órbita de câmara; export JSON | Blender bridge |
| Baseline visual realtime | `BlackHole3D` (GLFW/OpenGL) |

## O que não fazer

- Não mover o integrador para Geometry Nodes.
- Não afirmar que um `.blend` “valida” Schwarzschild/Kerr.
- Não misturar fluidos do Blender com claims científicos deste repo.
