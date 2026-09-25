#pragma once
// Conserved quantities and null-condition diagnostics for Schwarzschild null geodesics.

#include "black_hole/ray_state.hpp"
#include "black_hole/schwarzschild.hpp"

namespace bh {

/// Angular momentum about the polar axis: L = r² sinθ dφ/dλ (legacy convention).
inline double angular_momentum(const RayState& ray) {
    return ray.r * ray.r * std::sin(ray.theta) * ray.dphi;
}

/// Energy-like conserved quantity from the *correct* null condition:
/// E = sqrt( (dr)² + f r²(dθ² + sin²θ dφ²) ), so g_μν k^μ k^ν = 0 with dt=E/f.
/// Differs from the legacy GPU seed E = f * sqrt((dr)²/f + …); see conserved.cpp.
double energy_from_null_condition(const RayState& ray, double rs);

/// Null-constraint residual g_μν k^μ k^ν (should be ≈ 0 for a null geodesic).
/// Uses signature (−,+,+,+) and dt/dλ = E / f.
double null_constraint(const RayState& ray, double rs);

/// Absolute drift of the null constraint |g_μν k^μ k^ν|.
inline double null_constraint_abs(const RayState& ray, double rs) {
    return std::abs(null_constraint(ray, rs));
}

}  // namespace bh
