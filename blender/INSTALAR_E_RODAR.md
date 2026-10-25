# Instalar e rodar — Black Hole Bridge (Steam Blender 4.x)

Addon **funcional** (não é só documentação): cria horizonte preto, disco âmbar
`vec3(1,r,0.2)`, grade cinza warping e câmara em órbita — estilo travado em
`docs/ESTILO_VISUAL_SIMULACAO.md`. Física / lensing GR ficam no C++.

## Pré-requisito

- **Blender 4.x** já instalado via **Steam** (máquina do utilizador).
- Pasta do add-on neste repo:
  `blender/addons/black_hole_bridge/`
  ou o zip: `blender/addons/black_hole_bridge.zip`

## 1. Abrir o Blender pelo Steam

1. Abra a **Steam**.
2. Biblioteca → **Blender** → **Jogar** / Launch.
3. Confirme na splash / About que a versão é **4.0+**.

## 2. Instalar o add-on

### Opção A — pasta (recomendado em desenvolvimento)

1. No Blender: **Edit → Preferences → Add-ons**.
2. Clique **Install…** / **Install from Disk** (texto varia com a 4.x).
3. Navegue até a pasta do repositório:
   `…/black_hole/blender/addons/black_hole_bridge`
   - Em alguns builds Steam, selecione o **zip** (Opção B) em vez da pasta.
4. Ative a caixa **Black Hole Bridge**.
5. (Opcional) Save Preferences.

### Opção B — zip

1. Use o ficheiro `blender/addons/black_hole_bridge.zip` (gerado no repo).
2. **Edit → Preferences → Add-ons → Install from Disk** → escolha o `.zip`.
3. Ative **Black Hole Bridge**.

Se o add-on não aparecer: Preferences → Add-ons → filtro “Black Hole”;
ou Refresh. Em Blender 4.2+ extensions, “Install from Disk” aceita o zip com
`blender_manifest.toml` + `__init__.py` na raiz do arquivo.

## 3. Usar (um clique)

1. Janela **3D Viewport** → tecla **N** (sidebar) → separador **Black Hole**.
2. Clique **Build Full Scene**.
   - Collections: `BH_Core`, `BH_Disk`, `BH_Grid`, `BH_Camera`, `BH_Lights`
   - Horizonte preto (`rs_geo=1`), disco 2.2–5.2 rs âmbar, grade cinza warped,
     câmara a olhar a origem.
3. Ajuste Mass / disk factors / grid / câmera nos subpainéis se quiser.
4. Clique **Bake Orbit Animation** → turntable (azimuth) com keyframes.
5. **Timeline** → Play, ou **Render → Render Animation**.

Shading sugerido no viewport: **Material Preview** ou **Rendered** (Cycles/EEVEE).
Fundo quase preto; disco emissivo; grade em wire cinza.

## 4. Exportar JSON para o C++

No painel **JSON I/O**:

- **Export Scene JSON (path)** ou file browser Export.
- Schema alinhado a DiskUBO / Camera (`scene_params` v1).
- O binário C++ **ainda não carrega** este JSON automaticamente — o ficheiro é
  a ponte de parâmetros / documentação viva.

## 5. Desinstalar

Preferences → Add-ons → Black Hole Bridge → Remove / desative.

## Estilo (não mudar)

| Elemento | Valor |
|----------|--------|
| Disco | `vec3(1.0, r, 0.2)` — âmbar (geodesic.comp) |
| Grade | `vec4(0.5, 0.5, 0.5, 0.7)` (grid.frag) |
| Horizonte | preto, raio = rs |
| Fundo | quase preto |
| Disco raios | inner 2.2 rs, outer 5.2 rs |

**Não** usar paleta neon/cyberpunk. Lensing real = C++; Blender = aproximação artística.

## Headless (opcional)

Só se tiver `blender` no PATH (não é o caso típico do Steam):

```bash
blender --background --python blender/scripts/build_scene_headless.py
```

No Steam, o executável costuma estar sob o steamapps; o fluxo normal é a UI
acima (Build Full Scene + Bake Orbit).

## EN (short)

Steam → launch Blender 4.x → Preferences → Add-ons → Install from Disk →
`black_hole_bridge/` or `.zip` → enable → Sidebar **Black Hole** →
**Build Full Scene** → **Bake Orbit Animation**. Style locked to OpenGL sim.
