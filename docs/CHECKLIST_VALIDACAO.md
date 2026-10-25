# Checklist de validação / Validation checklist

## Sempre (qualquer mudança)

- [ ] `python3 tests/validate_source_invariants.py --root .` → PASS
- [ ] Ou: `bash scripts/apply_baseline_check.sh`
- [ ] Não alterar física default de `geodesic.comp` / constantes protegidas (`docs/BASELINE.md`)
- [ ] `Gravity_Sim/` permanece no tree
- [ ] Sem timings GPU inventados, sem “Kerr implementado”, sem medições fictícias

## Regressão de hashes (opcional)

```bash
python3 scripts/hash_tree.py --help
python3 scripts/hash_tree.py --dry-run --root .
python3 scripts/hash_tree.py --root .
```

## Científico / CI headless (se presente)

```bash
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON
cmake --build build/scientific
ctest --test-dir build/scientific --output-on-failure
```

Esperado:

- [ ] `source_invariants` PASS
- [ ] `scientific_ref` PASS (photon sphere, 1/b, null drift RK4≤Euler, E/L) — se o target existir

Ou: `make test` (se `Makefile` presente).

## Com OpenGL (máquina com deps)

```bash
cmake -S . -B build/default -DBLACK_HOLE_BUILD_GL=ON
# ou: cmake --preset default / vcpkg-release / ci-linux
cmake --build build/default
cmake --build build/default --target validate_shaders   # se glslangValidator
```

- [ ] `BlackHole2D` e `BlackHole3D` linkam
- [ ] Shaders **raiz** (`geodesic.comp`, `grid.*`) ainda são os carregados em runtime pelo baseline

## Docs / packaging

- [ ] Links em `docs/INDEX.md` resolvem
- [ ] README “O que este projeto é / não é” continua honesto
- [ ] Sem `LICENSE` inventada — ver `LICENSE-NOTES.md`
