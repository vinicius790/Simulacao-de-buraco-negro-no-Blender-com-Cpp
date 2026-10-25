#pragma once
// Geometric accretion-disk annulus helpers matching the legacy visual baseline.
//
// IMPORTANT — honesty contract:
//   Novikov–Thorne (or any relativistic thin-disk emission / temperature /
//   redshift / Doppler model) is NOT implemented. This header only exposes
//   the geometric inner/outer radii used by black_hole.cpp + geodesic.comp
//   so tests, Blender, and scene JSON stay in lockstep with the visual style.
//
// Legacy factors (locked by tests/validate_source_invariants.py):
//   disk_r1 = rs * 2.2
//   disk_r2 = rs * 5.2
//   thickness = 1e9 m (CPU uploadDiskUBO)
//
// Disk shading in geodesic.comp (must match Blender / docs/ESTILO):
//   r_norm = length(pos) / disk_r2
//   diskColor = vec3(1.0, r_norm, 0.2)
//   alpha    = r_norm

#include <cmath>

namespace bh {
namespace disk {

/// Legacy inner-radius factor in units of rs (event-horizon scale).
inline constexpr double LEGACY_INNER_FACTOR_RS = 2.2;

/// Legacy outer-radius factor in units of rs.
inline constexpr double LEGACY_OUTER_FACTOR_RS = 5.2;

/// Legacy disk half-thickness uploaded by black_hole.cpp (metres).
inline constexpr double LEGACY_THICKNESS_M = 1.0e9;

/// Legacy disk_num UBO field (visual baseline uses 2.0).
inline constexpr double LEGACY_DISK_NUM = 2.0;

struct Annulus {
    double inner_m = 0.0;
    double outer_m = 0.0;
    double thickness_m = LEGACY_THICKNESS_M;
    double disk_num = LEGACY_DISK_NUM;
};

/// Geometric annulus matching uploadDiskUBO(): r_inner = factor_in * rs, etc.
inline Annulus legacy_annulus(double rs,
                              double inner_factor = LEGACY_INNER_FACTOR_RS,
                              double outer_factor = LEGACY_OUTER_FACTOR_RS,
                              double thickness_m = LEGACY_THICKNESS_M) {
    Annulus a;
    a.inner_m = rs * inner_factor;
    a.outer_m = rs * outer_factor;
    a.thickness_m = thickness_m;
    a.disk_num = LEGACY_DISK_NUM;
    return a;
}

/// True if cylindrical radius rho = sqrt(x^2+z^2) is inside the annulus
/// (equatorial hit test used by geodesic.comp crossesEquatorialPlane).
inline bool contains_cylindrical_radius(const Annulus& a, double rho) {
    return rho >= a.inner_m && rho <= a.outer_m;
}

/// Radial colour parameter matching geodesic.comp: length(pos)/disk_r2.
inline double color_radial_param(double radius_m, const Annulus& a) {
    if (a.outer_m <= 0.0) {
        return 0.0;
    }
    return radius_m / a.outer_m;
}

/// RGB components of the legacy disk colour (alpha is the same r_norm).
inline void legacy_disk_rgb(double r_norm, double& out_r, double& out_g, double& out_b) {
    out_r = 1.0;
    out_g = r_norm;
    out_b = 0.2;
}

}  // namespace disk
}  // namespace bh
