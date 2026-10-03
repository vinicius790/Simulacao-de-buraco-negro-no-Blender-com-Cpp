#pragma once
// Adaptive Dormand–Prince RK5(4) integrator for Schwarzschild null geodesics.
//
// Same 6-vector reduction and RHS as integrator_rk4.hpp (corrected Christoffel
// factor r·f on the angular terms). Embedded 4th-order solution gives a local
// error estimate used for step control (standard PI-free controller with
// safety factor 0.9, growth limited to ×5, shrink limited to ×0.2).
//
// This path is NOT used by the default GPU render (legacy Euler remains default).

#include "black_hole/ray_state.hpp"

namespace bh {

struct Rk45Options {
    double abs_tol = 1e-10;
    double rel_tol = 1e-10;
    double h_min = 1e-6;
    double h_max = 1.0;
};

struct Rk45Result {
    double h_used = 0.0;      // step actually taken (λ advance)
    double h_next = 0.0;      // suggested next step
    double error_norm = 0.0;  // scaled error of the accepted step (≤ 1)
    int rejections = 0;       // attempts rejected before acceptance
};

/// Advance `ray` by ONE accepted adaptive step starting from trial size `h`.
/// Returns the step taken and a suggestion for the next one. If the error
/// cannot be met at h_min the step is still taken at h_min. If the RHS is
/// non-finite even at h_min (invalid state), the ray is left unchanged and
/// the result has h_used = 0 and error_norm = +inf.
Rk45Result rk45_adaptive_step(RayState& ray, double h, double rs, const Rk45Options& opt = {});

}  // namespace bh
