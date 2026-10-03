# Notas de licença / origem

Este pacote **não inventa** um ficheiro `LICENSE`.

## Upstream

O código original e o histórico do projeto vêm de:

**https://github.com/kavan010/black_hole**

Consulte esse repositório (e quaisquer declarações do autor) para o estado real da licença. Se o upstream publicar um `LICENSE` claro, copie-o para a raiz deste tree em vez de improvisar termos.

## Este pacote de engenharia

As adições de documentação, testes estáticos, presets CMake, CI e scripts de regressão neste tree destinam-se a preservar e documentar o baseline. Elas **não** constituem por si só uma concessão de licença nova.

## Estado verificado (2026-10-03)

| Parte | Licença declarada | Fonte |
|---|---|---|
| Código C++/GLSL de origem upstream (`black_hole.cpp`, `geodesic.comp`, `2D_lensing.cpp`, `ray_tracing.cpp`, `CPU-geodesic.cpp`, `grid.*`) | **nenhuma** | Upstream `kavan010/black_hole` no commit `dc263bb` (2026-04-07): sem ficheiro `LICENSE`/`COPYING` e sem menção de licença no `README.md`. |
| Addon Blender (`blender/addons/black_hole_bridge/`) | **MIT** (`license = ["SPDX:MIT"]` em `blender_manifest.toml`) | Declarado pelo dono deste repositório no commit `0288612`. O campo é obrigatório no manifesto de extensões do Blender 4.2+. |
| Restante deste pacote (biblioteca científica, renderer CPU, ferramentas, testes, docs) | nenhuma | Nenhuma declaração dos detentores dos direitos. |

Como ler a tabela:

- A declaração MIT do manifesto cobre **apenas** o código Python do addon. Esse código é original deste repositório e não contém código upstream; os valores numéricos que espelha (raio de Schwarzschild, cores, raios do disco) são parâmetros, não código. Ela **não** se estende ao código C++ upstream nem ao resto do pacote.
- O texto MIT exige um aviso de copyright com o nome do titular. Este pacote não o escreve por conta própria: para completar a declaração, o dono do repositório deve adicionar `blender/addons/black_hole_bridge/LICENSE` com o texto MIT e o seu próprio aviso de copyright, e então regenerar os zips (`make package-addon`).
- Para trocar ou retirar a licença do addon, basta editar o campo `license` do manifesto (o Blender só aceita identificadores SPDX) e regenerar os zips.

## O que fazer se precisar de redistribuir

1. Verificar o GitHub upstream e issues/PRs sobre licensing.
2. Contactar o autor original se a licença estiver omissa.
3. Só então adicionar um `LICENSE` na raiz com o texto legal correto — nunca um placeholder “MIT”/“Apache” inventado.

Ficheiros relacionados: `CITATION.cff` (autores placeholder), `BASELINE_SHA256.txt`, `docs/BASELINE.md`.
