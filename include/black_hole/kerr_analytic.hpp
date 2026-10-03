#pragma once
// Closed-form Kerr characteristic radii (Bardeen, Press & Teukolsky 1972).
//
// HONESTY CONTRACT: this header provides ANALYTIC radii only. No Kerr metric,
// no Kerr geodesic integration, no frame-dragging rendering is implemented
// anywhere in this tree. Use these numbers for annotations, guide rings and
// sanity checks; do not claim a Kerr simulation.
//
// Geometric units G = c = 1, M = rs_schwarzschild / 2, dimensionless spin
// a* = a / M. Only |a*| matters (clamped to [0, 1]); the ORBIT direction is
// carried by the `prograde` flag (true = co-rotating with the hole).
// All functions reduce to Schwarzschild for a* = 0:
//   r_+ = 2M, r_isco = 6M, r_ph = 3M, r_ergo(equatorial) = 2M.

#include <cmath>

namespace bh {
namespace kerr {

/// |a*| clamped to [0, 1].
inline double clamp_spin(double a_star) {
    a_star = std::abs(a_star);
    return (a_star > 1.0) ? 1.0 : a_star;
}

/// Outer event horizon r_+ = M (1 + sqrt(1 − a*²)).
inline double outer_horizon(double M, double a_star) {
    a_star = clamp_spin(a_star);
    return M * (1.0 + std::sqrt(1.0 - a_star * a_star));
}

/// Inner (Cauchy) horizon r_− = M (1 − sqrt(1 − a*²)).
inline double inner_horizon(double M, double a_star) {
    a_star = clamp_spin(a_star);
    return M * (1.0 - std::sqrt(1.0 - a_star * a_star));
}

/// Equatorial ergosurface radius (static limit) r_E(θ=π/2) = 2M.
inline double ergosphere_equatorial(double M) { return 2.0 * M; }

/// Ergosurface at polar angle θ: r_E = M (1 + sqrt(1 − a*² cos²θ)).
inline double ergosphere_radius(double M, double a_star, double theta) {
    a_star = clamp_spin(a_star);
    const double c = std::cos(theta);
    return M * (1.0 + std::sqrt(1.0 - a_star * a_star * c * c));
}

/// ISCO radius. prograde=true → co-rotating orbit (smaller radius for a*>0).
inline double isco_radius(double M, double a_star, bool prograde) {
    a_star = clamp_spin(a_star);
    const double a2 = a_star * a_star;
    const double z1 = 1.0 + std::cbrt(1.0 - a2) * (std::cbrt(1.0 + a_star) + std::cbrt(1.0 - a_star));
    const double z2 = std::sqrt(3.0 * a2 + z1 * z1);
    const double root = std::sqrt((3.0 - z1) * (3.0 + z1 + 2.0 * z2));
    return M * (3.0 + z2 + (prograde ? -root : root));
}

/// Equatorial circular photon orbit: r_ph = 2M {1 + cos[(2/3) arccos(∓a*)]}
/// (− for prograde, + for retrograde).
inline double photon_orbit_radius(double M, double a_star, bool prograde) {
    a_star = clamp_spin(a_star);
    const double arg = prograde ? -a_star : a_star;
    return 2.0 * M * (1.0 + std::cos((2.0 / 3.0) * std::acos(arg)));
}

/// Radiative efficiency of a thin Kerr disk: η = 1 − Ẽ(r_isco).
/// At the ISCO the BPT expression Ẽ = (r^{3/2} − 2M r^{1/2} ± a M^{1/2}) /
/// (r^{3/4} sqrt(r^{3/2} − 3M r^{1/2} ± 2a M^{1/2})) reduces to the identity
/// Ẽ_isco = sqrt(1 − 2M / (3 r_isco)), which stays finite at a* = 1 prograde
/// (0/0 in the general form): η(1, pro) = 1 − 1/√3 ≈ 0.4226.
inline double thin_disk_efficiency(double M, double a_star, bool prograde) {
    const double r = isco_radius(M, a_star, prograde);
    return 1.0 - std::sqrt(1.0 - 2.0 * M / (3.0 * r));
}

}  // namespace kerr
}  // namespace bh
