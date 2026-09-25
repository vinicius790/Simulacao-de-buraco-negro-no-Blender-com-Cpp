# Física Schwarzschild — o que É e o que NÃO É modelado

## PT / EN

### Coordenadas / Coordinate chart

Usamos coordenadas de **Schwarzschild** `(t, r, θ, φ)` com assinatura `(−,+,+,+)`.

\[
\mathrm{d}s^2 = -f\,\mathrm{d}t^2 + f^{-1}\,\mathrm{d}r^2 + r^2\mathrm{d}\theta^2 + r^2\sin^2\theta\,\mathrm{d}\phi^2,
\quad f = 1 - \frac{r_s}{r},\quad r_s = \frac{2GM}{c^2}.
\]

Horizonte de eventos: \(r = r_s\).

### O que É modelado (biblioteca científica)

| Item | Valor / status |
|------|----------------|
| Raio de Schwarzschild \(r_s\) | `2GM/c²` (helpers em `units.hpp` / `schwarzschild.hpp`) |
| Esfera de fótons | \(r = 1{,}5\,r_s = 3M\) |
| ISCO (Schwarzschild, órbitas estáveis) | \(r = 6M = 3\,r_s\) |
| Deflexão fraca analítica | \(\alpha \approx 4GM/(c^2 b) = 2 r_s / b\) |
| Estado de raio nulo (CPU, `double`) | `RayState` |
| Integrador RK4 verdadeiro (4 estágios) | `rk4_step` |
| Diagnósticos E, L e \(g_{\mu\nu}k^\mu k^\nu\) | `conserved.hpp` |
| Mapeamento Sag A* (massa legada) | \(M = 8{,}54\times10^{36}\,\mathrm{kg}\) → \(r_s \approx 1{,}269\times10^{10}\,\mathrm{m}\) |

**Nota sobre “ISCO = 6 rs”:** em unidades geométricas com \(r_s = 2M\), ISCO = \(6M = 3 r_s\). Textos que escrevem “6 rs” frequentemente confundem \(r_s\) com \(M\). Neste código, `isco_radius(rs) = 3*rs` e `isco_in_units_of_M() = 6`.

### O que o baseline GPU legado faz (default)

- Integrador **Euler explícito de 1 estágio** (`legacyEulerStep`), historicamente chamado de RK4 por engano.
- Mesma forma algébrica de `geodesicRHS` (redução de 2ª ordem).
- Disco de acreção como anel equatorial simples (cor por raio), **sem** emissão relativística / Doppler / redshift gravitacional completo.
- Hit-testing por amostragem pontual / cruzamento de plano equatorial.
- Constantes históricas: `D_LAMBDA=1e7`, 60 000 passos, `ESCAPE_R=1e30`.

### O que NÃO é modelado (ainda)

- **Kerr / spin** — não implementado; não reivindicar.
- Métrica de Kerr–Newman, ergósfera, frame-dragging.
- Plasma / MHD do disco; opacidades; transferência radiativa.
- Lensing de fundo celeste / HDRI (apenas silhueta + disco + objetos).
- Critérios de escape cientificamente derivados (o `1e30` é legado).
- Robustez completa nos polos (\(\sin\theta\to 0\)).
- Modo científico GPU ligado por padrão (`geodesic_scientific.comp` é stub).

### Equações do RHS (referência)

Estado: \((r,\theta,\phi,\dot r,\dot\theta,\dot\phi)\) com \(\dot{}=\mathrm{d}/\mathrm{d}\lambda\), \(E\) e \(L\) conservados.

\(f=1-r_s/r\), \(\mathrm{d}t/\mathrm{d}\lambda = E/f\).

As derivadas segundas seguem `CPU-geodesic.cpp` / `geodesic.comp` (ver `integrator_rk4.cpp`).

### Correção científica no RHS (vs legado)

No termo de aceleração radial, a forma de Christoffel correta é

\[
\frac{\mathrm{d}^2 r}{\mathrm{d}\lambda^2} \supset + r\, f\,(\dot\theta^2 + \sin^2\theta\,\dot\phi^2).
\]

O baseline `geodesic.comp` / `CPU-geodesic.cpp` usa `+ r*(...)` **sem** o fator \(f\). Isso impede a conservação da condição nula. A biblioteca `bh_scientific` aplica a forma corrigida; o shader default **não** foi alterado.


## Biblioteca CPU `bh_scientific` (detalhe)

A lib em `include/black_hole/` + `src/scientific/` é a referência em `double`:

| Módulo | Papel |
|--------|-------|
| `units.hpp` | G, c, massa Sag A*, `LEGACY_SAGA_RS_M = 1.269e10` |
| `schwarzschild.hpp` | f=1−rs/r, fotão 1.5 rs, ISCO 3 rs (=6M) |
| `ray_state.hpp` | estado nulo (r,θ,φ,dr,dθ,dφ,E,L) + Cartesian |
| `integrator_rk4.hpp` | Euler (legado, testes) e **RK4 verdadeiro** |
| `conserved.hpp` | E, L, residual nulo gμν kμ kν |
| `weak_field.hpp` | α ≈ 2 rs / b |
| `disk_model.hpp` | anulo 2.2–5.2 rs; Novikov–Thorne **não** |
| `camera_model.hpp` | órbita + warp do grid |
| `scene_params.hpp` | JSON I/O sem nlohmann |

### RHS científico vs legado

Termo angular radial:

- **Científico:** `+ r * f * (θ̇² + sin²θ φ̇²)`
- **Legado GPU/CPU visual:** `+ r * (…)` **sem** f

Por isso o modo científico conserva melhor a condição nula; o visual default permanece intocado.

### Unidades geométricas no shader científico

`shaders/geodesic_scientific.comp` assume **rs = 1**. O host deveria escalar câmera/disco antes de ligar esse shader (ainda não há switch de produção).

### Limitações honestas

- Sem spin (Kerr), sem plasma, sem transferência radiativa.
- Polos (`sinθ → 0`) singularidades de coordenada — permanecer equatorial ou clampar.
- `ESCAPE_R = 1e30` é legado, não um critério de escape derivado.
- Disco: hit geométrico + cor; sem redshift gravitacional / Doppler / temperatura.
