#include "black_hole/planar_geodesic.hpp"

#include "black_hole/conserved.hpp"
#include "black_hole/integrator_rk4.hpp"
#include "black_hole/schwarzschild.hpp"

#include <cmath>

namespace bh {
namespace {

constexpr double HALF_PI = 1.57079632679489661923;

double dot(const Vec3d& a, const Vec3d& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

Vec3d cross(const Vec3d& a, const Vec3d& b) {
    return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

double norm(const Vec3d& a) { return std::sqrt(dot(a, a)); }

Vec3d scaled(const Vec3d& a, double s) { return {{a[0] * s, a[1] * s, a[2] * s}}; }

Vec3d add(const Vec3d& a, const Vec3d& b) { return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}}; }

// Any unit vector orthogonal to `a` (used for purely radial rays).
Vec3d any_perpendicular(const Vec3d& a) {
    const double ax = std::abs(a[0]), ay = std::abs(a[1]), az = std::abs(a[2]);
    Vec3d helper = (ax <= ay && ax <= az) ? Vec3d{{1.0, 0.0, 0.0}}
                   : (ay <= az)           ? Vec3d{{0.0, 1.0, 0.0}}
                                          : Vec3d{{0.0, 0.0, 1.0}};
    Vec3d p = cross(a, helper);
    const double n = norm(p);
    return (n > 0.0) ? scaled(p, 1.0 / n) : Vec3d{{0.0, 1.0, 0.0}};
}

}  // namespace

PlanarRay make_planar_ray(const Vec3d& pos, const Vec3d& dir, double rs, DirectionFrame frame) {
    PlanarRay ray;
    const double r = norm(pos);
    ray.e1 = scaled(pos, 1.0 / r);

    ray.L_vec = cross(pos, dir);
    const double L = norm(ray.L_vec);
    const double dir_len = norm(dir);

    if (L <= 1e-12 * r * dir_len) {
        // Radial ray: pick any plane containing e1.
        ray.degenerate_radial = true;
        ray.e2 = any_perpendicular(ray.e1);
        ray.normal = cross(ray.e1, ray.e2);
        ray.L_vec = {{0.0, 0.0, 0.0}};
        ray.L = 0.0;
    } else {
        ray.normal = scaled(ray.L_vec, 1.0 / L);
        ray.e2 = cross(ray.normal, ray.e1);  // right-handed: e1 × e2 = normal
        ray.L = L;
    }

    // Equatorial state: theta = π/2, φ = 0 at the start (position along e1).
    RayState& s = ray.state;
    s.r = r;
    s.theta = HALF_PI;
    s.phi = 0.0;
    // Static observer: the radial unit vector is e_r̂ = sqrt(f) ∂_r, so a local
    // radial component n_r corresponds to k^r = sqrt(f) n_r. Tangential
    // components are unaffected (e_φ̂ = ∂_φ / r in the equatorial plane).
    const double f = lapse_factor(r, rs);
    const double radial_scale = (frame == DirectionFrame::StaticObserver) ? std::sqrt(f > 0.0 ? f : 0.0) : 1.0;
    s.dr = radial_scale * dot(dir, ray.e1);
    s.dtheta = 0.0;
    s.dphi = dot(dir, ray.e2) / r;  // tangential speed / r (≥ 0 by basis choice)
    s.L = angular_momentum(s);        // = r² dphi = L
    s.E = energy_from_null_condition(s, rs);
    s.sync_cartesian_from_spherical();
    ray.E = s.E;
    return ray;
}

Vec3d planar_position(const PlanarRay& ray) {
    const double c = std::cos(ray.state.phi);
    const double s = std::sin(ray.state.phi);
    return add(scaled(ray.e1, ray.state.r * c), scaled(ray.e2, ray.state.r * s));
}

Vec3d planar_velocity(const PlanarRay& ray) {
    const double c = std::cos(ray.state.phi);
    const double s = std::sin(ray.state.phi);
    const double r = ray.state.r;
    const double dr = ray.state.dr;
    const double dphi = ray.state.dphi;
    // d/dλ [ r (c e1 + s e2) ] = dr (c e1 + s e2) + r dphi (−s e1 + c e2)
    const Vec3d radial = add(scaled(ray.e1, c), scaled(ray.e2, s));
    const Vec3d tangential = add(scaled(ray.e1, -s), scaled(ray.e2, c));
    return add(scaled(radial, dr), scaled(tangential, r * dphi));
}

void planar_rk4_step(PlanarRay& ray, double d_lambda, double rs) {
    rk4_step(ray.state, d_lambda, rs);
    // Keep the chart exactly equatorial (rounding guard).
    ray.state.theta = HALF_PI;
    ray.state.dtheta = 0.0;
}

Rk45Result planar_rk45_step(PlanarRay& ray, double h, double rs, const Rk45Options& opt) {
    Rk45Result res = rk45_adaptive_step(ray.state, h, rs, opt);
    ray.state.theta = HALF_PI;
    ray.state.dtheta = 0.0;
    return res;
}

double planar_null_constraint(const PlanarRay& ray, double rs) {
    return null_constraint(ray.state, rs);
}

}  // namespace bh
