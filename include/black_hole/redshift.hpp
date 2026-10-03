#pragma once
// Gravitational + Doppler frequency shift for photons emitted by accretion-disk
// matter on circular Schwarzschild orbits and received by a distant observer.
//
// Convention (ray tracing is done BACKWARDS, camera → disk):
//   The traced null geodesic has conserved E = −k_t > 0 and Cartesian angular
//   momentum vector  L⃗ = r⃗ × (dr⃗/dλ)  (conserved because of spherical symmetry;
//   its component along any axis equals the covariant k_φ about that axis).
//   The physical photon is the time-reverse: p^t = k^t, p^i = −k^i, so
//   p_φ = −k_φ. For an emitter with 4-velocity u = u^t (1, 0, 0, Ω) about the
//   disk axis n̂ (right-hand rule), the emitted frequency is −p·u, giving
//
//       g_∞ = ν_∞ / ν_emit = 1 / ( u^t · (1 + Ω · (L⃗·n̂) / E) )
//
//   for a receiver at INFINITY. A static receiver (camera) at finite r_obs
//   measures ν_obs = E / sqrt(f(r_obs)), hence
//
//       g_obs = g_∞ / sqrt(1 − rs / r_obs)     (see observer_frequency_factor).
//
//   Static emitter (Ω = 0):  g = sqrt(1 − rs/r)  (pure gravitational redshift).
//   g < 1 ⇒ redshift (receding side), g > 1 ⇒ blueshift (approaching side).
//
// Bolometric intensity transforms as I_obs = g⁴ I_emit; colour temperature as
// T_obs = g · T_emit (Liouville / Lorentz invariance of I_ν/ν³).

#include "black_hole/orbits.hpp"
#include "black_hole/schwarzschild.hpp"

#include <cmath>

namespace bh {
namespace redshift {

/// Pure gravitational redshift factor for a static emitter at radius r:
/// g = sqrt(f) = sqrt(1 − rs/r). Returns 0 at/inside the horizon.
inline double gravitational_factor(double r, double rs) {
    const double f = lapse_factor(r, rs);
    return (f > 0.0) ? std::sqrt(f) : 0.0;
}

/// 1 + z for a static emitter: 1/sqrt(f).
inline double gravitational_one_plus_z(double r, double rs) {
    const double g = gravitational_factor(r, rs);
    return (g > 0.0) ? 1.0 / g : 0.0;
}

/// Combined gravitational + Doppler factor g = ν_obs/ν_emit for a photon hitting
/// a disk element on a circular orbit at radius r.
///
/// @param r            emission radius (coordinate)
/// @param rs           Schwarzschild radius
/// @param E            traced-ray conserved energy (−k_t)
/// @param L_along_axis traced-ray angular momentum component along the disk's
///                     rotation axis n̂, i.e. (r⃗ × dr⃗/dλ)·n̂
/// @param spin_sign    ≥ 0: matter rotates counter-clockwise about n̂; < 0 reversed.
///                     Only the SIGN is used (a Keplerian orbit has a fixed speed).
/// Returns g for a receiver at infinity (multiply by observer_frequency_factor
/// for a static camera at finite radius).
/// Falls back to the static-emitter factor when no circular orbit exists (r ≤ 1.5 rs).
inline double doppler_gravitational_factor(double r, double rs, double E,
                                           double L_along_axis, double spin_sign = 1.0) {
    if (E <= 0.0) {
        return 0.0;
    }
    if (!orbits::circular_orbit_exists(r, rs)) {
        return gravitational_factor(r, rs);
    }
    const double ut = orbits::u_t(r, rs);
    const double omega = (spin_sign < 0.0 ? -1.0 : 1.0) * orbits::angular_velocity(r, rs);
    const double denom = ut * (1.0 + omega * L_along_axis / E);
    return (denom > 0.0) ? 1.0 / denom : 0.0;
}

/// Frequency factor of a static receiver at r_obs relative to infinity:
/// ν_obs / ν_∞ = 1 / sqrt(1 − rs/r_obs)  (blueshift of light falling inwards).
inline double observer_frequency_factor(double r_obs, double rs) {
    const double g = gravitational_factor(r_obs, rs);
    return (g > 0.0) ? 1.0 / g : 0.0;
}

/// g measured by a static camera at r_obs (= g_∞ · observer_frequency_factor).
inline double doppler_gravitational_factor_at(double r, double rs, double E, double L_along_axis,
                                              double r_obs, double spin_sign = 1.0) {
    return doppler_gravitational_factor(r, rs, E, L_along_axis, spin_sign) *
           observer_frequency_factor(r_obs, rs);
}

/// Bolometric intensity boost I_obs / I_emit = g⁴.
inline double intensity_boost(double g) {
    const double g2 = g * g;
    return g2 * g2;
}

/// Observed colour temperature T_obs = g · T_emit.
inline double observed_temperature(double g, double T_emit) {
    return g * T_emit;
}

}  // namespace redshift
}  // namespace bh
