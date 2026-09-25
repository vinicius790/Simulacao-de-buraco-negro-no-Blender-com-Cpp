// Minimal scientific-lib demo (no OpenGL). Build target: quickstart_scientific

#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"
#include "black_hole/weak_field.hpp"

#include <iostream>

int main() {
    const double rs = bh::units::sagittarius_a_rs_si();
    const double M = bh::units::mass_parameter_from_rs(rs);

    std::cout << "Sagittarius A* (legacy mass mapping)\n";
    std::cout << "  M_SI     = " << bh::units::SAGITTARIUS_A_MASS_KG << " kg\n";
    std::cout << "  rs       = " << rs << " m\n";
    std::cout << "  M_geom   = " << M << " m  (rs/2)\n";
    std::cout << "  r_photon = " << bh::photon_sphere_radius(rs) << " m  (1.5 rs)\n";
    std::cout << "  r_ISCO   = " << bh::isco_radius(rs) << " m  (3 rs = 6 M)\n";

    const double b = 100.0 * rs;
    const double alpha = bh::weak_field::deflection_angle(rs, b);
    std::cout << "Weak-field deflection at b = 100 rs:\n";
    std::cout << "  alpha ≈ " << alpha << " rad (" << (alpha * 180.0 / 3.141592653589793)
              << " deg)\n";
    return 0;
}
