# Brief de revisão científica — visualização Schwarzschild (C++/OpenGL)

**Escopo:** expansão em escala GLUE-LP (modo científico separado), **sem alterar** o baseline visual legado.
**Alvos mantidos:** `BlackHole2D` (`2D_lensing.cpp`), `BlackHole3D` (`black_hole.cpp` + `geodesic.comp`).
**Fora do CMake:** `Gravity_Sim/` (protótipos CUDA legados).
**Não é nativo Blender;** o Blender é um frontend opcional (addon `black_hole_bridge`), dono de nada da física.

**Atualizado para a versão 0.8.0.** Legenda de estado: ✔ feito · ◐ parcial · ✘ aberto. Medições feitas num
container Linux com 4 CPUs, Mesa **llvmpipe** (OpenGL 4.5 por software) e Blender 4.0.2; nenhuma GPU de hardware.

---

## 1) O que a simulação realmente modela

### Caminho default (inalterado)

| Camada | Afirmação correta |
|---|---|
| Métrica | Equações de geodésicas nulas em coordenadas esféricas Schwarzschild (termos ~\(r_s/r\), \(f=1-r_s/r\)), **não** Kerr, **não** matéria acoplada, **não** eletromagnetismo. O termo radial `+ r*(…)` não tem o fator \(f\). |
| Integrador GPU (`geodesic.comp`) | **Euler de 1 estágio** (`legacyEulerStep`; historicamente rotulado `rk4`). Passo fixo `D_LAMBDA=1e7`, até 60 000 passos, `SagA_rs=1.269e10`, `ESCAPE_R=1e30` (inalcançável), disco UBO \(2.2\)–\(5.2\,r_s\). |
| CPU (`CPU-geodesic.cpp`) | Caminho RK4 real separado, legado; **não** é o que o render GPU executa. |
| Disco / horizonte | Disco fino por troca de sinal entre amostras + intercepto \(r\le r_s\); cor `vec3(1, r, 0.2)` com alpha `r`. **Não** é disco relativístico. A borda interna 2.2 rs fica dentro da ISCO (3 rs). |
| Vista inicial | Artefato documentado: na elevação `π/2` em `float` a câmera fica ≈ 2.8 km abaixo do plano, dentro do anel, e a metade de cima do quadro inicial sai amarela ([FISICA](FISICA_SCHWARZSCHILD.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)). |

### Caminhos científicos (novos na 0.8.0, ligados explicitamente)

| Camada | Afirmação correta |
|---|---|
| Referência CPU (`bh_scientific`, `double`) | Geodésicas nulas de Schwarzschild com RHS corrigido (`r·f`), RK4 e RK45 Dormand–Prince, carta do plano orbital (sem polos), câmera = observador estático, hits contínuos, escape provado. |
| Renderer CPU (`bh_render_cpu`) | Headless, determinístico, multithread; modos `legacy`, `relativistic` (Doppler + redshift gravitacional, \(g^4\)), `blackbody` (Page–Thorne, \(T_{\rm obs}=gT\)). |
| GPU científica (`BlackHole3D --scientific` / `--relativistic`) | `geodesic_scientific.comp`: mesma formulação em `float` (RK4 na carta plana, hits contínuos, escape provado, rs = 1), Doppler opcional; concordância com a CPU testada no llvmpipe. |
| Disco relativístico | Cinemática de órbitas circulares, fator \(g\), beaming \(g^4\), fluxo de Page–Thorne em forma fechada, Eddington, corpo negro → sRGB. **Sem** transferência radiativa. Ver [DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md). |
| Kerr | **Só raios analíticos** (`kerr_analytic.hpp`). Nada de geodésicas de Kerr. |

**Frase segura para papers/README:** *visualização OpenGL de geodésicas nulas de Schwarzschild; o caminho default
usa o integrador Euler legado (baseline preservado por contrato, `docs/BASELINE.md`); um modo científico separado
(RK4/RK45 em `double` na CPU e RK4 em `float` na GPU, carta do plano orbital, escape provado, disco de Page–Thorne
com Doppler e redshift gravitacional) é validado por testes de referência e por concordância CPU↔GPU no Mesa
llvmpipe. Kerr não é simulado.*

---

## 2) Correções por prioridade — modo científico separado

Nenhuma delas tocou o `geodesic.comp` da raiz.

### P0 (bloqueantes para claim científico)

| # | Item | Estado | Onde / evidência |
|---|------|--------|------------------|
| 1 | Integrador verdadeiro em shader dedicado; tolerância/erro explícitos | ✔ | GPU: RK4 em `geodesic_scientific.comp` (passo geométrico, sem adaptação); CPU: RK4 + RK45 Dormand–Prince com `abs_tol`/`rel_tol`. Tolerância CPU↔GPU publicada em `gpu_cpu_agreement` |
| 2 | Referência CPU independente (`double`), mesma métrica, comparação GPU↔CPU com tolerâncias publicadas | ✔ (llvmpipe) | `bh_scientific` + `bh_render_cpu`; `gpu_cpu_agreement`: 6 cenas/poses (inclusive FOV 40° com alvo deslocado, meia massa e `"objects": []`) × 2 modos, gate fração de pixels ruins ≤ 2e-3 e erro médio ≤ 0.5/255 |
| 3 | Diagnósticos de conservação (\(g_{\mu\nu}k^\mu k^\nu\), \(E\), \(L\)) | ◐ | vínculo nulo testado (Euler 8.8e-3 vs RK4 2.4e-9 em 100 passos; carta plana < 1e-8 em 3000); \(E\) é parâmetro do RHS; **sem** teste dedicado de \(\Delta L/L\) |
| 4 | Critério de escape derivado + comportamento no horizonte | ✔ | `escape.hpp` (saindo, além da esfera de fótons, além da cena); RHS congela a aceleração para \(f\le0\) |
| 5 | Hit testing contínuo (segmento–plano, segmento–esfera) | ✔ | `hit_testing.hpp`, shader científico; `test_hit_testing` |

### P1 (robustez / unidades)

| # | Item | Estado | Onde / evidência |
|---|------|--------|------------------|
| 6 | Unidades geométricas \(O(1)\) com mapa SI documentado | ✔ | rs = 1 na CPU e na GPU científica; `units.hpp`; host divide por `r_s_m` |
| 7 | Singularidades polares | ✔ | carta do plano orbital (exata); `test_pole_safety`, `test_pole_camera_and_rk45`; RK45 recusa estado inválido na carta 3-D |
| 8 | Passo adaptativo perto de \(1.5\,r_s\) e do horizonte | ◐ | CPU: RK45 adaptativo (`--integrator rk45`) e passo geométrico \(\propto r\) (menor perto do buraco); GPU: só passo geométrico |
| 9 | Suite científica no `ctest`, desacoplada dos invariantes do baseline | ✔ | `scientific_ref`, `cpu_render`, `shader_contract`, `gpu_cpu_agreement`, testes de CLI — separados de `source_invariants` / `style_contract` |

### P2 (depois de Schwarzschild validado)

| # | Item | Estado | Onde / evidência |
|---|------|--------|------------------|
| 10 | Kerr (\(a/M\)) com testes de frame-dragging | ✘ | só raios analíticos (`kerr_analytic.hpp`, limites testados em `test_kerr_analytic`); nenhuma geodésica de Kerr |
| 11 | Emissão de disco + Doppler / redshift gravitacional | ✔ CPU, ◐ GPU | CPU: `relativistic` e `blackbody` (Page–Thorne); GPU: `--relativistic` (Doppler/redshift/\(g^4\) sobre a paleta legada, sem corpo negro) |
| 12 | Fundo celeste/HDRI; resolução adaptativa | ◐ | estrelas procedurais opcionais na CPU (`--stars`), consultadas na direção assintótica do raio; sem HDRI; CPU com qualquer resolução e supersampling; sem amostragem adaptativa |
| 13 | Adaptador Blender — só frontend | ✔ | addon 0.8.0: render bridge (`bh_render_cpu` → fundo de câmera / plano emissivo), anéis-guia (1.5 rs, 2.598 rs, 3 rs), presets de animação, paridade de cor; verificado no Blender 4.0.2 headless (Cycles). O Blender não faz lensing |

---

## 3) Testes sugeridos — estado e resultados

### A) Deflexão fraca (luz) — ✔

Para \(b\gg r_s=2M\): \(\delta^{(1)}=2r_s/b\); 2ª ordem \(\delta^{(2)}=2r_s/b+\tfrac{15\pi}{16}(r_s/b)^2\) (`weak_field.hpp`).
**Refs:** Carroll, *Spacetime and Geometry* (2004), cap. 5; Wald, *General Relativity* (1984), §6.3; MTW (1973), §25;
Epstein & Shapiro, Phys. Rev. D 22, 2947 (1980) para a 2ª ordem.

**Medido** (`test_weak_field_numeric_deflection`, carta plana + RK45, \(b=50\,r_s\)): numérico 0.0412157 rad;
1ª ordem 0.04; 2ª ordem 0.0411781 → a 0.09 % da 2ª ordem (gate 1 %), a 3 % da 1ª ordem (gate 5 %).

**Correção do critério original:** o aceite "\(10^{-4}\) contra \(2r_s/b\) em \(b\ge20\,r_s\)" era inalcançável por
construção: o termo de 2ª ordem vale \(\tfrac{15\pi}{32}\,r_s/b\) relativo à 1ª ordem (≈ 3 % em \(b=50\,r_s\)), então a
comparação justa é contra a fórmula de 2ª ordem (ou contra a integral exata).

### B) Esfera de fótons / captura crítica — ✔

\(r_{\rm ph}=\tfrac32r_s\), \(b_c=\tfrac{3\sqrt3}{2}r_s\approx2.598\,r_s\).
**Refs:** Carroll §5.3; Chandrasekhar, *The Mathematical Theory of Black Holes* (1983), cap. 3; MTW Box 25.7.

**Medido:** `test_critical_capture_escape` — partindo de 60 rs, \(b=2.0,\,2.5\,r_s\) capturados e \(b=2.7,\,3.2\,r_s\)
escapam. `test_traced_shadow_edge_vs_synge` — borda traçada da sombra = fórmula de Synge em
\(r_{\rm cam}=1.3,\,3,\,4.997,\,10,\,60\,r_s\) (gate \(2\times10^{-3}\); ex.: 0.483637 rad em 4.997 rs).
`test_shadow_size_matches_theory` — área da sombra renderizada 0.131484 vs 0.131327 previsto (gate 8 %).

### C) Quantidades conservadas — ◐

\(E=f\dot t\), \(L=r^2\sin^2\theta\,\dot\phi\), \(g_{\mu\nu}\dot x^\mu\dot x^\nu=0\).
**Refs:** Carroll §5.4; Wald §6.3; MTW §25.

**Medido:** vínculo nulo após 100 passos (\(r_0=20\,r_s\), \(d\lambda=0.25\)): Euler \(8.8\times10^{-3}\), RK4
\(2.4\times10^{-9}\); carta plana \(<10^{-8}\) após 3000 passos. **Aberto:** teste dedicado de \(\max|\Delta L|/L_0\)
ao longo do raio (o \(E\) entra no RHS como constante).

### D) Extras — ✔

| Extra | Estado | Teste |
|-------|--------|-------|
| Geodésicas nulas radiais | ✔ | `test_conserved_definitions` (semente), `test_pole_safety` (raio radial cai e fica no eixo) |
| CPU RK4 ↔ GPU científico, mesmas condições | ✔ (llvmpipe) | `gpu_cpu_agreement`: az 0 / el 1.25 com erro máximo 1/255; nas outras poses ≤ 4 pixels de borda em 30 000 fora da tolerância, erro médio < 0.08/255 |
| Câmera perto do polo \(\theta\approx0,\pi\) | ✔ | `test_pole_camera_and_rk45` (perto do polo e sobre o eixo) |
| Planar vs carta 3-D | ✔ | `test_rk45_vs_rk4_and_planar_vs_3d` (\(10^{-6}\)) |
| RK45 vs RK4 fino | ✔ | mesmo estado com 120 passos vs 3000 |
| Page–Thorne forma fechada vs quadratura | ✔ | `test_page_thorne_flux` (gate \(2\times10^{-3}\)) |
| Assimetria Doppler e inversão do spin | ✔ | `test_relativistic_asymmetry` |
| Determinismo do renderer | ✔ | bit a bit idêntico para qualquer nº de threads |

---

## 4) Adaptador Blender — implementado como frontend

```
[ C++ core físico ]  --export-->  JSON scene_params/v1  +  PNG/sequência do bh_render_cpu
        ^                              |
        |                              v
   (única fonte da verdade)      [ addon black_hole_bridge 0.8.0 ]
                                      |
                                      v
                    cena: disco (paridade de cor), grid, anéis-guia, câmera, animação;
                    fundo de câmera / plano emissivo com o render lenteado da CPU
```

- **Física permanece em C++.** O Blender **não** reimplementa geodésicas e não faz lensing; ele importa os renders do
  `bh_render_cpu` (`render_bridge.py`).
- Camada única de coordenadas (`constants.cpp_to_blender`): C++ (x, y, z) Y-up → Blender (x, −z, y) Z-up, rotação
  própria de +90° em X, para que a câmera do Blender enquadre como a do C++.
- Paridade de cor: emissão pura \((1, r, 0.2)\cdot r\) do pixel OpenGL, view transform "Raw"; aviso quando a câmera
  está dentro da fatia do disco (caso do default do C++); elevação 1.25 recomendada.
- Pacote separado; **não** misturado no target `BlackHole3D`.

---

## 5) O que **não** inventar

| Proibido | Motivo |
|---|---|
| Screenshots / frames "de validação" fabricados | As imagens em `docs/images/` são renders reais (`BlackHole3D --capture` no llvmpipe, `bh_render_cpu`, Blender 4.0.2 Cycles). Qualquer imagem nova tem de sair desses comandos |
| Números de desempenho de GPU | Só há llvmpipe (software). Nenhum FPS ou tempo de GPU de hardware foi medido |
| Claims de Kerr "com acurácia medida" | Kerr é só analítico; não há geodésica de Kerr para medir |
| Afirmar que o GPU legado é RK4 | É Euler; o nome já foi corrigido |
| Trocar constantes do baseline (`SagA_rs`, `D_LAMBDA`, 60k, `ESCAPE_R`, disco 2.2–5.2, câmera, FOV 60°) no path default | Quebra o contrato visual (`docs/BASELINE.md` + `tests/validate_source_invariants.py`) |
| "Corrigir" o artefato da vista inicial no default | Faz parte do baseline travado; só pode existir como opção explícita |
| Promover `Gravity_Sim/` como produto | Legado CUDA, fora do CMake |
| "GR completa" / plasma / EHT-ready / espectro calibrado | Fora do modelo: sem transferência radiativa, brilho relativo (Reinhard), cor de um único \(T_{\rm obs}\) |
| Tratar o disco a 1 % de Eddington como o Sgr A* real | O Sgr A* real é sub-Eddington extremo (RIAF); o cenário é didático |
| Números de erro inventados | Só reportar o que `ctest` / os binários medirem (ver `STATUS.md`) |

---

## Ordem operacional sugerida (GLUE-LP) — estado

1. ✔ Congelar baseline (invariantes + hashes).
2. ✔ Scaffold científico (CPU `double` + shader novo + flags `--scientific` / `--relativistic`).
3. ✔ P0 + testes A–B; ◐ teste C (falta \(\Delta L/L\) dedicado).
4. ✔ Documentar limiares reais medidos (`STATUS.md`, este brief, [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md)).
5. ◐ Disco relativístico ✔, Blender ✔; **Kerr ✘**; validação em GPU de hardware ✘.

**Gate de honestidade:** modo científico "completo" = P0 + testes A–C passando com tolerâncias publicadas; baseline
legado intacto. Hoje: P0 e A–B ✔, C parcial (vínculo nulo ✔, \(\Delta L\) ✘); baseline intacto (`source_invariants`,
`style_contract`).
