# Brief de revisão científica — visualização Schwarzschild (C++/OpenGL)

**Escopo:** expansão em escala GLUE-LP (modo científico separado), **sem alterar** o baseline visual legado.  
**Alvos mantidos:** `BlackHole2D` (`2D_lensing.cpp`), `BlackHole3D` (`black_hole.cpp` + `geodesic.comp`).  
**Fora do CMake:** `Gravity_Sim/` (protótipos CUDA legados).  
**Não é nativo Blender;** Blender só como adaptador futuro de frontend (`docs/SCIENTIFIC_MODE_SPEC.md`).

---

## 1) O que a simulação atual realmente modela

| Camada | Afirmação correta |
|---|---|
| Métrica | Equações de geodésicas nulas em coordenadas esféricas Schwarzschild (termos ~\(r_s/r\), \(f=1-r_s/r\)), **não** Kerr, **não** matéria acoplada, **não** eletromagnetismo. |
| Integrador GPU (`geodesic.comp`) | Passo explícito de **Euler de 1 estágio** (`legacyEulerStep`; historicamente rotulado `rk4`). Passo fixo `D_LAMBDA=1e7`, até 60 000 passos, `SagA_rs=1.269e10`, `ESCAPE_R=1e30`, disco UBO tipicamente \(2.2\)–\(5.2\,r_s\). |
| CPU (`CPU-geodesic.cpp`) | Caminho **RK4 real** separado — útil como referência futura, **não** é o que o render GPU legado executa. |
| Disco / horizonte | Disco fino por amostragem de plano + intercepto \(r\le r_s\); **não** é solução de disco relativístico (Novikov–Thorne, etc.). |
| Lensing | Aproximação de **ray-marching de geodésicas nulas** para visualização; **não** é GR completa (sem backreaction, sem plasma, sem spin, sem redshift espectral calibrado). |
| Engenharia já presente | Barreira GL image+texture, layouts `std140`, textura compute persistente, UBOs dirty, nome honesto do integrador, testes estáticos de invariantes — **não** equivalem a validação dinâmica GR. |

**Frase segura para papers/README:** *visualização OpenGL de geodésicas nulas Schwarzschild com integrador Euler legado no GPU; baseline preservado por contrato (`docs/BASELINE.md`).*

---

## 2) Correções por prioridade — **modo científico separado**

Não substituir silenciosamente `geodesic.comp` legado. Novo shader/programa + flag/preset + referência CPU `double`.

### P0 (bloqueantes para claim científico)
1. **Integrador verdadeiro:** RK4 clássico ou RK embutido adaptativo (ex.: Dormand–Prince) em shader dedicado; tolerância/erro explícitos.
2. **Referência CPU independente** (double): mesmas CI, mesma métrica documentada; comparação GPU↔CPU com tolerâncias publicadas.
3. **Diagnósticos de conservação:** \(g_{\mu\nu}k^\mu k^\nu=0\), energia \(E\) e momento angular \(L\) (estáticos) ao longo do raio.
4. **Critério de escape** derivado (substituir `1e30` só no modo científico) + comportamento no horizonte.
5. **Hit-testing contínuo:** cruzamento segmento–plano (disco) e segmento–esfera (horizonte), sem depender só de amostragem pontual.

### P1 (robustez / unidades)
6. Unidades geométricas \(G=c=1\) (valores \(\sim O(1)\)) com mapa SI documentado; evitar float extremos.
7. Singularidades polares: clamp em \(\arcsin/\arccos\); tratar \(\sin\theta\to 0\).
8. Passo adaptativo perto de \(r\sim 1.5\,r_s\) e do horizonte.
9. Suite de regressão científica (abaixo) no `ctest`, **desacoplada** de `validate_source_invariants.py` (que trava o baseline visual).

### P2 (depois de Schwarzschild validado)
10. Kerr (\(a/M\)) com testes de frame-dragging — **só** após P0/P1.
11. Emissão de disco + Doppler / redshift gravitacional.
12. Fundo celestial/HDRI para lensing; resolução adaptativa.
13. Adaptador Blender (§4) — frontend apenas.

---

## 3) Testes sugeridos (fórmulas + refs reais)

Use refs padrão; cite edições concretas no README científico.

### A) Deflexão fraca (luz)
Para impacto \(b\gg r_s=2M\):
\[
\delta \approx \frac{4GM}{c^2 b} = \frac{2 r_s}{b}
\]
(à 1ª ordem pós-Newtoniana).  
**Refs:** Carroll, *Spacetime and Geometry* (Addison-Wesley, 2004), cap. 5 / §7.3; Wald, *General Relativity* (Chicago, 1984), §6.3; MTW, *Gravitation* (Freeman, 1973), §18.4 / Box 25.6.  
**Aceite:** \(|\delta_{\mathrm{num}}-\delta_{\mathrm{th}}|/\delta_{\mathrm{th}} < \varepsilon\) (ex. \(10^{-2}\) Euler→RK4; \(10^{-4}\) double adaptativo) em \(b\ge 20\,r_s\).

### B) Esfera de fótons / captura crítica
Raio da órbita nula circular (Schwarzschild): \(r_{\mathrm{ph}}=3M=\tfrac{3}{2}r_s\).  
Impacto crítico: \(b_{\mathrm{c}}=3\sqrt{3}\,M = \tfrac{3\sqrt{3}}{2} r_s\).  
**Refs:** Carroll §5.3; Chandrasekhar, *The Mathematical Theory of Black Holes* (Oxford, 1983), cap. 3; MTW Box 25.7.  
**Aceite:** raios com \(b<b_{\mathrm{c}}-\Delta\) capturados; \(b>b_{\mathrm{c}}+\Delta\) escapam; faixa \(\Delta\) documentada vs. passo numérico.

### C) Quantidades conservadas (geodésica nula)
Com Killing \(\partial_t\), \(\partial_\phi\) (Schwarzschild estático, \(c=G=1\)):
\[
E = f\,\dot{t},\quad L = r^2\sin^2\theta\,\dot{\phi},\quad
f=1-\frac{2M}{r},\qquad
g_{\mu\nu}\dot{x}^\mu\dot{x}^\nu = 0.
\]
**Refs:** Carroll §5.4; Wald §6.3; MTW §25.  
**Aceite:** \(\max|\Delta E|/E_0\), \(\max|\Delta L|/L_0\), \(\max|g_{\mu\nu}k^\mu k^\nu|\) abaixo de limiares vs. \(\lambda\) e vs. CPU double.

### D) Extra útil
- Geodésicas nulas radiais (sinal de \(\dot{r}\), sem deflexão azimutal).  
- Acordo CPU RK4 (`CPU-geodesic.cpp` evoluído) ↔ GPU científico, mesmas seeds.  
- Estados de câmera perto do polo \(\theta\approx 0,\pi\).

---

## 4) Onde fica um adaptador Blender opcional

```
[ C++ core físico ]  --export-->  params JSON / meshes / buffers
        ^                              |
        |                              v
   (única fonte da verdade)      [ Blender addon ]
                                      |
                                      v
                              visualização / composição
```

- **Física permanece em C++** (CPU ref + shaders científicos). Blender **não** reimplementa geodésicas.
- Exportar: \(r_s\), \(M\), limiares de disco, malha do disco, amostras de raios, LUTs de deflexão, metadados de modo (`legacy` vs `scientific`).
- Importar (opcional): pose de câmera / FOV → reenviar ao motor C++.
- Empacotar como addon separado; **não** misturar no target `BlackHole3D` default.
- Alinha a `SCIENTIFIC_MODE_SPEC.md` §“Future optional capabilities”.

---

## 5) O que **não** inventar

| Proibido | Motivo |
|---|---|
| Screenshots / frames “de validação” fabricados | Ambiente sem GLEW/OpenGL completo não validou pixels (`docs/VALIDATION_STATUS.md`). |
| Claims de Kerr “com acurácia medida” sem suite | Kerr é P2; sem testes A–C em Schwarzschild, qualquer % de erro é fiction. |
| Afirmar que o GPU legado é RK4 | É Euler; nome já corrigido — não reverter o claim. |
| Trocar constantes baseline (`SagA_rs`, `D_LAMBDA`, 60k, `ESCAPE_R`, disco 2.2–5.2) no path default | Quebra contrato visual (`docs/BASELINE.md` + `tests/validate_source_invariants.py`). |
| Promover `Gravity_Sim/` como produto | Legado CUDA, fora do CMake. |
| “GR completa” / plasma / EHT-ready | Fora do modelo; no máximo roadmap P2+ com refs e dados reais. |
| Números de erro, FPS ou AUC inventados | Só reportar o que `ctest`/máquina de desenvolvimento medir. |

---

## Ordem operacional sugerida (GLUE-LP)

1. Congelar baseline (invariantes + hashes).  
2. Scaffold `scientific/` (CPU double + shader novo + flag).  
3. Implementar P0 + testes A–C.  
4. Documentar limiares reais medidos.  
5. Só então Kerr / disco relativístico / Blender adapter.

**Gate de honestidade:** modo científico “completo” = P0 + testes A–C passando com tolerâncias publicadas; baseline legado bit-a-bit (ou frame-a-frame) intacto.
