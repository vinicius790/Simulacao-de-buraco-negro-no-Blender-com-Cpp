// SPDX-License-Identifier: MIT
/**
 * @file kerr.hpp
 * @brief Boyer-Lindquist metric, horizons, ergosphere, photon orbits and ISCO for Kerr black holes.
 */

#ifndef INCLUDE_BLACK_HOLE_KERR_HPP_
#define INCLUDE_BLACK_HOLE_KERR_HPP_

#include <algorithm>
#include <array>
#include <cmath>

#include "black_hole/units.hpp"

namespace bh {
namespace kerr {

inline constexpr double MAX_VALID_SPIN = 0.999999;

inline double clamp_spin(double a_star) noexcept {
    return std::clamp(a_star, -MAX_VALID_SPIN, MAX_VALID_SPIN);
}

inline double spin_parameter_a(double rs, double a_star) noexcept {
    const double M = 0.5 * rs;
    return clamp_spin(a_star) * M;
}

inline double sigma(double r, double theta, double a) noexcept {
    const double cos_t = std::cos(theta);
    return r * r + a * a * cos_t * cos_t;
}

inline double delta(double r, double rs, double a) noexcept {
    return r * r - rs * r + a * a;
}

inline double metric_A(double r, double theta, double rs, double a) noexcept {
    const double r2 = r * r;
    const double a2 = a * a;
    const double sin_t = std::sin(theta);
    const double sig = sigma(r, theta, a);
    return (r2 + a2) * (r2 + a2) - a2 * delta(r, rs, a) * sin_t * sin_t;
}

inline double outer_horizon_radius(double rs, double a_star) noexcept {
    const double M = 0.5 * rs;
    const double a_s = clamp_spin(a_star);
    const double disc = 1.0 - a_s * a_s;
    if (disc <= 0.0) return M;
    return M * (1.0 + std::sqrt(disc));
}

inline double inner_horizon_radius(double rs, double a_star) noexcept {
    const double M = 0.5 * rs;
    const double a_s = clamp_spin(a_star);
    const double disc = 1.0 - a_s * a_s;
    if (disc <= 0.0) return M;
    return M * (1.0 - std::sqrt(disc));
}

inline double ergosphere_radius(double rs, double a_star, double theta) noexcept {
    const double M = 0.5 * rs;
    const double a_s = clamp_spin(a_star);
    const double cos_t = std::cos(theta);
    const double disc = 1.0 - a_s * a_s * cos_t * cos_t;
    if (disc <= 0.0) return M;
    return M * (1.0 + std::sqrt(disc));
}

inline double frame_dragging_omega(double r, double theta, double rs, double a_star) noexcept {
    const double a = spin_parameter_a(rs, a_star);
    const double sig = sigma(r, theta, a);
    if (sig <= 1e-12) return 0.0;
    const double A = metric_A(r, theta, rs, a);
    if (std::abs(A) <= 1e-12) return 0.0;
    return (rs * r * a) / A;
}

inline double prograde_photon_orbit(double rs, double a_star) noexcept {
    const double M = 0.5 * rs;
    const double a_s = std::clamp(std::abs(a_star), 0.0, MAX_VALID_SPIN);
    return 2.0 * M * (1.0 + std::cos(2.0 / 3.0 * std::acos(-a_s)));
}

inline double retrograde_photon_orbit(double rs, double a_star) noexcept {
    const double M = 0.5 * rs;
    const double a_s = std::clamp(std::abs(a_star), 0.0, MAX_VALID_SPIN);
    return 2.0 * M * (1.0 + std::cos(2.0 / 3.0 * std::acos(a_s)));
}

inline double isco_radius_kerr(double rs, double a_star, bool prograde = true) noexcept {
    const double M = 0.5 * rs;
    const double a = std::clamp(std::abs(a_star), 0.0, MAX_VALID_SPIN);
    const double sign = prograde ? 1.0 : -1.0;
    const double z1 = 1.0 + std::cbrt(1.0 - a * a) * (std::cbrt(1.0 + a) + std::cbrt(1.0 - a));
    const double z2 = std::sqrt(3.0 * a * a + z1 * z1);
    const double term = std::sqrt(std::max(0.0, (3.0 - z1) * (3.0 + z1 + 2.0 * z2)));
    return M * (3.0 + z2 - sign * term);
}

inline double radiative_efficiency_kerr(double a_star, bool prograde = true) noexcept {
    const double r_isco_m = isco_radius_kerr(2.0, a_star, prograde);
    const double e_isco = (r_isco_m * r_isco_m - 2.0 * r_isco_m + a_star * std::sqrt(r_isco_m)) /
                          (r_isco_m * std::sqrt(r_isco_m * r_isco_m - 3.0 * r_isco_m + 2.0 * a_star * std::sqrt(r_isco_m)));
    return std::max(0.0, 1.0 - e_isco);
}

} // namespace kerr
} // namespace bh

extern "C" {
inline double bh_kerr_outer_horizon(double rs, double a_star) { return bh::kerr::outer_horizon_radius(rs, a_star); }
inline double bh_kerr_inner_horizon(double rs, double a_star) { return bh::kerr::inner_horizon_radius(rs, a_star); }
inline double bh_kerr_ergosphere_radius(double rs, double a_star, double theta) { return bh::kerr::ergosphere_radius(rs, a_star, theta); }
inline double bh_kerr_frame_dragging_omega(double r, double theta, double rs, double a_star) { return bh::kerr::frame_dragging_omega(r, theta, rs, a_star); }
inline double bh_kerr_isco_radius(double rs, double a_star, int prograde) { return bh::kerr::isco_radius_kerr(rs, a_star, prograde != 0); }
inline double bh_kerr_photon_orbit(double rs, double a_star, int prograde) { return prograde ? bh::kerr::prograde_photon_orbit(rs, a_star) : bh::kerr::retrograde_photon_orbit(rs, a_star); }
}

#endif // INCLUDE_BLACK_HOLE_KERR_HPP_
