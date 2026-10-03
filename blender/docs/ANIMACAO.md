# Animação no Blender — presets, easing e disco (addon 0.8.0)

Guia canônico do addon: [`../INSTALAR_E_RODAR.md`](../INSTALAR_E_RODAR.md).
Este arquivo detalha as fórmulas e a ligação com as sequências do C++.

![Cena do addon renderizada no Cycles, elevação 1.25](../../docs/images/blender_el125_cycles.png)

*Pose recomendada para animar no Blender: Elevação 1.25 rad. A pose padrão do
C++ (π/2, raio 4.997 rs) põe a câmera dentro da laje do disco.*

## Fluxo

1. **Cena → Build Full Scene** (com **Parâmetros → Elevação = 1.25** para sair
   do disco).
2. **Câmara / animação** → **Preset** + **Easing** (+ campos do preset) →
   **Bake Camera Animation**. Isso define FPS, `frame_start = 1` e
   `frame_end = FPS × Duração` (mínimo 2) e keyframa `location` da
   `BH_OrbitCamera` com interpolação linear. A câmera mira a origem pelo Track
   To.
3. *(Opcional)* **Disco (spin) → Bake Disk Spin**, depois do passo 2 (usa o
   intervalo de frames da cena).
4. **Render → Render Animation**. Para renders headless no Ubuntu, desligue o
   denoising do Cycles (o pacote da distro vem sem OpenImageDenoise).

## Presets — `camera_path_sample(mode, t, …, t_azimuth)`, t ∈ [0, 1]

| Preset | Raio | Azimute | Elevação |
|--------|------|---------|----------|
| `TURNTABLE` (padrão) | r | az₀ + 2π·t_az | el |
| `ELEVATION_SWEEP` | r | az₀ | el_ini + t·(el_fim − el_ini) |
| `DOLLY` | r + t·(r_fim − r) | az₀ | el |
| `SPIRAL` | r | az₀ + 2π·t_az | el_ini + t·(el_fim − el_ini) |

No bake de N frames (`bake_camera_path`), o frame k (k = 0…N−1) usa
t = k/(N−1) para raio e elevação (os valores finais são atingidos exatamente) e
t_az = k/N para o azimute periódico: o último frame **não** repete o primeiro,
então um turntable em loop não engasga — a mesma amostragem do
`bh_render_cpu --frames N`.

- `r`, `az₀`, `el` = **Raio câmara**, **Azimute**, **Elevação** do painel
  Parâmetros; `el_ini`/`el_fim` = **Elevação início/fim** (aparecem com
  `ELEVATION_SWEEP` e `SPIRAL`); `r_fim` = **Raio final (× rs)** (aparece com
  `DOLLY`).
- Elevação sempre limitada a (0.01, π − 0.01), como em `Camera::position()`.
- A posição é calculada com a fórmula do C++ (Y-up) e convertida por
  `cpp_to_blender`, (x, y, z) → (x, −z, y). Visto de cima no Blender (+Z), o
  azimute crescente gira no sentido horário.

### Easing — `ease_t(t, easing)`

| Easing | Função | Efeito |
|--------|--------|--------|
| `LINEAR` | t | velocidade constante |
| `SINE` | 0.5 − 0.5·cos(πt) | começa e termina devagar; t = 0 e t = 1 preservados |

O easing é aplicado antes do preset, a t e a t_az:
`camera_path_sample(mode, ease_t(t), …, ease_t(t_az))`.

### Padrões (`constants.py`)

| Campo | Valor |
|-------|-------|
| FPS | 24 |
| Duração | 8 s → 192 frames |
| Elevação início / fim | 0.35 rad / π − 0.35 rad |
| Raio final (DOLLY) | 12 rs |

### Dicas de pose

- `TURNTABLE` com a elevação padrão π/2 gira **dentro** do disco no Blender.
  Use 1.25 (≈ 18° acima do plano) ou um raio > 5.2 rs.
- `ELEVATION_SWEEP` / `SPIRAL` de 0.35 a π − 0.35 atravessam o plano do disco
  no meio da animação. Nesse instante a câmera passa pela laje se o raio
  estiver entre os fatores interno e externo.
- `DOLLY` até 12 rs afasta a câmera para fora do anel 2.2–5.2 rs.

## Disco: spin, turbulência, brilho

**Disco (spin) → Bake Disk Spin** reconstrói `BH_Disk_Mat` com um nó *Mapping*
(`BH_DiskSpin`, tipo POINT) entre o UV e o gradiente e keyframa
`Location.x` (UV.u = ângulo/2π):

| Frame | `Location.x` |
|-------|--------------|
| `frame_start` | 0 |
| `frame_end` | **Voltas do disco** × (n − 1)/n, com n = `frame_end − frame_start + 1` (padrão 1.0 volta) |

Interpolação e extrapolação **lineares**: `frame_end + 1` cairia exatamente em
**Voltas do disco** (≡ `frame_start` para voltas inteiras), então o loop não
repete imagem.

- **Turbulência (brilho)**, de 0 a 1 (padrão **0 = desligado**): ruído sem
  costura amostrado em (cos 2πu, sin 2πu, 3v) multiplica a **força** da emissão
  por um fator em [1 − t, 1 + t]. O gradiente é radialmente simétrico, então
  sem turbulência o spin é invisível.
- **Brilho do disco** (`disk_glow`, padrão 1.0 = paridade com o OpenGL sob o
  view transform Raw): multiplica a força da emissão; > 1 é artístico.
- A cor é sempre `(1, r, 0.2)·r`. Nenhum desses controles muda o matiz.
- **Rebuild Disk** restaura o material sem spin.

## Sequências vindas do C++

`bh_render_cpu --frames N` grava `stem_0000.png … stem_{N−1}.png` varrendo o
azimute a partir de az₀:

| | C++ `--frames N` | Blender `TURNTABLE` com N frames (easing `LINEAR`) |
|---|------------------|---------------------------------|
| Azimute do frame k (k = 0…N−1) | az₀ + 2π·k/N | az₀ + 2π·k/N |
| Último frame | az₀ + 2π·(N−1)/N (loop sem repetição) | az₀ + 2π·(N−1)/N (loop sem repetição) |
| Sentido | azimute crescente | azimute crescente |

Com easing `LINEAR` os dois coincidem quadro a quadro. **Render C++ → Blender**
com **Frames** = N não usa a varredura da CLI: chama o renderer uma vez por
frame da cena (`frame_start … frame_start + N − 1`, `--frames 1`) com a pose
**animada** da `BH_OrbitCamera`, então qualquer preset e qualquer easing saem
sincronizados com o viewport.

**Import background** / **Import as plane** detectam o sufixo `_NNNN`,
carregam a sequência inteira (`image.source = 'SEQUENCE'`) a partir do
`frame_start` da cena (arquivo `stem_0000` ↔ `frame_start`) e ajustam
`frame_end = frame_start + N − 1`.
O background da câmera só aparece no viewport; para renderizar a sequência,
use **Import as plane**.

Exemplo, 96 frames relativísticos a 1.25 rad:

```bash
./build/scientific/bh_render_cpu --scene scene_params.json --out bh_render/turn.png \
    --mode relativistic --elevation 1.25 --frames 96 --width 640 --height 480
# → bh_render/turn_0000.png … turn_0095.png ; no Blender: Import as plane → turn_0000.png
```

## Estilo

Disco `(1, r, 0.2)·r`, grade e guias no cinza `(0.5, 0.5, 0.5, ·)`, fundo
preto. Os presets mudam só o caminho da câmera. No Cycles não há lensing:
física e lensing ficam no C++. Ver
[`../examples/animation_style_guide.md`](../examples/animation_style_guide.md).
