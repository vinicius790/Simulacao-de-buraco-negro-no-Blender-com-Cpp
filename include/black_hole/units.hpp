#pragma once
// Geometric-unit helpers for Schwarzschild reference work (G = c = 1).
// SI mapping uses the Sagittarius A* mass adopted by the legacy visual baseline.

#include <cmath>

namespace bh {
namespace units {

/// Newtonian gravitational constant (CODATA-style value used by the legacy code).
inline constexpr double G_SI = 6.67430e-11;  // m^3 kg^-1 s^-2

/// Speed of light in vacuum (exact SI definition).
inline constexpr double C_SI = 299792458.0;  // m/s

/// Sagittarius A* mass used by CPU-geodesic.cpp / the visual baseline.
inline constexpr double SAGITTARIUS_A_MASS_KG = 8.54e36;

/// Rounded Schwarzschild radius hardcoded in geodesic.comp (metres).
/// The G/c/M formula with SAGITTARIUS_A_MASS_KG yields ≈ 1.26839e10 m;
/// the shader keeps the historical rounded literal 1.269e10.
inline constexpr double LEGACY_SAGA_RS_M = 1.269e10;

/// Schwarzschild radius rs = 2 G M / c^2 for a given mass (SI metres).
inline constexpr double schwarzschild_radius_si(double mass_kg) {
    return 2.0 * G_SI * mass_kg / (C_SI * C_SI);
}

/// Legacy Sag A* rs used in geodesic.comp (≈ 1.269e10 m).
inline constexpr double sagittarius_a_rs_si() {
    return schwarzschild_radius_si(SAGITTARIUS_A_MASS_KG);
}

/// Geometric mass parameter M such that rs = 2 M (G = c = 1).
inline constexpr double mass_parameter_from_rs(double rs) {
    return 0.5 * rs;
}

/// Convert a length measured in units of M into SI metres (given rs = 2M).
inline constexpr double length_from_M(double value_in_M, double rs) {
    const double M = mass_parameter_from_rs(rs);
    return value_in_M * M;
}

/// Convert a SI length into units of M.
inline constexpr double M_from_length(double length_si, double rs) {
    const double M = mass_parameter_from_rs(rs);
    return length_si / M;
}

/// Convert geometric time (units of M, G=c=1) to SI seconds.
inline constexpr double seconds_from_M(double value_in_M, double rs) {
    // With c = 1, length and time share the same geometric unit.
    return length_from_M(value_in_M, rs) / C_SI;
}

}  // namespace units
}  // namespace bh
