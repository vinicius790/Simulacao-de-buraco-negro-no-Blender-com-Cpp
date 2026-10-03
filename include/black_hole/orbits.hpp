#pragma once
// Circular timelike geodesics (Keplerian disk matter) in Schwarzschild spacetime.
//
// Geometric units G = c = 1 with M = rs / 2. All radii are Schwarzschild
// coordinate radii. Stable circular orbits exist only for r > 6M = 3 rs;
// unstable ones down to the photon sphere r = 3M = 1.5 rs.
//
// Formulas (standard, e.g. Bardeen 1972 / MTW §25):
//   Ω      = sqrt(M / r^3)                      (dφ/dt, coordinate angular velocity)
//   u^t    = 1 / sqrt(1 - 3M/r)                 (dt/dτ)
//   E~     = (1 - 2M/r) / sqrt(1 - 3M/r)        (specific energy, per unit mass)
//   L~     = sqrt(M r) / sqrt(1 - 3M/r)         (specific angular momentum)
//   v_loc  = sqrt(M / (r - 2M))                 (speed measured by a static observer)
//   T_coord = 2π / Ω                            (coordinate orbital period)
//
// These feed the relativistic disk shading (redshift.hpp / disk_emission.hpp).
// Novikov–Thorne *structure* (thickness, pressure) is NOT modelled; only the
// orbital kinematics and the Page–Thorne flux profile are.

#include <cmath>

namespace bh {
namespace orbits {

/// Geometric mass M = rs / 2.
inline double mass_from_rs(double rs) { return 0.5 * rs; }

/// Coordinate angular velocity Ω = sqrt(M / r³) of a circular orbit.
inline double angular_velocity(double r, double rs) {
    const double M = mass_from_rs(rs);
    return std::sqrt(M / (r * r * r));
}

/// True if a circular orbit at r is timelike (r > 3M = 1.5 rs).
inline bool circular_orbit_exists(double r, double rs) {
    return r > 1.5 * rs;
}

/// True if the circular orbit at r is stable (r ≥ 6M = 3 rs).
inline bool circular_orbit_is_stable(double r, double rs) {
    return r >= 3.0 * rs;
}

/// Lorentz-like factor u^t = dt/dτ = 1/sqrt(1 − 3M/r). Diverges at the photon sphere.
inline double u_t(double r, double rs) {
    const double M = mass_from_rs(rs);
    const double arg = 1.0 - 3.0 * M / r;
    return (arg > 0.0) ? 1.0 / std::sqrt(arg) : 0.0;
}

/// Specific energy Ẽ = (1 − 2M/r)/sqrt(1 − 3M/r). At ISCO: sqrt(8/9) ≈ 0.9428.
inline double specific_energy(double r, double rs) {
    const double M = mass_from_rs(rs);
    const double arg = 1.0 - 3.0 * M / r;
    return (arg > 0.0) ? (1.0 - 2.0 * M / r) / std::sqrt(arg) : 0.0;
}

/// Specific angular momentum L̃ = sqrt(M r)/sqrt(1 − 3M/r). At ISCO: 2√3 M.
inline double specific_angular_momentum(double r, double rs) {
    const double M = mass_from_rs(rs);
    const double arg = 1.0 - 3.0 * M / r;
    return (arg > 0.0) ? std::sqrt(M * r) / std::sqrt(arg) : 0.0;
}

/// Orbital speed measured by a static observer: v = sqrt(M/(r − 2M)).
/// Equals 0.5 c at ISCO and 1 c at the photon sphere.
inline double local_orbital_speed(double r, double rs) {
    const double M = mass_from_rs(rs);
    const double denom = r - 2.0 * M;
    return (denom > 0.0) ? std::sqrt(M / denom) : 1.0;
}

/// Coordinate orbital period T = 2π/Ω (in the same length unit as r, with c = 1).
inline double coordinate_period(double r, double rs) {
    return 2.0 * 3.14159265358979323846 / angular_velocity(r, rs);
}

/// Radiative efficiency of a Schwarzschild thin disk: η = 1 − Ẽ(ISCO) = 1 − sqrt(8/9).
inline double thin_disk_efficiency() {
    return 1.0 - std::sqrt(8.0 / 9.0);
}

}  // namespace orbits
}  // namespace bh
