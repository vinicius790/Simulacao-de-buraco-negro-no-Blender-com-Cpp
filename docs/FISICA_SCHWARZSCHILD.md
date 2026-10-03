# Física Schwarzschild — o que É e o que NÃO É modelado

Referência das geodésicas nulas, da carta plana sem polos, do critério de escape, do tamanho da sombra, da
deflexão fraca, dos integradores e dos raios analíticos de Kerr da versão **0.8.0**. A física do **disco**
(órbitas, redshift, beaming, Page–Thorne) está em [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md).

> O caminho **default** de `BlackHole3D` (sem flags) continua sendo o `geodesic.comp` legado (`legacyEulerStep`,
> `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 passos, `ESCAPE_R = 1e30`). Tudo o que está marcado como
> "científico" abaixo vive em caminhos separados: `bh_scientific` (CPU, `double`), `bh_render_cpu` e
> `shaders/geodesic_scientific.comp` (`--scientific` / `--relativistic`).

## Coordenadas / Coordinate chart

Coordenadas de **Schwarzschild** \((t, r, \theta, \phi)\), assinatura \((-,+,+,+)\):

\[
\mathrm{d}s^2 = -f\,\mathrm{d}t^2 + f^{-1}\,\mathrm{d}r^2 + r^2\mathrm{d}\theta^2 + r^2\sin^2\theta\,\mathrm{d}\phi^2,
\quad f = 1 - \frac{r_s}{r},\quad r_s = \frac{2GM}{c^2}=2M\;(G=c=1).
\]

Horizonte de eventos: \(r = r_s\). Os caminhos científicos usam **rs = 1** (unidades geométricas, \(M=0.5\));
o mapeamento SI é \(r_s\) da cena (`SceneParams::r_s_m`, default = literal legado \(1.269\times10^{10}\) m).

---

## O que É modelado (biblioteca científica)

| Item | Valor / status | Onde |
|------|----------------|------|
| Raio de Schwarzschild \(r_s\) | `2GM/c²`; Sgr A* (\(M=8.54\times10^{36}\) kg) → \(1.268388\times10^{10}\) m | `units.hpp`, `schwarzschild.hpp` |
| Esfera de fótons | \(r = 1.5\,r_s = 3M\) | `photon_sphere_radius` |
| ISCO | \(r = 3\,r_s = 6M\) | `isco_radius`, `orbits.hpp` |
| Parâmetro de impacto crítico | \(b_c=\tfrac{3\sqrt3}{2}r_s\approx2.598\,r_s\) | `escape.hpp` |
| Raio angular da sombra (observador estático) | \(\sin\alpha=b_c\sqrt{f(r_o)}/r_o\) | `escape::shadow_angular_radius` |
| Deflexão fraca | \(2r_s/b\) (1ª ordem) e \(2r_s/b+\tfrac{15\pi}{16}(r_s/b)^2\) (2ª ordem) | `weak_field.hpp` |
| Estado de raio nulo (`double`) | `RayState` (3-D) e `PlanarRay` (plano orbital) | `ray_state.hpp`, `planar_geodesic.hpp` |
| Integradores | Euler (legado, só testes), RK4 clássico, RK45 Dormand–Prince adaptativo | `integrator_rk4.hpp`, `integrator_rk45.hpp` |
| Diagnósticos | \(E\), \(L\), \(g_{\mu\nu}k^\mu k^\nu\) | `conserved.hpp` |
| Hit tests contínuos | segmento–plano (cruzamento interpolado), segmento–esfera | `hit_testing.hpp` |
| Escape provado | saindo + além da esfera de fótons + além da cena | `escape.hpp` |
| Disco relativístico | órbitas circulares, \(g\), \(g^4\), Page–Thorne, corpo negro | ver [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md) |
| Kerr | **só raios analíticos** (ISCO, horizontes, ergosfera, órbitas de fótons, \(\eta\)) | `kerr_analytic.hpp` |

**Nota sobre "ISCO = 6 rs":** com \(r_s = 2M\), ISCO = \(6M = 3\,r_s\). Textos que escrevem "6 rs" confundem
\(r_s\) com \(M\). Neste código, `isco_radius(rs) = 3*rs` e `isco_in_units_of_M() = 6`.

---

## O que o baseline GPU legado faz (default)

- Integrador **Euler explícito de 1 estágio** (`legacyEulerStep`), historicamente chamado de RK4 por engano.
- Carta esférica **global** com eixo polar = eixo Z do mundo (`theta = acos(z/r)`), que fica **dentro** do plano do
  disco (XZ); raios perto desse eixo passam pela singularidade de coordenada \(\sin\theta\to0\).
- Termo radial `+ r*(dθ² + sin²θ dφ²)` **sem** o fator \(f\) (ver abaixo).
- Hit do disco por troca de sinal de \(y\) entre duas amostras, avaliando \(\rho\) na amostra **nova** (erro de até
  um passo); cor `vec3(1, r, 0.2)` com alpha `r`, `r = |pos|/disk_r2`.
- `ESCAPE_R = 1e30` é inalcançável: com `D_LAMBDA = 1e7` m e direção inicial normalizada, 60 000 passos
  percorrem da ordem de \(6\times10^{11}\) m (≈ 47 rs); na prática o laço termina pelo orçamento de passos.
- Disco 2.2–5.2 rs: a borda interna está **dentro da ISCO** (3 rs).

### Descoberta: artefato da vista inicial (documentado, **não** alterado)

Na elevação default `M_PI / 2.0f`, \(\cos(\pi/2)\) em `float` vale \(\approx-4.37\times10^{-8}\): a câmera fica
\(\approx2.8\) km **abaixo** do plano do disco. Como o raio default 6.34194e10 m ≈ 4.997 rs está **dentro** do anel
2.2–5.2 rs, todo raio que sobe "cruza o disco" no primeiro passo e a metade de cima do quadro inicial sai amarela
sólida até o usuário orbitar. Verificado no Mesa llvmpipe com `BlackHole3D --capture`
([`images/legacy_default_gpu.png`](images/legacy_default_gpu.png)). O modo científico (GPU) e o renderer CPU tratam a
câmera no plano corretamente: o primeiro segmento não conta como cruzamento.

| Default legado (GPU) | Legado, elevação 1.25 (GPU) |
|---|---|
| ![default](images/legacy_default_gpu.png) | ![el 1.25](images/legacy_el125_gpu.png) |

---

## Equações do RHS

Estado 3-D: \((r,\theta,\phi,\dot r,\dot\theta,\dot\phi)\), \(\dot{}=\mathrm{d}/\mathrm{d}\lambda\), com \(E\) e \(L\)
conservados e \(\dot t = E/f\). Pelos símbolos de Christoffel \(\Gamma^r_{\theta\theta}=-rf\),
\(\Gamma^r_{\phi\phi}=-rf\sin^2\theta\):

\[
\ddot r=-\frac{r_s}{2r^2}f\,\dot t^2+\frac{r_s}{2r^2f}\dot r^2+r\,f\left(\dot\theta^2+\sin^2\theta\,\dot\phi^2\right),
\]
\[
\ddot\theta=-\frac{2}{r}\dot r\dot\theta+\sin\theta\cos\theta\,\dot\phi^2,\qquad
\ddot\phi=-\frac{2}{r}\dot r\dot\phi-2\cot\theta\,\dot\theta\dot\phi .
\]

Implementação: `geodesic_rhs` em `src/scientific/integrator_rk4.cpp`.

### Correção científica no RHS (vs legado)

| Caminho | Termo angular de \(\ddot r\) |
|---------|------------------------------|
| **Científico** (`bh_scientific`, `geodesic_scientific.comp`) | `+ r * f * (θ̇² + sin²θ φ̇²)` |
| **Legado** (`geodesic.comp`, `CPU-geodesic.cpp`) | `+ r * (…)` **sem** \(f\) |

Sem o \(f\) a condição nula não é preservada. O shader default **não** foi alterado.

### Dentro do horizonte

Se um estágio intermediário de RK cai em \(f\le0\), `geodesic_rhs` **congela a aceleração** (derivadas segundas = 0)
em vez de dividir por \(f\): o raio já está capturado e o estado continua finito (sem NaN). O shader científico faz
o mesmo (`if (f <= 0.0) return vec4(dr, dphi, 0.0, 0.0)`).

---

## Carta plana (plano orbital) — sem singularidade nos polos

`include/black_hole/planar_geodesic.hpp`, `src/scientific/planar_geodesic.cpp`.

Em Schwarzschild, por simetria esférica, \(\vec L=\vec r\times\mathrm{d}\vec r/\mathrm{d}\lambda\) é conservado e
cada geodésica fica no plano que passa pela origem com normal \(\hat n=\vec L/|\vec L|\). Escolhendo

\[
\hat e_1=\frac{\vec r_0}{|\vec r_0|},\qquad \hat n=\frac{\vec r_0\times\vec d_0}{|\vec r_0\times\vec d_0|},\qquad
\hat e_2=\hat n\times\hat e_1 ,
\]

e girando esse plano para \(\theta=\pi/2\), o problema 3-D vira 2-D em \((r,\phi)\) com \(\sin\theta\equiv1\),
\(\dot\theta\equiv0\):

\[
\ddot r=-\frac{r_s}{2r^2}f\,\dot t^2+\frac{r_s}{2r^2f}\dot r^2+r\,f\,\dot\phi^2,\qquad
\ddot\phi=-\frac{2}{r}\dot r\dot\phi,\qquad
\vec x(\lambda)=r\left(\cos\phi\,\hat e_1+\sin\phi\,\hat e_2\right).
\]

- **Exata**, não uma aproximação: é a mesma geodésica numa carta rodada. Os polos da carta global nunca aparecem.
- Raio puramente radial (\(|\vec L|\approx0\)): plano qualquer que contenha \(\hat e_1\) (`degenerate_radial`).
- Após cada passo o código reimpõe \(\theta=\pi/2\), \(\dot\theta=0\) (guarda contra arredondamento).
- Usada pelo renderer CPU e pelo shader científico (mesma formulação em `float`).

### Câmera = observador estático

`DirectionFrame::StaticObserver` (default) interpreta a direção do pixel no referencial ortonormal de um observador
**estático** na posição da câmera: \(\hat e_{\hat r}=\sqrt f\,\partial_r\Rightarrow k^r=\sqrt f\,n_r\);
\(\hat e_{\hat\phi}=\partial_\phi/r\Rightarrow k^\phi=n_\phi/r\); para \(|\vec n|=1\), \(E=\sqrt{f(r_{\rm cam})}\).
`DirectionFrame::Coordinate` (convenção do `geodesic.comp` / `make_ray_from_cartesian`) é mantido para comparação
com a carta 3-D. Segundo o comentário do shader científico, usar a direção como velocidade coordenada inflaria a
sombra em ~10 % em ângulo no raio default; o teste `test_traced_shadow_edge_vs_synge` pega esse erro.

### Testes

| Teste | Verifica |
|-------|----------|
| `scientific_ref` / `test_rk45_vs_rk4_and_planar_vs_3d` | raio genérico: carta plana vs carta 3-D, mesma semente (`Coordinate`), 3000 passos RK4 de \(d\lambda=0.002\): posições iguais a \(10^{-6}\); vínculo nulo plano \(<10^{-8}\) |
| `scientific_ref` / `test_pole_safety` | raio cujo plano contém o eixo polar fica finito; raio radial cai no horizonte e permanece no eixo |
| `scientific_ref` / `test_rk45_invalid_state` | na carta 3-D com \(\theta=0\) o RHS é não finito: o RK45 recusa o passo (`h_used = 0`, `error_norm = inf`) e não altera o estado |
| `cpu_render` / `test_pole_camera_and_rk45` | câmera perto do polo e **sobre** o eixo polar: render sem NaN, sem estourar o limite de passos |

---

## Critério de escape (`escape.hpp`)

Para uma geodésica nula com \(b=L/E\):

\[
\left(\frac{\mathrm{d}r}{\mathrm{d}\lambda}\right)^2=E^2-\frac{L^2f(r)}{r^2}=E^2\left[1-b^2V(r)\right],
\qquad V(r)=\frac{f}{r^2}=\frac{1}{r^2}-\frac{2M}{r^3}.
\]

\[
V'(r)=-\frac{2}{r^3}\left(1-\frac{3M}{r}\right)
\quad\Longrightarrow\quad
\text{máximo único em } r=3M=1.5\,r_s,\qquad V_{\max}=\frac{1}{27M^2}.
\]

1. **Parâmetro de impacto crítico.** Um raio vindo do infinito só tem ponto de retorno se \(b^2V_{\max}\ge1\):
   \[
   b_c=\frac{1}{\sqrt{V_{\max}}}=3\sqrt3\,M=\frac{3\sqrt3}{2}r_s\approx2.598\,r_s .
   \]
   \(b<b_c\): capturado; \(b>b_c\): defletido e escapa (`captured_from_infinity`).
2. **Escape provado.** Para \(r>3M\), \(V\) é estritamente decrescente. Se o raio está **saindo** (\(\dot r>0\)) em
   \(r>1.5\,r_s\), então \(r\) cresce, \(V\) diminui e \(\dot r^2=E^2(1-b^2V)\) só aumenta: \(\dot r\) nunca chega a
   zero e o raio vai ao infinito monotonicamente (`is_outgoing_beyond_photon_sphere`).
3. **Escape da cena.** Se, além disso, \(r\) já passou de um raio que envolve toda a geometria renderizável
   (disco e esferas), nada mais pode ser atingido (`will_escape_scene`). O renderer usa
   `scene_bound = 1.05·max(r_out, |c|+R) + 0.5` (rs).

Isso substitui, **só nos caminhos novos**, o `ESCAPE_R = 1e30` inalcançável do legado.

**Teste** `test_critical_capture_escape` (`scientific_ref`): raios lançados de \(r_0=60\,r_s\) com RK45
(tolerância \(10^{-11}\)), \(b=2.0,\,2.5\,r_s\) **capturados**; \(b=2.7,\,3.2\,r_s\) **escapam**, como previsto por
\(b_c\) (usando o \(b\) efetivo \(L/E\) da semente); mais checagens do predicado (saindo / entrando / dentro da esfera
de fótons).

---

## Sombra

Para um observador estático em \(r_o\), o ângulo \(\psi\) de um raio em relação à direção radial para dentro obedece
\(\sin\psi=(L/r_o)/(E/\sqrt{f(r_o)})=b\sqrt{f(r_o)}/r_o\). A borda da sombra é o raio com \(b=b_c\) (Synge 1966):

\[
\sin\alpha_{\rm sh}=\frac{b_c\sqrt{1-r_s/r_o}}{r_o},\qquad
\alpha_{\rm sh}\xrightarrow{r_o\gg r_s}\frac{b_c}{r_o}.
\]

`shadow_angular_radius` devolve \(\alpha\le\pi/2\) fora da esfera de fótons, \(\alpha=\pi-\arcsin(\dots)>\pi/2\)
dentro dela (cone de escape menor que um hemisfério) e \(\alpha\to\pi\) no horizonte; `shadow_angular_radius_far`
devolve \(b_c/r_o\).

| Observador | \(\alpha_{\rm sh}\) | Fonte |
|------------|--------------------|-------|
| 4.997 rs (câmera default) | 0.4836 rad = 27.7° | `quickstart_scientific` |
| 18 rs (cena showcase) | 0.1407 rad = 8.06° | `quickstart_scientific` |
| 1000 rs | 0.0026 rad = 0.15° | `quickstart_scientific` |

Na câmera default a sombra tem ≈ 55° de diâmetro angular para um FOV vertical de 60°.

**Testes:**

- `cpu_render` / `test_traced_shadow_edge_vs_synge`: bissecção do ângulo crítico captura/escape traçando raios a
  partir de câmeras em \(r=1.3,\,3,\,4.997,\,10,\,60\,r_s\); gate \(2\times10^{-3}\) relativo. Medido: traçado = teoria
  em todos os dígitos impressos (ex.: 4.997 rs → 0.483637 rad; 1.3 rs → 1.854383 rad, ramo \(>\pi/2\)).
- `cpu_render` / `test_shadow_size_matches_theory`: câmera a 60 rs, FOV 12°, sem disco; fração de pixels na sombra
  0.131484 vs 0.131327 previsto (gate 8 %).
- `scientific_ref` / `test_shadow_branches`: \(\pi/2\) na esfera de fótons, \(>\pi/2\) dentro, \(\to\pi\) no horizonte,
  continuidade em 1.5 rs; `test_critical_capture_escape`: limite distante \(b_c/r\) e forma \(\sin\alpha=b_c\sqrt f/r\).

---

## Deflexão fraca (`weak_field.hpp`)

\[
\alpha^{(1)}=\frac{4GM}{c^2b}=\frac{2r_s}{b},\qquad
\alpha^{(2)}=\frac{4M}{b}+\frac{15\pi}{4}\left(\frac{M}{b}\right)^2=\frac{2r_s}{b}+\frac{15\pi}{16}\left(\frac{r_s}{b}\right)^2
\quad(\text{Epstein \& Shapiro 1980}).
\]

Funções: `deflection_angle`, `deflection_angle_si`, `deflection_angle_second_order`, `deflection_scale_ratio`.

**Medido** (`test_weak_field_numeric_deflection`, carta plana + RK45, \(b=50\,r_s\), partindo e terminando em
\(r=4000\,r_s\)):

| | \(\alpha\) (rad) |
|--|-----------------|
| Numérico | 0.0412157 |
| 1ª ordem | 0.0400000 |
| 2ª ordem | 0.0411781 |

O numérico fica a ≈ 0.09 % da 2ª ordem (gate 1 %) e a ≈ 3 % da 1ª ordem (gate 5 %), e o teste exige que a 2ª
ordem esteja mais perto. O resíduo (≈ 3.8e-5 rad) é da ordem do termo de 3ª ordem omitido,
\(\tfrac{128}{3}(M/b)^3\approx4\times10^{-5}\) rad; os pontos inicial e final a 4000 rs (e não no infinito)
contribuem com uma correção menor.

---

## Integradores

| Integrador | Avaliações do RHS / passo | Passo | Onde |
|------------|---------------------------|-------|------|
| `legacyEulerStep` (GPU legado) | 1 | fixo `D_LAMBDA = 1e7` m | `geodesic.comp` |
| `euler_step` (CPU, só testes) | 1 | fixo | `integrator_rk4.cpp` |
| `rk4_step` / `planar_rk4_step` | 4 | renderer: geométrico \(d\lambda=\mathrm{clamp}(0.02\,r,\,0.005,\,2.0)\) rs; no CPU, além de \(1.05\,r_{\rm out}\) o teto vira \(\max(2.0,\,0.02\,r)\) | `integrator_rk4.cpp`, `planar_geodesic.cpp` |
| `rk45_adaptive_step` / `planar_rk45_step` | 7 por tentativa (estágio FSAL incluído) | adaptativo | `integrator_rk45.cpp` |

### RK45 (Dormand–Prince 5(4))

- Tableau de Dormand–Prince; solução de 5ª ordem propagada, solução embutida de 4ª ordem para o erro.
- Norma do erro: RMS sobre as 6 componentes de \((y_5-y_4)/(\mathrm{abs\_tol}+\mathrm{rel\_tol}\cdot\max(|y_0|,|y_5|))\);
  aceita se \(\le1\) (ou se já está em `h_min`).
- Controle: fator \(0.9\,\mathrm{err}^{-1/5}\), limitado a \([0.2,\,5]\); até 64 tentativas.
- Estado inválido (RHS não finito mesmo em `h_min`): devolve `h_used = 0`, `error_norm = +inf` e **não** altera o
  estado (não cai silenciosamente num passo de Euler).
- Defaults de `Rk45Options`: `abs_tol = rel_tol = 1e-10`, `h_min = 1e-6`, `h_max = 1.0`; o renderer usa
  `rk45_tol = 1e-8`.

### Medições

| Medida | Resultado | Fonte |
|--------|-----------|-------|
| Deriva do vínculo nulo \(|g_{\mu\nu}k^\mu k^\nu|\) após 100 passos (\(r_0=20\,r_s\), \(d\lambda=0.25\)) | Euler \(8.8\times10^{-3}\) vs RK4 \(2.4\times10^{-9}\) | `test_rk4_beats_euler_null_drift` |
| RK45 vs RK4 fino até \(\lambda=6\) | mesmo estado (\(10^{-6}\)) com **120** passos vs **3000** | `test_rk45_vs_rk4_and_planar_vs_3d` |
| Passos médios por raio, `bh_render_cpu` 400×300, cena default, elevação 1.25 | RK4 geométrico ≈ 111; RK45 (tol \(10^{-8}\)) ≈ 32 | JSON `mean_steps` |
| Passos médios por raio, mesma cena, pose default (elevação π/2) | RK4 ≈ 184 | JSON `mean_steps` |

Cada passo RK45 custa mais avaliações do RHS que um passo RK4, então o ganho em tempo é menor que a razão de passos.

---

## Kerr — só raios analíticos (`kerr_analytic.hpp`)

> **Kerr NÃO é simulado.** Não há métrica de Kerr, geodésicas de Kerr nem frame-dragging em lugar nenhum do código.
> Estes números servem para anotações, anéis-guia e checagens.

Fórmulas (Bardeen, Press & Teukolsky 1972), \(G=c=1\), \(a_*=a/M\), só \(|a_*|\) importa (clamp em [0, 1]); o
sentido da órbita vem do flag `prograde`:

\[
r_\pm=M\left(1\pm\sqrt{1-a_*^2}\right),\qquad r_E(\theta)=M\left(1+\sqrt{1-a_*^2\cos^2\theta}\right),
\]
\[
r_{\rm ISCO}=M\left(3+Z_2\mp\sqrt{(3-Z_1)(3+Z_1+2Z_2)}\right),\quad
Z_1=1+(1-a_*^2)^{1/3}\left[(1+a_*)^{1/3}+(1-a_*)^{1/3}\right],\quad Z_2=\sqrt{3a_*^2+Z_1^2},
\]
\[
r_{\rm ph}=2M\left\{1+\cos\left[\tfrac23\arccos(\mp a_*)\right]\right\},\qquad
\eta=1-\sqrt{1-\frac{2M}{3r_{\rm ISCO}}}
\]

(sinal de cima = prógrada). A forma de \(\eta\) usa a identidade \(\tilde E_{\rm ISCO}=\sqrt{1-2M/(3r_{\rm ISCO})}\),
finita em \(a_*=1\) prógrado (onde a expressão geral de \(\tilde E\) vira 0/0).

| \(a_*\) | \(r_+/M\) | ISCO prógrada | ISCO retrógrada | \(r_{\rm ph}\) pró / retro | \(\eta\) prógrada |
|---------|-----------|---------------|-----------------|----------------------------|-------------------|
| 0 | 2.0000 | 6.0000 M | 6.0000 M | 3M / 3M | 0.0572 (\(1-\sqrt{8/9}\)) |
| 0.5 | 1.8660 | 4.2330 M | 7.5546 M | — | 0.0821 |
| 0.9 | 1.4359 | 2.3209 M | 8.7174 M | — | 0.1558 |
| 0.998 | 1.0632 | 1.2370 M | 8.9944 M | — | 0.3210 |
| 1 | 1 | M | 9M | M / 4M | \(1-1/\sqrt3\approx0.4226\) (retrógrada: \(1-\sqrt{25/27}\approx0.0377\)) |

Linhas \(a_*=0.5,\,0.9,\,0.998\): saída de `build/gl/quickstart_scientific`. Linhas \(a_*=0\) e \(a_*=1\) e o
valor \(\eta(0.998)=0.32099\pm2\times10^{-5}\): asserções de `test_kerr_analytic` (`scientific_ref`), que também
verifica o limite \(a_*=0\) = Schwarzschild e a simetria \(\pm a_*\).

Ergosfera equatorial: \(2M\) para qualquer \(a_*\).

---

## O que NÃO é modelado

- **Kerr / spin** — só raios analíticos; nenhuma simulação.
- Kerr–Newman, carga, constante cosmológica.
- Plasma / MHD / GRMHD; opacidades; transferência radiativa; polarização.
- Disco com espessura física no hit test (o plano é infinitamente fino; ver [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md) §9).
- Fundo celeste real / HDRI: só um campo de estrelas procedural opcional (`--stars`) no renderer CPU; ele é
  consultado na direção assintótica do raio que escapa (o raio é levado a \(r\ge1000\,r_s\)), então as estrelas
  aparecem lenteadas, mas não são um catálogo real.
- Câmera em movimento (aberração); a câmera é um observador estático.
- Na GPU científica: sem modo corpo negro e sem estrelas; exposure / spin / orçamento de passos fixos no código
  (`SciParams`: 2.0, +1, 4000).
- No caminho **default**: nada disto — continua Euler, carta global, amostragem pontual, `ESCAPE_R = 1e30`.

---

## Biblioteca CPU `bh_scientific` (detalhe)

| Módulo | Papel |
|--------|-------|
| `units.hpp` | G, c, massa Sgr A*, `LEGACY_SAGA_RS_M = 1.269e10` |
| `schwarzschild.hpp` | \(f=1-r_s/r\), fótons 1.5 rs, ISCO 3 rs (= 6M) |
| `ray_state.hpp` | estado nulo 3-D (r, θ, φ, dr, dθ, dφ, E, L) + cartesiano |
| `integrator_rk4.hpp` | Euler (legado, testes) e **RK4 verdadeiro**; RHS congela dentro do horizonte |
| `integrator_rk45.hpp` | Dormand–Prince 5(4) adaptativo |
| `planar_geodesic.hpp` | carta do plano orbital, sem polos; câmera = observador estático |
| `conserved.hpp` | E, L, resíduo nulo \(g_{\mu\nu}k^\mu k^\nu\) |
| `escape.hpp` | \(b_c\), escape provado, sombra |
| `hit_testing.hpp` | segmento–plano, anel, segmento–esfera |
| `weak_field.hpp` | deflexão de 1ª e 2ª ordem |
| `orbits.hpp`, `redshift.hpp`, `disk_emission.hpp` | disco relativístico ([DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md)) |
| `kerr_analytic.hpp` | raios analíticos de Kerr (sem simulação) |
| `disk_model.hpp` | anel geométrico 2.2–5.2 rs do baseline |
| `camera_model.hpp` | órbita da câmera + warp do grid (paridade com o legado) |
| `scene_params.hpp` | JSON `black_hole.scene_params/v1` sem dependências |
| `cpu_renderer.hpp` | renderer headless determinístico multithread |
| `image_io.hpp` | PNG/BMP/PPM sem dependências |

## Limitações honestas

- Sem Kerr, sem plasma, sem transferência radiativa.
- A carta plana resolve os polos **nos caminhos novos**; o `geodesic.comp` legado mantém a carta global.
- O escape provado e os hit tests contínuos valem **só nos caminhos novos**; o legado mantém `ESCAPE_R = 1e30` e
  a amostragem pontual por contrato.
- Toda afirmação sobre GPU se refere ao Mesa **llvmpipe** (renderizador por software); não há medições em GPUs de
  hardware.

## Referências

- C. W. Misner, K. S. Thorne, J. A. Wheeler, *Gravitation* (1973), §25.
- S. M. Carroll, *Spacetime and Geometry* (2004), cap. 5.
- J. L. Synge, *The escape of photons from gravitationally intense stars*, MNRAS 131, 463 (1966).
- R. Epstein, I. I. Shapiro, *Post-post-Newtonian deflection of light by the Sun*, Phys. Rev. D 22, 2947 (1980).
- J. M. Bardeen, W. H. Press, S. A. Teukolsky, ApJ 178, 347 (1972).
- J. R. Dormand, P. J. Prince, *A family of embedded Runge–Kutta formulae*, J. Comput. Appl. Math. 6, 19 (1980).
