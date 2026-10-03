# Comparativo: baseline legado vs. modo científico (GPU) vs. renderer CPU

Desde a 0.8.0 há **três** caminhos de render para a mesma cena. O baseline continua sendo o default e não mudou;
os outros dois são separados e ligados explicitamente. Não use a existência deles para reivindicar Kerr ou GRMHD.

| Caminho | Como rodar |
|---------|-----------|
| **Baseline legado** | `BlackHole3D` (sem flags) → `geodesic.comp` |
| **Científico GPU** | `BlackHole3D --scientific` ou `--relativistic` → `shaders/geodesic_scientific.comp` |
| **Renderer CPU** | `bh_render_cpu` (ou `bh::render::render`) → `src/scientific/cpu_renderer.cpp` |

## Tabela principal

| Aspecto | Baseline legado (default) | Científico GPU | Renderer CPU (`bh_render_cpu`) |
|---------|---------------------------|----------------|--------------------------------|
| Integrador | `legacyEulerStep`: Euler explícito, 1 avaliação do RHS por passo | RK4 clássico, 4 avaliações | RK4 (default) ou RK45 Dormand–Prince adaptativo (`--integrator rk45`, tol \(10^{-8}\)) |
| RHS | termo radial `+ r*(…)` **sem** \(f\) (não preserva o vínculo nulo) | termo corrigido `+ r*f*(…)`; aceleração congelada se \(f\le0\) | idem GPU científico |
| Carta | esférica **global**, eixo polar = Z do mundo (dentro do plano do disco): singular perto desse eixo | **plano orbital** de cada raio (\(\theta\equiv\pi/2\)): sem polos, exata por simetria esférica | idem (carta plana, `planar_geodesic.hpp`) |
| Modelo de câmera | direção do pixel usada como velocidade coordenada | direção no referencial do **observador estático** (\(k^r=\sqrt f\,n_r\)) | idem; borda da sombra = Synge (teste) |
| Hit do disco | troca de sinal de \(y\) entre amostras, \(\rho\) avaliado na amostra nova (erro de até um passo) | segmento–plano com cruzamento **interpolado** | idem (`hit_testing.hpp`) |
| Hit de horizonte / objetos | amostragem pontual; a esfera marcadora preta do buraco negro é desenhada como objeto | segmento–esfera, hit mais cedo no segmento; a esfera marcadora é omitida e a sombra vem do horizonte exato | idem |
| Câmera no plano do disco | primeiro passo conta como cruzamento → metade do quadro amarela na vista default | primeiro segmento ignorado (\(|y|<10^{-6}r\)) | idem (mesma tolerância) |
| Escape | `ESCAPE_R = 1e30` (inalcançável; na prática o laço para no orçamento de passos) | **provado**: saindo, além da esfera de fótons e além da cena | idem (`escape.hpp`) |
| Unidades | metros (`SagA_rs = 1.269e10`) | **rs = 1** (o host divide por `r_s_m`) | rs = 1; SI só para Page–Thorne |
| Precisão | `float` | `float` | `double` |
| Passo | fixo `D_LAMBDA = 1e7` m | geométrico \(d\lambda=\mathrm{clamp}(0.02\,r,\,0.005,\,2.0)\) rs | idem dentro de \(1.05\,r_{\rm out}\); fora, teto \(\max(2.0,\,0.02\,r)\) (RK4) — ou adaptativo (RK45) |
| Orçamento de passos | 60 000 | 4000 (`SciParams.maxSteps`) | 20 000 (`--max-steps`) |
| Passos médios por raio (medido) | — (não medido) | — (não medido) | 400×300, elevação 1.25: RK4 ≈ 111, RK45 ≈ 32; pose default: RK4 ≈ 184 |
| Sombreamento do disco | `vec4(1, r, 0.2, r)` composto sobre preto | `--scientific`: paleta legada; `--relativistic`: paleta × \(g^4\) + Reinhard (exposure 2.0) | `legacy`, `relativistic` (idem GPU) e `blackbody` (Page–Thorne, \(T_{\rm obs}=gT\)) |
| Redshift / Doppler | não | sim (`--relativistic`), \(g\) medido pela câmera estática | sim (`relativistic`, `blackbody`) |
| Fundo | preto | preto | preto ou estrelas procedurais (`--stars`), consultadas na direção assintótica do raio (lenteadas) |
| Resolução | janela 800×600, compute 200×150 | compute 200×150 | qualquer (`--width/--height`), supersampling S×S |
| FOV / alvo | 60°, origem (default); `--scene` pode definir `fov_y_deg` e `target_m` | idem | `fov_y_deg`/`target_m` da cena ou `--fov-y-deg` |
| Determinismo | não testado entre drivers | não testado entre drivers | bit a bit idêntico para qualquer nº de threads (testado) |
| Velocidade (medida) | sem medição de tempo publicada | sem medição de tempo publicada (só Mesa llvmpipe, que é software) | 400×300 `legacy`, 4 threads: ≈ 0.5–1.4 s neste container (4 CPUs), conforme pose e carga |
| Validação | `validate_source_invariants.py`, `test_style_contract.py` | `shader_contract`, `gpu_cpu_agreement` (vs CPU) | `scientific_ref`, `cpu_render`, testes de CLI |
| Kerr / spin | não | não | não (só raios analíticos em `kerr_analytic.hpp`) |
| GRMHD / transferência radiativa | não | não | não |

## Concordância GPU ↔ CPU (medida, Mesa llvmpipe)

`gpu_cpu_agreement` roda `BlackHole3D --capture` e `bh_render_cpu` na mesma cena (200×150) e compara com
`tools/image_diff.py`. Gate: fração de pixels com erro > 24/255 ≤ \(2\times10^{-3}\) e erro médio ≤ 0.5/255.

Execução mais recente neste container (erros em unidades de 1/255; "ruins" = pixels com erro > 24/255, de 30 000):

| Cena / pose | `--scientific` vs `legacy` | `--relativistic` vs `relativistic` |
|-------------|----------------------------|------------------------------------|
| az 0, el 1.25, 4.997 rs | máx 1, 0 ruins | máx 1, 0 ruins |
| az 0.7, el 0.60, 8e10 m | máx 3, 0 ruins, médio 0.070 | máx 1, 0 ruins |
| az 2.1, el 2.00 (abaixo do disco) | 2 ruins (6.7e-5), médio 0.062 | 2 ruins (6.7e-5), médio 0.006 |
| az 0.3, el 1.30, 9e10 m, FOV 40°, alvo deslocado em y | 1 ruim (3.3e-5), médio 0.060 | 1 ruim (3.3e-5), médio 0.005 |
| meia massa (\(r_s=6.345\times10^9\) m), az 0, el 1.25 | máx 1, 0 ruins | máx 1, 0 ruins |

Em execuções anteriores (com as três primeiras poses) o pior caso foi de 4 pixels ruins (fração ≤ 1.3e-4) e
`--relativistic` saiu byte-idêntico em az 0 / el 1.25; o erro médio ficou sempre < 0.08/255.

As diferenças restantes ficam em pixels de borda (sombra/disco), onde `float` e `double` decidem lados opostos.
Nenhuma medição foi feita em GPU de hardware.

## Diferenças visíveis entre baseline e científico

- **Vista default:** o baseline mostra a metade de cima amarela (câmera ≈ 2.8 km abaixo do plano por causa de
  \(\cos(\pi/2)\) em `float`, dentro do anel 2.2–5.2 rs); o científico e o CPU mostram a vista correta
  ([`images/legacy_default_gpu.png`](images/legacy_default_gpu.png)).
- **Tamanho da sombra:** o modelo de câmera estática dá a sombra de Synge (27.7° de raio na câmera default); usar a
  direção como velocidade coordenada a inflaria (~10 % em ângulo segundo o comentário do shader científico).
- **Disco:** cruzamento interpolado deixa as bordas do anel nítidas; o baseline erra a posição em até um passo.

| Baseline, elevação 1.25 (GPU) | Renderer CPU, paleta legada, elevação 1.25 | Relativístico, elevação 1.25 |
|---|---|---|
| ![](images/legacy_el125_gpu.png) | ![](images/scientific_el125.png) | ![](images/relativistic_el125.png) |

## Constantes travadas (inalteradas na 0.8.0)

| Item | Valor |
|------|-------|
| `SagA_rs` | `1.269e10` |
| `D_LAMBDA`, passos, `ESCAPE_R` | `1e7`, 60 000, `1e30` |
| Janela / compute | 800×600 / 200×150 |
| Disco | 2.2–5.2 rs, cor `vec3(1, r_norm, 0.2)`, alpha `r_norm` |
| Grid | `vec4(0.5, 0.5, 0.5, 0.7)` |
| Câmera | raio 6.34194e10 m (≈ 4.997 rs), azimute 0, elevação π/2, FOV 60° |

## Regra de ouro

Mudanças que alterem trajetórias ou pixels do caminho default **não** entram silenciosamente no baseline. Ou:

1. ficam atrás de um modo/flag/shader separado (como `--scientific` / `--relativistic` e `bh_render_cpu`), ou
2. são classificadas como CORREÇÃO de API OpenGL/layout (ex.: barreiras, std140) sem mudar a aritmética da geodésica.

Ver também: [BASELINE.md](BASELINE.md), [ENGINEERING_UPGRADE.md](ENGINEERING_UPGRADE.md),
[SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md), [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md),
[DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md).
