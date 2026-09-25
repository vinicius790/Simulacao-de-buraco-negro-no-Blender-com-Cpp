#pragma once
// Double-precision null-geodesic state in Schwarzschild spherical coordinates.
// Chart: (t, r, θ, φ) with affine parameter λ. Conserved E, L are stored on the ray.

#include <cmath>

namespace bh {

struct RayState {
    // Cartesian embedding (convenience; not integrated directly).
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // Spherical coordinates and velocities w.r.t. affine parameter λ.
    double r = 0.0;
    double theta = 0.0;
    double phi = 0.0;
    double dr = 0.0;
    double dtheta = 0.0;
    double dphi = 0.0;

    // Conserved energy-like and angular-momentum quantities (see conserved.hpp).
    double E = 0.0;
    double L = 0.0;

    void sync_cartesian_from_spherical();
    void sync_spherical_from_cartesian();
};

/// Build a ray from Cartesian position + direction, seeding E and L like the
/// legacy geodesic.comp / CPU-geodesic.cpp convention (null condition at init).
RayState make_ray_from_cartesian(double px, double py, double pz,
                                 double dx, double dy, double dz,
                                 double rs);

}  // namespace bh
