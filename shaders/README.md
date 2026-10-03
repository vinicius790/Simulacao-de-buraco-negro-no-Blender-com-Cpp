# Shaders

## Baseline canônico (não quebrar os invariantes)

A **fonte da verdade** do caminho de render padrão continua sendo as cópias da
raiz do repositório:

- `../geodesic.comp` — compute shader legado de geodésicas nulas (`legacyEulerStep`,
  `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 passos, `ESCAPE_R = 1e30`)
- `../grid.vert` / `../grid.frag` — grade de spacetime

`tests/validate_source_invariants.py` lê esses arquivos da raiz. Não os apague nem
deixe divergir. `BlackHole3D` **sem flags** carrega `geodesic.comp`.

## Este diretório

| Arquivo | Papel | Carregado por |
|---------|-------|---------------|
| `geodesic.comp`, `grid.vert`, `grid.frag` | Espelhos **byte a byte** dos arquivos da raiz (empacotamento / layout) | — (o binário usa as cópias da raiz) |
| `geodesic_scientific.comp` | Modo científico na GPU (RK4 planar) | `BlackHole3D --scientific` / `--relativistic` |

`tests/test_shader_contract.py` (CTest `shader_contract`) falha se um espelho
divergir da raiz.

## `geodesic_scientific.comp` — ligado via `--scientific`

Desde a 0.8.0 o shader científico está **ligado**, mas só por flag explícita:

```bash
cd build/gl                       # o POST_BUILD copia os 4 shaders para cá
./BlackHole3D --scientific        # paleta legada, física corrigida
./BlackHole3D --relativistic      # + Doppler / redshift gravitacional / beaming g⁴
./BlackHole3D --scene ../../examples/scene_relativistic_showcase.json --relativistic
xvfb-run -a ./BlackHole3D --scientific --capture sci.png   # sem monitor (Mesa llvmpipe)
```

Sem flags nada muda: o `Engine` é construído depois da CLI e escolhe
`geodesic.comp` ou `geodesic_scientific.comp`.

O que o shader faz (gêmeo GPU de `bh_scientific` / `bh_render_cpu`):

| Aspecto | `geodesic.comp` (baseline) | `geodesic_scientific.comp` |
|---------|----------------------------|----------------------------|
| Unidades | metros | rs = 1 (o host divide câmera, objetos e espessura do disco por `r_s_m`) |
| Carta | esférica global | plano orbital de cada raio (θ ≡ π/2), sem singularidade polar |
| Integrador | Euler 1 estágio | RK4 clássico (4 avaliações), termo de Christoffel corrigido `r·f·φ̇²` |
| Passo | `D_LAMBDA = 1e7` m fixo | geométrico `dλ = clamp(stepK·r, stepMin, stepMax)` |
| Semente | direção como velocidade coordenada | referencial ortonormal da câmera estática (`k^r = √f·n_r`) |
| Disco | troca de sinal de y, pontual | segmento–plano com ponto interpolado; 1º segmento ignorado se a câmera está no plano |
| Horizonte / objetos | pontual | segmento–esfera; o marcador do BH não é enviado (o horizonte é exato) |
| Escape | `ESCAPE_R = 1e30` | por prova: `dr > 0`, `r > 1,5` e `r > sceneBound` |
| Cor | `vec4(1, r, 0.2, r)` | `colorMode 0`: igual; `colorMode 1`: × Doppler/redshift (g⁴, Reinhard) |
| Kerr | não | **não** (marcado no próprio shader e verificado pelo `shader_contract`) |

Referência CPU: `include/black_hole/planar_geodesic.hpp`, `redshift.hpp`,
`escape.hpp`, `hit_testing.hpp`, `cpu_renderer.hpp`.

### Bindings

| Binding | Bloco | Bytes | Legado | Científico |
|---------|-------|-------|--------|------------|
| 0 | `image2D outImage` (rgba8) | — | ✔ | ✔ |
| 1 | `Camera` (std140) | 80 | ✔ | ✔ (posição em rs) |
| 2 | `Disk` (std140) | 16 | ✔ (metros) | ✔ (fatores de rs) |
| 3 | `Objects` (std140, `vec4 mass[16]`) | 784 | ✔ | ✔ (em rs, sem o marcador do BH) |
| 4 | `SciParams` (std140) | 32 | **✘ proibido** | ✔ |

### UBO `SciParams` (binding 4)

```glsl
layout(std140, binding = 4) uniform SciParams {
    int   colorMode;   // 0 = paleta legada vec3(1,r,0.2)·r ; 1 = legada + Doppler/redshift
    int   maxSteps;    // limite de passos por raio
    float stepK;       // fator do passo geométrico
    float stepMin;     // rs
    float stepMax;     // rs
    float sceneBound;  // raio de escape (rs)
    float exposure;    // Reinhard (colorMode 1)
    float spinSign;    // +1 / −1: rotação do disco em torno de +Y
} sci;
```

Espelho C++ em `black_hole.cpp`:

```cpp
struct alignas(16) SciParamsUBOData {
    std::int32_t colorMode; std::int32_t maxSteps;
    float stepK; float stepMin; float stepMax; float sceneBound; float exposure; float spinSign;
};
static_assert(sizeof(SciParamsUBOData) == 32, "SciParams UBO layout must match geodesic_scientific.comp.");
```

Valores enviados pelo host (`uploadSciUBO`, fixos no código):

| Campo | Valor |
|-------|-------|
| `colorMode` | 0 com `--scientific`, 1 com `--relativistic` |
| `maxSteps` | 4000 |
| `stepK`, `stepMin`, `stepMax` | 0.02, 0.005, 2.0 |
| `sceneBound` | `max(r_externo, \|c_i\| + R_i) · 1,05 + 0,5` (rs) |
| `exposure` | 2.0 |
| `spinSign` | +1 |

Para variar exposição, spin ou orçamento de passos, use o `bh_render_cpu`
(`--exposure`, `--spin-sign`, `--max-steps`), que tem os mesmos padrões.

### Contrato e validação

- `shader_contract`: espelhos idênticos; todo binding usado pelo host existe no
  shader carregado; `SciParams` igual campo a campo entre GLSL e C++; o shader
  legado não declara `SciParams` nem binding 4; marcadores de honestidade e o fator
  `r·f` presentes no científico.
- `validate_shaders` (glslangValidator): `grid.vert`, `grid.frag`, `geodesic.comp`
  e `shaders/geodesic_scientific.comp`.
- `gpu_cpu_agreement`: `BlackHole3D --scientific|--relativistic --capture` contra
  `bh_render_cpu` em 6 cenas × 2 modos (Mesa llvmpipe), gate `bad_fraction ≤ 0,002`.
- `legacy_golden`: `BlackHole3D` sem flags (carrega o `geodesic.comp` da raiz)
  idêntico à imagem de referência `docs/images/legacy_default_gpu.png`.
  Resultados em [`../docs/VALIDATION_STATUS.md`](../docs/VALIDATION_STATUS.md).

### Limitações

- Precisão `float` (o CPU usa `double`): poucos pixels de borda podem divergir.
- Sem modo corpo negro (Page–Thorne só no `bh_render_cpu`).
- Sem Kerr, sem transferência radiativa.
- Validado só em Mesa llvmpipe; nenhum número de desempenho de GPU é afirmado.
