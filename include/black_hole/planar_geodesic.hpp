#pragma once
// Pole-safe null-geodesic propagation by integrating in the ray's own orbital plane.
//
// Schwarzschild spacetime is spherically symmetric, so every geodesic lies in a
// plane through the origin: the plane spanned by the initial position r⃗₀ and
// direction d⃗₀ (normal n̂ = r⃗₀ × d⃗₀ / |…|). Rotating that plane onto θ = π/2
// turns the 3-D problem into a 2-D (r, φ) problem where sinθ ≡ 1 and the
// coordinate singularities at the poles never occur. The result is then rotated
// back to world Cartesian coordinates.
//
// This is exact (no approximation) and is the integrator used by the CPU
// renderer (cpu_renderer.hpp). Legacy geodesic.comp keeps its global spherical
// chart; the scientific compute shader uses this same planar formulation.
//
// Units: any consistent length unit (rs passed explicitly). Cartesian frame is
// the caller's (the renderer uses world Y-up, disk in XZ).

#include "black_hole/integrator_rk45.hpp"
#include "black_hole/ray_state.hpp"

#include <array>

namespace bh {

using Vec3d = std::array<double, 3>;

struct PlanarRay {
    // Orthonormal in-plane basis and normal (world frame).
    Vec3d e1{{1.0, 0.0, 0.0}};
    Vec3d e2{{0.0, 1.0, 0.0}};
    Vec3d normal{{0.0, 0.0, 1.0}};
    // Conserved quantities of the traced ray.
    double E = 0.0;
    double L = 0.0;          // signed angular momentum about `normal` (≥ 0 by construction)
    Vec3d L_vec{{0.0, 0.0, 0.0}};  // r⃗ × dr⃗/dλ in world frame (conserved)
    // Equatorial state used for integration (theta = π/2, dtheta = 0).
    RayState state;
    bool degenerate_radial = false;  // |L| ≈ 0: purely radial ray
};

/// How the direction passed to make_planar_ray is interpreted.
enum class DirectionFrame {
    /// Unit-ish vector in the orthonormal frame of a STATIC observer at `pos`
    /// (what a camera hovering there actually sees): k^r = sqrt(f)·n_r,
    /// k^φ = n_φ / r, local photon energy = |dir|, E = sqrt(f)·|dir|.
    /// This is the physically correct camera model (Synge shadow size).
    StaticObserver,
    /// Coordinate velocity dr⃗/dλ in the Cartesian embedding (legacy
    /// geodesic.comp / make_ray_from_cartesian convention). Kept for
    /// comparisons with the 3-D chart; distorts angles near the hole.
    Coordinate,
};

/// Build a planar ray from world Cartesian position and direction (direction
/// need not be normalised; its length sets the affine normalisation).
PlanarRay make_planar_ray(const Vec3d& pos, const Vec3d& dir, double rs,
                          DirectionFrame frame = DirectionFrame::StaticObserver);

/// World Cartesian position of the ray.
Vec3d planar_position(const PlanarRay& ray);

/// World Cartesian coordinate velocity dr⃗/dλ of the ray.
Vec3d planar_velocity(const PlanarRay& ray);

/// Current coordinate radius.
inline double planar_radius(const PlanarRay& ray) { return ray.state.r; }

/// Current radial velocity dr/dλ.
inline double planar_dr(const PlanarRay& ray) { return ray.state.dr; }

/// Fixed-step classical RK4 advance in the orbital plane.
void planar_rk4_step(PlanarRay& ray, double d_lambda, double rs);

/// Adaptive RK45 advance in the orbital plane.
Rk45Result planar_rk45_step(PlanarRay& ray, double h, double rs, const Rk45Options& opt = {});

/// Null-constraint residual of the planar state (should stay ≈ 0).
double planar_null_constraint(const PlanarRay& ray, double rs);

/// Geometric step heuristic used by the renderer: dλ = clamp(k·r, min, max).
inline double geometric_step(double r, double rs, double k = 0.02, double min_rs = 0.005, double max_rs = 2.0) {
    double h = k * r;
    if (h < min_rs * rs) h = min_rs * rs;
    if (h > max_rs * rs) h = max_rs * rs;
    return h;
}

}  // namespace bh
