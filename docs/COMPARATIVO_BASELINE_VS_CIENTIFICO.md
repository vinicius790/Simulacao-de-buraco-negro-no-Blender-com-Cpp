# Comparativo: baseline legado vs. modo científico

O núcleo científico (`include/black_hole/`, `src/scientific/`, `bh_scientific`) **está presente** neste tree como caminho **separado**. Esta tabela contraste o baseline default com esse modo / com `SCIENTIFIC_MODE_SPEC.md`. Não use a existência da lib para reivindicar Kerr ou GRMHD.

| Aspecto | Baseline (default) | Modo científico (spec / se presente) |
|---------|--------------------|--------------------------------------|
| Entrada principal | `black_hole.cpp` + `geodesic.comp` | `include/black_hole/` + `src/scientific/` + `shaders/geodesic_scientific.comp` |
| Integrador | `legacyEulerStep` (1 avaliação RHS; Euler explícito) | RK4 ou RK embutido adaptativo; **não** reutiliza o nome/caminho legado |
| Precisão típica | `float` no compute shader | Referência CPU em `double`; GPU corrigida documentada |
| Resolução janela | 800×600 | Pode subir; não pode mudar o default sem decisão explícita |
| Compute image | 200×150 | Independente; regressão pixel do legado não é o critério |
| `SagA_rs` | `1.269e10` (constante histórica) | Unidades geométricas / mapeamento SI documentado |
| Passo `D_LAMBDA` | `1e7` fixo | Tolerância/erro explícitos se adaptativo |
| Orçamento de passos | 60 000 | Critério de escape/captura derivado, não `1e30` “inalcançável” |
| Disco | `2.2 r_s` … `5.2 r_s` (visual) | Hit-test contínuo segmento–plano; emissão relativística só depois |
| Objetivo | Visual realtime inspirado em Sgr A* | Trajetórias e conservação auditáveis |
| Validação | `validate_source_invariants.py` | Gates: nulos radiais, deflexão fraca, captura crítica, drift de vínculo/ constantes, CPU↔GPU |
| Kerr / spin | Fora de escopo | Opcional **depois** de Schwarzschild validado — sem claims prematuros |
| Blender | Não no default | Adaptador frontend opcional (`BLENDER_ADAPTER.md`) |
| GRMHD | Não | Não — este projeto não é código GRMHD completo |

## Regra de ouro

Mudanças que alterem trajetórias ou pixels do caminho default **não** entram silenciosamente no baseline. Ou:

1. ficam atrás de um modo/flag/shader separado, ou
2. são classificadas como CORREÇÃO de API OpenGL/layout (ex.: barreiras, std140) sem mudar a aritmética da geodésica.

Ver também: `BASELINE.md`, `ENGINEERING_UPGRADE.md`, `VALIDATION_STATUS.md`.
