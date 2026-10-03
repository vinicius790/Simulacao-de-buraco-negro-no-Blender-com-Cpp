# Entrega — black_hole 0.8.0

**Óptica relativística, renderer CPU de referência, modo científico na GPU e ponte de render para o Blender.**

Versão anterior: 0.7.0-grand. Upstream original: <https://github.com/kavan010/black_hole>. Visão geral e galeria: [`README.md`](README.md). Portões de verificação: [`STATUS.md`](STATUS.md). Lista detalhada: [`CHANGELOG.md`](CHANGELOG.md).

![Showcase relativístico](docs/images/showcase_relativistic.png)

---

## Resumo em 30 segundos

- `BlackHole3D` **sem flags continua idêntico ao original** (Euler `legacyEulerStep`, disco âmbar 2,2–5,2 rs, grade cinza, câmera orbital). Tudo o que é novo fica atrás de flags explícitas ou em ferramentas separadas.
- Novo **modo científico na GPU**: `--scientific` (RK4 no plano orbital, colisões contínuas, fuga demonstrada) e `--relativistic` (+ Doppler e redshift gravitacional no disco).
- Novo **renderer CPU** `bh_render_cpu`: determinístico, multithread, double; modos `legacy`, `relativistic`, `blackbody` (Page–Thorne); gera PNG/BMP/PPM e sequências para o Blender.
- **GPU e CPU concordam**: erro máximo 1/255 nos dois modos na pose principal (no relativístico, um único canal de um pixel difere), medido com Mesa llvmpipe. O baseline sem flags é idêntico byte a byte à imagem de referência `docs/images/legacy_default_gpu.png` (teste `legacy_golden`).
- **Addon Blender 0.8.0**: orientação correta (Z-up), disco com espessura, grade e anéis visíveis no render, cor em paridade exata com o OpenGL, anéis-guia, presets de animação e importação dos renders C++.

---

## Como usar

### 1. Só ciência / imagens (sem OpenGL)

```bash
make test        # compila build/scientific e roda o CTest
make render      # PNGs em build/renders/ (inclui o showcase)

./build/scientific/bh_render_cpu --scene examples/scene_relativistic_showcase.json \
    --mode relativistic --width 640 --height 360 --supersample 2 --stars --out showcase.png
```

### 2. Visualização OpenGL

```bash
# Debian/Ubuntu: sudo apt install build-essential cmake libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev
# Windows/macOS: vcpkg install && vcpkg integrate install (ver README)
make build-gl
cd build/gl
./BlackHole3D                    # baseline histórico
./BlackHole3D --relativistic     # modo científico + Doppler/redshift
./BlackHole3D --scene ../../examples/scene_relativistic_showcase.json --relativistic
./BlackHole3D --capture frame.png   # um frame 200×150 e sai (xvfb-run -a … sem monitor)
```

Controles: arrastar com botão esquerdo/meio = orbitar; roda = zoom; `G` = liga/desliga gravidade dos objetos; botão direito segurado = gravidade ligada.

### 3. Blender (Steam ou não, 4.0+)

1. `Edit → Preferences → Add-ons → Install from Disk` → `blender/black_hole_bridge.zip` → ative **Black Hole Bridge**.
2. Viewport → `N` → aba **Black Hole** → **Cena → Build Full Scene**.
3. Se aparecer o aviso de câmera dentro do disco (default C++), ajuste **Parâmetros → Elevação (rad) = 1.25**.
4. **Guias → Build Guide Rings** (1,5 rs · 2,598 rs · 3 rs).
5. **Câmara / animação → Bake Camera Animation** (`TURNTABLE`, `ELEVATION_SWEEP`, `DOLLY`, `SPIRAL`; easing `LINEAR`/`SINE`).
6. **Render C++ → Blender → Render via C++ (CPU)** e depois **Import background** ou **Import as plane** para trazer a lente gravitacional calculada em C++.

Detalhes: [`blender/INSTALAR_E_RODAR.md`](blender/INSTALAR_E_RODAR.md). Headless: `make blender-scene`.

---

## O que foi entregue

| Área | Entrega |
|---|---|
| Biblioteca científica `bh_scientific` | Órbitas circulares (`orbits.hpp`), fator de redshift g (`redshift.hpp`), fluxo Page–Thorne + Eddington + corpo negro → sRGB + Reinhard (`disk_emission.hpp`), colisões contínuas (`hit_testing.hpp`), fuga por prova e sombra (`escape.hpp`), RK45 Dormand–Prince (`integrator_rk45.*`), integração no plano orbital sem polos (`planar_geodesic.*`), raios Kerr analíticos (`kerr_analytic.hpp`), deflexão de 2.ª ordem (`weak_field.hpp`), RK4 estável dentro do horizonte. |
| Renderer CPU | `cpu_renderer.*` + `image_io.*` (PNG/BMP/PPM sem dependências) + CLI `tools/bh_render_cpu.cpp`. |
| GPU | `shaders/geodesic_scientific.comp` reescrito e ligado; `black_hole.cpp` com `--scene` (inclusive FOV e alvo da câmera), `--scientific`, `--relativistic`, `--capture`, `--help`; UBO `SciParams` (binding 4, 32 bytes, `static_assert`); build sem warnings. |
| Testes | 14 testes CTest na árvore completa (12 sem OpenGL): contratos de baseline/estilo/shader, addon Blender (compilação e paridade), autoteste das ferramentas (`tools_selftest`), `scientific_ref` ampliado, `cpu_render`, testes de CLI (inclusive `render_cli_contract`, todas as flags), `gpu_cpu_agreement` e `legacy_golden` (GPU real via Mesa llvmpipe). |
| Blender | Addon 0.8.0: `guides.py`, `render_bridge.py`, presets de animação, spin do disco, correções de coordenadas/espessura/visibilidade/cor/vista "Raw", aviso de câmera dentro do disco, zips determinísticos (`package_addon.py`). |
| Engenharia | `Makefile` raiz versionado, CI com 4 jobs (inclui Windows/macOS sem OpenGL), `tools/image_diff.py`, `tools/frames_to_gif.py`, cena `examples/scene_relativistic_showcase.json`, galeria `docs/images/`. |

---

## Números medidos (resumo)

| Medida | Valor |
|---|---|
| GPU vs CPU, pose principal (az 0, el 1,25) | erro máx. 1/255 (científico e relativístico) |
| GPU vs CPU, demais 5 cenas/poses | no máximo 4 pixels de borda em 30 000; erro médio < 0,08/255 |
| `BlackHole3D` sem flags vs imagem de referência | idêntico byte a byte (`legacy_golden`, llvmpipe) |
| Deriva do vínculo nulo (100 passos) | Euler 8,8 × 10⁻³ · RK4 2,4 × 10⁻⁹ |
| RK45 vs RK4 fino | 120 passos vs 3000 para o mesmo estado |
| Sombra na câmera default (4,997 rs) | 0,4836 rad = 27,7° |
| b_c | 2,598 rs |
| Sgr A\* a 1 % de Eddington | L_Edd = 5,40 × 10³⁷ W; Ṁ = 1,05 × 10²⁰ kg/s; T_eff máx. ≈ 86 700 K |
| Kerr a\* = 0,998 (analítico) | ISCO 1,237 M; r₊ 1,063 M; η 32,1 % |

Tabela completa em [`README.md`](README.md#física-resumo-com-números-medidos) e [`STATUS.md`](STATUS.md).

---

## O que NÃO mudou (proposital)

- `geodesic.comp` da raiz (Euler `legacyEulerStep`, `SagA_rs = 1.269e10`, `D_LAMBDA = 1e7`, 60 000 passos, `ESCAPE_R = 1e30`) e o caminho default do `BlackHole3D`.
- Constantes travadas: janela 800×600, compute 200×150, disco 2,2–5,2 rs, cor `vec3(1, r_norm, 0.2)` com alfa `r_norm`, grade `vec4(0.5, 0.5, 0.5, 0.7)`, câmera 6,34194 × 10¹⁰ m (≈ 4,997 rs), azimute 0, elevação π/2, FOV 60°.
- O artefato da vista inicial (metade superior amarela) foi **explicado e documentado, não corrigido**: a câmera em float fica ≈ 2,8 km abaixo do plano do disco e dentro do anel 2,2–5,2 rs, então todo raio que sobe "acerta o disco" no primeiro passo. Basta orbitar para sumir.

## Limites honestos

- **Kerr não é simulado** — só raios analíticos.
- Sem transferência radiativa, GRMHD, coroa ou linhas espectrais.
- Números de GPU só em Mesa llvmpipe (software); nenhum tempo de GPU real é afirmado.
- O disco legado começa em 2,2 rs, dentro da ISCO (3 rs); a cena showcase usa 3–12 rs.
- Licença: o upstream não publica nenhuma; só o addon Blender declara MIT (escolha do dono, cobre apenas o addon). Ver `LICENSE-NOTES.md`; `CITATION.cff` mantém autores placeholder.

## Mapa

Comece por [`README.md`](README.md), depois [`STATUS.md`](STATUS.md) e [`docs/INDEX.md`](docs/INDEX.md).
