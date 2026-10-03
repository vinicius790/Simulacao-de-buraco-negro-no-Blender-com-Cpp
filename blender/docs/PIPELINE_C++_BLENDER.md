# Pipeline C++ ↔ Blender (duas direções)

O C++ é dono da física (geodésicas, lensing, sombra, Doppler). O Blender é o
frontend: monta a cena com a paleta travada, anima a câmera e recebe as
imagens do C++. A troca acontece por **JSON** (`black_hole.scene_params/v1`) num
sentido e por **imagens** (PNG/BMP, still ou sequência) no outro.

```
 ┌───────────────────────────────────────────────────────────────┐
 │ C++  (dono da física)                                          │
 │   BlackHole3D   GL interativo; --scene --scientific            │
 │                 --relativistic --capture                       │
 │   bh_render_cpu CPU headless; legacy | relativistic | blackbody│
 └──────────▲─────────────────────────────────────┬──────────────┘
            │ --scene scene_params.json           │ frame.png / stem_0000.png …
            │ (+ flags de câmera do painel)       │ + 1 linha JSON no stdout
            │                                     ▼
 ┌──────────┴─────────────────────────────────────────────────────┐
 │ Blender — addon Black Hole Bridge 0.8.0                         │
 │   json_io.py        export / import scene_params/v1             │
 │   render_bridge.py  roda bh_render_cpu, importa background/plano│
 │   constants.py      cpp_to_blender: (x,y,z) → (x,−z,y)          │
 └─────────────────────────────────────────────────────────────────┘
```

| Responsabilidade | Onde |
|------------------|------|
| Integração geodésica, cruzamento do disco, escape, cor final (legacy / Doppler / corpo negro) | C++ (`geodesic.comp`, `geodesic_scientific.comp`, `bh_scientific`, `bh_render_cpu`) |
| Cena de referência: horizonte, disco `(1, r, 0.2)·r`, grade, guias, câmera, animação | addon (`scene_builder`, `materials`, `guides`, `camera_orbit`) |
| Parâmetros compartilhados | JSON `black_hole.scene_params/v1` |
| Imagens do C++ dentro do Blender | `render_bridge.py` (background da câmera ou plano emissivo) |

## Unidades e eixos

- Cena Blender em unidades geométricas: **1 unidade = rs** (horizonte = esfera
  de raio 1). Metros = unidades × `r_s_m`.
- O C++ é Y-up (disco em XZ); o Blender é Z-up. A conversão é feita só por
  `constants.cpp_to_blender` (x, y, z) → (x, −z, y), uma rotação própria
  (det +1). A câmera do Blender, com Track To e up = +Z, enquadra igual à câmera
  C++ com up = +Y.
- A câmera vai para o JSON e para a CLI **no referencial C++**
  (`azimuth_rad`, `elevation_rad`, `radius_m` / `--radius-rs`). Não há rotação
  a desfazer do lado C++.

## Direção 1 — Blender → C++ (JSON)

**JSON ↔ C++ → Export Scene JSON (path)** (ou **Export Scene Params JSON**).
Seções escritas pelo addon e quem as lê:

| Seção | Campos | Lido pelo C++? |
|-------|--------|----------------|
| `schema` | `black_hole.scene_params/v1` | sim (validado) |
| `black_hole` | `name`, `mass_kg`, `r_s_m` (= 2GM/c²), `position_m` | sim |
| `camera` | `radius_m`, `azimuth_rad`, `elevation_rad`, `fov_y_deg`, `target_m` (+ `radius_rs` informativo) | sim (`radius_rs` não) |
| `disk` | `inner_factor_rs`, `outer_factor_rs`, `thickness_m`, `disk_num` | sim |
| `disk` | `inner_radius_m`, `outer_radius_m`, `spin_turns`, `turbulence`, cores | não (os fatores têm precedência) |
| `render_baseline` | `window`, `compute`, `integrator`, `scientific_default` | sim, só informativo |
| `units`, `guides`, `grid`, `animation`, `render`, `style_lock`, `notes` | — | não (ignorados) |
| `gravity`, `objects[]` | — | o addon **não** escreve; o C++ mantém os padrões (Gravity OFF, 3 objetos legados) |

Uso:

```bash
# CPU headless (honra camera.fov_y_deg e camera.target_m)
./build/scientific/bh_render_cpu --scene scene_params.json --out out.png
# GL interativo / captura de um frame (também honra FOV e alvo; janela 800×600, compute 200×150)
./build/gl/BlackHole3D --scene scene_params.json
./build/gl/BlackHole3D --scene scene_params.json --relativistic --capture gl.png
```

Verificado neste container com um JSON exportado pelo `json_io` do addon
(elevação 1.25): o `BlackHole3D` imprime
`[INFO] scene loaded: Sagittarius A* rs=1.26839e+10 m, camera R=6.34194e+10 az=0 el=1.25, 3 objects, Gravity OFF`
e captura o frame (Mesa llvmpipe, `xvfb-run`). O `bh_render_cpu` renderiza o
mesmo arquivo normalmente.

> O addon grava `r_s_m = 2GM/c²` ≈ 1.26839e10 m para a massa padrão; o
> literal legado do shader é 1.269e10 m (diferença ≈ 0.05%). Sem `--scene`,
> os binários usam o baseline exato.

## Direção 2 — C++ → Blender (imagens)

### Passo a passo pelo painel

1. Compile: `make build` → `build/scientific/bh_render_cpu` (sem OpenGL). Ou
   `cmake --preset scientific && cmake --build build/scientific --target bh_render_cpu`.
2. Salve o .blend e rode **Build Full Scene** (sugestão: Elevação 1.25).
3. **Render C++ → Blender → Renderer path** = o binário ou a pasta dele
   (vazio = busca automática em `build/scientific`, `build/sci`,
   `build/default`, `build` a partir da pasta do .blend e acima, da pasta de
   trabalho e da raiz do repo quando o addon roda do repo; depois o `PATH`).
4. Ajuste Largura/Altura, Modo (`Legado` / `Relativístico` / `Corpo negro`),
   Integrador (`RK4` / `RK45`), Frames, Supersample, Estrelas, Saída, Timeout.
5. **Render via C++ (CPU)**. O addon:
   1. grava `<stem>_scene_params.json` ao lado da saída;
   2. para cada frame (o atual, ou `frame_start … frame_start + N − 1` quando
      Frames = N > 1), posiciona a cena no frame, converte a posição animada
      da `BH_OrbitCamera` para o referencial C++
      (`orbit_params_from_cpp_position`) e o FOV vertical da câmera
      (`camera_fov_y`), monta o argv com `render_bridge.build_render_command`
      e executa uma vez (síncrono; a interface espera até o timeout):

      ```
      bh_render_cpu --scene <json> --out <saída ou stem_NNNN> --width W --height H
                    --mode legacy|relativistic|blackbody --integrator rk4|rk45
                    --azimuth A --elevation E --radius-rs R --fov-y-deg D
                    --frames 1 --supersample S [--stars]
      ```

      As flags de câmera vêm da câmera animada e têm precedência sobre o JSON;
   3. com exit 0, lê a última linha JSON do stdout
      (`render_bridge.parse_render_summary`) e guarda em
      `Scene["bh_last_render_summary"]` (com `frames` = N, `scene_frames` e o
      primeiro comando), por exemplo
      `{"frames":2,"width":400,"height":300,"mode":"legacy","integrator":"rk4","shadow_fraction":0.43865,"disk_fraction":0.4841,…,"seconds":1.45}`;
   4. carrega o primeiro arquivo (`expected_frame_paths`) como **background
      da `BH_OrbitCamera`** (FIT, alpha 1) e ajusta a resolução de render da
      cena ao tamanho da imagem.
6. Olhe pela câmera (**Numpad 0**).

### Background × plano

| | Background da câmera | **Import as plane** |
|---|----------------------|---------------------|
| Aparece no render final | **não** (só no viewport) | sim |
| Objeto | slot de background da câmera | `BH_RenderPlane`, filho da câmera, z local = −10 |
| Tamanho | FIT | 2·10·tan(FOV_y/2) de altura × aspecto da imagem (`plane_size_for_fov`) |
| Material | — | Emission 1.0 da textura; *Closest* até 200 px de largura, senão *Linear* |
| Uso | comparar enquadramento Blender × C++ | render / composição no Cycles ou EEVEE |

Com o view transform **Raw** que o addon aplica, a emissão 1.0 devolve os
pixels do C++ sem alteração (a menos do ruído de amostragem). Os objetos
`BH_*` ficam entre a câmera e o plano, então desligue-os do render se quiser
só a imagem C++.

### Sequências

- Pelo painel, Frames = N (N > 1) grava `stem_0000.ext … stem_{N−1}.ext`, um
  arquivo por frame da cena (`expected_frame_paths`), cada um com a pose
  animada da câmera.
- Na CLI, `--frames N` (N > 1) grava a mesma nomenclatura com azimute
  az₀ + 2π·T·k/N (k = 0…N−1, `--azimuth-turns T`, padrão 1) e, opcionalmente,
  elevação interpolada até `--elevation-end`.
- `find_sequence_files` reconhece `stem_NNNN.ext` e lista os irmãos;
  `_configure_sequence` faz `image.source = 'SEQUENCE'` a partir do
  `frame_start` da cena (arquivo `stem_0000` ↔ `frame_start`) e ajusta
  `frame_end = frame_start + N − 1`.
- Funciona em **Render via C++**, **Import background** e **Import as plane**.

### Saída e formatos

- A extensão de `--out` escolhe o formato: `.png` (deflate *stored*, sem
  dependências), `.bmp`, `.ppm`. O Blender carrega PNG e BMP; PPM não.
- Uma linha JSON no stdout; exit ≠ 0 → o addon mostra as 3 últimas linhas do
  stderr.

### Renders que o painel não expõe

O painel cobre `legacy`, `relativistic` e `blackbody` e as estrelas. Para
exposição, gama, fração de Eddington, sentido de rotação do disco, threads ou
limite de passos, rode o binário à mão com o JSON exportado e importe a
imagem:

```bash
./build/scientific/bh_render_cpu --scene scene_params.json --out bh_render/bb.png \
    --mode blackbody --width 640 --height 360 --supersample 2 --stars \
    [--exposure E] [--gamma G] [--mdot-edd F] [--spin-sign ±1] [--threads T] [--max-steps N]
```

Referência medida: 400×300 legado ≈ 0.5 s com 4 threads; média de ≈ 77–150
passos RK4 por raio, conforme a pose.

## Importar JSON escrito pelo C++ ou à mão

**Import Scene Params JSON** preenche o painel; rode **Build Full Scene**
depois. São lidos:

| Seção | Campos |
|-------|--------|
| `black_hole` | `mass_kg` |
| `disk` | `inner_factor_rs`, `outer_factor_rs`, `disk_num`, `thickness_m`, `spin_turns`, `turbulence` |
| `camera` | `radius_m` (ou `radius_rs`), `azimuth_rad`, `elevation_rad`, `fov_y_deg` |
| `grid` | `grid_size` |
| `animation` | `fps`, `duration_s`, `mode`, `easing`, `elev_start_rad`, `elev_end_rad`, `radius_end_rs` |
| `render` | `width`, `height`, `mode` (`legacy`/`relativistic`/`blackbody`), `integrator`, `frames`, `supersample` |

Ignorados: `objects`, `gravity`, `guides` (constantes), `render_baseline` e
chaves desconhecidas. `animation.mode = "turntable_azimuth"` (0.7.x) vira
`TURNTABLE`. Exemplo pronto: `examples/scene_relativistic_showcase.json`
(disco 3–12 rs a partir da ISCO, câmera a 18 rs, elevação 1.40, FOV 38°).

## Testes que protegem o contrato

| Teste | Cobre |
|-------|-------|
| `blender_addon_parity` (CTest, sem Blender) | constantes vs headers C++, `cpp_to_blender`, `build_render_command`, `parse_render_summary`, `expected_frame_paths`, `find_sequence_files`, `plane_size_for_fov`, JSON ida e volta, presets/easing, zips |
| `render_cli_smoke`, `render_cli_relativistic_sequence` | `bh_render_cpu` com `--scene` e com sequência |
| `render_cli_contract` | todas as flags do `bh_render_cpu` (`tests/test_render_cli.py`) |
| `gpu_cpu_agreement` | `BlackHole3D --capture` × `bh_render_cpu` (6 cenas × 2 modos, inclusive FOV/alvo da cena e `"objects": []`; pula com código 77 sem GL) |
| CI `blender-headless` | Blender 4.0.x do `apt` roda `build_scene_headless.py` e gera o .blend |

Estilo: [`docs/ESTILO_VISUAL_SIMULACAO.md`](../../docs/ESTILO_VISUAL_SIMULACAO.md).
Arquitetura: [`docs/BLENDER_ADAPTER.md`](../../docs/BLENDER_ADAPTER.md).
