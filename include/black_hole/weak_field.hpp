#pragma once
// Analytic weak-field light deflection for Schwarzschild (GR leading order).
// α ≈ 4 G M / (c² b) = 2 rs / b  for impact parameter b ≫ rs.

#include <cmath>

namespace bh {
namespace weak_field {

/// Deflection angle in radians: α = 2 * rs / b  (= 4GM/(c²b)).
inline double deflection_angle(double rs, double impact_parameter_b) {
    return 2.0 * rs / impact_parameter_b;
}

/// Same formula written explicitly in SI constants (returns radians).
inline double deflection_angle_si(double G, double M, double c, double b) {
    return 4.0 * G * M / (c * c * b);
}

/// Ratio α(b1)/α(b2) should equal b2/b1 (exact 1/b scaling of the weak-field formula).
inline double deflection_scale_ratio(double rs, double b1, double b2) {
    return deflection_angle(rs, b1) / deflection_angle(rs, b2);
}

}  // namespace weak_field
}  // namespace bh
