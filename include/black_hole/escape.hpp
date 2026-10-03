#pragma once
// Derived escape / capture criteria for Schwarzschild null geodesics.
//
// Radial equation (affine parameter λ, b = L/E impact parameter):
//   (dr/dλ)² = E² − L² f(r)/r²  =  E² [ 1 − b² f(r)/r² ].
// The effective potential V(r) = f/r² has a single maximum at the photon
// sphere r = 3M = 1.5 rs, where V_max = 1/(27 M²). Hence:
//
//   * critical impact parameter  b_c = 3√3 M = (3√3/2) rs ≈ 2.598 rs;
//     rays from infinity with b < b_c are captured, b > b_c are deflected.
//   * an OUTGOING ray (dr/dλ > 0) at r > 1.5 rs can never turn around,
//     because V decreases monotonically for r > 3M. This replaces the legacy
//     unreachable ESCAPE_R = 1e30 with a proof-based test: once such a ray
//     is also beyond every scene object, it will not hit anything.
//   * shadow seen by a static observer at r_o:  sin α_sh = b_c sqrt(f(r_o)) / r_o.

#include "black_hole/ray_state.hpp"
#include "black_hole/schwarzschild.hpp"

#include <cmath>

namespace bh {
namespace escape {

/// Critical impact parameter b_c = (3√3 / 2) rs.
inline double critical_impact_parameter(double rs) {
    return 1.5 * std::sqrt(3.0) * rs;
}

/// Impact parameter of a ray from its conserved quantities: b = |L| / E.
inline double impact_parameter(double E, double L) {
    return (E > 0.0) ? std::abs(L) / E : 0.0;
}

/// Will a ray coming from far away be captured? (b < b_c)
inline bool captured_from_infinity(double b, double rs) {
    return b < critical_impact_parameter(rs);
}

/// Proof-based escape test: outgoing and already outside the photon sphere.
inline bool is_outgoing_beyond_photon_sphere(double r, double dr_dlambda, double rs) {
    return dr_dlambda > 0.0 && r > photon_sphere_radius(rs);
}

/// Full scene escape criterion: outgoing beyond the photon sphere AND beyond a
/// bounding radius that encloses every renderable object (disk / spheres).
inline bool will_escape_scene(double r, double dr_dlambda, double rs, double scene_bound_r) {
    return is_outgoing_beyond_photon_sphere(r, dr_dlambda, rs) && r > scene_bound_r;
}

/// Angular radius of the black-hole shadow for a static observer at r_obs,
/// measured from the direction towards the hole (Synge 1966):
///   sin α = b_c sqrt(f(r_obs)) / r_obs,
/// with α ≤ π/2 outside the photon sphere and α = π − asin(…) > π/2 inside it
/// (there the escape cone is narrower than a hemisphere). → π at the horizon.
inline double shadow_angular_radius(double rs, double r_obs) {
    const double f = lapse_factor(r_obs, rs);
    if (f <= 0.0) {
        return 3.14159265358979323846;
    }
    double s = critical_impact_parameter(rs) * std::sqrt(f) / r_obs;
    if (s > 1.0) s = 1.0;
    const double a = std::asin(s);
    return (r_obs >= photon_sphere_radius(rs)) ? a : 3.14159265358979323846 - a;
}

/// Far-observer limit of the shadow angular radius: b_c / r_obs.
inline double shadow_angular_radius_far(double rs, double r_obs) {
    return critical_impact_parameter(rs) / r_obs;
}

}  // namespace escape
}  // namespace bh
