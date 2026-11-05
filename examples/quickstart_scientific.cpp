// Minimal scientific-lib demo (no OpenGL). Build target: quickstart_scientific

#include <iostream>

#include "black_hole/relativistic.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"
#include "black_hole/weak_field.hpp"

int main() {
    const double M_kg = bh::units::SAGITTARIUS_A_MASS_KG;
    const double rs = black_hole::schwarzschild_radius(M_kg);
    const double M = bh::units::mass_parameter_from_rs(rs);

    std::cout << "Sagittarius A* (legacy mass mapping)\n";
    std::cout << "  M_SI     = " << M_kg << " kg\n";
    std::cout << "  rs       = " << rs << " m\n";
    std::cout << "  M_geom   = " << M << " m  (rs/2)\n";
    std::cout << "  r_photon = " << bh::photon_sphere_radius(rs)
              << " m  (1.5 rs)\n";
    std::cout << "  r_ISCO   = " << bh::isco_radius(rs) << " m  (3 rs = 6 M)\n";

    const double b = 100.0 * rs;
    const double alpha = black_hole::light_deflection_angle(M_kg, b);
    std::cout << "Weak-field deflection at b = 100 rs:\n";
    const double deg = alpha * 180.0 / 3.141592653589793;
    std::cout << "  alpha ≈ " << alpha << " rad (" << deg << " deg)\n";

    const double r_obs = bh::isco_radius(rs);
    const double gamma = black_hole::time_dilation_factor(M_kg, r_obs);
    std::cout << "Gravitational time dilation at ISCO (r = 3 rs):\n";
    std::cout << "  dtau/dt ≈ " << gamma << "\n";
    return 0;
}
