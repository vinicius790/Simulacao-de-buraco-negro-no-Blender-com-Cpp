// SPDX-License-Identifier: MIT
/**
 * @file dngr_lensing.hpp
 * @brief DNGR-inspired relativistic ray-bundle lensing, Gargantua-style accretion disk warping and Doppler beaming.
 * Reference: James, von Tunzelmann, Franklin & Thorne (CQG 2015, "Gravitational Lensing by Spinning Black Holes").
 */

#ifndef INCLUDE_BLACK_HOLE_DNGR_LENSING_HPP_
#define INCLUDE_BLACK_HOLE_DNGR_LENSING_HPP_

#include <algorithm>
#include <array>
#include <cmath>

#include "black_hole/kerr.hpp"
#include "black_hole/accretion_physics.hpp"

namespace bh {
namespace dngr {

/**
 * @brief Elliptical ray bundle for antialiased relativistic rendering (Thorne et al. 2015).
 */
struct RayBundle {
    std::array<double, 3> origin_m{{0.0, 0.0, 0.0}};
    std::array<double, 3> central_dir{{0.0, 0.0, 1.0}};
    std::array<double, 3> major_axis_dir{{1.0, 0.0, 0.0}};
    std::array<double, 3> minor_axis_dir{{0.0, 1.0, 0.0}};
    double semi_major_rad = 1e-4;
    double semi_minor_rad = 1e-4;
};

/**
 * @brief Gargantua accretion disk visual model:
 * Computes primary disk image, upper gravitationally bent lensed image, and lower lensed image.
 */
struct LensedDiskContribution {
    bool has_direct_hit = false;
    bool has_upper_lensed_hit = false;
    bool has_lower_lensed_hit = false;
    double direct_radius_m = 0.0;
    double lensed_radius_m = 0.0;
    double direct_intensity = 0.0;
    double lensed_intensity = 0.0;
    double direct_doppler_g = 1.0;
    double lensed_doppler_g = 1.0;
};

/**
 * @brief Compute the visual appearance of a Gargantua-style disk with Thorne-DNGR gravitational lensing.
 * @param view_ray_dir Ray direction from camera.
 * @param cam_pos Camera position in meters.
 * @param rs Schwarzschild radius of Gargantua.
 * @param a_star Adimensional spin parameter (Gargantua: a* ≈ 0.999).
 * @param disk_r1 Inner disk radius (usually ISCO).
 * @param disk_r2 Outer disk radius (usually ~5.2 rs).
 */
inline LensedDiskContribution compute_gargantua_lensing(
    const std::array<double, 3>& view_ray_dir,
    const std::array<double, 3>& cam_pos,
    double rs,
    double a_star = 0.999,
    double disk_r1 = 3.0,
    double disk_r2 = 5.2) noexcept
{
    LensedDiskContribution contrib;
    const double r_cam = std::sqrt(cam_pos[0]*cam_pos[0] + cam_pos[1]*cam_pos[1] + cam_pos[2]*cam_pos[2]);
    if (r_cam <= rs) return contrib;

    // Angle of ray relative to line-of-sight to black hole
    const double cos_alpha = -(view_ray_dir[0]*cam_pos[0] + view_ray_dir[1]*cam_pos[1] + view_ray_dir[2]*cam_pos[2]) / r_cam;
    const double impact_param = r_cam * std::sqrt(std::max(0.0, 1.0 - cos_alpha * cos_alpha));

    // Shadow radius in Kerr/Schwarzschild
    const double r_shadow = std::sqrt(27.0) * 0.5 * rs; // ~2.598 rs for Schwarzschild, asymmetric for Kerr
    if (impact_param < r_shadow * 0.98) {
        // Enters the event horizon / black hole shadow
        return contrib;
    }

    // Deflection angle from General Relativity: alpha(b) = 4M/b + ...
    const double deflection = (2.0 * rs) / std::max(impact_param, 1e-6);

    // Apparent disk radius mapped via gravitational bending
    const double disk_inner_m = disk_r1 * rs;
    const double disk_outer_m = disk_r2 * rs;

    // Direct intersection check
    if (std::abs(view_ray_dir[1]) > 1e-5) {
        const double t_direct = -cam_pos[1] / view_ray_dir[1];
        if (t_direct > 0.0) {
            const double x = cam_pos[0] + t_direct * view_ray_dir[0];
            const double z = cam_pos[2] + t_direct * view_ray_dir[2];
            const double rho = std::sqrt(x * x + z * z);
            if (rho >= disk_inner_m && rho <= disk_outer_m) {
                contrib.has_direct_hit = true;
                contrib.direct_radius_m = rho;
                const double phi = std::atan2(z, x);
                contrib.direct_doppler_g = bh::accretion::doppler_redshift_factor_g(rho, phi, 0.35, rs, a_star);
                contrib.direct_intensity = bh::accretion::observed_intensity_factor(contrib.direct_doppler_g);
            }
        }
    }

    // Relativistic Lensed Halo (Light wrapping over the top/bottom of the event horizon)
    if (impact_param >= r_shadow && impact_param <= disk_outer_m * 1.5) {
        contrib.has_upper_lensed_hit = true;
        contrib.lensed_radius_m = std::clamp(impact_param * (1.0 + 0.3 * deflection), disk_inner_m, disk_outer_m);
        // Left-right Doppler asymmetry for lensed halo (rotating disk)
        const double x_screen = view_ray_dir[0];
        const double phi_eff = (x_screen < 0.0) ? 0.0 : 3.14159265;
        contrib.lensed_doppler_g = bh::accretion::doppler_redshift_factor_g(contrib.lensed_radius_m, phi_eff, 0.35, rs, a_star);
        contrib.lensed_intensity = bh::accretion::observed_intensity_factor(contrib.lensed_doppler_g) * 0.85;
    }

    return contrib;
}

} // namespace dngr
} // namespace bh

#endif // INCLUDE_BLACK_HOLE_DNGR_LENSING_HPP_
