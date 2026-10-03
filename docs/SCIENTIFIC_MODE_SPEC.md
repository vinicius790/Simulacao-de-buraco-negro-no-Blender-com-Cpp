# Especificação do modo científico / Scientific Mode Specification

Este documento define o modo científico **sem alterar o baseline legado** e, desde a versão **0.8.0**, registra o
estado de cada componente e de cada gate de validação, com ponteiros para o código e para os testes.

Legenda: ✔ **feito** · ◐ **parcial** · ✘ **aberto**.

> O modo científico **não é o default**. `BlackHole3D` sem flags continua rodando o `geodesic.comp` histórico
> (`legacyEulerStep`, `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 passos, `ESCAPE_R = 1e30`), protegido por
> `tests/validate_source_invariants.py` e `tests/test_style_contract.py`. O modo científico é ligado por flag
> explícita (`--scientific` / `--relativistic`) ou usado via `bh_scientific` / `bh_render_cpu`.

## Por que ele é separado

Várias correções mudariam trajetórias e pixels. Por isso elas vivem atrás de um modo distinto, em vez de entrar em
silêncio no shader atual. Isso continua valendo: nenhuma correção da 0.8.0 tocou o `geodesic.comp` da raiz.

---

## Componentes exigidos

### 1. Referência CPU independente em precisão dupla — ✔ feito

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Métrica de Schwarzschild numa carta documentada | ✔ | carta de Schwarzschild \((t,r,\theta,\phi)\), `schwarzschild.hpp`; carta do plano orbital, `planar_geodesic.hpp` ([FISICA](FISICA_SCHWARZSCHILD.md)) |
| Condições iniciais determinísticas | ✔ | `make_ray_from_cartesian`, `make_planar_ray` (observador estático); `test_static_observer_seeding`; render bit a bit idêntico para qualquer nº de threads (`cpu_render` / `test_determinism_threads`) |
| RK4 ou RK embutido adaptativo | ✔ ambos | `rk4_step` (4 estágios), `rk45_adaptive_step` (Dormand–Prince 5(4)); `test_rk45_vs_rk4_and_planar_vs_3d` |
| Diagnósticos de conservação (vínculo nulo, \(E\), \(L\)) | ✔ | `conserved.hpp`, `planar_null_constraint`; `test_conserved_definitions`, `test_rk4_beats_euler_null_drift` |

### 2. Integrador GPU corrigido — ✔ feito

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Shader/programa separado do `geodesic.comp` | ✔ | `shaders/geodesic_scientific.comp`, carregado só com `--scientific` / `--relativistic` |
| Sem reaproveitar o nome/caminho `legacyEulerStep` | ✔ | RK4 clássico (`rk4`) com o termo corrigido `r·f`; `shader_contract` verifica que o shader legado não contém código científico |
| Estratégia de erro/tolerância se houver passo adaptativo | ✔ (GPU usa passo **não** adaptativo) | GPU: RK4 com passo geométrico \(d\lambda=\mathrm{clamp}(0.02\,r,0.005,2.0)\); tolerância CPU↔GPU publicada em `gpu_cpu_agreement`. RK45 adaptativo existe só na CPU, com `abs_tol`/`rel_tol` explícitos |
| Layout de UBO verificado | ✔ | `SciParams` (binding 4, 32 bytes, `static_assert`), conferido GLSL ↔ C++ campo a campo em `shader_contract` |

### 3. Robustez de coordenadas — ✔ feito (caminhos novos)

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Clamp defensivo de entradas de funções trigonométricas inversas | ✔ | `std::clamp` antes de `acos` (renderer, testes); arcsin da sombra com `s ≤ 1` (`shadow_angular_radius`) |
| Evitar ou tratar singularidades polares | ✔ | carta do plano orbital (\(\theta\equiv\pi/2\)), exata por simetria esférica; `test_pole_safety`, `cpu_render` / `test_pole_camera_and_rk45` (câmera perto do polo e sobre o eixo). Na carta 3-D, o RK45 recusa estados inválidos (`test_rk45_invalid_state`) |
| Definir comportamento no/perto do horizonte | ✔ | segmento–esfera com o horizonte; RHS congela a aceleração quando \(f\le0\) (sem NaN); câmera dentro do horizonte = captura imediata |
| O mesmo no shader legado | ✘ por contrato | `geodesic.comp` mantém a carta global (eixo polar = Z do mundo, dentro do plano do disco) |

### 4. Hit testing contínuo — ✔ feito (caminhos novos)

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Cruzamento segmento–plano do disco | ✔ | `hit::segment_plane_crossing` (ponto interpolado, exatamente no plano) + `point_in_annulus`; `test_hit_testing` |
| Segmento–esfera para objetos/horizonte | ✔ | `hit::segment_sphere_hit` (menor \(t\in[0,1]\); começo dentro ⇒ \(t=0\)); `test_hit_testing` |
| Sem depender só de amostragem pontual | ✔ | renderer CPU e shader científico escolhem o hit mais cedo no segmento; câmera no plano do disco: o primeiro segmento não conta como cruzamento |
| O mesmo no shader legado | ✘ por contrato | amostragem pontual mantida (origem do artefato da vista inicial, ver [FISICA](FISICA_SCHWARZSCHILD.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)) |

### 5. Unidades normalizadas — ✔ feito

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Unidades geométricas com valores ~O(1) | ✔ | renderer CPU e shader científico em **rs = 1**; o host (`black_hole.cpp`, `unitScale()`) divide câmera, objetos e espessura por `r_s_m` |
| Mapeamento para SI mantido | ✔ | `units.hpp` (`length_from_M`, `M_from_length`, `seconds_from_M`, `sagittarius_a_rs_si`); `SceneParams::r_s_m`; `test_units_sag_a` |

### 6. Critério de escape seguro — ✔ feito (caminhos novos)

| Requisito | Estado | Onde / teste |
|-----------|--------|--------------|
| Critério derivado que **prova** que o raio não volta à geometria visível | ✔ | `escape::will_escape_scene`: saindo (\(\dot r>0\)) **e** além da esfera de fótons (onde \(V=f/r^2\) é decrescente, logo \(\dot r\) nunca zera) **e** além do raio que envolve a cena. Derivação em [FISICA](FISICA_SCHWARZSCHILD.md#critério-de-escape-escapehpp) |
| Substituir o raio inalcançável | ✔ nos caminhos novos | `ESCAPE_R = 1e30` permanece só no `geodesic.comp` legado |

---

## Gates de validação

| Gate | Estado | Teste (CTest / função) | Resultado medido |
|------|--------|------------------------|------------------|
| Geodésicas nulas radiais | ✔ | `scientific_ref` / `test_conserved_definitions` (semente radial: \(E>0\), vínculo nulo ≈ 0), `test_pole_safety` (raio radial cai no horizonte e fica no eixo) | — |
| Deflexão fraca | ✔ | `scientific_ref` / `test_weak_field_one_over_b`, `test_weak_field_numeric_deflection` | \(b=50\,r_s\): numérico 0.0412157 rad vs 1ª ordem 0.04 vs 2ª ordem 0.0411781 (≈ 0.09 % da 2ª ordem; gate 1 %) |
| Captura/escape quase críticos | ✔ | `scientific_ref` / `test_critical_capture_escape`; `cpu_render` / `test_traced_shadow_edge_vs_synge` | \(b=2.0,2.5\,r_s\) capturados, \(2.7,3.2\,r_s\) escapam; borda traçada da sombra = Synge (gate \(2\times10^{-3}\)) em 5 raios de câmera |
| Deriva do vínculo nulo | ✔ | `scientific_ref` / `test_rk4_beats_euler_null_drift`, `test_rk45_vs_rk4_and_planar_vs_3d` | 100 passos: Euler \(8.8\times10^{-3}\) vs RK4 \(2.4\times10^{-9}\); carta plana \(<10^{-8}\) após 3000 passos |
| Deriva das quantidades conservadas | ◐ | \(E\) é constante **por construção** (entra no RHS como parâmetro, \(\dot t=E/f\)); \(L=r^2\sin^2\theta\,\dot\phi\) é evoluído pelo integrador e **não** há teste dedicado de \(\Delta L/L\). A deriva do vínculo nulo, que acopla \(E\), \(L\) e \(\dot r\), é testada; `test_conserved_definitions` só confere as definições na semente | — |
| Estados de câmera/raio perto do polo | ✔ | `scientific_ref` / `test_pole_safety`; `cpu_render` / `test_pole_camera_and_rk45` | sem NaN, sem estouro de passos |
| Concordância de trajetória CPU vs GPU com tolerâncias documentadas | ✔ (Mesa llvmpipe) | `gpu_cpu_agreement` (`tests/test_gpu_cpu_agreement.py` + `tools/image_diff.py`): 6 cenas/poses (inclusive FOV 40° com alvo deslocado, meia massa e `"objects": []`) × 2 modos, 200×150; gate: fração de pixels com erro > 24/255 ≤ \(2\times10^{-3}\) e erro médio ≤ 0.5/255 | az 0 / el 1.25, meia massa e `"objects": []`: erro máximo 1/255; demais: no máximo 4 pixels de borda em 30 000 fora da tolerância (≤ \(1.3\times10^{-4}\)), erro médio < 0.08/255. **Não** executado em GPUs de hardware |

Gates adicionais que surgiram na 0.8.0:

| Gate | Estado | Teste |
|------|--------|-------|
| Tamanho da sombra renderizada vs \(b_c\) analítico | ✔ | `cpu_render` / `test_shadow_size_matches_theory` (0.131484 vs 0.131327; gate 8 %) |
| Page–Thorne forma fechada vs quadratura | ✔ | `scientific_ref` / `test_page_thorne_flux` (gate \(2\times10^{-3}\)) |
| Assimetria Doppler e inversão do spin | ✔ | `cpu_render` / `test_relativistic_asymmetry` |
| Paridade de cor com o legado (`vec3(1, r, 0.2)·r`) e simetria espelhada | ✔ | `cpu_render` / `test_legacy_structure` |
| Baseline intacto | ✔ | `source_invariants`, `style_contract`, `shader_contract` |

---

## Capacidades opcionais futuras

| Capacidade | Estado |
|------------|--------|
| Métrica de Kerr / spin | ✘ **aberto** — só raios analíticos (`kerr_analytic.hpp`: ISCO, horizontes, ergosfera, órbitas de fótons, eficiência). Nenhuma geodésica de Kerr, nenhum frame-dragging. Não reivindicar |
| Emissão relativística do disco e desvios Doppler/gravitacional | ✔ CPU (`relativistic`, `blackbody` com Page–Thorne); ◐ GPU (`--relativistic`: Doppler/redshift/\(g^4\) sobre a paleta legada; sem corpo negro). Ver [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md) |
| Fundo celeste / HDRI para visualizar o lensing | ◐ campo de estrelas procedural opcional (`--stars`) no CPU, consultado na direção assintótica do raio (lenteado); sem HDRI nem catálogo real; nada na GPU |
| Resolução maior ou adaptativa | ◐ `bh_render_cpu` aceita qualquer resolução e supersampling S×S; a GPU continua 200×150 (contrato); sem amostragem adaptativa |
| Adaptador Blender como frontend separado (não dono da física) | ✔ addon 0.8.0: render bridge (`bh_render_cpu` → fundo de câmera / plano emissivo), anéis-guia, presets de animação, paridade de cor; o Blender não faz lensing |
| Controles científicos em runtime no `BlackHole3D` | ◐ FOV e alvo da câmera vêm do `--scene`; exposure, spin e orçamento de passos ainda fixos no código (`SciParams`) |
| Validação em GPUs de hardware (NVIDIA / AMD / Intel) | ✘ aberto — todas as medições GPU são em Mesa llvmpipe |
| Transferência radiativa / GRMHD | ✘ fora do escopo |

## EN (summary)

All six required components are implemented in the new paths (`bh_scientific`, `bh_render_cpu`,
`geodesic_scientific.comp`): double-precision CPU reference with RK4 and adaptive Dormand–Prince RK45, a separate
GPU RK4 program with a verified `SciParams` UBO, a pole-free orbital-plane chart, continuous segment hit tests,
rs = 1 units with an SI mapping, and a proof-based escape criterion. Every validation gate passes except a
dedicated angular-momentum drift test (partial: \(E\) is a parameter of the RHS, \(L\) is evolved and not tested
on its own; the null-constraint drift is tested). CPU↔GPU agreement is enforced on Mesa llvmpipe only. Kerr remains analytic-only.
The legacy default path is unchanged.
