# Engineering Upgrade Notes / Notas de engenharia

## Scope

This upgrade intentionally improves engineering quality around the existing simulation without replacing the historical numerical model. The default render path remains the legacy baseline.

## 0.8.0 — itens de engenharia (PT)

Nenhum item abaixo altera a aritmética do baseline: `geodesic.comp` da raiz está
inalterado e `BlackHole3D` sem flags continua sendo o caminho histórico. A
classificação segue a mesma convenção das seções seguintes.

| # | Item | Classificação |
|---|------|---------------|
| 10 | Build sem avisos | CORREÇÃO |
| 11 | `include(CTest)` no topo do `CMakeLists.txt` | CORREÇÃO |
| 12 | `Threads` na biblioteca científica | CORREÇÃO / MODERNIZAÇÃO |
| 13 | `--capture`: captura determinística de um frame | INFRAESTRUTURA DE REGRESSÃO |
| 14 | `Engine` construído depois da CLI | REESTRUTURAÇÃO |
| 15 | Shader científico copiado e validado | INFRAESTRUTURA |
| 16 | RHS do RK4 sem NaN dentro do horizonte | CORREÇÃO (só caminho científico) |
| 17 | `Makefile` versionado e CI ampliada | MODERNIZAÇÃO |
| 18 | CLI do `bh_render_cpu` com parsing estrito | CORREÇÃO |

### 10. Build sem avisos

Com `-Wall -Wextra -Wpedantic` (GCC 13.3), o build tinha 4 avisos
`unused-parameter` nos callbacks de câmera de `black_hole.cpp`
(`processMouseButton`, `processScroll`, `processKey`). Os parâmetros não usados
ficaram sem nome (`/*xoffset*/`, …). Resultado: **0 avisos** nas duas árvores.
`-DBLACK_HOLE_WARNINGS_AS_ERRORS=ON` transforma qualquer aviso futuro em erro.

### 11. Ordem do CTest

`include(CTest)` estava depois do bloco OpenGL. O teste `gpu_cpu_agreement` é
registrado **dentro** desse bloco (precisa do caminho do `BlackHole3D`), e
`add_test()` antes de `enable_testing()` é ignorado em silêncio (`BUILD_TESTING`
ainda nem existia nesse ponto). `include(CTest)` agora é a segunda instrução
depois de `project()`, então todo `add_test()` é registrado.

### 12. `Threads`

O renderer CPU usa `std::thread`. `find_package(Threads REQUIRED)` +
`target_link_libraries(bh_scientific PUBLIC Threads::Threads)` garantem o link
correto (`-pthread`) em toolchains que não o adicionam sozinhas; todo consumidor
de `bh_scientific` herda a dependência.

### 13. `--capture`

`BlackHole3D --capture out.{png,bmp,ppm}` renderiza um frame, faz `glFinish`, lê a
textura de compute 200×150 com `glGetTexImage`, vira as linhas para a orientação
da tela (linha 0 = topo; o quad fullscreen mapeia a linha 0 da textura para baixo),
compõe RGB·alpha sobre o preto do clear e grava com os escritores sem
dependências de `image_io.hpp`. Funciona com o baseline e com os modos
científicos, inclusive sem monitor (`xvfb-run` + Mesa llvmpipe). É a base do teste
`gpu_cpu_agreement` e foi o que permitiu diagnosticar o artefato da vista inicial
([BASELINE.md](BASELINE.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)).
Não há números de desempenho de GPU associados (llvmpipe é software).

### 14. `Engine` depois da CLI

O `Engine` (janela, programas, UBOs) era um objeto global construído antes de
`main()`. Agora é construído em `main()` **depois** de `parseArgs`, para que o
programa de compute (`geodesic.comp` ou `geodesic_scientific.comp`) e o UBO
`SciParams` (binding 4) correspondam às flags. Sem flags, a sequência de
inicialização e os valores enviados são os do baseline.

### 15. Shader científico no build

`shaders/geodesic_scientific.comp` é copiado para junto do `BlackHole3D` no
`POST_BUILD` e validado pelo alvo `validate_shaders` (glslangValidator) com os três
shaders do baseline. `tests/test_shader_contract.py` confere bindings e o layout
de `SciParams` (32 bytes, `static_assert` no C++).

### 16. RK4 sem NaN no horizonte

`geodesic_rhs` (`src/scientific/integrator_rk4.cpp`) congela a aceleração quando
`f = 1 − rs/r ≤ 0`. Estágios intermediários de RK4 que passavam do horizonte
produziam NaN; o raio já é tratado como capturado nesse caso. Afeta só a
biblioteca científica, não o shader legado.

### 17. `Makefile` e CI

- O `Makefile` da raiz era ignorado pelo `.gitignore` (regra genérica para saída
  do CMake), embora a documentação citasse `make test`. Agora é versionado (exceção
  `!/Makefile`) e oferece `configure`, `build`, `test`, `configure-gl`, `build-gl`,
  `test-gl`, `test-all`, `shaders`, `render`, `package-addon`, `blender-scene`,
  `style-check`, `clean` ([ROTEIRO_BUILD.md](ROTEIRO_BUILD.md#atalhos-do-makefile)).
- `.github/workflows/ci.yml` tem os jobs `scientific-headless` (CTest sem GL, zips
  do addon em dia, renders CPU como artefato), `scientific-portable` (árvore
  científica em Windows/MSVC e macOS/Apple Clang), `linux-build` (Mesa llvmpipe + Xvfb
  dão OpenGL 4.5, então `gpu_cpu_agreement` roda de fato; `validate_shaders`;
  capturas GPU como artefato) e `blender-headless` (Blender 4.0.x do apt gera o
  `.blend`).
- `blender/scripts/package_addon.py` gera zips determinísticos; o CI falha se o
  zip versionado estiver desatualizado.

### 18. Parsing estrito no `bh_render_cpu`

Inteiros via `strtol` com faixa (sem conversão `double → int` indefinida), reais
via `strtod` exigindo string inteira consumida e valor finito, faixas por flag
(ex.: `--supersample` 1–16, `--fov-y-deg` em (0, 180)), código 2 com mensagem que
nomeia a flag, e a linha JSON com `out` escapado. Coberto por
`tests/test_render_cli.py` (CTest `render_cli_contract`). Ver
[RENDER_CPU.md](RENDER_CPU.md).

## Implemented changes (0.6.x engineering upgrade)

### 1. Correct OpenGL memory visibility

The compute shader writes the output texture with `imageStore`, then the fullscreen pass reads the same texture through `texture()`. The barrier now includes both `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT` and `GL_TEXTURE_FETCH_BARRIER_BIT` so the following texture fetch is synchronized with prior shader image writes.

Classification: **CORRECTION**.

### 2. Explicit std140 layouts

CPU-side Camera, Disk and Objects UBO layouts are now centralized and size-checked. The shader `moving` flag uses an integer rather than relying on C++ `bool` representation, and the object mass array uses a `vec4` stride to match std140 array rules.

Classification: **CORRECTION / RESTRUCTURING**.

### 3. Persistent compute texture allocation

The 200x150 RGBA8 compute texture is no longer reallocated every frame. It is recreated only if the compute resolution actually changes.

Classification: **OPTIMIZATION**.

### 4. Static UBO upload avoidance

Disk data and object data are uploaded only when dirty. The camera UBO remains per-frame because it depends on camera state.

Classification: **OPTIMIZATION**.

### 5. Grid topology reuse

The grid index topology and VAO/EBO setup are created once. Grid vertex data is regenerated only when legacy gravity actually moves scene objects.

Classification: **OPTIMIZATION / RESTRUCTURING**.

### 6. Uniform-location caching

`viewProj` and `screenTexture` uniform locations are queried once after program creation instead of every frame.

Classification: **OPTIMIZATION**.

### 7. Equatorial-plane early rejection

The expensive radial length computation for disk intersection is skipped unless a segment first crosses the equatorial plane. The predicate itself is unchanged.

Classification: **OPTIMIZATION**.

### 8. Honest integrator naming

The historical `rk4Step` function performs one explicit Euler stage. It is renamed to `legacyEulerStep` without changing the numerical operations. This prevents future maintenance from treating the baseline as a real fourth-order Runge-Kutta implementation.

Classification: **RESTRUCTURING / PRECISION DOCUMENTATION**.

### 9. Tooling and regression infrastructure

Added:

- `CMakePresets.json`;
- compiler warning configuration;
- optional `validate_shaders` CMake target;
- CTest integration for static source invariants;
- `.clang-format`;
- `.clang-tidy`;
- GitHub Actions Linux build/static/GLSL validation workflow.

Classification: **MODERNIZATION / RESTRUCTURING**.

## Deliberately not changed

These are known issues or scientific limitations, but changing them in the default path would change the observable baseline:

- the single-stage Euler integrator;
- the 3D geodesic equations currently implemented;
- the initial null-geodesic energy expression;
- the historical angular-momentum expression;
- the 60,000-step budget;
- the `1e30` escape radius;
- the fullscreen quad topology;
- blending state inherited by the fullscreen composition pass;
- the frame-rate-dependent legacy object-gravity update.

They should be addressed in an optional scientific/corrected mode after deterministic regression capture exists.

## Skill/workflow principles used

The upgrade follows the useful parts of the referenced development-skill workflows:

- inspect the repository before choosing architecture;
- separate bug/correctness review from feature work;
- create regression guards before risky refactors;
- keep changes reviewable and localized;
- validate with independent tools where possible;
- do not add technology merely because it is newer;
- document known limitations instead of hiding them behind more abstraction.

Agent-SDK-specific scaffolding and OpenAI API integration were intentionally not added because this is a C++/OpenGL numerical-rendering project, not an agent application.
