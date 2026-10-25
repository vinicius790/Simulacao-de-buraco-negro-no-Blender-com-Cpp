# Roteiro de build

Instruções alinhadas ao `README.md` raiz, com notas extras para vcpkg, apt e Windows.

## Requisitos

1. Compilador C++17 ou mais recente (MSVC, GCC, Clang)
2. [CMake](https://cmake.org/) ≥ 3.21
3. [Git](https://git-scm.com/)
4. Dependências: **GLEW**, **GLFW3**, **GLM**, **OpenGL** (4.3+ recomendado para o compute shader)

## Opção A — vcpkg (recomendado / multiplataforma)

1. Clone:
   ```bash
   git clone https://github.com/kavan010/black_hole.git
   cd black_hole
   ```
2. Instale pacotes do manifesto:
   ```bash
   vcpkg install
   ```
   (`vcpkg.json` pede `glfw3`, `glm`, `glew`.)
3. Obtenha o toolchain:
   ```bash
   vcpkg integrate install
   ```
   Anote o caminho `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake`.
4. Configure e compile (manual):
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/caminho/para/vcpkg.cmake
   cmake --build build
   ```
5. Ou use o preset (se `VCPKG_ROOT` estiver definido):
   ```bash
   cmake --preset vcpkg-release
   cmake --build --preset vcpkg-release
   ```
6. Executáveis ficam sob `build/` (ou `build/vcpkg-release/`). Shaders (`*.vert`, `*.frag`, `*.comp`) são copiados para o diretório do `BlackHole3D` no POST_BUILD.

## Opção B — Debian/Ubuntu (apt)

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
  libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev
# opcional, para validate_shaders:
sudo apt install glslang-tools
```

Depois:

```bash
cmake --preset default
# ou CI-style:
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --test-dir build/ci-linux --output-on-failure
```

## Notas Windows

- O autor original reportou execução em Windows com GPU; OpenGL 4.3+ (ou driver compatível) é necessário para `geodesic.comp`.
- Com **Visual Studio** + vcpkg:
  - Instale workload “Desktop development with C++”.
  - Configure com o toolchain file do vcpkg (Preset `vcpkg-release` ou `-DCMAKE_TOOLCHAIN_FILE=...`).
  - Abra o diretório no VS ou gere com `-G "Visual Studio 17 2022"`.
- Binários históricos em `Gravity_Sim/bin/` (`*.exe`, `glew32.dll`, `glfw3.dll`) **não** são o caminho de build mantido; sirvem só de referência legado.
- Configurações auxiliares existem em `vs_code/` e `Gravity_Sim/.vscode/` (paths antigos — ajuste se reutilizar).
- Se o executável não achar shaders, rode a partir do diretório de saída do CMake (onde o POST_BUILD copia os arquivos) ou copie `geodesic.comp`, `grid.vert`, `grid.frag` para junto do `.exe`.

## Presets disponíveis (`CMakePresets.json`)

| Preset | Uso |
|--------|-----|
| `default` | Release + `BUILD_TESTING=ON` |
| `vcpkg-release` | Herda default + toolchain `$env{VCPKG_ROOT}` |
| `debug` | Debug |
| `ci-linux` | Ninja, usado no GitHub Actions |

Opção CMake útil: `-DBLACK_HOLE_WARNINGS_AS_ERRORS=ON`.

## Testes estáticos (sem GPU)

```bash
python3 tests/validate_source_invariants.py --root .
bash scripts/apply_baseline_check.sh
python3 scripts/hash_tree.py --root .
```

## O que não instalar “por padrão”

Blender, CUDA toolkit, Vulkan SDK e runtimes de agentes **não** são dependências do build default. Ver `BLENDER_ADAPTER.md` e `SCIENTIFIC_MODE_SPEC.md`.
