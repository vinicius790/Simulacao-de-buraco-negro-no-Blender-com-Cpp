# Bíblia do estilo visual — baseline legado (travado)

Documento de contrato visual. **Blender e qualquer frontend devem espelhar estes números.**
Testes: `tests/validate_source_invariants.py` + `tests/test_style_contract.py`
(+ `tests/test_shader_contract.py` para os shaders e `tests/test_blender_addon_parity.py`
para o addon).

> **0.8.0: o estilo travado não mudou.** Os modos novos (`BlackHole3D --scientific` /
> `--relativistic`, `bh_render_cpu --mode relativistic|blackbody`) são **opt-in** e
> descritos à parte em [Modos opt-in](#modos-opt-in--fora-do-estilo-travado).
> `BlackHole3D` sem flags e `bh_render_cpu --mode legacy` usam a paleta abaixo.

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

## Disco de acreção (anel geométrico)

**No baseline NÃO é Novikov–Thorne nem Page–Thorne.** Só geometria + cor.

| Item | Valor |
|------|-------|
| Raio interno | `rs * 2.2` (dentro da ISCO de 3 rs — mantido por contrato) |
| Raio externo | `rs * 5.2` |
| Espessura UBO | `1e9` m (≈ 0,079 rs) |
| `disk_num` | `2.0` (upload CPU) |
| Hit | cruzamento do plano equatorial (Y muda de sinal) com ρ ∈ [r1, r2] |

### Cor do disco (`geodesic.comp`)

```glsl
float r = length(pos) / disk_r2;   // r_norm
vec3 diskColor = vec3(1.0, r, 0.2);
color = vec4(diskColor, r);        // alpha = r_norm
```

Misturado sobre o fundo preto, o pixel final é `(1, r_norm, 0.2) · r_norm`. É essa
cor que o `bh_render_cpu --mode legacy` e o material de emissão do Blender
reproduzem.

Helpers C++: `include/black_hole/disk_model.hpp` (`legacy_annulus`, `legacy_disk_rgb`).

## Câmera orbital

Espelhada em `include/black_hole/camera_model.hpp` e `black_hole.cpp::Camera`:

| Campo | Default |
|-------|---------|
| `radius` | 6.34194e10 m (≈ 4,997 rs — dentro do anel 2,2–5,2 rs) |
| `azimuth` | 0 |
| `elevation` | π/2 (vista equatorial) |
| FOV Y | 60° → `tanHalfFov = tan(30°)` |
| Aspect | 800/600 |
| Up | +Y; disco no plano XZ |
| Target | (0,0,0) |
| Elevação | clamp (0.01, π−0.01) |
| Zoom | radius ∈ [1e10, 1e12] |

Posição:

```
x = r * sin(e) * cos(a)
y = r * cos(e)
z = r * sin(e) * sin(a)
```

Esses são os defaults de `BlackHole3D` sem flags. Com `--scene file.json`, raio,
azimute, elevação, FOV e alvo vêm do JSON (o estilo de cor não muda).

**Pose default e artefato.** Em `float`, `cos(π/2) ≈ −4,37e−8`: a câmera fica
≈ 2,8 km abaixo do plano, dentro do anel, e a metade de cima do primeiro quadro sai
amarela sólida. Faz parte do baseline travado — ver
[BASELINE.md](BASELINE.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado).
Para imagens de referência e para o Blender, a pose recomendada é **elevação 1,25**.

## Grid de spacetime (overlay)

| Item | Valor |
|------|-------|
| Tamanho | 25 × 25 |
| Spacing | 1e10 m |
| Topologia | `GL_LINES` (índices estáticos) |
| Cor | `vec4(0.5, 0.5, 0.5, 0.7)` em `grid.frag` (o comentário diz "azul"; a cor é cinza) |

### Fórmula de warp (`generateGrid`)

Para cada massa, `dist = sqrt(dx² + dz²)`:

- se `dist > rs`: `deltaY = 2 * sqrt(rs * (dist - rs))`; `y += deltaY - 3e10`
- senão (dentro/no horizonte): `y += 2 * rs - 3e10` (poço profundo)

É **estético**, não embedding isométrico. Helper: `bh::camera::grid_warp_delta_y`.

## Compose final (ordem real do frame loop)

1. Clear color preto `(0,0,0,1)`
2. Grade warp desenhada com `GL_LINES` e blending `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`
3. Compute escreve RGBA8 200×150 (`imageStore`)
4. Barreira `IMAGE_ACCESS | TEXTURE_FETCH`
5. Quad fullscreen amostra a textura com o **blending herdado**: onde o compute
   deixou alpha 0 (fundo), a grade continua visível; disco/objetos/horizonte
   cobrem a grade conforme o alpha

`BlackHole3D --capture` grava só a textura de compute (passo 3, RGB·alpha sobre
preto), **sem** a grade.

## Objetos de cena (defaults)

1. Esfera amarela `(4e11, 0, 0)`, raio 4e10, massa solar
2. Esfera vermelha `(0, 0, 4e11)`, raio 4e10, massa solar
3. Marcador do BH em `(0,0,0)` com raio `rs`, cor preta

O shader legado desenha o marcador como esfera preta. Os caminhos científicos o
descartam (`bh::is_black_hole_marker`) e desenham o buraco pelo teste de horizonte
exato; visualmente continua preto.

## Gravity flag

- Default **OFF**
- Tecla `G` alterna; botão direito pressionado liga. **Não** muda o integrador geodésico
- Usado para movimento N-corpos dos objetos no loop CPU

## Modos opt-in — fora do estilo travado

(Adicionados na 0.8.0.)

Nenhum destes modos é default. Eles existem para comparação científica e para
imagens de divulgação; não substituem nem alteram a paleta acima.

| Modo | Como ligar | Aparência |
|------|------------|-----------|
| Científico GPU | `BlackHole3D --scientific` | **mesma paleta** `(1, r, 0.2)·r`; geometria corrigida (RK4 planar, colisões contínuas, escape por prova) |
| Relativístico GPU | `BlackHole3D --relativistic` | paleta legada × Doppler/redshift gravitacional, beaming g⁴, Reinhard (exposição 2,0): um lado mais brilhante e esbranquiçado, o outro mais escuro e avermelhado |
| Legacy CPU | `bh_render_cpu --mode legacy` | **mesma paleta** do baseline (referência em `double`) |
| Relativístico CPU | `bh_render_cpu --mode relativistic` | igual ao relativístico GPU; `--exposure`, `--spin-sign` ajustáveis |
| Corpo negro CPU | `bh_render_cpu --mode blackbody` | cor física de Page–Thorne deslocada por g: **branco-azulado** para Sgr A\* a 1 % de Eddington (pico ≈ 86 700 K); zero dentro da ISCO |
| Estrelas | `bh_render_cpu --stars` | campo estelar procedural nos raios que escapam (o baseline tem fundo preto) |

Exemplos: [`images/scientific_el125.png`](images/scientific_el125.png) (paleta
legada), [`images/relativistic_el125.png`](images/relativistic_el125.png),
[`images/blackbody_el100.png`](images/blackbody_el100.png),
[`images/showcase_relativistic.png`](images/showcase_relativistic.png).

## O que Blender DEVE casar

- Fatores de disco 2,2 / 5,2 (ou os fatores reais da cena carregada)
- Espessura do disco `1e9` m ≈ 0,079 rs (modificador Solidify; sem espessura o
  disco some de perfil)
- Câmera orbital (radius/az/el/FOV), com conversão de eixos **só** por
  `constants.cpp_to_blender`: (x, y, z) C++ Y-up → (x, −z, y) Blender Z-up
- Cor do disco = pixel OpenGL: emissão pura `(1, r_norm, 0.2) · r_norm`, força 1,0
  (`disk_glow` = 1,0 é paridade; > 1 é brilho artístico opcional)
- Grade e guias com o cinza da grade: emissão 1,0 → `0,5 · 0,7 = 0,35` sobre preto
- View transform **Raw** (`materials.setup_color_parity`); o AgX padrão do
  Blender 4.x lava o âmbar para quase branco
- Grade como quads + Wireframe e anéis como curvas com bevel (arestas soltas não
  aparecem em Cycles/EEVEE)
- Warp do grid (fórmula acima)
- Janela 800×600 e compute 200×150 nos metadados de cena
- **Não** reivindicar Kerr, Novikov–Thorne, Page–Thorne ou RK4 no caminho default

### Extras do Blender que respeitam o estilo

| Extra | Regra |
|-------|-------|
| Guias de Schwarzschild | anéis em 1,5 rs (esfera de fótons), 2,598 rs (b_c) e 3 rs (ISCO), no cinza da grade com alpha 0,5 |
| Presets de câmera | `TURNTABLE` (padrão), `ELEVATION_SWEEP`, `DOLLY`, `SPIRAL`; easing `LINEAR` / `SINE` — mudam só a trajetória, não a paleta |
| Spin do disco | gira a textura (UV.u); cor inalterada |
| Turbulência | modula **só o brilho**, nunca a cor; **desligada por padrão** |
| Aviso de pose | Build Full Scene avisa quando a câmera está dentro da laje do disco (verdadeiro para a pose default) e recomenda elevação 1,25 |

O Blender **não** faz lente gravitacional: para ver a lente, importe um render do
C++ (`bh_render_cpu`) como fundo da câmera — ver [RENDER_CPU.md](RENDER_CPU.md#11-uso-pela-ponte-do-blender).

JSON de referência: `examples/scene_params_example.json` (espelhado em `blender/examples/`).
