// SPDX-License-Identifier: MIT
/**
 * @file hit_test.hpp
 * @brief Continuous segment-plane disk, segment-sphere horizon, polar regularization, and asymptotic escape.
 */

#ifndef INCLUDE_BLACK_HOLE_HIT_TEST_HPP_
#define INCLUDE_BLACK_HOLE_HIT_TEST_HPP_

#include <algorithm>
#include <array>
#include <cmath>

namespace bh {

struct DiskHitResult {
    bool hit = false;
    double t_frac = 0.0;
    std::array<double, 3> hit_pos_m{{0.0, 0.0, 0.0}};
    double cylindrical_rho_m = 0.0;
    double phi_rad = 0.0;
};

inline DiskHitResult intersect_segment_disk_annulus(
    const std::array<double, 3>& p0,
    const std::array<double, 3>& p1,
    double inner_r_m,
    double outer_r_m,
    double half_thickness_m = 0.0) noexcept
{
    DiskHitResult res;
    const double y0 = p0[1];
    const double y1 = p1[1];

    if ((y0 > half_thickness_m && y1 > half_thickness_m) ||
        (y0 < -half_thickness_m && y1 < -half_thickness_m)) {
        return res;
    }

    const double dy = y1 - y0;
    if (std::abs(dy) < 1e-12) return res;

    const double t = -y0 / dy;
    if (t < 0.0 || t > 1.0) return res;

    const double x = p0[0] + t * (p1[0] - p0[0]);
    const double y = p0[1] + t * (p1[1] - p0[1]);
    const double z = p0[2] + t * (p1[2] - p0[2]);
    const double rho = std::sqrt(x * x + z * z);

    if (rho >= inner_r_m && rho <= outer_r_m) {
        res.hit = true;
        res.t_frac = t;
        res.hit_pos_m = {x, y, z};
        res.cylindrical_rho_m = rho;
        res.phi_rad = std::atan2(z, x);
    }
    return res;
}

struct SphereHitResult {
    bool hit = false;
    double t_frac = 0.0;
    std::array<double, 3> hit_pos_m{{0.0, 0.0, 0.0}};
};

inline SphereHitResult intersect_segment_sphere_horizon(
    const std::array<double, 3>& p0,
    const std::array<double, 3>& p1,
    double horizon_r_m) noexcept
{
    SphereHitResult res;
    const double dx = p1[0] - p0[0];
    const double dy = p1[1] - p0[1];
    const double dz = p1[2] - p0[2];

    const double a = dx * dx + dy * dy + dz * dz;
    if (a < 1e-12) return res;

    const double b = 2.0 * (p0[0] * dx + p0[1] * dy + p0[2] * dz);
    const double c = (p0[0] * p0[0] + p0[1] * p0[1] + p0[2] * p0[2]) - horizon_r_m * horizon_r_m;

    const double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return res;

    const double sqrt_disc = std::sqrt(disc);
    double t = (-b - sqrt_disc) / (2.0 * a);
    if (t < 0.0 || t > 1.0) {
        t = (-b + sqrt_disc) / (2.0 * a);
    }

    if (t >= 0.0 && t <= 1.0) {
        res.hit = true;
        res.t_frac = t;
        res.hit_pos_m = {p0[0] + t * dx, p0[1] + t * dy, p0[2] + t * dz};
    }
    return res;
}

inline void regularize_polar_coordinates(double& theta, double& phi, double& dtheta) noexcept {
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;

    while (phi < 0.0) phi += TWO_PI;
    while (phi >= TWO_PI) phi -= TWO_PI;

    if (theta < 0.0) {
        theta = -theta;
        dtheta = -dtheta;
        phi += PI;
        if (phi >= TWO_PI) phi -= TWO_PI;
    } else if (theta > PI) {
        theta = TWO_PI - theta;
        dtheta = -dtheta;
        phi += PI;
        if (phi >= TWO_PI) phi -= TWO_PI;
    }
}

inline double pole_safe_sin_theta(double theta, double eps = 1e-7) noexcept {
    return std::max(std::sin(theta), eps);
}

inline bool has_escaped_asymptotically(
    double r,
    double dr_dlambda,
    double rs,
    double impact_b,
    double escape_r_default = 1e30,
    double angle_tol_rad = 1e-4) noexcept
{
    if (r >= escape_r_default) return true;
    if (r > 1.5 * rs && dr_dlambda > 0.0) {
        const double residual_deflection = (2.0 * rs) / r;
        if (residual_deflection < angle_tol_rad) {
            return true;
        }
    }
    return false;
}

} // namespace bh

#endif // INCLUDE_BLACK_HOLE_HIT_TEST_HPP_
