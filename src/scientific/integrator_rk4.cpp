#include "black_hole/integrator_rk4.hpp"
#include "black_hole/schwarzschild.hpp"

#include <cmath>

namespace bh {
namespace {

void apply_state(RayState& ray, const double y[6]) {
    ray.r = y[0];
    ray.theta = y[1];
    ray.phi = y[2];
    ray.dr = y[3];
    ray.dtheta = y[4];
    ray.dphi = y[5];
}

void add_scaled(const double a[6], const double b[6], double factor, double out[6]) {
    for (int i = 0; i < 6; ++i) {
        out[i] = a[i] + b[i] * factor;
    }
}

}  // namespace

void geodesic_rhs(const RayState& ray, double rs, double rhs[6]) {
    // Coordinate chart: Schwarzschild spherical (r, θ, φ), affine λ.
    // Christoffel form for Γ^r_θθ = −r f, Γ^r_φφ = −r f sin²θ, etc.
    //
    // IMPORTANT vs legacy GPU/CPU visual path:
    //   geodesic.comp / CPU-geodesic.cpp use + r*(dθ²+sin²θ dφ²)
    //   (missing the factor f). That legacy form does not preserve the null
    //   constraint. Scientific mode uses the corrected + r*f*(...).
    // Poles (sinθ → 0) remain singular — stay equatorial or clamp θ.
    const double r = ray.r;
    const double theta = ray.theta;
    const double dr = ray.dr;
    const double dtheta = ray.dtheta;
    const double dphi = ray.dphi;
    const double E = ray.E;

    const double f = lapse_factor(r, rs);
    const double dt_dlambda = (f > 0.0) ? (E / f) : 0.0;
    const double st = std::sin(theta);
    const double ct = std::cos(theta);

    rhs[0] = dr;
    rhs[1] = dtheta;
    rhs[2] = dphi;

    rhs[3] = -(rs / (2.0 * r * r)) * f * dt_dlambda * dt_dlambda +
             (rs / (2.0 * r * r * f)) * dr * dr +
             r * f * (dtheta * dtheta + st * st * dphi * dphi);

    rhs[4] = -(2.0 / r) * dr * dtheta + st * ct * dphi * dphi;

    rhs[5] = -(2.0 / r) * dr * dphi - 2.0 * ct / st * dtheta * dphi;
}

void euler_step(RayState& ray, double d_lambda, double rs) {
    double k1[6];
    geodesic_rhs(ray, rs, k1);
    ray.r += d_lambda * k1[0];
    ray.theta += d_lambda * k1[1];
    ray.phi += d_lambda * k1[2];
    ray.dr += d_lambda * k1[3];
    ray.dtheta += d_lambda * k1[4];
    ray.dphi += d_lambda * k1[5];
    ray.sync_cartesian_from_spherical();
}

void rk4_step(RayState& ray, double d_lambda, double rs) {
    const double y0[6] = {ray.r, ray.theta, ray.phi, ray.dr, ray.dtheta, ray.dphi};
    double k1[6], k2[6], k3[6], k4[6], temp[6];

    geodesic_rhs(ray, rs, k1);

    add_scaled(y0, k1, d_lambda / 2.0, temp);
    RayState r2 = ray;
    apply_state(r2, temp);
    geodesic_rhs(r2, rs, k2);

    add_scaled(y0, k2, d_lambda / 2.0, temp);
    RayState r3 = ray;
    apply_state(r3, temp);
    geodesic_rhs(r3, rs, k3);

    add_scaled(y0, k3, d_lambda, temp);
    RayState r4 = ray;
    apply_state(r4, temp);
    geodesic_rhs(r4, rs, k4);

    ray.r += (d_lambda / 6.0) * (k1[0] + 2.0 * k2[0] + 2.0 * k3[0] + k4[0]);
    ray.theta += (d_lambda / 6.0) * (k1[1] + 2.0 * k2[1] + 2.0 * k3[1] + k4[1]);
    ray.phi += (d_lambda / 6.0) * (k1[2] + 2.0 * k2[2] + 2.0 * k3[2] + k4[2]);
    ray.dr += (d_lambda / 6.0) * (k1[3] + 2.0 * k2[3] + 2.0 * k3[3] + k4[3]);
    ray.dtheta += (d_lambda / 6.0) * (k1[4] + 2.0 * k2[4] + 2.0 * k3[4] + k4[4]);
    ray.dphi += (d_lambda / 6.0) * (k1[5] + 2.0 * k2[5] + 2.0 * k3[5] + k4[5]);
    ray.sync_cartesian_from_spherical();
}

}  // namespace bh
