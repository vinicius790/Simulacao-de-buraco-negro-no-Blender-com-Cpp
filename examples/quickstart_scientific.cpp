// Scientific-library tour (no OpenGL). Build target: quickstart_scientific
//
//   cmake -S . -B build/scientific -DBLACK_HOLE_BUILD_GL=OFF && cmake --build build/scientific
//   ./build/scientific/quickstart_scientific
//
// Walks through every module of bh_scientific with Sagittarius A* numbers:
// characteristic radii, circular orbits, redshift, Page–Thorne disk, shadow,
// a traced photon (planar RK45) compared with the weak-field formulas, and the
// analytic Kerr radii (annotation only — Kerr is NOT simulated).

#include "black_hole/disk_emission.hpp"
#include "black_hole/escape.hpp"
#include "black_hole/kerr_analytic.hpp"
#include "black_hole/orbits.hpp"
#include "black_hole/planar_geodesic.hpp"
#include "black_hole/redshift.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"
#include "black_hole/weak_field.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr double PI = 3.14159265358979323846;

void section(const char* title) { std::printf("\n== %s ==\n", title); }

}  // namespace

int main() {
    const double rs = bh::units::sagittarius_a_rs_si();
    const double M = bh::units::mass_parameter_from_rs(rs);
    const double G = bh::units::G_SI, c = bh::units::C_SI, M_kg = bh::units::SAGITTARIUS_A_MASS_KG;

    section("Sagittarius A* (legacy mass mapping)");
    std::printf("  M        = %.4e kg\n", M_kg);
    std::printf("  rs       = %.6e m   (shader literal 1.269e10)\n", rs);
    std::printf("  M_geom   = %.6e m   (rs/2)\n", M);
    std::printf("  r_photon = %.6e m   (1.5 rs = 3 M)\n", bh::photon_sphere_radius(rs));
    std::printf("  r_ISCO   = %.6e m   (3 rs = 6 M)\n", bh::isco_radius(rs));
    std::printf("  b_c      = %.6e m   (%.4f rs, shadow edge)\n", bh::escape::critical_impact_parameter(rs),
                bh::escape::critical_impact_parameter(1.0));

    section("Circular orbits (geo units, rs = 1)");
    std::printf("  %-6s %-10s %-10s %-10s %-8s\n", "r/rs", "v_local/c", "E~", "L~/M", "stable");
    for (double r : {1.6, 2.2, 3.0, 4.0, 5.2, 10.0}) {
        std::printf("  %-6.2f %-10.4f %-10.5f %-10.4f %-8s\n", r, bh::orbits::local_orbital_speed(r, 1.0),
                    bh::orbits::specific_energy(r, 1.0), bh::orbits::specific_angular_momentum(r, 1.0) / 0.5,
                    bh::orbits::circular_orbit_is_stable(r, 1.0) ? "yes" : "no");
    }
    std::printf("  thin-disk efficiency eta = 1 - sqrt(8/9) = %.4f\n", bh::orbits::thin_disk_efficiency());

    section("Redshift of disk matter at the ISCO (r = 3 rs)");
    const double g_static = bh::redshift::gravitational_factor(3.0, 1.0);
    const double g_transverse = bh::redshift::doppler_gravitational_factor(3.0, 1.0, 1.0, 0.0);
    std::printf("  static emitter        g = %.4f\n", g_static);
    std::printf("  orbiting, transverse  g = %.4f  (1/u^t)\n", g_transverse);
    std::printf("  approaching (L=-2.6)  g = %.4f  -> I x %.2f\n",
                bh::redshift::doppler_gravitational_factor(3.0, 1.0, 1.0, -2.6),
                bh::redshift::intensity_boost(bh::redshift::doppler_gravitational_factor(3.0, 1.0, 1.0, -2.6)));
    std::printf("  receding    (L=+2.6)  g = %.4f  -> I x %.2f\n",
                bh::redshift::doppler_gravitational_factor(3.0, 1.0, 1.0, 2.6),
                bh::redshift::intensity_boost(bh::redshift::doppler_gravitational_factor(3.0, 1.0, 1.0, 2.6)));

    section("Page-Thorne thin disk at 1% Eddington");
    const double eta = bh::orbits::thin_disk_efficiency();
    const double L_edd = bh::disk_emission::eddington_luminosity_si(M_kg, G, c);
    const double mdot = 0.01 * bh::disk_emission::eddington_accretion_rate_si(M_kg, G, c, eta);
    std::printf("  L_Edd = %.3e W, Mdot = %.3e kg/s\n", L_edd, mdot);
    double best_T = 0.0, best_r = 0.0;
    for (double r = 3.0; r < 20.0; r += 0.01) {
        const double T = bh::disk_emission::effective_temperature(
            bh::disk_emission::page_thorne_flux_si(r * rs, M_kg, mdot, G, c));
        if (T > best_T) { best_T = T; best_r = r; }
    }
    std::printf("  T_eff peak = %.0f K at r = %.2f rs (zero at the ISCO)\n", best_T, best_r);
    double cr, cg, cb;
    bh::disk_emission::blackbody_rgb(best_T, cr, cg, cb);
    std::printf("  blackbody colour at peak (linear sRGB, max=1): (%.2f, %.2f, %.2f)\n", cr, cg, cb);

    section("Shadow");
    for (double r_obs : {4.997, 18.0, 1000.0}) {
        const double a = bh::escape::shadow_angular_radius(1.0, r_obs);
        std::printf("  observer at %7.3f rs: shadow radius %.4f rad (%.2f deg)\n", r_obs, a, a * 180.0 / PI);
    }

    section("A traced photon vs weak-field theory (planar RK45, b = 50 rs)");
    const double b = 50.0, r0 = 4000.0;
    bh::PlanarRay ray = bh::make_planar_ray({{-std::sqrt(r0 * r0 - b * b), 0.0, b}}, {{1.0, 0.0, 0.0}}, 1.0);
    bh::Rk45Options opt;
    opt.abs_tol = opt.rel_tol = 1e-11;
    opt.h_max = 5.0;
    double h = 0.01;
    int steps = 0;
    while (!(bh::planar_dr(ray) > 0.0 && bh::planar_radius(ray) > r0) && steps < 1000000) {
        h = bh::planar_rk45_step(ray, h, 1.0, opt).h_next;
        ++steps;
    }
    const bh::Vec3d v = bh::planar_velocity(ray);
    const double alpha = std::acos(std::clamp(v[0] / std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), -1.0, 1.0));
    std::printf("  numeric   alpha = %.7f rad (%d adaptive steps)\n", alpha, steps);
    std::printf("  1st order alpha = %.7f rad (2 rs/b)\n", bh::weak_field::deflection_angle(1.0, b));
    std::printf("  2nd order alpha = %.7f rad (+ 15pi/16 (rs/b)^2)\n", bh::weak_field::deflection_angle_second_order(1.0, b));
    std::printf("  null constraint drift = %.2e\n", std::abs(bh::planar_null_constraint(ray, 1.0)));

    section("Kerr (ANALYTIC radii only - not simulated)");
    std::printf("  %-6s %-9s %-11s %-11s %-8s\n", "a*", "r+/M", "ISCO_pro/M", "ISCO_ret/M", "eta_pro");
    for (double a : {0.0, 0.5, 0.9, 0.998}) {
        std::printf("  %-6.3f %-9.4f %-11.4f %-11.4f %-8.4f\n", a, bh::kerr::outer_horizon(1.0, a),
                    bh::kerr::isco_radius(1.0, a, true), bh::kerr::isco_radius(1.0, a, false),
                    bh::kerr::thin_disk_efficiency(1.0, a, true));
    }
    return 0;
}
