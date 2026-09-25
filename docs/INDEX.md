# Índice da documentação / Package map

Mapa do repositório após o upgrade de engenharia **e** a expansão científica (quando presente). Preferência PT para docs de usuário.

## Começar aqui

| Documento | Descrição |
|-----------|-----------|
| [../README.md](../README.md) | Visão geral + secção **O que este projeto é / não é** (PT) |
| [../ENTREGA.md](../ENTREGA.md) | Resumo da mega-atualização (docs/organização/CI) |
| [ROTEIRO_BUILD.md](ROTEIRO_BUILD.md) | vcpkg + apt + notas Windows |
| [CHECKLIST_VALIDACAO.md](CHECKLIST_VALIDACAO.md) | Validação antes de aceitar mudanças |
| [MODO_CIENTIFICO.md](MODO_CIENTIFICO.md) | Uso da lib científica `bh_scientific` (se presente) |

## Contrato do baseline e ciência

| Documento | Descrição |
|-----------|-----------|
| [BASELINE.md](BASELINE.md) | Contrato legado (hashes, constantes travadas) |
| [ENGINEERING_UPGRADE.md](ENGINEERING_UPGRADE.md) | Correções/otimizações sem mudar o modelo numérico default |
| [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md) | Spec do modo científico (separado do default) |
| [COMPARATIVO_BASELINE_VS_CIENTIFICO.md](COMPARATIVO_BASELINE_VS_CIENTIFICO.md) | Tabela baseline vs. científico |
| [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md) | O que a física modela / não modela (se presente) |
| [BRIEF_REVISAO_CIENTIFICA.md](BRIEF_REVISAO_CIENTIFICA.md) | Brief de revisão científica (se presente) |
| [ARQUITETURA.md](ARQUITETURA.md) | Pipeline do engine (se presente) |
| [ESTILO_VISUAL_SIMULACAO.md](ESTILO_VISUAL_SIMULACAO.md) | Bíblia do estilo visual (Blender deve casar) |
| [ANIMACAO_PIPELINE.md](ANIMACAO_PIPELINE.md) | Frame loop OpenGL vs bake Blender |
| [VALIDATION_STATUS.md](VALIDATION_STATUS.md) | O que foi validado neste ambiente |
| [SKILLS_WORKFLOW.md](SKILLS_WORKFLOW.md) | Princípios de fluxo usados no upgrade |

## Organização e referência

| Documento | Descrição |
|-----------|-----------|
| [ESTRUTURA_REPOSITORIO.md](ESTRUTURA_REPOSITORIO.md) | Árvore e papéis dos targets |
| [GLOSSARIO.md](GLOSSARIO.md) | rs, photon sphere, ISCO, nested geodesics, UBO, compute shader, lensing |
| [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md) + [`../blender/`](../blender/) | Blender = frontend opcional; física no C++ |
| [../Gravity_Sim/README_LEGACY.md](../Gravity_Sim/README_LEGACY.md) | Inventário file-by-file de `Gravity_Sim/src/` |

## Núcleo científico (presente neste tree)

| Caminho | Papel |
|---------|--------|
| [`../include/black_hole/`](../include/black_hole/) | Headers da API científica |
| [`../src/scientific/`](../src/scientific/) | Schwarzschild / RK4 / conserved / ray_state |
| [`../shaders/geodesic_scientific.comp`](../shaders/geodesic_scientific.comp) | Shader **separado** de `geodesic.comp` |
| [`../tests/test_scientific_ref.cpp`](../tests/test_scientific_ref.cpp) | Testes headless (photon sphere, drift, …) |
| [`../examples/`](../examples/) | Quickstarts científicos |
| CMake `bh_scientific` / `BLACK_HOLE_BUILD_GL` | Lib sem GL; GL continua opcional |

O **default** de visualização continua `black_hole.cpp` + `geodesic.comp` (`legacyEulerStep`). Nada científico substitui esse caminho em silêncio.

## Scripts e regressão

| Artefato | Descrição |
|----------|-----------|
| [`../tests/validate_source_invariants.py`](../tests/validate_source_invariants.py) | Guarda estática do baseline |
| [`../scripts/apply_baseline_check.sh`](../scripts/apply_baseline_check.sh) | Wrapper do teste de invariantes |
| [`../scripts/hash_tree.py`](../scripts/hash_tree.py) | SHA-256 de fontes-chave |
| [`../BASELINE_SHA256.txt`](../BASELINE_SHA256.txt) | Hashes do arquivo original |
| [`../Makefile`](../Makefile) | Atalhos `test`, `test-scientific`, … (se presente) |

## Licença e citação

| Artefato | Descrição |
|----------|-----------|
| [`../LICENSE-NOTES.md`](../LICENSE-NOTES.md) | Origem upstream — **não** inventa LICENSE |
| [`../CITATION.cff`](../CITATION.cff) | Citação com autores placeholder |
| Upstream | https://github.com/kavan010/black_hole |

## EN (short)

Default render path remains the **historical Euler** baseline in `geodesic.comp`. `bh_scientific` is a separate double-precision reference. Kerr is **not** claimed. Blender is optional visualization only. Not a GRMHD code.
