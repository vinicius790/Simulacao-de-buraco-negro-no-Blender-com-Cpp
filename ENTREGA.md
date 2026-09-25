# Entrega — black_hole 0.7.0-grand

Pacote expandido a partir do `.rar` original: simulação de buraco negro **C++/OpenGL** + frontend **Blender** (Steam), mantendo o estilo visual da animação (disco âmbar, grade cinza, órbita centrada).

## Como usar no Blender (Steam) — caminho rápido

1. Extraia este arquivo.
2. No Blender da Steam: `Edit` → `Preferences` → `Add-ons` → `Install from Disk`.
3. Escolha `blender/black_hole_bridge.zip`.
4. Ative **Black Hole Bridge**.
5. Viewport → tecla `N` → aba **Black Hole** → **Build Full Scene**.
6. **Bake Orbit Animation** para a órbita estilo C++.
7. Detalhes: `blender/INSTALAR_E_RODAR.md`.

## Como buildar o C++

Ver `docs/ROTEIRO_BUILD.md` e o `README.md`. Resumo:

```bash
cmake -S . -B build -DBLACK_HOLE_BUILD_GL=ON
cmake --build build
# ou só científico (sem OpenGL):
cmake -S . -B build/sci -DBLACK_HOLE_BUILD_GL=OFF && cmake --build build/sci && ctest --test-dir build/sci
```

Targets: `BlackHole2D`, `BlackHole3D` (GPU `geodesic.comp` legado), `bh_scientific`, `test_scientific_ref`.

## O que foi adicionado (além do baseline de engenharia)

- Biblioteca científica `bh_scientific` (Schwarzschild, RK4 real, conservação, fraco campo)
- Shader científico separado (não ligado por padrão)
- Addon Blender completo + ZIP instalável + guia Steam
- Docs densos (física, arquitetura, estilo visual, checklist, glossário, brief científico)
- Makefile, testes de invariantes + estilo + ciência
- I/O JSON de cena (`bh_scene_dump`, exemplos)

## O que NÃO mudou (proposital)

- Integrador Euler legado em `geodesic.comp` (baseline visual)
- Constantes protegidas (800×600, 200×150, 2.2–5.2 rs, SagA_rs, …)
- Sem Kerr inventado; sem números de validação GPU falsos

## Mapa

Comece por `docs/INDEX.md` e `STATUS.md`.
