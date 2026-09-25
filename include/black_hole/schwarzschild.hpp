#pragma once
// Schwarzschild metric helpers in standard Schwarzschild coordinates (t, r, θ, φ).
// Signature (−,+,+,+). Horizon at r = rs. NOT Kerr — spin is unimplemented.

#include <cmath>

namespace bh {

struct MetricComponents {
    double g_tt;   // −f
    double g_rr;   // 1/f
    double g_thth; // r^2
    double g_phph; // r^2 sin^2 θ
    double f;      // 1 − rs/r
};

/// Schwarzschild radius rs = 2 G M / c^2 (or 2M in geometric units).
inline constexpr double schwarzschild_radius_from_M(double M) {
    return 2.0 * M;
}

/// Unstable photon sphere for Schwarzschild: r_ph = 3 M = 1.5 rs.
inline constexpr double photon_sphere_radius(double rs) {
    return 1.5 * rs;
}

/// Innermost stable circular orbit (prograde, Schwarzschild): r_ISCO = 6 M = 3 rs.
/// Note: sometimes loosely written “6 rs” when rs is confused with M; here rs is
/// the true Schwarzschild radius, so the SI/geometric radius is 3 * rs.
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

/// Diagonal metric components at (r, θ). Undefined / singular at r ≤ rs or sinθ ≈ 0.
MetricComponents metric_at(double r, double theta, double rs);

/// Inverse metric diagonal (raised indices) at (r, θ).
MetricComponents inverse_metric_at(double r, double theta, double rs);

}  // namespace bh
