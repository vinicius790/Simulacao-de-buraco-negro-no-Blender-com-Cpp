#ifndef INCLUDE_BLACK_HOLE_SCHWARZSCHILD_HPP_
#define INCLUDE_BLACK_HOLE_SCHWARZSCHILD_HPP_
// Schwarzschild metric helpers in standard Schwarzschild coordinates
// (t, r, θ, φ). Signature (−,+,+,+). Horizon at r = rs.
// NOT Kerr — spin is unimplemented.

#include <cmath>

#include "black_hole/relativistic.hpp"

namespace bh {

struct MetricComponents {
    double g_tt;    // −f
    double g_rr;    // 1/f
    double g_thth;  // r^2
    double g_phph;  // r^2 sin^2 θ
    double f;       // 1 − rs/r
};

/// Schwarzschild radius rs = 2 G M / c^2 (or 2M in geometric units).
inline constexpr double schwarzschild_radius_from_M(double M) {
    return 2.0 * M;
}

/// Schwarzschild radius rs = 2 G M / c^2 for a given mass in SI units (metres).
/// Delegates to black_hole::schwarzschild_radius.
inline constexpr double schwarzschild_radius_si(double mass_kg) {
    return black_hole::schwarzschild_radius(mass_kg);
}

/// Deflection angle in radians computed from mass and impact parameter (SI).
/// Delegates to black_hole::light_deflection_angle.
inline constexpr double deflection_angle_si(double mass_kg, double impact_m) {
    return black_hole::light_deflection_angle(mass_kg, impact_m);
}

/// Unstable photon sphere for Schwarzschild: r_ph = 3 M = 1.5 rs.
inline constexpr double photon_sphere_radius(double rs) {
    return 1.5 * rs;
}

/// Innermost stable circular orbit (prograde): r_ISCO = 6 M = 3 rs.
/// Note: sometimes loosely written "6 rs" when rs is confused with M;
/// here rs is the true Schwarzschild radius, so the radius is 3 * rs.
inline constexpr double isco_radius(double rs) {
    return 3.0 * rs;
}

/// ISCO expressed in units of the geometric mass M (rs = 2M ⇒ ISCO = 6M).
inline constexpr double isco_in_units_of_M() {
    return 6.0;
}

/// Photon-sphere radius in units of M.
inline constexpr double photon_sphere_in_units_of_M() {
    return 3.0;
}

/// Redshift/lapse factor f(r) = 1 − rs/r. Returns 0 at/inside the horizon.
double lapse_factor(double r, double rs);

/// Gravitational time dilation factor dtau / dt = sqrt(1 - rs/r)
/// for a stationary observer. Returns 0.0 at or inside horizon (r <= rs).
double time_dilation_factor(double r, double rs);

/// Gravitational time dilation factor in SI units for a stationary
/// observer at radial coordinate r around a black hole of mass mass_kg.
/// Delegates to black_hole::time_dilation_factor.
double time_dilation_factor_si(double mass_kg, double radius_m);

/// Diagonal metric components at (r, θ). Undefined at r ≤ rs or sinθ ≈ 0.
MetricComponents metric_at(double r, double theta, double rs);

/// Inverse metric diagonal (raised indices) at (r, θ).
MetricComponents inverse_metric_at(double r, double theta, double rs);

}  // namespace bh

#endif  // INCLUDE_BLACK_HOLE_SCHWARZSCHILD_HPP_
