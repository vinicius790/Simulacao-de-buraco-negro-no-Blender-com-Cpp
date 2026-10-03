#pragma once
// Thin-disk radiative flux (Page & Thorne 1974, Schwarzschild limit) and a
// blackbody colour model used by the SCIENTIFIC rendering mode.
//
// Honesty contract:
//   * This is the classical steady, geometrically thin, optically thick disk
//     with zero torque at the ISCO (r_in = 6M). It gives the emitted flux F(r)
//     and an effective temperature T_eff = (F/σ)^{1/4}.
//   * No radiative transfer, no plasma physics, no corona, no spectral lines.
//   * The default visual baseline (geodesic.comp / Blender) is unchanged. The
//     flux / temperature / blackbody functions feed only `bh_render_cpu --mode
//     blackbody`; `--mode relativistic` uses only tonemap_reinhard, and the
//     scientific compute shader uses none of this module.
//
// Closed form (derived in docs/DISCO_RELATIVISTICO.md and checked numerically
// against the Page–Thorne integral in tests/test_scientific_ref.cpp), with
// x = sqrt(r/M):
//
//   F(r) = 3 G M Ṁ / (8π r³) · R(x)
//   R(x) = (1 / (x (1 − 3/x²))) · [ x − √6 + (√3/2) ln( ((x+√3)(√6−√3)) / ((x−√3)(√6+√3)) ) ]
//
//   R(x) → 1 − C/x as x → ∞ with C = √6 − (√3/2) ln((√6−√3)/(√6+√3)) ≈ 3.976
//   (the Newtonian zero-torque disk gives 1 − √6/x: same leading behaviour,
//   different coefficient because of the relativistic log term).
//   F(6M) = 0; peak near r ≈ 9.6 M.

#include <cmath>

namespace bh {
namespace disk_emission {

inline constexpr double SIGMA_SB_SI = 5.670374419e-8;  // W m^-2 K^-4
inline constexpr double PROTON_MASS_KG = 1.67262192e-27;
inline constexpr double THOMSON_CROSS_SECTION_M2 = 6.6524587e-29;

/// Dimensionless relativistic correction R(x), x = sqrt(r/M). Zero for r ≤ 6M.
inline double page_thorne_correction(double r_over_M) {
    if (r_over_M <= 6.0) {
        return 0.0;
    }
    const double x = std::sqrt(r_over_M);
    const double s3 = std::sqrt(3.0);
    const double s6 = std::sqrt(6.0);
    const double log_arg = ((x + s3) * (s6 - s3)) / ((x - s3) * (s6 + s3));
    const double bracket = x - s6 + 0.5 * s3 * std::log(log_arg);
    return bracket / (x * (1.0 - 3.0 / (x * x)));
}

/// Emitted flux in SI (W/m²) at coordinate radius r_m for mass M_kg and accretion
/// rate mdot_kg_s. Uses G, c passed explicitly to stay unit-honest.
inline double page_thorne_flux_si(double r_m, double mass_kg, double mdot_kg_s,
                                  double G, double c) {
    const double M_geo = G * mass_kg / (c * c);  // metres
    if (r_m <= 6.0 * M_geo) {
        return 0.0;
    }
    const double newtonian = 3.0 * G * mass_kg * mdot_kg_s / (8.0 * 3.14159265358979323846 * r_m * r_m * r_m);
    return newtonian * page_thorne_correction(r_m / M_geo);
}

/// Effective temperature T = (F/σ)^{1/4}.
inline double effective_temperature(double flux_si) {
    return (flux_si > 0.0) ? std::pow(flux_si / SIGMA_SB_SI, 0.25) : 0.0;
}

/// Eddington luminosity L_Edd = 4π G M m_p c / σ_T (W).
inline double eddington_luminosity_si(double mass_kg, double G, double c) {
    return 4.0 * 3.14159265358979323846 * G * mass_kg * PROTON_MASS_KG * c /
           THOMSON_CROSS_SECTION_M2;
}

/// Eddington accretion rate Ṁ_Edd = L_Edd / (η c²) for radiative efficiency η.
inline double eddington_accretion_rate_si(double mass_kg, double G, double c, double efficiency) {
    return eddington_luminosity_si(mass_kg, G, c) / (efficiency * c * c);
}

/// Linear-sRGB colour (max component normalised to 1) of a blackbody at T kelvin
/// using the Planckian-locus approximation (Kim et al. 2002; valid 1667–25000 K,
/// clamped outside). Output components are ≥ 0.
inline void blackbody_rgb(double T, double& r, double& g, double& b) {
    if (T < 1667.0) T = 1667.0;
    if (T > 25000.0) T = 25000.0;
    const double t = T;
    const double t2 = t * t;
    const double t3 = t2 * t;
    double x;
    if (t <= 4000.0) {
        x = -0.2661239e9 / t3 - 0.2343589e6 / t2 + 0.8776956e3 / t + 0.179910;
    } else {
        x = -3.0258469e9 / t3 + 2.1070379e6 / t2 + 0.2226347e3 / t + 0.240390;
    }
    const double x2 = x * x;
    const double x3 = x2 * x;
    double y;
    if (t <= 2222.0) {
        y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    } else if (t <= 4000.0) {
        y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    } else {
        y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
    }
    // xyY (Y = 1) → XYZ → linear sRGB (D65).
    const double Y = 1.0;
    const double X = (y > 0.0) ? x * Y / y : 0.0;
    const double Z = (y > 0.0) ? (1.0 - x - y) * Y / y : 0.0;
    r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    if (r < 0.0) r = 0.0;
    if (g < 0.0) g = 0.0;
    if (b < 0.0) b = 0.0;
    double m = r;
    if (g > m) m = g;
    if (b > m) m = b;
    if (m > 0.0) {
        r /= m;
        g /= m;
        b /= m;
    }
}

/// Simple Reinhard tone map c / (1 + c) with exposure.
inline double tonemap_reinhard(double c, double exposure = 1.0) {
    const double v = c * exposure;
    return (v > 0.0) ? v / (1.0 + v) : 0.0;
}

}  // namespace disk_emission
}  // namespace bh
