# `bh_render_cpu` — manual do renderizador CPU de referência

Renderizador **headless, determinístico e multithread** da cena Schwarzschild
(Sagittarius A\*). Não precisa de OpenGL, GPU, driver nem display. Fonte:
[`tools/bh_render_cpu.cpp`](../tools/bh_render_cpu.cpp) (CLI) sobre
[`include/black_hole/cpu_renderer.hpp`](../include/black_hole/cpu_renderer.hpp) /
[`src/scientific/cpu_renderer.cpp`](../src/scientific/cpu_renderer.cpp) (biblioteca
`bh_scientific`).

![Cena showcase renderizada pelo bh_render_cpu (modo relativistic, estrelas)](images/showcase_relativistic.png)

*`images/showcase_relativistic.png`: `examples/scene_relativistic_showcase.json`,
640×360, `--mode relativistic --supersample 2 --stars`.*

> O caminho de render **padrão** do projeto continua sendo `BlackHole3D` sem flags
> (`geodesic.comp`, `legacyEulerStep`). O `bh_render_cpu` é uma ferramenta à parte:
> não altera nem substitui o baseline.

---

## 1. Para que serve

| Uso | Como |
|-----|------|
| Referência em `double` para o shader científico da GPU | `gpu_cpu_agreement` compara `BlackHole3D --scientific/--relativistic --capture` com `bh_render_cpu` pixel a pixel |
| Testes golden/estruturais sem GPU | `test_cpu_render` (CTest `cpu_render`) e os testes `render_cli_*` |
| Imagens científicas (Doppler, redshift gravitacional, corpo negro Page–Thorne) | `--mode relativistic` / `--mode blackbody` |
| Ponte C++ → Blender | o addon roda o binário e importa o PNG (ou a sequência) como fundo da câmera ou plano emissivo |
| Galeria e CI | `make render`, job `scientific-headless` (artefato `cpu-renders`) |

## 2. Build

O alvo existe nas duas árvores (com ou sem OpenGL):

```bash
make build                        # build/scientific/bh_render_cpu (sem OpenGL)
# ou
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON
cmake --build build/scientific --target bh_render_cpu
```

Na árvore completa (`make build-gl`) o binário fica em `build/gl/bh_render_cpu`,
ao lado de `BlackHole3D`.

## 3. Início rápido

```bash
B=build/scientific/bh_render_cpu
mkdir -p build/renders            # o renderer NÃO cria diretórios

# Pose clássica, paleta legada (elevação 1,25 evita o artefato da vista inicial do baseline)
$B --out build/renders/legacy_el125.png --width 640 --height 480 --elevation 1.25

# Doppler + redshift gravitacional + beaming g⁴, integrador adaptativo
$B --out build/renders/rel.png --width 640 --height 480 --elevation 1.25 \
   --mode relativistic --integrator rk45

# Cena showcase (disco 3–12 rs, câmera a 18 rs), supersampling 2×2, estrelas
$B --scene examples/scene_relativistic_showcase.json --mode relativistic \
   --width 640 --height 360 --supersample 2 --stars --out build/renders/showcase_relativistic.png

# Corpo negro Page–Thorne a 1 % de Eddington (padrão); mude com --mdot-edd
$B --scene examples/scene_relativistic_showcase.json --mode blackbody \
   --width 640 --height 360 --supersample 2 --stars --out build/renders/showcase_blackbody.png

# Turntable de 120 frames: turn_0000.png … turn_0119.png (azimute varre 2π)
$B --out build/renders/turn.png --width 320 --height 240 --elevation 1.25 --frames 120 --quiet
```

`make render` gera `legacy_el125.png`, `showcase_relativistic.png` e
`showcase_blackbody.png` em `build/renders/`.

## 4. CLI completa

```text
bh_render_cpu [--scene scene.json] --out image.png [--width W] [--height H]
              [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]
              [--azimuth RAD] [--elevation RAD] [--radius-rs X] [--fov-y-deg D]
              [--frames N] [--azimuth-turns T] [--elevation-end RAD]
              [--threads T] [--supersample S]
              [--exposure E] [--gamma G] [--mdot-edd F] [--spin-sign +1|-1]
              [--stars] [--max-steps N] [--quiet] [--help]
```

| Flag | Padrão | Faixa aceita | Descrição |
|------|--------|--------------|-----------|
| `--out PATH` | *(obrigatório)* | — | Arquivo de saída. A extensão escolhe o formato: `.png`, `.bmp` ou `.ppm`. O diretório precisa existir. |
| `--scene PATH` | cena default C++ | — | JSON `black_hole.scene_params/v1` (câmera, disco, objetos, massa/rs). Sem `--scene`: `make_default_scene_params()` = disco 2,2–5,2 rs, câmera 6,34194e10 m (≈ 4,997 rs), azimute 0, elevação π/2, FOV 60°, as 3 esferas default. |
| `--width W` / `--height H` | 200 / 150 | inteiro 1–16384 | Resolução (a mesma da textura de compute legada). |
| `--mode` | `legacy` | `legacy`, `relativistic` (alias `doppler`), `blackbody` | Ver §7. |
| `--integrator` | `rk4` | `rk4`, `rk45` | RK4 com passo geométrico ou Dormand–Prince 5(4) adaptativo. Ver §8. |
| `--azimuth RAD` | da cena (0) | real finito | Azimute da câmera orbital. |
| `--elevation RAD` | da cena (π/2) | real finito | Ângulo polar medido a partir de +Y (π/2 = plano do disco). |
| `--radius-rs X` | da cena (≈ 4,997) | > 0 | Raio da órbita da câmera em unidades de rs (multiplica `r_s_m` da cena). |
| `--fov-y-deg D` | da cena (60) | 0 < D < 180 | FOV vertical em graus. |
| `--frames N` | 1 | inteiro 1–100000 | N > 1 grava uma sequência (§5). |
| `--azimuth-turns T` | 1 | real finito | Com `--frames`: número de voltas completas do azimute (0 = azimute fixo). |
| `--elevation-end RAD` | — | real finito | Com `--frames`: a elevação vai linearmente da inicial até esta; o frame N−1 chega exatamente nela. |
| `--threads T` | 0 (= todos os núcleos) | inteiro 0–1024 | Linhas distribuídas entre threads. O resultado é **bit a bit idêntico** para qualquer T. |
| `--supersample S` | 1 | inteiro 1–16 | Grade regular S×S de sub-pixels por pixel (média). |
| `--exposure E` | 2.0 | ≥ 0 | Multiplicador antes do tonemap Reinhard (`relativistic` / `blackbody`). |
| `--gamma G` | 1.0 | > 0 | Gamma de saída (1.0 = linear, como a textura legada). |
| `--mdot-edd F` | 0.01 | > 0 | Taxa de acreção como fração de Eddington (`blackbody`). |
| `--spin-sign ±1` | +1 | só `+1` / `-1` | Sentido de rotação do disco (+1 = anti-horário em torno de +Y). |
| `--stars` | desligado | — | Campo estelar procedural determinístico para raios que escapam (deixa a lente visível). |
| `--max-steps N` | 20000 | inteiro 1–1e8 | Limite de passos por raio; raios que o atingem saem pretos e contam em `step_limit_rays`. |
| `--quiet` | — | — | Silencia o log por frame no stderr. A linha JSON continua no stdout. |
| `--help`, `-h` | — | — | Ajuda; código 0. |

**Precedência:** primeiro carrega `--scene` (se houver); depois `--azimuth`,
`--elevation`, `--radius-rs` e `--fov-y-deg` sobrescrevem a câmera da cena.

**Valores numéricos** são lidos de forma estrita: a string inteira precisa ser
consumida (`strtol` para inteiros, `strtod` para reais), sem estouro, e reais
precisam ser finitos. Inteiros não aceitam notação científica nem fração
(`--width 640`, não `6.4e2`). Valor inválido ou fora da faixa → mensagem
`bad value for --flag: '…'` e código 2.

## 5. Saída, orientação e sequências

- **Formatos:** PNG (deflate *stored*, sem dependências, CRC32/Adler32 corretos),
  BMP 24 bits e PPM P6, todos escritos por
  [`image_io.hpp`](../include/black_hole/image_io.hpp). O Blender carrega PNG e BMP.
- **Orientação:** linha 0 da imagem = **topo** do quadro, igual à tela do
  `BlackHole3D`. A captura `BlackHole3D --capture` vira as linhas da textura para
  a mesma convenção, por isso a comparação GPU ↔ CPU é 1:1.
- **Cor:** RGB linear em [0, 1] quantizado para 8 bits (com gamma, se `--gamma ≠ 1`).

### Nomes de sequência (`--frames N`, N > 1)

O arquivo `--out` vira um *stem*; cada frame recebe o sufixo `_%04d` antes da
extensão:

| `--out` | Arquivos gravados |
|---------|-------------------|
| `build/renders/orbit.png` | `orbit_0000.png`, `orbit_0001.png`, …, `orbit_{N−1}.png` |
| `build/renders/seq.bmp` | `seq_0000.bmp`, … |
| `build/renders/noext` | `noext_0000.png`, … (sem extensão → `.png`) |

Com `--frames 1` (padrão) o arquivo é exatamente o `--out`, que precisa ter
extensão `.png`, `.bmp` ou `.ppm`.

Pose do frame `f` (0 ≤ f < N):

```text
azimute(f)  = az₀ + 2π · T · f / N          (T = --azimuth-turns; frame N repetiria o frame 0 → loop sem emenda)
elevação(f) = el₀ + (el_fim − el₀) · f / (N − 1)   só com --elevation-end; senão fica em el₀
```

`az₀`/`el₀` são o azimute/elevação após a precedência do §4. É o formato que o
Blender reconhece como *image sequence* (`stem_NNNN.ext`).

## 6. Linha JSON de resumo

Ao terminar, o programa imprime **uma** linha JSON no stdout (o log por frame vai
para o stderr). Exemplo real (200×150, cena default, 4 threads):

```json
{"frames":1,"width":200,"height":150,"mode":"legacy","integrator":"rk4","shadow_fraction":0.482933,"disk_fraction":0.232133,"object_fraction":0.00573333,"escaped_fraction":0.2792,"step_limit_rays":0,"mean_steps":183.433,"min_g":0.393837,"max_g":1.58306,"seconds":0.334002,"out":"build/renders/def.png"}
```

| Campo | Tipo | Significado |
|-------|------|-------------|
| `frames` | int | Número de frames gravados (N). |
| `width`, `height` | int | Resolução de cada frame. |
| `mode` | string | `legacy`, `relativistic` ou `blackbody`. |
| `integrator` | string | `rk4` ou `rk45`. |
| `shadow_fraction` | float | Fração dos raios capturados pelo horizonte (pixels da sombra). |
| `disk_fraction` | float | Fração dos raios que terminaram no disco. |
| `object_fraction` | float | Fração dos raios que terminaram numa esfera da cena. |
| `escaped_fraction` | float | Fração dos raios que escaparam (critério por prova, §8). |
| `step_limit_rays` | int | **Contagem** (não fração) de raios que esgotaram `--max-steps`. Deve ser 0. |
| `mean_steps` | float | Média de passos de integração por raio. |
| `min_g`, `max_g` | float | Menor/maior fator g = ν_obs/ν_emit entre os raios que acertaram o disco (calculado em qualquer modo); 0 se nenhum raio acertou o disco. |
| `seconds` | float | Tempo de parede total (todos os frames, incluindo gravação). |
| `out` | string | O valor de `--out` como foi passado, escapado para JSON (aspas, barras invertidas de caminhos Windows, caracteres de controle). Para sequências é o *stem* + extensão, não cada arquivo. |

As frações e `mean_steps` são **agregadas** sobre todos os raios de todos os
frames: com supersampling, cada sub-pixel é um raio (raios = W·H·S²·N). Todo raio
cai em exatamente uma categoria: `shadow_fraction + disk_fraction + object_fraction
+ escaped_fraction + step_limit_rays / raios = 1`.

### Códigos de saída

| Código | Quando |
|--------|--------|
| 0 | Sucesso (inclui `--help`). |
| 1 | Falha ao carregar `--scene`; falha ao gravar (extensão não suportada, diretório inexistente, sem permissão). |
| 2 | Erro de argumento: flag desconhecida, valor ausente, valor inválido ou fora da faixa (§4), `--mode`/`--integrator` inválidos, `--spin-sign` ≠ ±1, `--out` ausente. |

## 7. Modos de cor

Em todos os modos: horizonte e limite de passos → preto; raio que escapa → preto
(ou estrela com `--stars`); esfera da cena → cor RGB da esfera × Lambert visto da
câmera (ambiente 0,1) × alpha da esfera (como o `vec4(cor, a)` do shader legado
misturado sobre preto). Só o disco muda entre os modos.
`r_norm = ρ / r_externo` (ρ = raio cilíndrico do cruzamento com o plano do disco).

| Modo | Cor do disco | Observações |
|------|--------------|-------------|
| `legacy` | `(1, r_norm, 0.2) · r_norm` | Exatamente o pixel de `geodesic.comp` (`vec4(1, r, 0.2, r)` misturado sobre preto). Paleta âmbar travada. |
| `relativistic` | `Reinhard_E( (1, clamp(r_norm·g), 0.2) · r_norm · g⁴ )` | Paleta legada × beaming g⁴; blueshift clareia (verde sobe), redshift avermelha. Lado que se aproxima mais brilhante; `--spin-sign -1` troca o lado. |
| `blackbody` | `Reinhard_E( RGB(T_obs) · 4 · g⁴ · (T_emit/T_pico)⁴ )` | T_emit = T_eff de Page–Thorne para a massa e Ṁ (`--mdot-edd`); T_obs = g·T_emit; RGB pelo locus planckiano (Kim et al. 2002). **Zero dentro da ISCO** (3 rs). T_pico é amostrado entre 3 e 10 rs. |

O fator g é o de [`redshift.hpp`](../include/black_hole/redshift.hpp) para matéria
em órbita circular, medido pela **câmera estática** em r_cam:
`g = g_∞ / √f(r_cam)`, `g_∞ = 1 / (u^t (1 + Ω·L_eixo/E))`.

Para Sgr A\* a 1 % de Eddington o pico de T_eff é ≈ 86 700 K (em r ≈ 4,78 rs):
o modo `blackbody` sai **branco-azulado** (pico no UV), não âmbar. A paleta âmbar
é só do modo `legacy`. Ver [`images/showcase_blackbody.png`](images/showcase_blackbody.png)
e [`images/blackbody_el100.png`](images/blackbody_el100.png).

## 8. Integração e testes de colisão

| Etapa | Implementação | Diferença em relação ao baseline GPU legado |
|-------|---------------|----------------------------------------------|
| Unidades | geométricas, rs = 1 (a cena em metros é dividida por `r_s_m`) | legado em metros, `float` |
| Semente do raio | direção no referencial ortonormal da **câmera estática** | legado usa a direção como velocidade coordenada |
| Carta | plano orbital de cada raio, θ ≡ π/2 ([`planar_geodesic.hpp`](../include/black_hole/planar_geodesic.hpp)) — exato por simetria esférica, sem singularidade nos polos | carta esférica global |
| Integrador | `rk4`: RK4 clássico com `dλ = clamp(0,02·r, 0,005, teto)` rs; `rk45`: Dormand–Prince 5(4), tolerância 1e-8, passo limitado pelo mesmo teto. Teto = 2 rs até 1,05·r_externo (igual à GPU); além disso, `max(2, 0,02·r)`, para que raios rumo a objetos distantes ou ao limite de escape não esgotem `--max-steps` | Euler de 1 estágio, `D_LAMBDA = 1e7`, 60 000 passos |
| Disco | cruzamento **contínuo** segmento–plano com ponto interpolado ([`hit_testing.hpp`](../include/black_hole/hit_testing.hpp)) | amostragem pontual |
| Horizonte e esferas | segmento–esfera contínuo | amostragem pontual |
| Escape | por prova: raio saindo (dr > 0), além da esfera de fótons e além do limite da cena ([`escape.hpp`](../include/black_hole/escape.hpp)) | `ESCAPE_R = 1e30` |
| Câmera no plano do disco (\|y\| < 1e-6·r, a mesma tolerância relativa do shader GPU; cobre também a elevação π/2 arredondada para `float`) | o primeiro segmento **não** conta como cruzamento | metade superior amarela na vista inicial ([BASELINE.md](BASELINE.md#descoberta-artefato-da-vista-inicial-documentado-não-alterado)) |
| Determinismo | linhas distribuídas por contador atômico; cada pixel só depende da sua pose | — |

O limite da cena é `max(r_externo, |c_i| + R_i) · 1,05 + 0,5` (rs), em que `c_i` e
`R_i` são o centro e o raio de cada esfera. A esfera-marcador do buraco negro (a
esfera preta default) é ignorada, porque o teste de horizonte desenha o buraco de
forma exata. O critério é `bh::is_black_hole_marker` (`scene_params.hpp`): centrada
no buraco **e** (raio a 5 % do rs da cena, ou do 2GM/c² da massa do próprio objeto,
ou massa ≥ metade da do buraco). O `BlackHole3D --scientific` usa o mesmo predicado.

## 9. Desempenho medido

Medições deste repositório com os binários de `build/gl/` (container Linux com
4 CPUs, compartilhado com outros processos; os tempos variam com a carga da
máquina e com a pose). Tempo = campo `seconds` da linha JSON.

| Caso | Resolução | Threads | `seconds` (2 execuções) | `mean_steps` |
|------|-----------|---------|-------------------------|--------------|
| `legacy`, cena default (az 0, el π/2, 4,997 rs) | 200×150 | 4 | 0,33–0,34 | 183 |
| `legacy`, cena default | 400×300 | 4 | 1,31–1,41 | 184 |
| `legacy`, elevação 1,25 | 400×300 | 4 | 0,76 | 111 |
| `legacy`, elevação 1,25 | 400×300 | 1 | 3,07–3,14 | 111 |
| `legacy` + `rk45`, elevação 1,25 | 400×300 | 4 | 0,64–0,80 | 31,6 |
| `relativistic`, elevação 1,25 | 400×300 | 4 | 0,83–0,87 | 111 |
| `blackbody`, elevação 1,00 | 400×300 | 4 | 0,93–0,98 | 121 |
| showcase `relativistic`, `--supersample 2 --stars` | 640×360 | 4 | 5,6–7,4 | 111 |

Registro anterior (STATUS.md): 400×300 `legacy`, 4 threads, ≈ 0,5–1,2 s e
≈ 77–152 passos RK4 por raio em média, conforme a pose. Nenhum caso acima atingiu
o limite de passos (`step_limit_rays = 0`).

Leitura: o RK45 dá ≈ 3,5× menos passos por raio que o RK4 geométrico, mas cada
passo custa mais avaliações; o tempo de parede fica parecido. As 1 e 4 threads
produzem a mesma imagem (`image_diff` → `max_error 0`).

## 10. Uso em testes golden

| Teste CTest | O que faz com o renderer |
|-------------|--------------------------|
| `cpu_render` (`tests/test_cpu_render.cpp`) | Chama a API direto: estrutura da imagem, paleta âmbar `(1, r, 0.2)·r`, cor de objeto multiplicada pelo alpha, simetria espelhada esquerda/direita, determinismo entre 1 e N threads, assimetria Doppler e inversão com o spin, corpo negro (inclusive independente do render anterior), câmera perto do polo e no eixo polar, elevação π/2 em `float32` sem o artefato de meio quadro, marcador do BH descartado para qualquer massa, objeto a 1e5 rs sem esgotar passos, área da sombra vs b_c analítico (≤ 8 %), borda da sombra traçada vs fórmula de Synge em 5 distâncias, PNG/CRC32/Adler32 (inclusive codificação concorrente). |
| `render_cli_smoke` | `--scene examples/scene_params_example.json --width 48 --height 36` grava um PNG. |
| `render_cli_contract` (`tests/test_render_cli.py`) | Contrato de ponta a ponta da CLI: toda flag é aceita e muda (ou mantém) a imagem como documentado; valores inválidos saem com código 2 e mensagem com o nome da flag; a linha JSON é válida mesmo com aspas/barras invertidas no `--out`; PNG/BMP/PPM legíveis; nomes `stem_NNNN.ext` (também com pontos em nomes de diretório); `--elevation-end`/`--azimuth-turns` reproduzem exatamente os renders de frame único nas pontas; `--help` lista toda flag aceita. |
| `render_cli_rejects_bad_spin` | `--spin-sign 0.5` precisa falhar (`WILL_FAIL`). |
| `render_cli_relativistic_sequence` | `--mode relativistic --integrator rk45 --frames 2` grava `smoke_rel_0000.bmp`, `smoke_rel_0001.bmp`. |
| `legacy_golden` (`tests/test_legacy_golden.py`, só na árvore GL) | Não usa o `bh_render_cpu`: compara `BlackHole3D` sem flags (`--capture`) com `docs/images/legacy_default_gpu.png` — exato no driver gravado em `legacy_default_gpu.json`, tolerância pequena nos demais — e confere que `--scene examples/scene_params_example.json` dá o mesmo frame que sem flags. |
| `gpu_cpu_agreement` (`tests/test_gpu_cpu_agreement.py`, só na árvore GL) | Para 6 cenas × 2 modos (3 poses de câmera, uma com FOV 40° e alvo fora da origem, uma com metade da massa e os objetos default, uma com `"objects": []` e disco 3–12 rs vista de az ≈ π) roda `BlackHole3D --scene S --scientific\|--relativistic --capture gpu.png` e `bh_render_cpu --scene S --mode legacy\|relativistic --out cpu.png` (200×150) e compara com `image_diff.compare` (`--pixel-tol 24`). Gate: `bad_fraction ≤ 0,002` e erro médio ≤ 0,5/255. Retorna 77 (SKIP) sem display/`xvfb-run` ou sem contexto OpenGL 4.3. |

Resultado medido do `gpu_cpu_agreement` (Mesa llvmpipe, OpenGL 4.5) nesta revisão:

| Cena | Modo | Erro médio (/255) | Erro máx. | `bad_fraction` |
|------|------|-------------------|-----------|----------------|
| az 0, el 1,25, 4,997 rs | `--scientific` | 0,058 | 1 | 0 |
| az 0, el 1,25, 4,997 rs | `--relativistic` | 0,00001 | 1 | 0 |
| az 0,7, el 0,60, 8e10 m | `--scientific` | 0,070 | 3 | 0 |
| az 0,7, el 0,60, 8e10 m | `--relativistic` | 0,0001 | 1 | 0 |
| az 2,1, el 2,00, 4,997 rs | `--scientific` | 0,062 | 246 | 6,7e-5 (2 de 30 000 pixels) |
| az 2,1, el 2,00, 4,997 rs | `--relativistic` | 0,006 | 173 | 6,7e-5 (2 de 30 000 pixels) |
| az 0,3, el 1,30, 9e10 m, FOV 40°, alvo (0, 5e9, 0) m | `--scientific` | 0,060 | 117 | 3,3e-5 (1 pixel) |
| az 0,3, el 1,30, 9e10 m, FOV 40°, alvo (0, 5e9, 0) m | `--relativistic` | 0,005 | 179 | 3,3e-5 (1 pixel) |
| az 0, el 1,25, metade da massa (r_s = 6,345e9 m), objetos default | `--scientific` | 0,035 | 1 | 0 |
| az 0, el 1,25, metade da massa (r_s = 6,345e9 m), objetos default | `--relativistic` | 0,00002 | 1 | 0 |
| az ≈ π, el 1,40, 2,2842e11 m (18 rs), disco 3–12 rs, `"objects": []` | `--scientific` | 0,042 | 1 | 0 |
| az ≈ π, el 1,40, 2,2842e11 m (18 rs), disco 3–12 rs, `"objects": []` | `--relativistic` | 0,00001 | 1 | 0 |

Os poucos pixels fora da tolerância ficam em bordas (sombra/disco), onde float32
(GPU) e double (CPU) podem decidir de lados diferentes. Registro anterior
(antes das correções de revisão da 0.8.0): erro máx. 1/255 (científico) e bytes idênticos (relativístico) na
pose az 0/el 1,25; 2–4 pixels de borda nas outras (fração ≤ 1,3e-4); erro médio
< 0,08/255. Medições de GPU só existem para **Mesa llvmpipe**.

Reproduzir à mão:

```bash
cd build/gl
xvfb-run -a ./BlackHole3D --scene ../../examples/scene_params_example.json --scientific --capture /tmp/gpu.png
./bh_render_cpu --scene ../../examples/scene_params_example.json --mode legacy --out /tmp/cpu.png --quiet
python3 ../../tools/image_diff.py /tmp/gpu.png /tmp/cpu.png --max-bad-fraction 0.002 --heatmap /tmp/diff.ppm
```

## 11. Uso pela ponte do Blender

O addon (`blender/addons/black_hole_bridge/render_bridge.py`, operador
**Render via C++ (CPU)** no painel **Black Hole**) usa o renderer nas duas direções:

1. **Blender → C++:** grava `<saída>_scene_params.json` (schema
   `black_hole.scene_params/v1`, referencial C++ Y-up) ao lado da imagem de saída.
   Para **cada frame** pedido, posiciona a cena nesse frame (`scene.frame_set`), lê
   a pose **real** da câmera animada (`blender_to_cpp` → raio/azimute/elevação) e
   o FOV vertical da câmera, e chama o binário uma vez com `--frames 1`
   (`build_render_command`):
   `--scene … --out stem_NNNN.png --width … --height … --mode … --integrator …
   --azimuth … --elevation … --radius-rs … --fov-y-deg … --frames 1 --supersample … [--stars]`.
   Uma imagem única usa o frame atual; uma sequência de N usa os frames
   `frame_start … frame_start + N − 1`. Assim o fundo C++ acompanha quadro a
   quadro qualquer preset/easing do Blender (não usa a varredura interna
   `--frames N` da CLI).
2. **C++ → Blender:** cada execução tem timeout configurável (900 s por padrão);
   código ≠ 0 cancela com as últimas linhas do stderr. A linha JSON do último frame
   vai para `Scene["bh_last_render_summary"]`, com `frames` = N, `scene_frames`
   (primeiro e último frame da cena) e `command` (o argv do primeiro frame). O
   PNG/BMP é importado como **fundo da câmera** `BH_OrbitCamera` ou como **plano
   emissivo** voltado para a câmera (`plane_size_for_fov`); arquivos
   `stem_NNNN.ext` viram uma *image sequence* sincronizada com o intervalo de
   frames da cena.

Busca do binário (`find_renderer`): campo **Renderer path** do painel → candidatos
`build/scientific`, `build/sci`, `build/default`, `build` (a partir da pasta do
`.blend`, de até 3 pais, do diretório atual e da raiz do repositório) → `PATH`.
Como `build/gl` não está na lista, aponte o **Renderer path** para
`build/gl/bh_render_cpu` se usar só a árvore GL.

Nesta revisão, o addon expõe `legacy`/`relativistic`/`blackbody`, `rk4`/`rk45`,
frames, supersample e estrelas; `--exposure`, `--gamma`, `--mdot-edd` e
`--spin-sign` ficam só na CLI. Os helpers puros
(`build_render_command`, `parse_render_summary`, `expected_frame_paths`,
`find_sequence_files`) são testados sem Blender por `blender_addon_parity`.
Detalhes: [BLENDER_ADAPTER.md](BLENDER_ADAPTER.md) e
[`blender/docs/PIPELINE_C++_BLENDER.md`](../blender/docs/PIPELINE_C++_BLENDER.md).

## 12. `tools/image_diff.py` — comparação de imagens

Comparador só com a biblioteca padrão do Python: PNG 8 bits RGB/RGBA (filtros
0–4; o canal alpha é ignorado), PPM P6 binário (maxval 255, comentários `#` no
cabeçalho aceitos) e BMP 24 bits (de baixo para cima ou de cima para baixo).

```bash
python3 tools/image_diff.py A B [--pixel-tol T] [--max-bad-fraction F] [--heatmap out.ppm]
```

| Opção | Padrão | Efeito |
|-------|--------|--------|
| `A`, `B` | — | Imagens a comparar; precisam ter o mesmo tamanho (senão `ValueError`). |
| `--pixel-tol T` | 24 | Um pixel é "ruim" se a maior diferença entre canais passar de T (0–255). |
| `--max-bad-fraction F` | — | Se `bad_fraction > F`, imprime `FAIL: …` no stderr e sai com código 1. |
| `--heatmap out.ppm` | — | Mapa de diferenças em PPM: cinza = diferença dentro da tolerância (brilho = diferença); vermelho = pixel ruim (intensidade 4× a diferença). |

Saída (stdout, uma linha JSON):

```json
{"width": 200, "height": 150, "mean_abs_error": 0.0, "max_error": 0, "pixel_tol": 24, "bad_fraction": 0.0}
```

| Campo | Significado |
|-------|-------------|
| `mean_abs_error` | Média de \|A − B\| sobre todos os canais, em unidades de 8 bits (0–255). |
| `max_error` | Maior diferença de canal encontrada. |
| `bad_fraction` | Fração de pixels com diferença > `pixel_tol`. |

Também pode ser importado (`image_diff.compare(a, b, pixel_tol, heatmap)`), como
faz o `gpu_cpu_agreement`.

### Sequências → GIF (`tools/frames_to_gif.py`)

```bash
python3 tools/frames_to_gif.py "build/renders/turn_*.png" -o turn.gif --fps 20 --pingpong
```

Também só stdlib: paleta global de 256 cores por *median cut* (sem cintilação
entre frames), GIF89a com loop NETSCAPE2.0, `--pingpong` acrescenta os frames em
ordem inversa. Coberto pelo teste `tools_selftest` (`tests/test_tools.py`).

## 13. Limites honestos

- **Só Schwarzschild.** Spin/Kerr não é simulado (o `--spin-sign` é o sentido de
  rotação do *disco*, não do buraco negro).
- **Sem transferência radiativa / GRMHD.** O disco é uma superfície fina e
  opaca; `blackbody` é emissão local de corpo negro com fluxo de Page–Thorne,
  deslocada por g.
- **O disco legado (2,2–5,2 rs) começa dentro da ISCO.** Nos modos `legacy` e
  `relativistic` ele é desenhado como está (paleta travada); o `blackbody` não
  emite entre 2,2 e 3 rs.
- **`bh_render_cpu` não é o `BlackHole3D`.** Ele reproduz a *paleta* e a
  *geometria de cena* do baseline, mas com a física corrigida (RK4/RK45,
  colisões contínuas, escape por prova); não reproduz o Euler legado.
- **Nenhum tempo de GPU real é afirmado.** Os números de desempenho acima são de
  CPU; as comparações de GPU usam apenas Mesa llvmpipe.

Ver também: [MODO_CIENTIFICO.md](MODO_CIENTIFICO.md), [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md),
[ARQUITETURA.md](ARQUITETURA.md), [VALIDATION_STATUS.md](VALIDATION_STATUS.md).
