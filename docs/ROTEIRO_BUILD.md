# Roteiro de build (0.8.0)

Instruções alinhadas ao `README.md` raiz, com notas para apt, vcpkg, testes GPU
headless (Mesa llvmpipe + Xvfb), Blender e Windows.

## Requisitos

| Componente | Obrigatório para | Observação |
|------------|------------------|------------|
| Compilador C++17 (GCC, Clang, MSVC) | tudo | O build é limpo com `-Wall -Wextra -Wpedantic` (0 avisos) |
| [CMake](https://cmake.org/) ≥ 3.21 | tudo | Presets em `CMakePresets.json` |
| Python 3 | testes CTest em Python, `tools/*.py` | Só biblioteca padrão |
| GLEW, GLFW3, GLM, OpenGL 4.3+ | `BlackHole2D` / `BlackHole3D` | Dispensáveis com `-DBLACK_HOLE_BUILD_GL=OFF` |
| `glslangValidator` | alvo `validate_shaders` | Opcional |
| Mesa (llvmpipe) + Xvfb | `gpu_cpu_agreement` e `--capture` sem monitor | Opcional; sem eles o teste devolve SKIP (77) |
| Blender 4.x | addon / `make blender-scene` | Opcional; nunca é dependência do simulador |

## Opção A — Debian/Ubuntu (apt)

```bash
sudo apt update
# Núcleo + OpenGL
sudo apt install build-essential cmake ninja-build python3 \
  libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev

# Opcional: validação GLSL
sudo apt install glslang-tools

# Opcional: GPU headless (Mesa llvmpipe + X virtual) para gpu_cpu_agreement / --capture
sudo apt install libgl1-mesa-dri mesa-utils xvfb xauth

# Opcional: Blender (o pacote do Ubuntu 24.04 é o 4.0.x, mínimo do addon)
sudo apt install --no-install-recommends blender
```

São os mesmos pacotes dos jobs `linux-build` e `blender-headless` do CI.
Confira o contexto OpenGL do llvmpipe:

```bash
xvfb-run -a glxinfo -B | grep -E "renderer|core profile version"
# esperado (container de referência): llvmpipe (LLVM …), 4.5 (Core Profile) Mesa 25.2.8
```

Depois, com o `Makefile`:

```bash
make build        # árvore científica (sem OpenGL) → build/scientific
make test         # CTest headless
make build-gl     # árvore completa → build/gl
make test-gl      # CTest completo, inclui gpu_cpu_agreement (usa xvfb-run se não houver display)
```

Ou com CMake puro:

```bash
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --test-dir build/ci-linux --output-on-failure
```

## Opção B — vcpkg (multiplataforma)

1. Clone o repositório (upstream original: <https://github.com/kavan010/black_hole>;
   use a URL do repositório que contém esta versão 0.8.0).
2. Instale os pacotes do manifesto (`vcpkg.json` pede `glfw3`, `glm`, `glew`):
   ```bash
   vcpkg install
   vcpkg integrate install   # anote -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake
   ```
3. Configure e compile:
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/caminho/para/vcpkg.cmake
   cmake --build build
   ```
   ou, com `VCPKG_ROOT` definido:
   ```bash
   cmake --preset vcpkg-release
   cmake --build --preset vcpkg-release
   ```
4. O `POST_BUILD` copia `geodesic.comp`, `grid.vert`, `grid.frag` e
   `shaders/geodesic_scientific.comp` para o diretório do `BlackHole3D`.

## Build só científico (sem OpenGL)

Compila `bh_scientific`, `bh_render_cpu`, `bh_scene_dump`, `quickstart_scientific`
e os testes — sem GLEW/GLFW/GLM:

```bash
make configure && make build && make test
# equivalente:
cmake --preset scientific
cmake --build --preset scientific
ctest --preset scientific
```

## Atalhos do Makefile

O `Makefile` da raiz é versionado (exceção no `.gitignore`) e é **opcional**: o
build canônico é CMake. Usa Ninja se estiver instalado (`GENERATOR`), `Release`
por padrão (`BUILD_TYPE`).

| Alvo | O que faz |
|------|-----------|
| `make configure` | Configura `build/scientific` com `BLACK_HOLE_BUILD_GL=OFF`, `BUILD_TESTING=ON` |
| `make build` | `configure` + compila a árvore científica (padrão de `make`) |
| `make test` | `build` + `ctest --test-dir build/scientific` (alias `make test-scientific`) |
| `make configure-gl` | Configura `build/gl` com OpenGL |
| `make build-gl` | Compila `BlackHole2D`/`BlackHole3D` + tudo |
| `make test-gl` | CTest completo em `build/gl`, inclusive `gpu_cpu_agreement` |
| `make test-all` | `test` + `test-gl` |
| `make shaders` | `validate_shaders` (glslangValidator nos 4 shaders) na árvore GL |
| `make render` | `bh_render_cpu` → `build/renders/legacy_el125.png`, `showcase_relativistic.png`, `showcase_blackbody.png` |
| `make package-addon` | `blender/scripts/package_addon.py`: zips determinísticos do addon |
| `make blender-scene` | `blender --background … build_scene_headless.py` → `build/black_hole_sim.blend` |
| `make style-check` | Só contratos estáticos: invariantes, estilo, shaders |
| `make clean` | Remove `build/` |
| `make help` | Mostra o cabeçalho com a lista de alvos |

## Presets (`CMakePresets.json`)

| Preset | Diretório | Uso |
|--------|-----------|-----|
| `default` | `build/default` | Release, GL ON, `BUILD_TESTING=ON` |
| `scientific` | `build/scientific` | Release, GL OFF (também tem build/test preset) |
| `vcpkg-release` | `build/vcpkg-release` | Herda `default` + toolchain `$env{VCPKG_ROOT}` |
| `debug` | `build/debug` | Debug, GL ON |
| `ci-linux` | `build/ci-linux` | Ninja, GL ON — job `linux-build` |
| `ci-scientific` | `build/ci-scientific` | Ninja, GL OFF — job `scientific-headless` |

Opções CMake: `-DBLACK_HOLE_BUILD_GL=ON|OFF`, `-DBLACK_HOLE_WARNINGS_AS_ERRORS=ON`
(`-Werror` / `/WX`).

## Rodando sem monitor (GPU headless)

```bash
cd build/gl
xvfb-run -a ./BlackHole3D --capture legacy_default.png                 # baseline, 1 frame, sai
xvfb-run -a ./BlackHole3D --scene ../../examples/scene_relativistic_showcase.json \
         --relativistic --capture showcase_relativistic.png
```

`--capture` grava a textura de compute 200×150 (linha 0 = topo) e sai. Em Mesa
llvmpipe tudo roda em CPU; nenhum número de desempenho de GPU real é afirmado
neste repositório.

## Notas Windows

- OpenGL 4.3+ (driver de GPU) é necessário para `geodesic.comp` e para o modo
  científico. A validação 0.8.0 registrada foi feita **só em Linux** (Mesa
  llvmpipe). O CI tem um job `scientific-portable` (Windows/MSVC e macOS/Apple
  Clang, árvore científica sem OpenGL), mas nenhum resultado dele está registrado
  nesta documentação; o `BlackHole3D` em Windows não foi testado.
- Com **Visual Studio** + vcpkg: workload "Desktop development with C++",
  toolchain do vcpkg (preset `vcpkg-release` ou `-DCMAKE_TOOLCHAIN_FILE=...`),
  gerador `-G "Visual Studio 17 2022"` se preferir.
- `bh_render_cpu` e a árvore científica não dependem de OpenGL e usam só a STL
  (`std::thread`); o addon procura `bh_render_cpu.exe` também.
- Binários históricos em `Gravity_Sim/bin/` não são o caminho de build mantido.
- Configurações auxiliares em `vs_code/` e `Gravity_Sim/.vscode/` têm caminhos
  antigos; ajuste se reutilizar.
- Se o executável não achar os shaders, rode a partir do diretório de saída do
  CMake (onde o `POST_BUILD` os copia).

## Verificações estáticas (sem GPU, sem build)

```bash
make style-check
# ou, individualmente:
python3 tests/validate_source_invariants.py --root .
python3 tests/test_style_contract.py --root .
python3 tests/test_shader_contract.py --root .
bash scripts/apply_baseline_check.sh
python3 scripts/hash_tree.py --root .
```

## O que não instalar "por padrão"

Blender, CUDA, Vulkan SDK e runtimes de agentes **não** são dependências do build
default. Ver [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md), [SCIENTIFIC_MODE_SPEC.md](SCIENTIFIC_MODE_SPEC.md)
e [CHECKLIST_VALIDACAO.md](CHECKLIST_VALIDACAO.md).
