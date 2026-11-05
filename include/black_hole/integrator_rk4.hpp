#ifndef INCLUDE_BLACK_HOLE_INTEGRATOR_RK4_HPP_
#define INCLUDE_BLACK_HOLE_INTEGRATOR_RK4_HPP_
// True 4-stage classical Runge–Kutta integrator for Schwarzschild null
// geodesics.
//
// Coordinate chart: Schwarzschild spherical (r, θ, φ) with affine parameter λ.
// Second-order reduction to a 6-vector (r,θ,φ,dr,dθ,dφ).
// Scientific RHS uses the correct Christoffel factor r*f on angular terms;
// legacy geodesic.comp omits f (documented; baseline unchanged).
//
// E and L are treated as constants of motion during integration.
// This path is NOT used by default GPU render (legacy Euler remains default).

#include <cstddef>

#include "black_hole/ray_state.hpp"

namespace bh {

/// Ray termination status from batch integration.
struct BatchRayResult {
    bool escaped = false;
    bool captured = false;
    int steps_taken = 0;
};

/// Evaluate geodesicRHS into a 6-vector: [dr, dθ, dφ, d²r, d²θ, d²φ].
void geodesic_rhs(const RayState& ray, double rs, double rhs[6]);

/// Single explicit Euler step (legacy arithmetic; for comparison tests only).
void euler_step(RayState& ray, double d_lambda, double rs);

/// True classical RK4 step (4 RHS evaluations).
void rk4_step(RayState& ray, double d_lambda, double rs);

/// Batch RK4 step for an array of rays in parallel using OpenMP.
void rk4_step_batch(RayState* rays, std::size_t count,
                    double d_lambda, double rs);

/// Batch RK4 integration marching rays until termination
/// (horizon hit, escape, or max_steps).
/// Parallelized across rays using OpenMP.
void integrate_rays_rk4(RayState* rays,
                        std::size_t count,
                        double d_lambda,
                        double rs,
                        int max_steps,
                        double escape_r,
                        BatchRayResult* results = nullptr);

}  // namespace bh

#endif  // INCLUDE_BLACK_HOLE_INTEGRATOR_RK4_HPP_
