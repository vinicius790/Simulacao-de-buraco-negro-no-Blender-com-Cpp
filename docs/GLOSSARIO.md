# Glossário

Termos usados neste repositório e na documentação científica adjacente. Definições curtas; não substituem um livro
de relatividade. Derivações completas: [FISICA_SCHWARZSCHILD.md](FISICA_SCHWARZSCHILD.md) e
[DISCO_RELATIVISTICO.md](DISCO_RELATIVISTICO.md).

Convenção: unidades geométricas \(G=c=1\), \(r_s=2M\). Os caminhos científicos trabalham com rs = 1.

## rs (raio de Schwarzschild)

Raio característico de um buraco negro sem rotação: \( r_s = 2GM/c^2 \). No shader baseline aparece como
`SagA_rs = 1.269e10` (valor histórico do projeto para Sgr A*; a massa \(8.54\times10^{36}\) kg dá
\(1.268388\times10^{10}\) m). O horizonte de eventos, na coordenada de Schwarzschild usual, fica em \( r = r_s \).

## Photon sphere (esfera de fótons)

Órbita circular **nula** instável, em \( r = 1.5\, r_s = 3M \) em Schwarzschild. Luz pode "pairar" ali por um
tempo; perturbações a levam ao horizonte ou ao infinito. Dentro dela não existem órbitas circulares de matéria.
Os caminhos científicos usam a esfera de fótons no critério de escape provado; o addon Blender desenha um anel-guia
em 1.5 rs.

## ISCO (Innermost Stable Circular Orbit)

Órbita circular **estável** mais interna para partículas massivas. Em Schwarzschild,
\( r_{\mathrm{ISCO}} = 6M = 3\, r_s \) (não "6 rs": isso confunde \(r_s\) com \(M\)). Vem de
\(\mathrm{d}\tilde E/\mathrm{d}r=0\) para a energia específica \(\tilde E=(1-2M/r)/\sqrt{1-3M/r}\). Na ISCO:
\(\tilde E=\sqrt{8/9}\), \(\tilde L=2\sqrt3\,M\), velocidade local \(0.5\,c\). Entre 1.5 rs e 3 rs há órbitas
circulares, mas instáveis. É a borda interna de um disco fino (torque nulo) e define a eficiência
\(\eta=1-\sqrt{8/9}\approx5.72\,\%\). O disco **visual** do baseline (2.2–5.2 rs) começa **dentro** da ISCO; a cena
`examples/scene_relativistic_showcase.json` começa nela (3–12 rs). Código: `orbits.hpp`, `isco_radius`.

## b_c (parâmetro de impacto crítico)

\( b_c = 3\sqrt3\,M = \tfrac{3\sqrt3}{2}r_s \approx 2.598\, r_s \). Um raio de luz vindo do infinito com parâmetro de
impacto \(b=L/E<b_c\) é capturado; com \(b>b_c\) é defletido e escapa. Sai do máximo do potencial efetivo
\(V=f/r^2\), que fica na esfera de fótons. Verificado em `test_critical_capture_escape` (\(b=2.0,\,2.5\) capturados;
\(2.7,\,3.2\) escapam). Código: `escape::critical_impact_parameter`.

## Sombra (shadow)

Região escura do céu de onde nenhum raio vindo do observador escapa: a "silhueta" do buraco negro. Para um
observador estático em \(r_o\), o raio angular é \(\sin\alpha=b_c\sqrt{1-r_s/r_o}/r_o\) (Synge 1966); para
\(r_o\gg r_s\), \(\alpha\approx b_c/r_o\). Na câmera default (4.997 rs): 0.4836 rad = 27.7°. É maior que o horizonte
porque a luz é curvada. Código: `escape::shadow_angular_radius`; teste: `test_traced_shadow_edge_vs_synge`.

## Fator de redshift g

Razão \(g=\nu_{\rm obs}/\nu_{\rm emit}\) entre a frequência recebida e a emitida. Para matéria do disco em órbita
circular, com um raio traçado para trás (câmera → disco):
\(g_\infty=1/\big(u^t(1+\Omega L_{\rm axis}/E)\big)\), em que \(L_{\rm axis}=(\vec r\times\mathrm{d}\vec r/\mathrm{d}\lambda)\cdot\hat n\).
Para um emissor parado, \(g=\sqrt{1-r_s/r}\) (redshift gravitacional puro). \(g<1\): redshift (lado que se afasta);
\(g>1\): blueshift (lado que se aproxima). A câmera estática a \(r_{\rm obs}\) mede \(g_\infty/\sqrt{1-r_s/r_{\rm obs}}\).
Código: `redshift.hpp`.

## Doppler beaming (feixe relativístico)

Reforço do brilho do lado que se aproxima e escurecimento do lado que se afasta. Pela invariância de
\(I_\nu/\nu^3\) (Liouville), a intensidade bolométrica muda como \(I_{\rm obs}=g^4I_{\rm emit}\) e a temperatura de
cor como \(T_{\rm obs}=g\,T_{\rm emit}\). Na ISCO, os extremos dão um contraste de 81:1. É o que deixa um lado do disco
muito mais claro em `--mode relativistic` e `BlackHole3D --relativistic`. Código: `redshift::intensity_boost`.

## Page–Thorne (fluxo de)

Fluxo radiado por um disco de acreção fino, estacionário e opticamente espesso em relatividade geral, com torque
nulo na ISCO (Page & Thorne 1974; Novikov & Thorne 1973). Para Schwarzschild tem forma fechada
\(F=\tfrac{3GM\dot M}{8\pi r^3}R(x)\), \(x=\sqrt{r/M}\); é zero na ISCO e tem pico em \(r\approx9.55\,M\approx4.78\,r_s\).
Longe do buraco tende ao disco newtoniano. A temperatura efetiva é \(T=(F/\sigma)^{1/4}\). Só a cinemática e o
perfil de fluxo são modelados (sem estrutura vertical). Código: `disk_emission.hpp`; teste: `test_page_thorne_flux`.

## Eddington (luminosidade / taxa de acreção)

Luminosidade em que a pressão de radiação sobre elétrons (espalhamento Thomson) equilibra a gravidade sobre
prótons: \(L_{\rm Edd}=4\pi GMm_pc/\sigma_T\). A taxa de acreção correspondente é \(\dot M_{\rm Edd}=L_{\rm Edd}/(\eta c^2)\).
Para a massa de Sgr A* do projeto: \(L_{\rm Edd}=5.40\times10^{37}\) W; a 1 % de Eddington (default de
`--mdot-edd`), \(\dot M=1.05\times10^{20}\) kg/s e o pico de \(T_{\rm eff}\) é ≈ 86 700 K. É um cenário didático: o
Sgr A* real acreta muito abaixo de Eddington.

## Plano orbital (carta plana)

Em Schwarzschild, por simetria esférica, cada geodésica fica num plano que passa pelo centro (normal
\(\vec r\times\mathrm{d}\vec r/\mathrm{d}\lambda\)). Integrar cada raio nesse plano, girado para \(\theta=\pi/2\), elimina
as singularidades de coordenada dos polos (\(\sin\theta\to0\)) sem nenhuma aproximação. Usado pelo renderer CPU e
pelo shader científico. Código: `planar_geodesic.hpp`.

## RK4, RK45 / Dormand–Prince

**RK4**: Runge–Kutta clássico de 4ª ordem, 4 avaliações do lado direito por passo (o caminho legado da GPU é Euler,
1 avaliação, apesar do nome histórico). **RK45 (Dormand–Prince 5(4))**: método embutido que calcula soluções de 5ª e
4ª ordem no mesmo passo; a diferença estima o erro e controla o tamanho do passo (adaptativo). Neste repo, o RK45
chega ao mesmo estado que um RK4 fino com 120 passos em vez de 3000. Código: `integrator_rk4.hpp`,
`integrator_rk45.hpp`.

## Golden image (imagem de referência)

Imagem tomada como referência para testes de regressão: um render novo é comparado pixel a pixel com ela, com
tolerâncias. Aqui, a referência da GPU é o render do `bh_render_cpu` (`double`) na mesma cena: `gpu_cpu_agreement`
compara as duas com `tools/image_diff.py` (fração de pixels ruins ≤ \(2\times10^{-3}\), erro médio ≤ 0.5/255).

## llvmpipe (Mesa)

Driver OpenGL **por software** do Mesa (rasteriza na CPU via LLVM). Expõe OpenGL 4.5, o que basta para os compute
shaders (4.3+). Com `xvfb-run` (servidor X virtual) permite rodar `BlackHole3D --capture` sem GPU e sem monitor,
em containers e na CI. Todas as afirmações de GPU deste repositório se referem ao llvmpipe; não há medições em GPUs
de hardware nem números de desempenho de GPU.

## Nested geodesics (geodésicas aninhadas / em lote)

No contexto deste renderer: muitos raios (geodésicas nulas) integrados em paralelo — tipicamente um por pixel (ou
por texel da textura de compute). "Nested" aqui refere-se ao padrão de lançar/integrar famílias de geodésicas a
partir da câmera, não a um formalismo matemático especial além da equação geodésica.

## UBO (Uniform Buffer Object)

Bloco de memória OpenGL (`GL_UNIFORM_BUFFER`) partilhado entre CPU e shader. Em `black_hole.cpp` há UBOs de câmera,
disco e objetos, com layouts `std140` verificados por `static_assert`; o modo científico acrescenta `SciParams`
(binding 4, 32 bytes), conferido campo a campo contra o GLSL pelo teste `shader_contract`.

## Compute shader

Estágio GLSL (`geodesic.comp`, `#version 430`) que corre na GPU fora do pipeline clássico vértice/fragmento. Aqui
integra geodésicas e escreve uma textura via `imageStore`; o pass fullscreen amostra essa textura. Requer OpenGL
4.3+ (ou equivalente). O modo científico usa outro compute shader, `shaders/geodesic_scientific.comp`.

## Lensing (lenteamento gravitacional)

Deflexão da luz pelo campo gravitacional. No projeto: raios da câmera são curvados perto do buraco negro,
produzindo anéis, a imagem do lado distante do disco arqueada sobre a sombra e a própria sombra. Há um caminho 2D
(`2D_lensing.cpp`), o caminho 3D GPU legado, o GPU científico e o renderer CPU. É visualização educativa — não um
substituto de pipelines de imagem do EHT. O Blender não faz lensing; ele importa os renders do `bh_render_cpu`.

## Outros termos úteis neste repo

| Termo | Significado local |
|-------|-------------------|
| Baseline | Caminho histórico default + constantes protegidas pelo teste de invariantes |
| `legacyEulerStep` | Integrador de um estágio (Euler) no shader; nome honesto após o upgrade |
| Modo científico | `--scientific` / `--relativistic` no `BlackHole3D`, `bh_render_cpu`, biblioteca `bh_scientific` |
| Observador estático | Observador parado em \(r\) fixo; é o modelo da câmera nos caminhos científicos |
| Escape provado | Raio saindo, além da esfera de fótons e além da cena: não volta (substitui `ESCAPE_R = 1e30` nos caminhos novos) |
| Hit-test contínuo | Interseção no **segmento** entre duas amostras (segmento–plano, segmento–esfera), não só no ponto |
| Reinhard | Tonemap \(c/(1+c)\) que comprime brilho HDR para \([0,1)\) |
| Sag A* / Sgr A* | Buraco negro supermassivo no centro da Via Láctea; inspiração do `SagA` no código |
| Kerr | Buraco negro em rotação. **Não simulado**: só raios analíticos em `kerr_analytic.hpp` |
| GRMHD | Magnetohidrodinâmica relativística geral — **fora** do escopo deste repositório |
