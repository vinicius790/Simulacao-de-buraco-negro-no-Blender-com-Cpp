# Guia de estilo de animação / Animation style guide (PT + EN)

![Cena do addon no Cycles, elevação 1.25](../../docs/images/blender_el125_cycles.png)

*Referência de look no Blender: `docs/images/blender_el125_cycles.png`
(Blender 4.0.2, Cycles, view transform Raw, elevação 1.25 rad).*

## PT

**Objetivo:** o Blender deve parecer o simulador OpenGL, pixel a pixel onde
der, e nunca fingir física que não tem.

| Elemento | Regra |
|----------|-------|
| Fundo | preto (mundo com força 0) |
| Horizonte | esfera preta, raio rs (silhueta, sem especular) |
| Disco | anel fino no plano equatorial, padrão 2.2–5.2 rs; Emission `(1, r, 0.2)·r` = pixel `vec4(1, r, 0.2, r)` do `geodesic.comp` misturado sobre preto |
| Grade | cinza `(0.5, 0.5, 0.5)` com alpha 0.7 → 0.35 sobre preto; deformação de `generateGrid` |
| Guias | mesmo cinza, alpha 0.5; esfera de fótons 1.5 rs, b_c ≈ 2.598 rs, ISCO 3 rs |
| Cor | view transform **Raw** (paridade). AgX/Filmic só como escolha artística consciente: lavam o âmbar para quase branco |
| Câmera | órbita mirando a origem, FOV **vertical** 60°, up = +Z no Blender (= +Y no C++) |
| Paleta | sem neon/cyberpunk, sem matizes novos |

**O que pode variar sem quebrar o estilo**

- Caminho da câmera: presets `TURNTABLE`, `ELEVATION_SWEEP`, `DOLLY`,
  `SPIRAL` e easing `LINEAR` / `SINE`.
- Brilho: `disk_glow` > 1 (padrão 1.0 = paridade) e turbulência > 0, que
  modula só a **intensidade**.
- Spin do disco: gira a textura; só aparece com turbulência > 0.

**Pose:** para renders no Blender use **Elevação 1.25 rad** (≈ 18° acima do
plano). A pose padrão do C++ (4.997 rs, π/2) fica dentro da laje do disco, e o
addon avisa. No OpenGL, essa mesma pose gera o frame inicial com a metade de
cima amarela ([`legacy_default_gpu.png`](../../docs/images/legacy_default_gpu.png)).

**Honestidade visual:** o Cycles não curva a luz. Sombra ampliada, anel de
fótons, lado de trás do disco arqueado e assimetria Doppler só existem nas
imagens do C++ (`bh_render_cpu` / `BlackHole3D --relativistic`), trazidas pelo
**Render C++ → Blender** como background (viewport) ou **Import as plane**
(render). Ao publicar, diga qual imagem é do Blender e qual é do C++.

| Fonte | Exemplo |
|-------|---------|
| Blender (geometria, sem lensing) | [`blender_el125_cycles.png`](../../docs/images/blender_el125_cycles.png) |
| C++, paleta legada, mesma pose | [`scientific_el125.png`](../../docs/images/scientific_el125.png) |
| C++, relativístico, mesma pose | [`relativistic_el125.png`](../../docs/images/relativistic_el125.png) |
| C++, cena showcase relativística | [`showcase_relativistic.png`](../../docs/images/showcase_relativistic.png) |

## EN

Match the OpenGL simulator: black background; black horizon sphere of radius
rs; thin equatorial disk (default 2.2–5.2 rs) emitting `(1, r, 0.2)·r`, which
is the `geodesic.comp` pixel `vec4(1, r, 0.2, r)` blended over black; grey
grid `(0.5, 0.5, 0.5, 0.7)`; guide rings in the same grey at alpha 0.5. Keep
the **Raw** view transform for parity. AgX/Filmic wash the amber out. Presets
and easing change only the camera path; `disk_glow` and turbulence change
brightness, never hue. Use **Elevation 1.25** for Blender renders, because the
locked C++ default camera sits inside the disk slab. Cycles does not bend
light: lensing, the photon ring and Doppler asymmetry come only from the C++
frames imported through the render bridge. Label Blender and C++ images
honestly. See `docs/ESTILO_VISUAL_SIMULACAO.md`.
