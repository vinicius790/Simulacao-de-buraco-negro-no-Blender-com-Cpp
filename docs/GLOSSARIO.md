# Glossário

Termos usados neste repositório e na documentação científica adjacente. Definições curtas; não substituem um livro de relatividade.

## rs (raio de Schwarzschild)

Raio característico de um buraco negro sem rotação: \( r_s = 2GM/c^2 \). No shader baseline aparece como `SagA_rs = 1.269e10` (valor histórico do projeto para Sgr A*). Horizonte de eventos na coordenada de Schwarzschild usual coincide com \( r = r_s \).

## Photon sphere (esfera de fótons)

Órbita circular nula instável (para Schwarzschild, em \( r = 1.5\, r_s \)). Luz pode “pairar” temporariamente; perturbações caem no horizonte ou escapam para o infinito. O baseline **visualiza** captura/escape com o integrador legado; não afirma ter medido a photon sphere com precisão científica.

## ISCO (Innermost Stable Circular Orbit)

Órbita circular estável mais interna para partículas massivas. Em Schwarzschild, \( r_{\mathrm{ISCO}} = 6\, r_s \) (órbitas prógradas, spin zero). Relevância futura para discos de acreção “realistas”; o disco do baseline usa raios empíricos `2.2 r_s`–`5.2 r_s` só para aparência.

## Nested geodesics (geodésicas aninhadas / em lote)

No contexto deste renderer: muitos raios (geodésicas nulas) integrados em paralelo — tipicamente um por pixel (ou por texel da textura de compute). “Nested” aqui refere-se ao padrão de lançar/integrar famílias de geodésicas a partir da câmara, não a um formalismo matemático especial além da equação geodésica.

## UBO (Uniform Buffer Object)

Bloco de memória OpenGL (`GL_UNIFORM_BUFFER`) partilhado entre CPU e shader. Em `black_hole.cpp` há UBOs de câmara, disco e objetos, com layouts `std140` verificados por `static_assert`. O compute shader lê esses bindings para parâmetros por frame.

## Compute shader

Estágio GLSL (`geodesic.comp`, `#version 430`) que corre na GPU fora do pipeline clássico vértice/fragmento. Aqui integra geodésicas e escreve uma textura via `imageStore`; o pass fullscreen amostra essa textura. Requer OpenGL 4.3+ (ou equivalente).

## Lensing (lenteamento gravitacional)

Deflexão da luz pelo campo gravitacional. No projeto: raios da câmara são curvados perto do buraco negro, produzindo anéis, distorção do disco e silhueta. Há um caminho 2D (`2D_lensing.cpp`) e o caminho 3D GPU. É visualização educativa/aproximada no baseline — não um substituto de pipelines de imagem EHT.

## Outros termos úteis neste repo

| Termo | Significado local |
|-------|-------------------|
| Baseline | Caminho histórico default + constantes protegidas pelo teste de invariantes |
| `legacyEulerStep` | Integrador de um estágio (Euler) no shader; nome honesto após o upgrade |
| Sag A* / Sgr A* | Buraco negro supermassivo no centro da Via Láctea; inspiração do `SagA` no código |
| GRMHD | Magnetohidrodinâmica relativista geral — **fora** do escopo deste repositório |
| Hit-test | Teste de interseção raio–geometria (disco, horizonte); no científico, preferir contínuo |
