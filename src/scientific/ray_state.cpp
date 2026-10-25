#include "black_hole/ray_state.hpp"
#include "black_hole/conserved.hpp"

#include <cmath>

namespace bh {

void RayState::sync_cartesian_from_spherical() {
    x = r * std::sin(theta) * std::cos(phi);
    y = r * std::sin(theta) * std::sin(phi);
    z = r * std::cos(theta);
}

void RayState::sync_spherical_from_cartesian() {
    r = std::sqrt(x * x + y * y + z * z);
    if (r == 0.0) {
        theta = 0.0;
        phi = 0.0;
        return;
    }
    // Clamp for acos domain safety.
    double z_over_r = z / r;
    if (z_over_r > 1.0) z_over_r = 1.0;
    if (z_over_r < -1.0) z_over_r = -1.0;
    theta = std::acos(z_over_r);
    phi = std::atan2(y, x);
}

RayState make_ray_from_cartesian(double px, double py, double pz,
                                 double dx, double dy, double dz,
                                 double rs) {
    RayState ray;
    ray.x = px;
    ray.y = py;
    ray.z = pz;
    ray.sync_spherical_from_cartesian();

    const double st = std::sin(ray.theta);
    const double ct = std::cos(ray.theta);
    const double sp = std::sin(ray.phi);
    const double cp = std::cos(ray.phi);

    ray.dr = st * cp * dx + st * sp * dy + ct * dz;
    ray.dtheta = (ct * cp * dx + ct * sp * dy - st * dz) / ray.r;
    ray.dphi = (-sp * dx + cp * dy) / (ray.r * st);

    ray.L = angular_momentum(ray);
    ray.E = energy_from_null_condition(ray, rs);
    return ray;
}

}  // namespace bh
