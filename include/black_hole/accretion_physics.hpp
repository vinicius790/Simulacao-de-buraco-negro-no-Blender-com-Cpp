// SPDX-License-Identifier: MIT
/**
 * @file accretion_physics.hpp
 * @brief Relativistic accretion disk physics: Novikov-Thorne flux, Doppler beaming, gravitational redshift and Planck blackbody.
 */

#ifndef INCLUDE_BLACK_HOLE_ACCRETION_PHYSICS_HPP_
#define INCLUDE_BLACK_HOLE_ACCRETION_PHYSICS_HPP_

#include <algorithm>
#include <array>
#include <cmath>

#include "black_hole/kerr.hpp"
#include "black_hole/units.hpp"

namespace bh {
namespace accretion {

inline constexpr double STEFAN_BOLTZMANN = 5.670374419e-8; // W / (m^2 K^4)

inline double keplerian_omega(double r, double rs, double a_star = 0.0) noexcept {
    const double M = 0.5 * rs;
    const double a = bh::kerr::spin_parameter_a(rs, a_star);
    const double denom = std::pow(r, 1.5) + a * std::sqrt(M);
    if (denom <= 1e-12) return 0.0;
    return std::sqrt(M) / denom;
}

inline double doppler_redshift_factor_g(double r, double phi, double inclination_rad, double rs, double a_star = 0.0) noexcept {
    const double r_isco = bh::kerr::isco_radius_kerr(rs, a_star, true);
    if (r < r_isco) return 0.0;

    const double v_phi = std::clamp(std::sqrt(rs / (2.0 * r)), 0.0, 0.999);
    const double gamma = 1.0 / std::sqrt(1.0 - v_phi * v_phi);
    const double gravitational_factor = std::sqrt(std::max(0.0, 1.0 - rs / r));

    const double beta_parallel = v_phi * std::sin(inclination_rad) * std::cos(phi);
    const double doppler_factor = 1.0 / (gamma * (1.0 - beta_parallel));

    return gravitational_factor * doppler_factor;
}

inline double observed_intensity_factor(double g) noexcept {
    const double g2 = g * g;
    return g2 * g2; // g^4 Liouville theorem
}

inline double novikov_thorne_flux(double r, double rs, double a_star = 0.0, double mdot_kg_s = 1e18) noexcept {
    const double r_isco = bh::kerr::isco_radius_kerr(rs, a_star, true);
    if (r <= r_isco) return 0.0;

    const double M = 0.5 * rs;
    const double prefactor = (3.0 * units::G_SI * (M * units::C_SI * units::C_SI / (2.0 * units::G_SI)) * mdot_kg_s) / (8.0 * 3.141592653589793 * r * r * r);
    const double r_norm = r / r_isco;
    const double f_profile = std::max(0.0, 1.0 - std::sqrt(1.0 / r_norm));
    return prefactor * f_profile;
}

inline double effective_temperature_k(double r, double rs, double a_star = 0.0, double mdot_kg_s = 1e18) noexcept {
    const double flux = novikov_thorne_flux(r, rs, a_star, mdot_kg_s);
    if (flux <= 0.0) return 0.0;
    return std::pow(flux / STEFAN_BOLTZMANN, 0.25);
}

inline void blackbody_kelvin_to_rgb(double temp_k, double& out_r, double& out_g, double& out_b) noexcept {
    if (temp_k <= 0.0) {
        out_r = out_g = out_b = 0.0;
        return;
    }
    const double t = std::clamp(temp_k / 100.0, 10.0, 400.0);

    // Red
    if (t <= 66.0) {
        out_r = 1.0;
    } else {
        out_r = std::clamp(1.29293618608 * std::pow(t - 60.0, -0.1332047592), 0.0, 1.0);
    }

    // Green
    if (t <= 66.0) {
        out_g = std::clamp(0.39008157876 * std::log(t) - 0.63184144378, 0.0, 1.0);
    } else {
        out_g = std::clamp(1.12981760819 * std::pow(t - 60.0, -0.0755148492), 0.0, 1.0);
    }

    // Blue
    if (t >= 66.0) {
        out_b = 1.0;
    } else if (t <= 19.0) {
        out_b = 0.0;
    } else {
        out_b = std::clamp(0.54320678911 * std::log(t - 10.0) - 1.19625408914, 0.0, 1.0);
    }
}

struct RelativisticDiskSample {
    double g_factor = 1.0;
    double beaming_factor = 1.0;
    double temp_k = 0.0;
    double r_rgb = 1.0;
    double g_rgb = 0.5;
    double b_rgb = 0.2;
};

inline RelativisticDiskSample sample_relativistic_disk(double r, double phi, double inclination_rad, double rs, double a_star = 0.0, double mdot_kg_s = 1e18) noexcept {
    RelativisticDiskSample sample;
    sample.g_factor = doppler_redshift_factor_g(r, phi, inclination_rad, rs, a_star);
    sample.beaming_factor = observed_intensity_factor(sample.g_factor);
    sample.temp_k = effective_temperature_k(r, rs, a_star, mdot_kg_s);
    blackbody_kelvin_to_rgb(sample.temp_k * sample.g_factor, sample.r_rgb, sample.g_rgb, sample.b_rgb);
    sample.r_rgb = std::clamp(sample.r_rgb * sample.beaming_factor, 0.0, 1.0);
    sample.g_rgb = std::clamp(sample.g_rgb * sample.beaming_factor, 0.0, 1.0);
    sample.b_rgb = std::clamp(sample.b_rgb * sample.beaming_factor, 0.0, 1.0);
    return sample;
}

} // namespace accretion
} // namespace bh

extern "C" {
inline double bh_accretion_doppler_factor(double r, double phi, double inclination_rad, double rs, double a_star) {
    return bh::accretion::doppler_redshift_factor_g(r, phi, inclination_rad, rs, a_star);
}
inline double bh_accretion_temperature_k(double r, double rs, double a_star, double mdot_kg_s) {
    return bh::accretion::effective_temperature_k(r, rs, a_star, mdot_kg_s);
}
inline void bh_blackbody_to_rgb(double temp_k, double* r, double* g, double* b) {
    if (r && g && b) {
        bh::accretion::blackbody_kelvin_to_rgb(temp_k, *r, *g, *b);
    }
}
}

#endif // INCLUDE_BLACK_HOLE_ACCRETION_PHYSICS_HPP_
