#include "black_hole/conserved.hpp"

#include <cmath>

namespace bh {

double energy_from_null_condition(const RayState& ray, double rs) {
    // Correct Schwarzschild null seeding (signature −+++):
    //   E = sqrt( (dr)^2 + f r^2 (dθ^2 + sin^2θ dφ^2) )
    // so that g_μν k^μ k^ν = 0 with dt/dλ = E/f.
    //
    // NOTE: geodesic.comp / CPU-geodesic.cpp historically use
    //   E_legacy = f * sqrt( (dr)^2/f + r^2 (...) )
    // which does NOT satisfy the continuous null condition. Scientific mode
    // deliberately uses the corrected definition; the GPU baseline is unchanged.
    const double f = lapse_factor(ray.r, rs);
    if (f <= 0.0) {
        return 0.0;
    }
    const double st = std::sin(ray.theta);
    const double spatial =
        ray.dr * ray.dr +
        f * ray.r * ray.r *
            (ray.dtheta * ray.dtheta + st * st * ray.dphi * ray.dphi);
    return std::sqrt(spatial);
}

double null_constraint(const RayState& ray, double rs) {
    // Residual of g_μν k^μ k^ν with dt/dλ = E/f.
    const double f = lapse_factor(ray.r, rs);
    if (f <= 0.0) {
        return 0.0;
    }
    const double dt = ray.E / f;
    const double st = std::sin(ray.theta);
    return -f * dt * dt + (ray.dr * ray.dr) / f +
           ray.r * ray.r *
               (ray.dtheta * ray.dtheta + st * st * ray.dphi * ray.dphi);
}

}  // namespace bh
