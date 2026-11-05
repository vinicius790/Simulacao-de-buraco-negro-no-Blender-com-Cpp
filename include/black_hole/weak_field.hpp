#ifndef INCLUDE_BLACK_HOLE_WEAK_FIELD_HPP_
#define INCLUDE_BLACK_HOLE_WEAK_FIELD_HPP_
// Analytic weak-field light deflection for Schwarzschild (GR leading order).
// α ≈ 4 G M / (c² b) = 2 rs / b  for impact parameter b ≫ rs.

#include <cmath>

#include "black_hole/relativistic.hpp"

namespace bh {
namespace weak_field {

/// Deflection angle in radians: α = 2 * rs / b  (= 4GM/(c²b)).
inline double deflection_angle(double rs, double impact_parameter_b) {
    return 2.0 * rs / impact_parameter_b;
}

/// Deflection angle in radians computed from mass and impact parameter (SI).
/// Delegates to black_hole::light_deflection_angle.
inline double deflection_angle_si(double mass_kg, double impact_m) {
    return black_hole::light_deflection_angle(mass_kg, impact_m);
}

/// Same formula written explicitly with arbitrary constants (returns radians).
/// When default physical constants match, delegates to
/// black_hole::light_deflection_angle.
inline double deflection_angle_si(double G, double M, double c, double b) {
    if (G == black_hole::G && c == black_hole::c) {
        return black_hole::light_deflection_angle(M, b);
    }
    return 4.0 * G * M / (c * c * b);
}

/// Ratio α(b1)/α(b2) should equal b2/b1 (exact 1/b scaling).
inline double deflection_scale_ratio(double rs, double b1, double b2) {
    return deflection_angle(rs, b1) / deflection_angle(rs, b2);
}

}  // namespace weak_field
}  // namespace bh

#endif  // INCLUDE_BLACK_HOLE_WEAK_FIELD_HPP_
