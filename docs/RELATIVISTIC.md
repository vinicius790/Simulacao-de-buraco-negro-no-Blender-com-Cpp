# Utilidades Relativísticas / Relativistic Utilities

Documentação das utilidades relativísticas implementadas em `include/black_hole/relativistic.hpp` e seus testes unitários em `tests/test_relativistic.cpp`.

---

## 1. Visão Geral / Overview

O módulo relativístico (`black_hole/relativistic.hpp`) faz parte do núcleo científico de referência do projeto (`bh_scientific`). Ele fornece utilitários header-only com precisão dupla (`double`) e unidades no Sistema Internacional (SI) para cálculo de propriedades fundamentais do espaço-tempo de Schwarzschild.

- **Header:** [`include/black_hole/relativistic.hpp`](../include/black_hole/relativistic.hpp)
- **Namespace:** `black_hole`
- **Dependências:** Apenas `<cmath>` (independente de OpenGL, GLEW ou GLFW)
- **Constantes físicas:**
  - $G = 6{,}67430 \times 10^{-11} \ \mathrm{m^3 \cdot kg^{-1} \cdot s^{-2}}$ (constante gravitacional)
  - $c = 299\,792\,458 \ \mathrm{m/s}$ (velocidade da luz no vácuo)

---

## 2. Funções e Formulação Física / Functions and Physics Formulation

### 2.1 Raio de Schwarzschild (`schwarzschild_radius`)

Calcula o raio do horizonte de eventos para uma massa esfericamente simétrica e estática (métrica de Schwarzschild):

$$r_s = \frac{2 G M}{c^2}$$

- **Assinatura:** `inline double schwarzschild_radius(double mass_kg)`
- **Parâmetros:** `mass_kg` — Massa do corpo gravitacional em quilogramas ($M$).
- **Retorno:** Raio de Schwarzschild em metros ($r_s$).
- **Exemplo físico:** Para $10 \ M_\odot \approx 1{,}98847 \times 10^{31} \ \mathrm{kg}$, $r_s \approx 29{,}53 \ \mathrm{km}$ ($\sim 2{,}95 \ \mathrm{km}$ por massa solar).

---

### 2.2 Ângulo de Deflexão Gravitacional de Luz (`light_deflection_angle`)

Calcula a deflexão angular de um raio de luz (fóton) passando a uma distância de aproximação / parâmetro de impacto $b$ na aproximação de campo fraco (Einstein weak-field limit, onde $b \gg r_s$):

$$\delta = \frac{4 G M}{c^2 b} = \frac{2 r_s}{b}$$

- **Assinatura:** `inline double light_deflection_angle(double mass_kg, double impact_m)`
- **Parâmetros:**
  - `mass_kg` — Massa do corpo defletor em quilogramas.
  - `impact_m` — Parâmetro de impacto $b$ em metros ($b > r_s$).
- **Retorno:** Ângulo de deflexão $\delta$ em radianos.
- **Validade:** Aplica-se para raios que passam suficientemente longe do horizonte. Para campos fortes próximos à esfera de fótons ($r = 1{,}5 \ r_s$), deve-se usar o integrador geodésico RK4 (`bh::rk4_step`).

---

### 2.3 Fator de Dilatação Temporal Gravitacional (`time_dilation_factor`)

Calcula o fator de dilatação gravitacional do tempo para um observador estático posicionado na coordenada radial $r$ de Schwarzschild:

$$\gamma = \frac{\mathrm{d}\tau}{\mathrm{d}t} = \sqrt{1 - \frac{r_s}{r}} = \sqrt{1 - \frac{2GM}{c^2 r}}$$

onde $\mathrm{d}\tau$ é o tempo próprio registrado pelo relógio do observador estacionário e $\mathrm{d}t$ é o tempo de coordenadas de um observador no infinito.

- **Assinatura:** `inline double time_dilation_factor(double mass_kg, double radius_m)`
- **Parâmetros:**
  - `mass_kg` — Massa do buraco negro em quilogramas.
  - `radius_m` — Coordenada radial $r$ onde o observador está localizado em metros.
- **Retorno:** Razão $\mathrm{d}\tau / \mathrm{d}t$ no intervalo $[0, 1)$.
- **Comportamento de fronteira:** Se $r \le r_s$ (no horizonte de eventos ou em seu interior), a função retorna `0.0` para sinalizar a quebra de coordenadas estáticas e horizonte de eventos.

---

## 3. Exemplos de Uso / Usage Examples

### 3.1 Exemplo em C++

```cpp
#include "black_hole/relativistic.hpp"
#include <iostream>
#include <iomanip>

int main() {
    // 10 Massas Solares
    constexpr double solar_mass_kg = 1.98847e30;
    const double mass_kg = 10.0 * solar_mass_kg;

    // 1. Raio de Schwarzschild
    const double rs = black_hole::schwarzschild_radius(mass_kg);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Massa: 10 M_sun\n";
    std::cout << "Raio de Schwarzschild: " << rs / 1e3 << " km\n";

    // 2. Deflexão de luz em b = 100 * rs
    const double impact_param = 100.0 * rs;
    const double deflection_rad = black_hole::light_deflection_angle(mass_kg, impact_param);
    std::cout << "Deflexao em b = 100 rs: " << deflection_rad << " rad\n";

    // 3. Dilatação temporal em r = 3 * rs (ISCO)
    const double r_isco = 3.0 * rs;
    const double dilation = black_hole::time_dilation_factor(mass_kg, r_isco);
    std::cout << "Fator dtau/dt no ISCO (3 rs): " << dilation << "\n";

    // 4. Verificação no horizonte (r <= rs)
    const double inside_factor = black_hole::time_dilation_factor(mass_kg, 0.5 * rs);
    std::cout << "Fator em r = 0.5 rs: " << inside_factor << " (horizonte ultrapassado)\n";

    return 0;
}
```

---

## 4. Testes Unitários / Unit Tests

Os testes unitários das rotinas relativísticas estão centralizados em [`tests/test_relativistic.cpp`](../tests/test_relativistic.cpp), implementados com **GoogleTest** (`gtest`) ou com um test runner embutido independente (zero dependências) quando o GTest não estiver instalado no ambiente.

### Casos de Teste

| Caso de Teste | Suíte | O que valida |
|---------------|-------|--------------|
| `SchwarzschildRadius` | `Relativistic` | Valida $r_s = 2GM/c^2$ com precisão $10^{-6}$ e confere o valor numérico para $10 \ M_\odot$ dentro de 1 km da referência ($\sim 29{,}53 \ \mathrm{km}$). |
| `LightDeflection` | `Relativistic` | Valida que $\delta = 2 r_s / b$ em $b = 10 r_s$ com precisão analítica ($10^{-12}$). |
| `TimeDilation` | `Relativistic` | Valida $\gamma = \sqrt{1 - r_s / r}$ em $r = 5 r_s$ com tolerância $10^{-12}$, e garante que para $r \le r_s$ (ex: $r = 0{,}5 r_s$) o retorno seja exatamente `0.0`. |

---

## 5. Como Executar os Testes com CTest / Running with CTest

Os testes são integrados ao CTest através do CMake (`BUILD_TESTING=ON`).

### 5.1 Configuração e Build

```bash
# Configura o build científico headless
cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON

# Compila os alvos de teste
cmake --build build/scientific
```

### 5.2 Execução com CTest

Para rodar todos os testes do projeto:

```bash
ctest --test-dir build/scientific --output-on-failure
```

Para rodar especificamente a suíte de testes relativísticos:

```bash
ctest --test-dir build/scientific -R test_relativistic --output-on-failure
```

Ou usando CMake Presets:

```bash
ctest --preset scientific
```

---

## 6. Integração com o Addon Blender / Blender Addon Integration

O addon `black_hole_bridge` integra diretamente as utilidades relativísticas à geração de cena e shaders no Blender:

- **Módulo Python Relativístico:** [`blender/addons/black_hole_bridge/relativistic.py`](../blender/addons/black_hole_bridge/relativistic.py)
  - Conecta-se às funções C-linkage exportadas (`bh_schwarzschild_radius`, `bh_light_deflection_angle`, `bh_time_dilation_factor`) via `ctypes`.
  - Fallback automático para implementação analítica de referência em Python caso a biblioteca compilada não esteja presente.
- **Nós de Shaders (`ShaderNodeValue`):**
  - `BH_Horizon_Mat`: nós `SchwarzschildRadius`, `TimeDilation`, `DeflectionAngle`.
  - `BH_Lensing_Mat`: nós `DeflectionAngle`, `SchwarzschildRadius`, `TimeDilation`, modulando o IOR efetivo do BSDF de Refração ($n_{\text{eff}} = 1.0 + \alpha \times 0.15$) e o redshift espectral.
  - `BH_Disk_Mat`: nós `SchwarzschildRadius`, `TimeDilation`, `DeflectionAngle` integrados ao gradiente âmbar estilizado.
- **Esfera de Lensing Gravitacional:**
  - Criação da malha `BH_LensingSphere` localizada na esfera de fótons ($1{,}5 \ r_s$).
- **Exportação JSON:**
  - Seção `"relativistic"` incluída nos parâmetros da cena com valores físicos em metros, ângulos de deflexão em radianos e graus, e fatores adimensionais.
- **Arquivo de Demonstração:**
  - Cena salva e validada em [`blender/examples/output/black_hole_sim.blend`](../blender/examples/output/black_hole_sim.blend).

