# Estrutura do repositório / Repository structure

Árvore após upgrade de engenharia + núcleo científico (quando presente). Binários legados podem existir; o CMake raiz não compila `Gravity_Sim/`.

```text
black_hole/
├── README.md / ENTREGA.md / CHANGELOG.md
├── CITATION.cff / LICENSE-NOTES.md
├── BASELINE_SHA256.txt
├── CMakeLists.txt          # GL opcional: BLACK_HOLE_BUILD_GL
├── CMakePresets.json       # default, vcpkg-release, debug, ci-linux
├── Makefile                # atalhos test / configure (se presente)
├── vcpkg.json
├── .editorconfig / .clang-format / .clang-tidy / .gitignore
├── .github/workflows/ci.yml
│
├── black_hole.cpp          # Baseline 3D host (OpenGL + UBOs)
├── geodesic.comp           # Baseline GPU — legacyEulerStep (invariantes)
├── grid.vert / grid.frag
├── 2D_lensing.cpp          # BlackHole2D
├── CPU-geodesic.cpp        # Protótipo CPU RK4 (não é o default GPU)
├── ray_tracing.cpp         # Experimento histórico
│
├── include/black_hole/     # API científica (headers)
├── src/scientific/         # Implementação científica CPU
├── shaders/                # geodesic_scientific.comp (separado do legado)
├── examples/               # quickstart_scientific.*
│
├── docs/                   # Este índice: INDEX.md
├── tests/
│   ├── validate_source_invariants.py
│   └── test_scientific_ref.cpp
├── scripts/
│   ├── hash_tree.py
│   └── apply_baseline_check.sh
├── blender/                # Bridge opcional Blender 4.x (frontend only)
├── vs_code/
└── Gravity_Sim/            # Snapshot LEGACY — inventário em README_LEGACY.md
```

## Targets CMake (típicos)

| Target | GL? | Descrição |
|--------|-----|-----------|
| `BlackHole2D` / `BlackHole3D` | Sim (`BLACK_HOLE_BUILD_GL=ON`) | Visualização baseline |
| `bh_scientific` | Não | Biblioteca/objetos científicos |
| `test_scientific_ref` | Não | Asserts headless |
| `quickstart_scientific` | Não | Demo de impressão |
| `validate_shaders` | — | Opcional (`glslangValidator`) |
| CTest `source_invariants` | Não | Guarda estática do baseline |

## O que não é o build default

- `Gravity_Sim/**` — protótipos/binários históricos
- Blender como dependência de runtime do simulador
- Substituição silenciosa de `geodesic.comp` pelo shader científico

Ver `docs/INDEX.md`, `docs/BLENDER_ADAPTER.md`, `docs/COMPARATIVO_BASELINE_VS_CIENTIFICO.md`.
