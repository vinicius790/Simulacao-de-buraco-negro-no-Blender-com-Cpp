// SPDX-License-Identifier: MIT
/**
 * @file relativistic.hpp
 * @brief Utilities for relativistic calculations used in black-hole
 * simulations.
 *
 * This header provides functions to compute the Schwarzschild radius,
 * gravitational deflection angles, and relativistic time dilation.
 * All implementations are header-only and use double precision.
 * Units are SI unless otherwise noted.
 */

#ifndef INCLUDE_BLACK_HOLE_RELATIVISTIC_HPP_
#define INCLUDE_BLACK_HOLE_RELATIVISTIC_HPP_

#include <cmath>

namespace black_hole {

/**
 * Physical constants (SI).
 */
constexpr double G = 6.67430e-11;    // gravitational constant
constexpr double c = 299'792'458.0;  // speed of light

/**
 * @brief Compute the Schwarzschild radius for a given mass.
 * @param mass_kg Mass in kilograms.
 * @return Radius in meters.
 */
inline constexpr double schwarzschild_radius(double mass_kg) {
    if (mass_kg <= 0.0) {
        return 0.0;
    }
    return 2.0 * G * mass_kg / (c * c);
}

/**
 * @brief Deflection angle of a light ray passing at impact parameter b.
 * @param mass_kg Mass of the gravitating body.
 * @param impact_m Impact parameter in meters.
 * @return Deflection angle in radians (small-angle approximation).
 */
inline constexpr double light_deflection_angle(double mass_kg,
                                               double impact_m) {
    if (mass_kg <= 0.0 || impact_m <= 0.0) {
        return 0.0;
    }
    // Approximation: delta = 4GM/(c^2 b) = 2 * rs / b
    return 2.0 * schwarzschild_radius(mass_kg) / impact_m;
}

/**
 * @brief Time dilation factor for a stationary observer at radius r
 * from a Schwarzschild black hole.
 * @param mass_kg Mass of the black hole.
 * @param radius_m Radial coordinate where the observer is located
 * (must be > rs).
 * @return Ratio of proper time to coordinate time (gamma).
 */
inline double time_dilation_factor(double mass_kg, double radius_m) {
    if (mass_kg <= 0.0 || radius_m <= 0.0) {
        return 0.0;
    }
    const double rs = schwarzschild_radius(mass_kg);
    if (radius_m <= rs) {
        // At or inside horizon the factor diverges; return 0 for breakdown.
        return 0.0;
    }
    return std::sqrt(1.0 - rs / radius_m);
}

}  // namespace black_hole

#ifdef __cplusplus
extern "C" {
#endif

/**
 * C-linkage export helpers for ctypes / shared-library bindings.
 */
inline double bh_schwarzschild_radius(double mass_kg) {
    return black_hole::schwarzschild_radius(mass_kg);
}

inline double bh_light_deflection_angle(double mass_kg, double impact_m) {
    return black_hole::light_deflection_angle(mass_kg, impact_m);
}

inline double bh_time_dilation_factor(double mass_kg, double radius_m) {
    return black_hole::time_dilation_factor(mass_kg, radius_m);
}

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_BLACK_HOLE_RELATIVISTIC_HPP_
