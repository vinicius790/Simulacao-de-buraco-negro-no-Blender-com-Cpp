# Legacy Baseline Contract / Contrato do baseline legado

This repository keeps the historical render/physics path as the reference baseline. Engineering changes in the default path must preserve its observable behavior unless a change is explicitly classified as a correction that fixes undefined/incorrect API behavior.

Este repositório mantém o caminho histórico de render/física como baseline de referência. Em 0.8.0 ele continua sendo o que `BlackHole3D` executa **sem flags**; toda física nova está atrás de flags explícitas (`--scientific`, `--relativistic`) ou em ferramentas separadas (`bh_render_cpu`).

## Original archive

- Source archive: `black_hole-main.rar`
- SHA-256: `5f283b7dc4531123d0f20b54caf5ee1dc1308b069a944e1b58449a6b3a2ee5ee`

## Original key-file hashes

```text
d5f39a36322ed110f8674e3eb4168ae1dce5ec8bf98386892b9a6d8ad75e6c52  black_hole.cpp
9924ebbcaa42a667a429eee73fd9d438843ace32718af5d0118ba5893e23d4fe  geodesic.comp
260053e384c4bcf6d19fbcf361d3687d725b63e1de2897d846372dc1f4dcb990  CMakeLists.txt
2eaba452e1fd9c747fcdcf043f06066682f6a9c8bb1b4b58994488af4043c3d7  2D_lensing.cpp
82d8ba198b26644b0171e8d9515172ef5ab4a23e81cf9d41847716b7d6f811c5  CPU-geodesic.cpp
a9891e8dff1f1f50f05255e1ea508c56b20952fcb72181c71e03f90da74e925b  ray_tracing.cpp
76fde24d01fe2d607ba97b25a82d2884c0abd03d40cf4329bf5b4787e8f85e21  README.md
37abd591e48e432daa747ea7983db2419e860a2c628dfb658f12c9a2e264ba73  vcpkg.json
```

These are the hashes of the **original archive**. `black_hole.cpp`, `CMakeLists.txt` and `README.md` have since received documented engineering changes (see [ENGINEERING_UPGRADE.md](ENGINEERING_UPGRADE.md)); the protected *behavior* is enforced by the static tests below, not by these hashes.

## Locked default behavior

The static regression guards `tests/validate_source_invariants.py` and `tests/test_style_contract.py` protect these historical defaults:

- window: 800x600;
- compute image: 200x150;
- `SagA_rs = 1.269e10` in the compute shader;
- `D_LAMBDA = 1e7`;
- 60,000 integration steps;
- `ESCAPE_R = 1e30`;
- disk radii `2.2 * r_s` and `5.2 * r_s`;
- disk colour `vec3(1.0, r_norm, 0.2)` with alpha `r_norm`;
- grid colour `vec4(0.5, 0.5, 0.5, 0.7)`;
- orbit camera: radius `6.34194e10` m (≈ 4.997 rs), azimuth 0, elevation π/2, vertical FOV 60°;
- legacy fullscreen `GL_TRIANGLE_STRIP` behavior;
- legacy alpha blending/compositing behavior;
- the single-stage Euler integrator historically mislabeled as RK4 (now named `legacyEulerStep`, 1 RHS evaluation).

`tests/test_shader_contract.py` additionally checks that the legacy shader carries **no** scientific code (no `SciParams`, no binding 4) and that the `shaders/` mirrors are byte-identical to the root files.

The tests do **not** claim these choices are scientifically ideal. They exist to prevent a refactor from silently changing the current visual/physics baseline.

## 0.8.0: what changed around the baseline (and what did not)

| Item | Status |
|------|--------|
| Root `geodesic.comp` | **Unchanged** |
| `BlackHole3D` with no flags | Exact historical baseline (legacy compute program, legacy constants) |
| New CLI `--scene`, `--scientific`, `--relativistic`, `--capture`, `--help` | Additive; the `Engine` is constructed after CLI parsing so the selected program matches the mode |
| Scientific GPU path | `shaders/geodesic_scientific.comp`, only with `--scientific` / `--relativistic` |
| CPU reference | `bh_render_cpu` (separate binary) |
| Known baseline inaccuracies (Euler, point-sampled hits, `ESCAPE_R`, global spherical chart, default-view artifact below) | Kept as-is by contract; corrected only in the new paths |

## Descoberta: artefato da vista inicial (documentado, não alterado)

**Sintoma.** No primeiro quadro do `BlackHole3D` sem flags, a **metade de cima da imagem sai amarela sólida**; a metade de baixo mostra a sombra com a borda do disco. O artefato some assim que o usuário orbita a câmera com o mouse.

![Captura GPU do baseline na vista inicial (200×150, Mesa llvmpipe)](images/legacy_default_gpu.png)

*[`images/legacy_default_gpu.png`](images/legacy_default_gpu.png): `BlackHole3D --capture` sem outras flags, Mesa llvmpipe. Compare com a mesma câmera a elevação 1,25: [`images/legacy_el125_gpu.png`](images/legacy_el125_gpu.png).*

**Causa raiz** (verificada com `BlackHole3D --capture` em Mesa llvmpipe):

1. **A câmera está ligeiramente abaixo do plano do disco.** A elevação default é `M_PI / 2.0f` em `float`. Em precisão simples, `cos(π/2f) ≈ −4,37 × 10⁻⁸` (não zero), então
   `y = r · cos(e) ≈ 6,34194 × 10¹⁰ m × (−4,37 × 10⁻⁸) ≈ −2,8 km`.
2. **A câmera está dentro do anel do disco.** O raio default 6,34194 × 10¹⁰ m ≈ **4,997 rs** fica entre a borda interna (2,2 rs) e a externa (5,2 rs).
3. **O teste de disco legado é pontual.** `geodesic.comp` marca acerto quando o sinal de `y` muda entre dois passos e o raio cilíndrico está no anel. Todo raio que sobe cruza `y = 0` já no **primeiro** passo, a ≈ 4,997 rs do centro, ou seja, dentro do anel → "acerta o disco" imediatamente, com `r_norm ≈ 4,997/5,2 ≈ 0,96` → cor quase `(1, 0,96, 0,2)` com alpha ≈ 0,96: amarelo sólido.

Raios que descem não cruzam o plano no primeiro passo e são integrados normalmente, por isso a metade de baixo parece correta.

**Por que não foi corrigido.** O baseline é contrato: câmera, elevação, raios do disco e o teste pontual são valores travados pelos testes. Mudar qualquer um deles mudaria a imagem default. A correção existe nos caminhos novos:

| Caminho | Comportamento com a câmera no plano do disco |
|---------|----------------------------------------------|
| `BlackHole3D` sem flags | artefato (metade de cima amarela) — **mantido** |
| `BlackHole3D --scientific` / `--relativistic` | correto: com \|y\| < 1e-6·r a câmera é tratada como **no** plano e o primeiro segmento não conta como cruzamento (`cameraOnPlane`) |
| `bh_render_cpu` | correto: mesma regra e mesma tolerância (`camera_on_plane`); o teste `cpu_render` confere que a elevação π/2 arredondada para `float32` renderiza igual à pose em `double` |
| Addon Blender | **avisa** em Build Full Scene que a câmera está dentro da laje do disco e recomenda Elevação 1,25 |

**Como evitar no uso.** Orbite a câmera, ou carregue uma cena com outra elevação (`BlackHole3D --scene cena.json`, com `elevation_rad` 1,25, por exemplo), ou use os modos científicos.

**Observação relacionada.** A borda interna legada do disco (2,2 rs) está **dentro da ISCO** (3 rs = 6 M), onde não existem órbitas circulares estáveis (a 2,2 rs a órbita circular é instável, com v_loc ≈ 0,645 c). A geometria legada é mantida; o modo `blackbody` do `bh_render_cpu` não emite dentro da ISCO, e a cena `examples/scene_relativistic_showcase.json` usa um disco de 3 a 12 rs.

Uma eventual correção **opt-in** (por exemplo, uma flag que tire a câmera do plano) nunca pode alterar o default.

## Required validation for future changes

1. Run `make style-check` (or `python3 tests/validate_source_invariants.py --root .`, `tests/test_style_contract.py`, `tests/test_shader_contract.py`).
2. Build both trees (`make build`, `make build-gl`) warning-free.
3. Run the GLSL validation target when `glslangValidator` is available (`make shaders`).
4. Capture deterministic reference frames (`BlackHole3D --capture`) on the same GPU/driver and compare them with `tools/image_diff.py`; the baseline default capture must still show the documented artifact.
5. For scientific-mode changes, compare against the independent double-precision CPU reference (`bh_render_cpu`, CTest `gpu_cpu_agreement`) rather than requiring equality with the legacy render.

See [CHECKLIST_VALIDACAO.md](CHECKLIST_VALIDACAO.md) and [VALIDATION_STATUS.md](VALIDATION_STATUS.md).
