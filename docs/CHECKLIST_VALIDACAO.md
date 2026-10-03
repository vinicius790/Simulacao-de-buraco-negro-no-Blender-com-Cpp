# Checklist de validação / Validation checklist (0.8.0)

Use antes de aceitar qualquer mudança. Os portões estão em ordem de custo: os
estáticos rodam em segundos e sem build; os de GPU precisam de OpenGL (real ou
Mesa llvmpipe).

## 1. Sempre (qualquer mudança)

```bash
make style-check
# = validate_source_invariants.py + test_style_contract.py + test_shader_contract.py
```

- [ ] `source_invariants` PASS — `geodesic.comp` da raiz inalterado (`legacyEulerStep`
      com 1 avaliação de RHS, `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 passos,
      `ESCAPE_R = 1e30`), 800×600 / 200×150, disco 2,2–5,2 rs, std140, barreira
- [ ] `style_contract` PASS — cor do disco `vec3(1, r_norm, 0.2)` com alpha `r_norm`,
      grade `vec4(0.5, 0.5, 0.5, 0.7)`, câmera 6.34194e10 m / az 0 / el π/2 / FOV 60°
- [ ] `shader_contract` PASS — espelhos `shaders/geodesic.comp`, `grid.*` idênticos à
      raiz; todo binding usado pelo host existe no shader; `SciParams` (binding 4,
      32 bytes) igual campo a campo entre GLSL e `SciParamsUBOData`; shader legado
      sem código científico; marcadores de honestidade (Kerr não implementado) e
      termo corrigido `r·f` no shader científico
- [ ] `BlackHole3D` **sem flags** continua sendo o baseline histórico
- [ ] `Gravity_Sim/` permanece no tree
- [ ] Sem timings de GPU inventados (só Mesa llvmpipe foi medido), sem "Kerr
      implementado", sem medições fictícias

## 2. Científico / headless (sem OpenGL)

```bash
make test
# ou
cmake --preset scientific && cmake --build --preset scientific && ctest --preset scientific
```

| Teste | Esperado |
|-------|----------|
| [ ] `scientific_ref` | PASS: esfera de fótons, ISCO, órbitas, redshift, Page–Thorne forma fechada vs quadratura (≤ 2e-3), colisões contínuas, captura/escape em b = 2,0 / 2,5 / 2,7 / 3,2 rs, deflexão fraca numérica a ≤ 1 % da 2.ª ordem, RK45 vs RK4, plano orbital vs carta 3-D, polos, limites Kerr analíticos |
| [ ] `cpu_render` | PASS: paleta âmbar, simetria, determinismo entre threads, Doppler + inversão do spin, corpo negro, câmera no polo, área da sombra vs b_c (≤ 8 %), borda da sombra vs Synge, PNG/CRC/Adler |
| [ ] `render_cli_smoke` | PNG gerado a partir de `examples/scene_params_example.json` |
| [ ] `render_cli_contract` | toda flag do `bh_render_cpu` aceita e com efeito documentado; valores inválidos → código 2; JSON válido; sequências e pontas das varreduras exatas; `--help` completo |
| [ ] `render_cli_rejects_bad_spin` | `--spin-sign 0.5` recusado (`WILL_FAIL`) |
| [ ] `render_cli_relativistic_sequence` | `smoke_rel_0000.bmp`, `smoke_rel_0001.bmp` |
| [ ] `blender_addon_compile` | todos os módulos/scripts do addon compilam sem `bpy` |
| [ ] `blender_addon_parity` | constantes e fórmulas do addon = headers C++; `cpp_to_blender`; JSON ida e volta; contrato da CLI `bh_render_cpu`; sequências; presets/easing; zips determinísticos |
| [ ] `tools_selftest` | `image_diff` (filtros PNG 0–4, PPM, BMP) e `frames_to_gif` (LZW, GIF89a) |

## 3. Com OpenGL (máquina com deps, ou Mesa llvmpipe + Xvfb)

```bash
make test-gl          # configura/compila build/gl e roda o CTest completo
make shaders          # validate_shaders
```

- [ ] Build com **0 avisos** (`-Wall -Wextra -Wpedantic`); opcionalmente
      `-DBLACK_HOLE_WARNINGS_AS_ERRORS=ON`
- [ ] `BlackHole2D` e `BlackHole3D` linkam
- [ ] `validate_shaders` PASS — `grid.vert`, `grid.frag`, `geodesic.comp`,
      `shaders/geodesic_scientific.comp`
- [ ] `gpu_cpu_agreement` PASS (não SKIP) — `BlackHole3D --scientific|--relativistic --capture`
      × `bh_render_cpu` em 6 cenas × 2 modos (inclui FOV/alvo da cena, metade da massa e `"objects": []`); gate `bad_fraction ≤ 0,002` e erro
      médio ≤ 0,5/255. SKIP (código 77) só é aceitável numa máquina sem display,
      sem `xvfb-run` ou sem OpenGL 4.3
- [ ] `legacy_golden` PASS (não SKIP) — `BlackHole3D` sem flags idêntico a
      `docs/images/legacy_default_gpu.png` (exato no driver gravado em
      `docs/images/legacy_default_gpu.json`, tolerância pequena nos demais) e
      `--scene examples/scene_params_example.json` = sem flags
- [ ] Os shaders **raiz** (`geodesic.comp`, `grid.*`) continuam sendo os carregados
      pelo baseline; `geodesic_scientific.comp` só com `--scientific`/`--relativistic`
- [ ] Captura manual do default ainda mostra o artefato documentado (metade de
      cima amarela) — se sumir, o baseline mudou (o `legacy_golden` automatiza
      esta comparação):
      ```bash
      cd build/gl && xvfb-run -a ./BlackHole3D --capture /tmp/legacy_default.png
      python3 ../../tools/image_diff.py /tmp/legacy_default.png ../../docs/images/legacy_default_gpu.png
      ```

## 4. Blender

```bash
make package-addon                      # regrava os 2 zips de forma determinística
git diff --exit-code -- blender/addons/black_hole_bridge.zip blender/black_hole_bridge.zip
make blender-scene                      # precisa de `blender` no PATH
```

- [ ] Zips do addon em dia (o job `scientific-headless` falha se não estiverem)
- [ ] `build_scene_headless.py` roda em `blender --background --factory-startup
      --python-exit-code 1` e gera um `.blend` não vazio (job `blender-headless`)
- [ ] Build Full Scene com a pose default emite o aviso de câmera dentro da laje do
      disco; com Elevação 1,25 a cena renderiza em Cycles (disco âmbar, grade
      cinza, anéis-guia, horizonte preto). No Ubuntu, desligue o denoising (o
      pacote vem sem OpenImageDenoise)
- [ ] Render via C++ (CPU) a partir do painel importa o PNG (ou a sequência) como
      fundo da câmera

## 5. Regressão de hashes (opcional)

```bash
python3 scripts/hash_tree.py --help
python3 scripts/hash_tree.py --dry-run --root .
python3 scripts/hash_tree.py --root .
```

## 6. Docs / packaging

- [ ] Links em `docs/INDEX.md` resolvem; imagens em `docs/images/` referenciadas
      com caminhos relativos corretos
- [ ] README "Matriz de funcionalidades" e "Limitações honestas" continuam honestos
- [ ] Números novos em docs vêm de execução (`quickstart_scientific`, linha JSON do
      `bh_render_cpu`, saída dos testes) — nunca estimados
- [ ] Sem `LICENSE` inventada — ver `LICENSE-NOTES.md`

## CI (referência)

| Job | Portões |
|-----|---------|
| `scientific-headless` | preset `ci-scientific` + CTest; zips do addon em dia; renders CPU como artefato |
| `scientific-portable` | árvore científica (sem GL) + CTest em Windows/MSVC e macOS/Apple Clang |
| `linux-build` | preset `ci-linux` (Mesa llvmpipe + Xvfb, OpenGL 4.5); CTest com `gpu_cpu_agreement` e `legacy_golden` executando; `validate_shaders`; capturas GPU como artefato |
| `blender-headless` | Blender 4.0.x do apt roda `build_scene_headless.py` → `.blend` |
