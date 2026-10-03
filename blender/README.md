# Black Hole Bridge 0.8.0 — frontend Blender

Addon opcional para **Blender 4.x** que monta, no Blender, a mesma cena do
simulador C++/OpenGL (Sagittarius A\*, Schwarzschild) e traz para dentro dele
as imagens calculadas pelo renderer C++ `bh_render_cpu`.

> **A física fica no C++.** O Blender não curva a luz: a cena dele é uma
> pré-visualização geométrica com paleta travada. Lensing, sombra, anel de
> fótons e Doppler vêm das imagens do C++. Kerr não é simulado.

**Guia completo (instalação, uso, headless): [INSTALAR_E_RODAR.md](INSTALAR_E_RODAR.md).**

## Galeria: mesma pose, dois mundos

| Blender (addon, Cycles) | C++ (`bh_render_cpu`, paleta legada) |
|-------------------------|--------------------------------------|
| ![Blender Cycles, elevação 1.25](../docs/images/blender_el125_cycles.png) | ![bh_render_cpu, elevação 1.25](../docs/images/scientific_el125.png) |
| `docs/images/blender_el125_cycles.png` — Blender 4.0.2, elevação 1.25 rad: disco âmbar `(1, r, 0.2)·r`, grade cinza deformada, anéis-guia, horizonte preto. Geometria euclidiana, **sem lensing**. | `docs/images/scientific_el125.png` — mesma pose (elevação 1.25) no C++: o lado de trás do disco aparece arqueado sobre a sombra e surge o anel de fótons. |

Outras imagens do projeto: [`showcase_relativistic.png`](../docs/images/showcase_relativistic.png)
(cena `examples/scene_relativistic_showcase.json`, modo relativístico) e
[`legacy_default_gpu.png`](../docs/images/legacy_default_gpu.png) (pose
padrão do OpenGL com a metade de cima amarela, explicada abaixo).

## Começo rápido

1. **Edit → Preferences → Add-ons → Install…** (4.0/4.1) ou **Install from
   Disk…** (4.2+, o zip traz `blender_manifest.toml`) →
   `blender/addons/black_hole_bridge.zip` → ative **Black Hole Bridge**.
2. **3D Viewport → N → aba Black Hole → Cena → Build Full Scene.**
3. **Parâmetros → Elevação = 1.25** → Build de novo (sai do aviso "câmera
   dentro do disco").
4. **Câmara / animação → Bake Camera Animation** → Play.
5. *(Opcional)* `make build` na raiz → **Render C++ → Blender → Renderer
   path** = `build/scientific/bh_render_cpu` → **Render via C++ (CPU)**.

## O que a 0.8.0 faz

| Recurso | Resumo |
|---------|--------|
| **Build Full Scene** | Horizonte preto (rs = 1), disco 2.2–5.2 rs com espessura travada 1e9 m ≈ 0.079 rs (Solidify), grade 25×25 deformada (quads + Wireframe), câmera em órbita com FOV **vertical** 60° (`Camera::position()`), mundo preto, Cycles |
| **Eixos** | C++ Y-up → Blender Z-up por uma única função, `constants.cpp_to_blender`: (x, y, z) → (x, −z, y), rotação própria de +90° em X (det +1), então o enquadramento é idêntico ao do C++ |
| **Paridade de cor** | View transform **Raw**; disco = Emission `(1, r, 0.2)·r`, exatamente o pixel OpenGL `vec4(1, r, 0.2, r)` misturado sobre preto, usando os fatores reais do disco; grade 0.35 (= 0.5 · 0.7); `disk_glow` 1.0 = paridade, > 1 artístico |
| **Aviso de câmera no disco** | O padrão travado do C++ (4.997 rs, elevação π/2) fica dentro da laje do disco: Build Full Scene emite WARNING. Para renders limpos, use **Elevação 1.25** |
| **Guias** | Anéis `BH_Guides` com bevel (aparecem no render): esfera de fótons 1.5 rs, b_c = (3√3/2) rs ≈ 2.598 rs, ISCO 3 rs (`schwarzschild.hpp`) |
| **Animação** | Presets `TURNTABLE` (padrão), `ELEVATION_SWEEP`, `DOLLY`, `SPIRAL`; easing `LINEAR` / `SINE`; azimute e spin do disco em loop sem costura; `camera_path_sample()` puro, coberto pelos testes |
| **Disco** | Spin = rotação keyframada do UV; turbulência opcional só no brilho (desligada por padrão); o matiz nunca muda |
| **Render C++ → Blender** | Exporta o JSON, roda `bh_render_cpu` (legacy / relativistic / blackbody, RK4 / RK45, estrelas opcionais) uma vez por frame com a pose animada da câmera (still ou sequência) e carrega o PNG/BMP ou a sequência como background da câmera (viewport) ou como plano emissivo (render) |
| **JSON ↔ C++** | `black_hole.scene_params/v1`; lido por `BlackHole3D --scene` e `bh_render_cpu --scene`; import aceita JSON do C++ |
| **Headless** | `make blender-scene` / `blender --background --factory-startup --python blender/scripts/build_scene_headless.py -- --out X.blend` |

### Por que a pose padrão sai estranha

O C++ guarda π/2 como `float`: cos(π/2) ≈ −4.37e−8 deixa a câmera ≈ 2.8 km
**abaixo** do plano do disco, e o raio padrão 4.997 rs está **dentro** do anel
2.2–5.2 rs. No OpenGL, todo raio que sobe cruza o disco no primeiro passo e a
metade de cima do frame inicial sai amarela até você orbitar. No Blender, a
câmera fica dentro da laje. É o baseline legado, documentado e mantido. O modo
científico e o `bh_render_cpu` tratam a câmera no plano corretamente. Detalhes
em [INSTALAR_E_RODAR.md](INSTALAR_E_RODAR.md), seção 5.

## Layout

```
blender/
  README.md                         # este arquivo
  INSTALAR_E_RODAR.md               # guia canônico (PT)
  black_hole_bridge.zip             # idêntico a addons/black_hole_bridge.zip
  addons/black_hole_bridge.zip      # instalável (Install… / Install from Disk)
  addons/black_hole_bridge/         # código do addon 0.8.0
    __init__.py                     # bl_info, BHBridgeSettings, sub-painéis, register
    blender_manifest.toml           # manifesto de extension (4.2+)
    constants.py                    # espelho dos headers C++, cpp_to_blender, camera_inside_disk
    scene_builder.py                # Build Full Scene (coleções, Raw, Cycles, aviso)
    horizon.py · disk_mesh.py · grid_mesh.py
    materials.py                    # nós: disco (1,r,0.2)·r, grade, guias, horizonte, spin, Raw
    guides.py                       # anéis fóton / b_c / ISCO
    camera_orbit.py                 # rig, presets, easing, bake
    render_bridge.py                # CLI do bh_render_cpu, import background/plano, sequências
    json_io.py                      # export/import scene_params/v1
    export_ops.py                   # operadores (Build, Rebuild, Bake, JSON)
  scripts/build_scene_headless.py   # blender --background → .blend
  scripts/export_scene_params.py    # exportador JSON mínimo, sem bpy
  scripts/package_addon.py          # zips determinísticos (--check)
  examples/scene_params_example.json / .md
  examples/animation_style_guide.md
  docs/INSTALACAO_ADDON.md · docs/ANIMACAO.md · docs/PIPELINE_C++_BLENDER.md
```

## Testes (sem Blender)

Todos os módulos importam em `python3` puro (`bpy` protegido por
`try/except`), então o CTest roda sem Blender:

| Teste CTest | O que verifica |
|-------------|----------------|
| `blender_addon_compile` | `py_compile` de todos os módulos e scripts |
| `blender_addon_parity` | constantes e fórmulas vs `include/black_hole/*.hpp`, mapeamento de eixos, JSON, linha de comando do render bridge, presets/easing, zips atualizados |

O CI também roda o Blender 4.0.x do `apt` com `build_scene_headless.py` e
exige que os zips estejam em dia (`package_addon.py` + `git diff --exit-code`).

## Ver também

- [docs/BLENDER_ADAPTER.md](../docs/BLENDER_ADAPTER.md) — arquitetura do adaptador (duas direções)
- [docs/PIPELINE_C++_BLENDER.md](docs/PIPELINE_C++_BLENDER.md) — JSON e frames, passo a passo
- [docs/ANIMACAO.md](docs/ANIMACAO.md) — presets, easing, spin, sequências do C++
- [docs/ESTILO_VISUAL_SIMULACAO.md](../docs/ESTILO_VISUAL_SIMULACAO.md) — paleta travada

---

**EN:** optional Blender 4.x frontend. Install the zip, then sidebar
**Black Hole → Build Full Scene**. C++ Y-up is mapped to Blender Z-up by a
proper rotation (`cpp_to_blender`), colours match the OpenGL framebuffer under
the "Raw" view transform, and the locked C++ default camera sits inside the
disk, so use Elevation 1.25 for Blender renders. Real lensing comes from
`bh_render_cpu`, imported as a camera background or plane. Physics stays in C++.
