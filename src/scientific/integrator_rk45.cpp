#include "black_hole/integrator_rk45.hpp"
#include "black_hole/integrator_rk4.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bh {
namespace {

void pack(const RayState& ray, double y[6]) {
    y[0] = ray.r;
    y[1] = ray.theta;
    y[2] = ray.phi;
    y[3] = ray.dr;
    y[4] = ray.dtheta;
    y[5] = ray.dphi;
}

void unpack(RayState& ray, const double y[6]) {
    ray.r = y[0];
    ray.theta = y[1];
    ray.phi = y[2];
    ray.dr = y[3];
    ray.dtheta = y[4];
    ray.dphi = y[5];
}

void rhs_at(const RayState& base, const double y[6], double rs, double out[6]) {
    RayState tmp = base;
    unpack(tmp, y);
    geodesic_rhs(tmp, rs, out);
}

// Dormand–Prince 5(4) tableau.
constexpr double A21 = 1.0 / 5.0;
constexpr double A31 = 3.0 / 40.0, A32 = 9.0 / 40.0;
constexpr double A41 = 44.0 / 45.0, A42 = -56.0 / 15.0, A43 = 32.0 / 9.0;
constexpr double A51 = 19372.0 / 6561.0, A52 = -25360.0 / 2187.0, A53 = 64448.0 / 6561.0,
                 A54 = -212.0 / 729.0;
constexpr double A61 = 9017.0 / 3168.0, A62 = -355.0 / 33.0, A63 = 46732.0 / 5247.0,
                 A64 = 49.0 / 176.0, A65 = -5103.0 / 18656.0;
constexpr double B1 = 35.0 / 384.0, B3 = 500.0 / 1113.0, B4 = 125.0 / 192.0,
                 B5 = -2187.0 / 6784.0, B6 = 11.0 / 84.0;
// 4th-order embedded weights.
constexpr double E1 = 5179.0 / 57600.0, E3 = 7571.0 / 16695.0, E4 = 393.0 / 640.0,
                 E5 = -92097.0 / 339200.0, E6 = 187.0 / 2100.0, E7 = 1.0 / 40.0;

}  // namespace

Rk45Result rk45_adaptive_step(RayState& ray, double h, double rs, const Rk45Options& opt) {
    Rk45Result res{};
    double y0[6];
    pack(ray, y0);

    h = std::clamp(h, opt.h_min, opt.h_max);

    for (int attempt = 0; attempt < 64; ++attempt) {
        double k1[6], k2[6], k3[6], k4[6], k5[6], k6[6], k7[6], tmp[6], y5[6];
        rhs_at(ray, y0, rs, k1);
        for (int i = 0; i < 6; ++i) tmp[i] = y0[i] + h * A21 * k1[i];
        rhs_at(ray, tmp, rs, k2);
        for (int i = 0; i < 6; ++i) tmp[i] = y0[i] + h * (A31 * k1[i] + A32 * k2[i]);
        rhs_at(ray, tmp, rs, k3);
        for (int i = 0; i < 6; ++i) tmp[i] = y0[i] + h * (A41 * k1[i] + A42 * k2[i] + A43 * k3[i]);
        rhs_at(ray, tmp, rs, k4);
        for (int i = 0; i < 6; ++i)
            tmp[i] = y0[i] + h * (A51 * k1[i] + A52 * k2[i] + A53 * k3[i] + A54 * k4[i]);
        rhs_at(ray, tmp, rs, k5);
        for (int i = 0; i < 6; ++i)
            tmp[i] = y0[i] + h * (A61 * k1[i] + A62 * k2[i] + A63 * k3[i] + A64 * k4[i] + A65 * k5[i]);
        rhs_at(ray, tmp, rs, k6);
        for (int i = 0; i < 6; ++i)
            y5[i] = y0[i] + h * (B1 * k1[i] + B3 * k3[i] + B4 * k4[i] + B5 * k5[i] + B6 * k6[i]);
        rhs_at(ray, y5, rs, k7);  // FSAL stage

        double err_norm = 0.0;
        for (int i = 0; i < 6; ++i) {
            const double y4 = y0[i] + h * (E1 * k1[i] + E3 * k3[i] + E4 * k4[i] + E5 * k5[i] +
                                           E6 * k6[i] + E7 * k7[i]);
            const double scale = opt.abs_tol + opt.rel_tol * std::max(std::abs(y0[i]), std::abs(y5[i]));
            const double e = (y5[i] - y4) / scale;
            err_norm += e * e;
        }
        err_norm = std::sqrt(err_norm / 6.0);

        const bool finite = std::isfinite(err_norm);
        const bool accept = finite && ((err_norm <= 1.0) || (h <= opt.h_min * (1.0 + 1e-12)));
        // Non-finite stages (e.g. a 3-D chart ray on the polar axis) must SHRINK
        // the step, never grow it.
        double factor = !finite ? 0.2 : (err_norm > 1e-300) ? 0.9 * std::pow(err_norm, -0.2) : 5.0;
        factor = std::clamp(factor, 0.2, 5.0);

        if (accept) {
            unpack(ray, y5);
            ray.sync_cartesian_from_spherical();
            res.h_used = h;
            res.h_next = std::clamp(h * factor, opt.h_min, opt.h_max);
            res.error_norm = err_norm;
            return res;
        }
        ++res.rejections;
        h = std::clamp(h * factor, opt.h_min, opt.h_max);
    }

    // Pathological: no acceptable step after 64 attempts. Leave the state
    // untouched and report it (error_norm = +inf, h_used = 0) so callers can
    // detect the failure instead of silently receiving an Euler step.
    res.h_used = 0.0;
    res.h_next = h;
    res.error_norm = std::numeric_limits<double>::infinity();
    return res;
}

}  // namespace bh
