# Instalar e rodar — Black Hole Bridge 0.8.0 (Blender 4.x)

Guia canônico do addon. O **Black Hole Bridge** monta, dentro do Blender, a
mesma cena do simulador C++/OpenGL (horizonte preto, disco âmbar, grade cinza
deformada, câmera em órbita), acrescenta guias de Schwarzschild e presets de
animação, e faz a **ponte com o renderer C++ `bh_render_cpu`**: o C++ calcula
as geodésicas (lensing, sombra, anel de fótons, Doppler) e o Blender recebe as
imagens prontas.

> **A física fica no C++.** O Blender não curva a luz: a cena Blender é uma
> pré-visualização geométrica com paleta travada. Lensing real só nas imagens
> vindas do `bh_render_cpu` / `BlackHole3D`. Apenas Schwarzschild; Kerr não é
> simulado em lugar nenhum do projeto.

![Render Cycles do addon (Blender 4.0.2, elevação 1.25)](../docs/images/blender_el125_cycles.png)

*`docs/images/blender_el125_cycles.png` — Blender 4.0.2 headless, Cycles, cena
do addon com **Elevação 1.25 rad**: disco âmbar `(1, r, 0.2)·r`, grade cinza
deformada, anéis-guia, horizonte preto. Sem lensing no Blender, por projeto.
A receita exata está na [seção 12](#12-uso-headless-sem-interface).*

Os rótulos do painel são citados exatamente como aparecem no addon (alguns em
pt-PT, por exemplo **Câmara / animação**).

---

## 1. Pré-requisitos

| Item | Detalhe |
|------|---------|
| Blender | 4.0 ou mais novo (`bl_info["blender"] = (4, 0, 0)`, `blender_version_min = "4.0.0"`). Verificado em **Blender 4.0.2** (headless, Cycles). Steam, site oficial ou pacote da distro. |
| Addon | `blender/addons/black_hole_bridge.zip` (cópia byte a byte idêntica em `blender/black_hole_bridge.zip`), gerado por `blender/scripts/package_addon.py` |
| Renderer C++ (opcional) | `bh_render_cpu`, só para a ponte **Render C++ → Blender** (seção 10). Compila sem OpenGL. |

## 2. Instalar o addon

### 2.1 Blender 4.0 / 4.1 (add-on clássico)

1. **Edit → Preferences → Add-ons → Install…**
2. Escolha `blender/addons/black_hole_bridge.zip` (não descompacte).
3. Marque **Black Hole Bridge** (categoria *Physics*). Use o filtro "Black Hole"
   se não aparecer.

### 2.2 Blender 4.2+ (extensions)

1. **Edit → Preferences → Get Extensions** (ou **Add-ons**) → menu **˅** no
   canto superior direito → **Install from Disk…**
2. Escolha o mesmo `black_hole_bridge.zip`. O zip traz
   `black_hole_bridge/blender_manifest.toml` (id `black_hole_bridge`,
   versão `0.8.0`, permissão `files` para exportar/importar JSON e ler os
   PNG/BMP do renderer) além do `bl_info` clássico.
3. Ative **Black Hole Bridge**.

> O manifesto foi escrito para o formato de extensions do 4.2+, mas a
> verificação real deste release foi feita no 4.0.2. Se o 4.2+ recusar o zip,
> use a instalação por pasta (2.3), que o Blender ainda aceita para add-ons
> clássicos.

### 2.3 Pasta (desenvolvimento)

Copie ou crie um link simbólico de `blender/addons/black_hole_bridge/` dentro
da pasta de add-ons do usuário e reinicie o Blender:

| Sistema | Pasta (troque `4.0` pela sua versão) |
|---------|--------------------------------------|
| Linux | `~/.config/blender/4.0/scripts/addons/` |
| Windows | `%APPDATA%\Blender Foundation\Blender\4.0\scripts\addons\` |
| macOS | `~/Library/Application Support/Blender/4.0/scripts/addons/` |

Vantagem do link simbólico: o addon continua dentro do repositório, então a
busca automática do `bh_render_cpu` (seção 10) encontra `build/scientific/`
sem configurar nada.

### 2.4 Atualizar de 0.7.x

**Remove** o addon antigo → instale o zip 0.8.0 → reinicie o Blender. Os
`bl_idname` são os mesmos, mas cenas montadas pela 0.7.x estão **em pé** (eixo Y
para cima) e com o material antigo do disco: rode **Build Full Scene** de novo
para aplicar a convenção de eixos, a paridade de cor e os novos modificadores.

### 2.5 Desinstalar

Preferences → Add-ons → Black Hole Bridge → **Remove** (ou só desmarcar).

---

## 3. Primeiro uso: Build Full Scene

**3D Viewport → tecla N → aba Black Hole.** Painel **Black Hole Bridge** com
sub-painéis:

| Sub-painel | Conteúdo |
|------------|----------|
| **Cena** | **Build Full Scene**, **Rebuild Horizon**, **Rebuild Disk**, **Rebuild Grid** |
| **Parâmetros** | M (kg), Disco interno/externo (× rs), Unidades geométricas, Raio câmara, Azimute, Elevação, FOV Y, Grade (N), Grade multi-plano |
| **Guias (…)** *(fechado)* | **Build Guide Rings**, limpar (×) |
| **Câmara / animação** | **Align Camera From Params**, Preset, Easing, FPS, Duração, **Bake Camera Animation** |
| **Disco (spin)** *(fechado)* | Voltas do disco, Brilho do disco, Turbulência, **Bake Disk Spin** |
| **Render C++ → Blender** *(fechado)* | Renderer path, resolução, modo, integrador, frames, supersample, estrelas, saída, timeout, **Render via C++ (CPU)**, **Import background**, **Import as plane** |
| **JSON ↔ C++** | Caminho JSON, Export / Import |

**Cena → Build Full Scene** cria (ou recria):

| Coleção (dentro de `BH_Scene`) | Objeto | O que é |
|--------------------------------|--------|---------|
| `BH_Core` | `BH_Horizon` | esfera UV preta, raio 1 (= rs) |
| `BH_Disk` | `BH_AccretionDisk` | anel 2.2–5.2 rs, emissão `(1, r, 0.2)·r`, modificador Solidify `BH_DiskThickness` com a espessura travada 1e9 m ≈ 0.079 rs |
| `BH_Grid` | `BH_SpacetimeGrid` (+ `_XY`, `_YZ` se multi-plano) | grade 25×25 deformada (`generateGrid`), quads + modificador Wireframe `BH_GridLines` |
| `BH_Camera` | `BH_OrbitPivot`, `BH_OrbitCamera` | pivô na origem + câmera com Track To (`BH_TrackOrigin`), FOV **vertical** 60° |
| `BH_Lights` | `BH_FillLight` | luz de área fraca (0.5 W); tudo o que brilha é emissivo |
| `BH_Guides` | *(vazia até Build Guide Rings)* | anéis-guia (seção 7) |

Além disso: fundo do mundo preto (força 0), view transform **Raw**
(seção 6), motor **Cycles** e `film_transparent = False`. O relatório do
operador mostra a nota da cena e, quando for o caso, o **aviso de câmera dentro
do disco** (seção 5).

Depois: ajuste **Parâmetros** e rode Build de novo (ou **Align Camera From
Params** só para a câmera) → **Bake Camera Animation** → Play / Render.

Para conferir: **Numpad 0** (olhar pela câmera) e shading **Rendered**.

---

## 4. Convenção de coordenadas (C++ Y-up → Blender Z-up)

O simulador é **Y-up** com o disco no plano **XZ** (`black_hole.cpp`,
`geodesic.comp`). O Blender é **Z-up**. Uma única camada faz a conversão,
`constants.cpp_to_blender`:

```
(x, y, z)_C++  →  (x, −z, y)_Blender        # rotação própria de +90° em torno de X
blender_to_cpp: (x, y, z)_Blender → (x, z, −y)_C++
```

| | C++ | Blender |
|---|-----|---------|
| Eixo "para cima" | +Y | +Z |
| Plano do disco | XZ (y = 0) | XY (z = 0) |
| Câmera | `Camera::position()`, up = +Y | mesma posição mapeada, Track To com up = +Z |
| Exemplo | (1, 2, 3) | (1, −3, 2) |

**Por quê uma rotação e não uma troca de eixos?** O determinante é +1, então
produtos vetoriais são preservados: `right = forward × up` dá o mesmo vetor nos
dois mundos e a câmera do Blender enquadra **exatamente** como a do C++. Uma
troca simples (x, z, y) tem determinante −1 e espelharia a imagem. Na 0.7.x não
havia conversão: o disco ficava em pé no Blender e a vista girada 90°.

Os helpers puros (malhas, órbita, anéis) continuam no referencial C++ — é isso
que os testes de paridade comparam com os headers C++. A conversão só acontece
quando os dados do Blender são escritos. Consequência prática: visto de cima
(+Z), o azimute cresce no sentido **horário** no Blender, porque `y_B = −z_C++`.

## 5. Aviso "câmera dentro do disco" e a elevação 1.25

O padrão travado do C++ é raio 6.34194e10 m ≈ **4.997 rs**, azimute 0 e
elevação **π/2**. Esse raio fica **dentro** do anel 2.2–5.2 rs e a elevação π/2
põe a câmera no plano do disco. Por isso **Build Full Scene** emite um
**WARNING**:

```
WARNING: camera is inside the accretion disk (C++ default view) —
raise Elevation (e.g. 1.25) for a clean render.
```

O teste é `constants.camera_inside_disk`: ρ = r·sin(el) entre os fatores
interno e externo **e** |r·cos(el)| ≤ metade da espessura (0.039 rs).

O que acontece em cada lado:

- **OpenGL (baseline, documentado e não alterado):** π/2 guardado como `float`
  dá cos ≈ −4.37e−8, então a câmera fica ≈ 2.8 km **abaixo** do plano do
  disco. Todo raio que sobe "cruza o disco" no primeiro passo e a metade de
  cima do primeiro frame sai amarela sólida até o usuário orbitar
  ([`legacy_default_gpu.png`](../docs/images/legacy_default_gpu.png)).
- **Blender:** a câmera enxerga o interior da laje de 0.079 rs.
- **Modo científico / `bh_render_cpu`:** tratam a câmera no plano
  corretamente (o primeiro segmento não conta como cruzamento).

**Recomendação para renders no Blender: Parâmetros → Elevação = 1.25 rad.** A
câmera sobe ≈ 18° acima do plano (altura 1.58 rs, ρ = 4.74 rs), sai da laje e
o aviso some — é a pose de [`blender_el125_cycles.png`](../docs/images/blender_el125_cycles.png)
e das imagens `*_el125*` da galeria. Também resolve: raio maior que o fator
externo (> 5.2 rs).

## 6. Paridade de cor com o OpenGL

| Elemento | OpenGL | Blender 0.8.0 |
|----------|--------|---------------|
| Disco | `geodesic.comp` escreve `vec4(1, r, 0.2, r)`, `r = |pos| / disk_r2`; o passe de tela mistura com `SRC_ALPHA` sobre preto → **`(1, r, 0.2)·r`** | Emission pura com cor `(1, r, 0.2)·r`; `r` vem do UV radial remapeado para `[interno/externo, 1]` com os fatores **reais** do disco |
| Valores do disco (2.2/5.2) | borda interna r = 0.423 → (0.423, 0.179, 0.085); borda externa (1, 1, 0.2) | idem |
| Grade | `grid.frag` `vec4(0.5, 0.5, 0.5, 0.7)` sobre preto → cinza 0.35 | Emission cinza 0.5, força 1.0, misturada 70/30 com Transparent → 0.35 |
| Anéis-guia | — | mesmo cinza, alpha 0.5 → 0.25 |
| Horizonte / fundo | preto | Principled preto sem especular / mundo preto força 0 |

Duas configurações de cena garantem que o valor no PNG seja o valor do
framebuffer:

- **View transform "Raw"** (`materials.setup_color_parity`, com `look = None`,
  exposição 0, gama 1). O padrão do Blender 4.0, **AgX**, lavava o disco âmbar
  para quase branco. Se quiser um look cinematográfico, troque para AgX/Filmic
  depois — mas aí não há mais paridade.
- **Cycles** selecionado por atribuição direta (`scene.render.engine = "CYCLES"`).
  A verificação antiga (`"CYCLES" in dir(bpy.types)`) era sempre falsa.

**Disco (spin) → Brilho do disco** (`disk_glow`, padrão **1.0** = paridade).
Valores > 1 dão brilho artístico; o matiz não muda. Vale a partir da próxima
reconstrução do material (**Rebuild Disk**, **Build Full Scene** ou **Bake
Disk Spin**).

Correções de visibilidade que vieram junto:

- Disco com Solidify (espessura 1e9 m ≈ 0.079 rs): antes tinha espessura zero e
  sumia visto de lado.
- Grade como quads + Wireframe, anéis como curvas com bevel (raio 0.012 rs):
  arestas soltas são invisíveis para Cycles/EEVEE, e antes a grade e os anéis
  nunca apareciam no render final.

## 7. Guias (fóton 1.5 rs · b_c ≈ 2.598 rs · ISCO 3 rs)

**Guias → Build Guide Rings** cria três curvas fechadas no plano do disco, na
coleção `BH_Guides`, com o material `BH_Guide_Mat` (cinza, alpha 0.5) e
`show_in_front`. Aparecem no viewport **e** no render.

| Objeto | Raio | Significado |
|--------|------|-------------|
| `BH_Guide_PhotonSphere` | 1.5 rs (= 3 M) | esfera de fótons (órbita circular instável da luz) |
| `BH_Guide_CriticalImpact` | (3√3/2) rs ≈ 2.598 rs | parâmetro de impacto crítico b_c — raio aparente da sombra para um observador distante |
| `BH_Guide_ISCO` | 3.0 rs (= 6 M) | órbita circular estável mais interna |

Constantes espelhadas de `include/black_hole/schwarzschild.hpp` e conferidas
pelo teste de paridade. Cada objeto tem `bh_role = "guide"`, `bh_radius_rs` e
`bh_meaning`. **Build** apaga e recria; **×** remove.

Os anéis são **referências geométricas**, não a sombra vista pela câmera. Sem
lensing, o horizonte do Blender (raio 1 rs) visto de 4.997 rs ocupa
asin(1/4.997) ≈ 11.5°. A sombra real calculada pelo C++ nessa distância tem
raio angular **27.7°** (sin α = b_c·√(1 − rs/r)/r).

O disco legado começa em 2.2 rs, **dentro** da ISCO (3 rs): entre 2.2 e 3 rs
só existem órbitas circulares instáveis (a 2.2 rs a velocidade orbital local
seria 0.645 c; na ISCO, 0.5 c). Para um disco fisicamente consistente use fator interno 3.0,
como em `examples/scene_relativistic_showcase.json` (3–12 rs).

## 8. Presets de animação

Em **Câmara / animação** escolha **Preset** e **Easing** →
**Bake Camera Animation**. A câmera sempre mira a origem (Track To).

| Preset | Raio | Azimute | Elevação |
|--------|------|---------|----------|
| `TURNTABLE` (padrão) | fixo | az₀ → az₀ + 2π·(N−1)/N (loop sem costura) | fixa |
| `ELEVATION_SWEEP` | fixo | fixo | *Elevação início* → *Elevação fim* |
| `DOLLY` | *Raio câmara* → *Raio final* | fixo | fixa |
| `SPIRAL` | fixo | az₀ → az₀ + 2π·(N−1)/N | início → fim |

| Easing | Curva aplicada a t ∈ [0, 1] |
|--------|-----------------------------|
| `LINEAR` | t |
| `SINE` | 0.5 − 0.5·cos(πt) (acelera e desacelera; extremos preservados) |

N = número de frames. Com easing `LINEAR`, o azimute do frame k (k = 0…N−1) é
az₀ + 2π·k/N (com `SINE`, az₀ + 2π·ease(k/N)), a mesma amostragem do
`bh_render_cpu --frames N`: o último frame não repete o primeiro,
então o turntable fecha o loop sem engasgo. Elevação e raio usam k/(N−1) e
chegam exatamente aos valores finais.

Padrões: FPS 24, duração 8 s (→ 192 frames), elevação 0.35 → π − 0.35 rad,
raio final 12 rs. Elevação sempre limitada a (0.01, π − 0.01), como no C++.
Detalhes e fórmulas: [`docs/ANIMACAO.md`](docs/ANIMACAO.md).

## 9. Disco: spin, turbulência e brilho

**Disco (spin) → Bake Disk Spin** reconstrói o material do disco com um nó
*Mapping* (`BH_DiskSpin`) e anima `Location.x` (= UV.u, ângulo/2π) de 0 em
`frame_start` até **Voltas do disco** × (n − 1)/n em `frame_end` (n frames;
interpolação e extrapolação lineares), de modo que `frame_end + 1` equivale a
`frame_start` e o loop não repete imagem. Rode **depois** do Bake Camera Animation, que define o
intervalo de frames.

- O gradiente do disco é radialmente simétrico, então o spin **não aparece**
  sozinho. Para enxergar a rotação, suba **Turbulência (brilho)** (0 = desligado,
  padrão): um ruído sem costura em (cos 2πu, sin 2πu, v) multiplica só a
  **força** da emissão, entre 1 − t e 1 + t. A cor continua `(1, r, 0.2)·r`.
- **Rebuild Disk** volta ao material sem spin.

## 10. Render C++ → Blender (passo a passo)

### 10.1 Compilar o `bh_render_cpu`

Na raiz do repositório:

```bash
make build                    # árvore científica, sem OpenGL → build/scientific/bh_render_cpu
# ou só CMake:
cmake --preset scientific
cmake --build build/scientific --target bh_render_cpu
# conferir:
./build/scientific/bh_render_cpu --help
```

Também há um `bh_render_cpu` na árvore OpenGL (`make build-gl` →
`build/gl/bh_render_cpu`), mas `build/gl` **não** está na busca automática:
aponte o **Renderer path** para ele. No Windows o binário é `.exe`; com
geradores multi-config (Visual Studio) ele costuma ficar em
`build/scientific/Release/`.

### 10.2 Configurar o painel

1. **Salve o .blend** (os caminhos `//…` são relativos ao arquivo, e a busca
   automática usa a pasta dele).
2. Monte a cena (**Build Full Scene**) e acerte **Parâmetros** (sugestão:
   Elevação 1.25).
3. **Render C++ → Blender → Renderer path:** o binário ou a pasta que o contém.
   Vazio = procurar `build/scientific`, `build/sci`, `build/default` e `build`
   na pasta do .blend e em até 3 pastas acima, na pasta de trabalho, na raiz do
   repositório (só quando o addon é carregado direto do repo, como na seção
   2.3) e por fim no `PATH`. Instalado pelo zip, o addon vive na pasta do
   Blender, então preencha o caminho ou salve o .blend dentro do repo.
4. **Largura / Altura** (padrão 800×600), **Modo** (`Legado` = paleta
   `vec3(1,r,0.2)`; `Relativístico` = paleta legada + Doppler, redshift
   gravitacional e beaming g⁴; `Corpo negro` = temperatura Page–Thorne
   deslocada por g, sem emissão dentro da ISCO), **Integrador** (`RK4` passo
   geométrico / `RK45` adaptativo Dormand–Prince), **Frames** (1 = still no
   frame atual; N > 1 = os frames `frame_start … frame_start + N − 1` da cena,
   cada um com a pose animada da câmera), **Supersample** (1–4),
   **Estrelas** (campo estelar procedural para raios que escapam), **Saída**
   (padrão `//bh_render/frame.png`; `.png`/`.bmp` carregam no Blender, `.ppm`
   não) e **Timeout (s)** (padrão 900, por frame).

### 10.3 Rodar

**Render via C++ (CPU)**:

1. Grava o JSON da cena ao lado da saída: `<stem>_scene_params.json`
   (por exemplo `bh_render/frame_scene_params.json`).
2. Para cada frame, posiciona a cena nele, lê a pose **real** da
   `BH_OrbitCamera` (posição animada convertida para o referencial C++ e FOV
   vertical da câmera) e executa uma vez, com `--frames 1` (a câmera vai
   explicitamente na linha de comando e tem precedência sobre a do JSON):

   ```
   bh_render_cpu --scene <json> --out <saída ou stem_NNNN> --width W --height H
                 --mode legacy|relativistic|blackbody --integrator rk4|rk45
                 --azimuth A --elevation E --radius-rs R --fov-y-deg D
                 --frames 1 --supersample S [--stars]
   ```

   Se a câmera ainda não existe (ou está na origem), ela é criada e posicionada
   pelos **Parâmetros** antes. A interface do Blender fica bloqueada até o
   último frame terminar (ou estourar o timeout). Referência medida neste projeto: 400×300 legado ≈ 0.5 s com 4
   threads; o tempo cresce com pixels × supersample² × frames.
3. Com exit 0, guarda a linha JSON do stdout (do último frame, com `frames` = N,
   o intervalo de frames da cena e o primeiro comando) em
   `Scene["bh_last_render_summary"]`. O painel mostra
   `Último render: frames N · W×H · sombra … · disco … · legacy`.
4. Carrega a imagem (ou a sequência) como **background da `BH_OrbitCamera`**
   (FIT, alpha 1, atrás da cena) e ajusta a resolução de render da cena ao
   tamanho da imagem.

Olhe pela câmera (**Numpad 0**).

### 10.4 Background ou plano?

| Opção | Onde aparece | Uso |
|-------|--------------|-----|
| **Background da câmera** (padrão do Render via C++ e **Import background**) | só no **viewport** (o Blender não renderiza background images de câmera) | conferir enquadramento, comparar Blender × C++ |
| **Import as plane** | viewport **e** render final | plano `BH_RenderPlane`, emissão 1.0, filho da câmera a 10 unidades (−Z local), dimensionado para preencher o FOV vertical × aspecto da imagem. Com view transform Raw, os pixels do C++ passam inalterados. Interpolação *Closest* para imagens ≤ 200 px de largura (resolução do compute), *Linear* acima. |

O plano tapa tudo o que estiver **atrás** dele. Os objetos `BH_*` ficam a
menos de 10 unidades da câmera, então aparecem **na frente**. Para render só da
imagem C++, desligue as coleções `BH_*` no render (ícone de câmera no
Outliner). Para compor, deixe visíveis só os objetos que quer sobrepor.

### 10.5 Sequências de imagens

- Com **Frames** = N > 1, o painel grava `stem_0000.png … stem_{N−1}.png`, um
  por frame da cena a partir de `frame_start`, cada um com a pose animada da
  câmera — qualquer preset (turntable, dolly, espiral…) sai sincronizado com o
  viewport.
- Na linha de comando, `bh_render_cpu --frames N` gera a mesma nomenclatura com
  azimute az₀ + 2π·k/N (k = 0…N−1; o último frame não repete o primeiro) — a
  mesma amostragem do `TURNTABLE` do addon.
- **Import background** / **Import as plane** aceitam qualquer arquivo
  `stem_NNNN.png|bmp`: os irmãos numerados viram uma `SEQUENCE` que começa no
  `frame_start` da cena (arquivo `stem_0000` ↔ `frame_start`), e o `frame_end`
  passa a `frame_start + N − 1`.
- Detalhes em [`docs/ANIMACAO.md`](docs/ANIMACAO.md).

### 10.6 Opções que o painel não expõe

O painel oferece os três modos (`legacy`, `relativistic`, `blackbody`) e as
estrelas. Para exposição, gama, fração de Eddington, sentido de rotação do
disco, threads ou limite de passos, rode o binário no terminal com o JSON
exportado e importe o resultado:

```bash
./build/scientific/bh_render_cpu --scene bh_render/frame_scene_params.json \
    --out bh_render/bb.png --mode blackbody --width 640 --height 360 \
    --supersample 2 --stars
# outras: --exposure E --gamma G --mdot-edd F --spin-sign ±1 --threads T --max-steps N --quiet
```

Depois: **Import background** ou **Import as plane** → `bh_render/bb.png`.

### 10.7 Erros comuns

| Mensagem | Causa / solução |
|----------|-----------------|
| `bh_render_cpu not found…` | compile (10.1) e preencha **Renderer path** |
| `bh_render_cpu exit N: …` | as 3 últimas linhas do stderr vêm no relatório; rode o mesmo comando no terminal |
| `timed out after Ns` | aumente **Timeout (s)** ou reduza resolução / supersample / frames |
| `Rendered … (PPM not loadable…)` | use `.png` ou `.bmp` na **Saída** |

## 11. JSON ↔ C++ (`black_hole.scene_params/v1`)

Sub-painel **JSON ↔ C++**: **Export Scene JSON (path)** grava em *Caminho
JSON*; **Export Scene Params JSON** / **Import Scene Params JSON** abrem o
seletor de arquivos.

**Blender → C++** — os dois binários carregam o JSON exportado:

```bash
./build/scientific/bh_render_cpu --scene scene_params.json --out out.png
./build/gl/BlackHole3D --scene scene_params.json                 # interativo
./build/gl/BlackHole3D --scene scene_params.json --relativistic --capture gl.png
```

O C++ lê só um conjunto restrito (`black_hole`, `camera`, `disk`, `gravity`,
`objects`) e ignora o resto (`guides`, `render`, `animation`, …). O addon não
exporta `objects` nem `gravity`, então o `BlackHole3D` mantém os 3 objetos
legados e Gravity OFF. `camera.fov_y_deg` e `camera.target_m` valem nos dois
binários (no `BlackHole3D`, desde a 0.8.0, inclusive a projeção da grade).

**C++ → Blender** — **Import Scene Params JSON** aceita arquivos escritos pelo
C++ (`save_scene_params_json`) ou à mão, como
`examples/scene_relativistic_showcase.json`. Chaves desconhecidas, `objects` e
`gravity` são ignorados; `animation.mode = "turntable_azimuth"` (0.7.x) vira
`TURNTABLE`. O import só preenche o painel: rode **Build Full Scene** em
seguida.

Campo a campo: [`examples/scene_params_example.md`](examples/scene_params_example.md).
Fluxo completo: [`docs/PIPELINE_C++_BLENDER.md`](docs/PIPELINE_C++_BLENDER.md).

## 12. Uso headless (sem interface)

Com `blender` no `PATH` (pacote da distro, por exemplo; o Steam normalmente
não põe o binário no `PATH`):

```bash
# .blend com cena + anéis-guia + turntable (192 frames)
blender --background --factory-startup \
    --python blender/scripts/build_scene_headless.py -- --out "$PWD/build/black_hole_sim.blend"
#   --no-bake    sem animação de câmera
#   --no-guides  sem anéis
#   sem --out    → blender/examples/output/black_hole_sim.blend

make blender-scene     # o mesmo comando, grava build/black_hole_sim.blend
```

O script parte de cena vazia, registra o addon sem instalar, usa a pose
**padrão do C++** (elevação π/2, imprime o WARNING da seção 5) e liga a grade
multi-plano (XZ + XY + YZ).

**Render still headless (receita da imagem da galeria).** Salve como
`render_el125.py` e rode a partir da raiz do repo:

```python
import os, sys
import bpy

sys.path.insert(0, os.path.abspath("blender/addons"))
import black_hole_bridge as bhb
from black_hole_bridge import constants as C, guides, scene_builder

bpy.ops.wm.read_factory_settings(use_empty=True)   # sem o cubo padrão
bhb.register()

s = bpy.context.scene.bh_bridge
s.camera_elevation = 1.25                           # fora da laje do disco
scene_builder.build_full_scene(bpy.context)
guides.build_guides(scene_builder.ensure_collections()[C.COLL_GUIDES])

sc = bpy.context.scene
sc.cycles.samples = 24
sc.cycles.use_denoising = False                     # Ubuntu: sem OpenImageDenoise
for vl in sc.view_layers:
    vl.cycles.use_denoising = False
sc.render.resolution_x, sc.render.resolution_y = 400, 300
sc.render.filepath = os.path.abspath("blender_el125.png")
bpy.ops.render.render(write_still=True)
```

```bash
blender --background --factory-startup --python render_el125.py
```

No Blender 4.0.2 deste container, essa receita reproduz
`docs/images/blender_el125_cycles.png` (400×300, Cycles CPU, ≈ 0.8 s).

> **Ubuntu / pacote `apt`:** o Blender da distro vem sem OpenImageDenoise.
> Desligue o denoising (`scene.cycles.use_denoising` e o de cada view layer)
> em renders Cycles headless.

Checagens que não precisam do Blender:

```bash
python3 tests/test_blender_addon_compile.py --root .   # py_compile de todos os módulos
python3 tests/test_blender_addon_parity.py --root .    # constantes/fórmulas vs include/black_hole/*.hpp
python3 blender/scripts/package_addon.py --check       # zips atualizados?
make package-addon                                     # regenera os dois zips (determinístico)
```

## 13. Estilo (travado)

| Elemento | Valor |
|----------|-------|
| Disco | `vec3(1.0, r, 0.2)` com alpha `r` → `(1, r, 0.2)·r` sobre preto |
| Raios do disco | interno 2.2 rs, externo 5.2 rs (padrão legado) |
| Grade | `vec4(0.5, 0.5, 0.5, 0.7)` (`grid.frag`) |
| Guias | mesmo cinza, alpha 0.5 |
| Horizonte / fundo | preto |

Sem paleta neon/cyberpunk. Referência: [`docs/ESTILO_VISUAL_SIMULACAO.md`](../docs/ESTILO_VISUAL_SIMULACAO.md).

---

## EN (short)

Install `blender/addons/black_hole_bridge.zip` (Preferences → Add-ons →
Install… on 4.0/4.1; **Install from Disk** on 4.2+, manifest included) →
enable → Sidebar **Black Hole** → **Build Full Scene**. C++ is Y-up, Blender
Z-up: `cpp_to_blender` maps (x, y, z) → (x, −z, y), a proper rotation, so the
framing matches the C++ camera. Colour parity: "Raw" view transform, disk
emission `(1, r, 0.2)·r`, `disk_glow` 1.0. The locked C++ default camera sits
inside the disk (WARNING): use **Elevation 1.25** for clean Blender renders.
Guides: photon sphere 1.5 rs, b_c 2.598 rs, ISCO 3 rs. Presets TURNTABLE /
ELEVATION_SWEEP / DOLLY / SPIRAL with LINEAR / SINE easing. Render bridge:
`make build` → set **Renderer path** → **Render via C++ (CPU)** → camera
background (viewport) or **Import as plane** (renders). Headless:
`make blender-scene`; disable Cycles denoising on Ubuntu's Blender.
