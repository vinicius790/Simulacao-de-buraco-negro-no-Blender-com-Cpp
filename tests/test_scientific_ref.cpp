// Headless scientific reference tests (no OpenGL / GLEW / GLFW).
#include "black_hole/camera_model.hpp"
#include "black_hole/conserved.hpp"
#include "black_hole/disk_model.hpp"
#include "black_hole/integrator_rk4.hpp"
#include "black_hole/ray_state.hpp"
#include "black_hole/scene_params.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"
#include "black_hole/weak_field.hpp"
#include "black_hole/orbits.hpp"
#include "black_hole/redshift.hpp"
#include "black_hole/disk_emission.hpp"
#include "black_hole/hit_testing.hpp"
#include "black_hole/escape.hpp"
#include "black_hole/integrator_rk45.hpp"
#include "black_hole/planar_geodesic.hpp"
#include "black_hole/kerr_analytic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++g_failures;
    } else {
        std::cout << "PASS: " << msg << "\n";
    }
}

void expect_near(double a, double b, double tol, const std::string& msg) {
    const double err = std::abs(a - b);
    if (err > tol) {
        std::cerr << "FAIL: " << msg << " (got " << a << ", expected " << b
                  << ", |err|=" << err << ", tol=" << tol << ")\n";
        ++g_failures;
    } else {
        std::cout << "PASS: " << msg << "\n";
    }
}

void test_photon_sphere_and_isco() {
    const double rs = 1.269e10;
    expect_near(bh::photon_sphere_radius(rs), 1.5 * rs, 0.0, "photon sphere = 1.5 rs");
    expect_near(bh::isco_radius(rs), 3.0 * rs, 0.0, "ISCO = 3 rs (= 6 M)");
    expect_near(bh::isco_in_units_of_M(), 6.0, 0.0, "ISCO = 6 M");
    expect_near(bh::photon_sphere_in_units_of_M(), 3.0, 0.0, "photon sphere = 3 M");
}

void test_units_sag_a() {
    const double rs = bh::units::sagittarius_a_rs_si();
    expect_near(rs, bh::units::LEGACY_SAGA_RS_M, 1e7,
                "Sag A* rs from M near legacy shader literal 1.269e10 m");
    expect_near(bh::units::LEGACY_SAGA_RS_M, 1.269e10, 0.0,
                "LEGACY_SAGA_RS_M equals geodesic.comp constant");
    expect_near(bh::units::mass_parameter_from_rs(rs), 0.5 * rs, 1e-9, "M = rs/2");
}

void test_weak_field_one_over_b() {
    const double rs = 1.0;
    const double b1 = 100.0;
    const double b2 = 200.0;
    const double a1 = bh::weak_field::deflection_angle(rs, b1);
    const double a2 = bh::weak_field::deflection_angle(rs, b2);
    expect_near(a1, 2.0 * rs / b1, 1e-15, "alpha = 2 rs / b");
    expect_near(a1 / a2, b2 / b1, 1e-12, "weak-field deflection scales as 1/b");
    expect_near(bh::weak_field::deflection_scale_ratio(rs, b1, b2), 2.0, 1e-12,
                "deflection_scale_ratio(b, 2b) == 2");
}

void test_conserved_definitions() {
    const double rs = 1.0;
    bh::RayState ray =
        bh::make_ray_from_cartesian(10.0 * rs, 0.0, 0.0, -1.0, 0.0, 0.0, rs);
    expect_near(ray.theta, 1.5707963267948966, 1e-9, "equatorial theta ≈ π/2");
    expect_near(ray.L, 0.0, 1e-12, "radial ray has L ≈ 0");
    expect(ray.E > 0.0, "radial null ray has E > 0");
    expect_near(bh::null_constraint(ray, rs), 0.0, 1e-9, "null constraint ≈ 0 at init");
    expect_near(bh::angular_momentum(ray), ray.L, 1e-15, "angular_momentum matches L");
}

void test_rk4_beats_euler_null_drift() {
    const double rs = 1.0;
    const double r0 = 20.0 * rs;
    const double d_lambda = 0.25;
    const int steps = 100;

    bh::RayState euler_ray =
        bh::make_ray_from_cartesian(r0, 0.0, 0.0, -0.75, 0.65, 0.0, rs);
    bh::RayState rk4_ray = euler_ray;
    const double null0 = bh::null_constraint_abs(euler_ray, rs);

    for (int i = 0; i < steps; ++i) {
        bh::euler_step(euler_ray, d_lambda, rs);
        bh::rk4_step(rk4_ray, d_lambda, rs);
        if (euler_ray.r <= 1.2 * rs || rk4_ray.r <= 1.2 * rs) {
            break;
        }
    }

    const double null_euler = bh::null_constraint_abs(euler_ray, rs);
    const double null_rk4 = bh::null_constraint_abs(rk4_ray, rs);
    std::cout << "  null |gkk| init=" << null0 << " euler=" << null_euler
              << " rk4=" << null_rk4 << "\n";

    expect(null0 < 1e-10, "seeded ray starts nearly null");
    expect(euler_ray.r > rs && rk4_ray.r > rs, "short march stays outside horizon");
    expect(null_rk4 <= null_euler * 1.05 + 1e-12,
           "RK4 null-constraint drift ≤ Euler drift");
}

void test_disk_factors_legacy() {
    const double rs = bh::units::LEGACY_SAGA_RS_M;
    expect_near(bh::disk::LEGACY_INNER_FACTOR_RS, 2.2, 0.0, "disk inner factor 2.2");
    expect_near(bh::disk::LEGACY_OUTER_FACTOR_RS, 5.2, 0.0, "disk outer factor 5.2");

    const bh::disk::Annulus a = bh::disk::legacy_annulus(rs);
    expect_near(a.inner_m, rs * 2.2, 1e-3, "annulus inner = 2.2 rs");
    expect_near(a.outer_m, rs * 5.2, 1e-3, "annulus outer = 5.2 rs");
    expect_near(a.thickness_m, 1.0e9, 0.0, "annulus thickness 1e9 m");
    expect(bh::disk::contains_cylindrical_radius(a, rs * 3.0), "rho=3 rs inside");
    expect(!bh::disk::contains_cylindrical_radius(a, rs * 1.5), "rho=1.5 rs outside");
    expect(!bh::disk::contains_cylindrical_radius(a, rs * 6.0), "rho=6 rs outside");

    double cr, cg, cb;
    const double r_norm = bh::disk::color_radial_param(a.outer_m, a);
    bh::disk::legacy_disk_rgb(r_norm, cr, cg, cb);
    expect_near(cr, 1.0, 0.0, "disk colour R=1");
    expect_near(cg, 1.0, 1e-12, "disk colour G=r_norm at outer edge");
    expect_near(cb, 0.2, 0.0, "disk colour B=0.2");
}

void test_camera_orbit_parity() {
    bh::camera::OrbitCamera cam;
    expect_near(cam.radius_m, 6.34194e10, 0.0, "default camera radius");
    expect_near(cam.elevation_rad, bh::camera::PI / 2.0, 0.0, "default elevation = π/2");
    expect_near(cam.azimuth_rad, 0.0, 0.0, "default azimuth = 0");

    double x, y, z;
    bh::camera::position(cam, x, y, z);
    expect_near(x, cam.radius_m, 1e-3, "equatorial az=0 → x = radius");
    expect_near(y, 0.0, 1e-3, "equatorial → y ≈ 0");
    expect_near(z, 0.0, 1e-3, "az=0 → z ≈ 0");

    cam.azimuth_rad = bh::camera::PI / 2.0;
    bh::camera::position(cam, x, y, z);
    expect_near(x, 0.0, 1e-3, "az=π/2 → x ≈ 0");
    expect_near(z, cam.radius_m, 1e-3, "az=π/2 → z = radius");

    expect_near(bh::camera::tan_half_fov(cam), std::tan(bh::camera::PI / 6.0), 1e-12,
                "tanHalfFov for 60° FOV");
    expect_near(bh::camera::clamp_elevation(0.0), bh::camera::ELEVATION_MIN, 0.0,
                "elevation clamp low");
    expect_near(bh::camera::clamp_elevation(bh::camera::PI), bh::camera::ELEVATION_MAX, 0.0,
                "elevation clamp high");

    const double rs = 1.269e10;
    const double dist = 4.0 * rs;
    const double expected = 2.0 * std::sqrt(rs * (dist - rs)) - 3.0e10;
    expect_near(bh::camera::grid_warp_delta_y(dist, rs), expected, 1e-3,
                "grid warp outside rs");
    expect_near(bh::camera::grid_warp_delta_y(0.5 * rs, rs), 2.0 * rs - 3.0e10, 1e-3,
                "grid warp deep pit inside rs");
}

void test_scene_params_roundtrip() {
    bh::SceneParams defaults = bh::make_default_scene_params();
    expect_near(defaults.disk_inner_factor_rs, 2.2, 0.0, "scene default inner 2.2");
    expect_near(defaults.disk_outer_factor_rs, 5.2, 0.0, "scene default outer 5.2");
    expect(defaults.gravity == false, "scene default Gravity OFF");
    expect(defaults.window_w == 800 && defaults.window_h == 600, "scene window 800x600");
    expect(defaults.compute_w == 200 && defaults.compute_h == 150, "scene compute 200x150");
    expect(defaults.objects.size() == 3, "scene default 3 objects");

    const std::string json = bh::scene_params_to_json(defaults);
    bh::SceneParams loaded;
    std::string err;
    expect(bh::scene_params_from_json(json, loaded, err), "roundtrip parse");
    if (!err.empty()) {
        std::cerr << "  parse err: " << err << "\n";
    }
    expect_near(loaded.camera.radius_m, defaults.camera.radius_m, 1.0, "roundtrip radius");
    expect_near(loaded.disk_inner_factor_rs, 2.2, 1e-12, "roundtrip disk inner");
    expect_near(loaded.disk_outer_factor_rs, 5.2, 1e-12, "roundtrip disk outer");
    expect(loaded.objects.size() == defaults.objects.size(), "roundtrip object count");

    const std::string fragment =
        R"({"schema":"black_hole.scene_params/v1",
            "disk":{"inner_factor_rs":2.2,"outer_factor_rs":5.2},
            "camera":{"radius_m":6.34194e10,"azimuth_rad":0.5,"elevation_rad":1.2},
            "gravity":true})";
    bh::SceneParams frag;
    expect(bh::scene_params_from_json(fragment, frag, err), "parse restricted fragment");
    expect(frag.gravity == true, "fragment sets Gravity");
    expect_near(frag.camera.azimuth_rad, 0.5, 1e-12, "fragment azimuth");
    expect_near(frag.camera.elevation_rad, 1.2, 1e-12, "fragment elevation");

    const auto annulus = bh::scene_disk_annulus(defaults);
    expect_near(annulus.inner_m, defaults.r_s_m * 2.2, 1.0, "scene_disk_annulus inner");
}

// ---------------------------------------------------------------------------
// 0.8.0 — scientific expansion tests
// ---------------------------------------------------------------------------

void test_circular_orbits() {
    const double rs = 2.0;  // M = 1
    expect_near(bh::orbits::local_orbital_speed(6.0, rs), 0.5, 1e-12, "v(ISCO) = 0.5 c");
    expect_near(bh::orbits::local_orbital_speed(3.0, rs), 1.0, 1e-12, "v(photon sphere) = c");
    expect_near(bh::orbits::specific_energy(6.0, rs), std::sqrt(8.0 / 9.0), 1e-12, "E(ISCO) = sqrt(8/9)");
    expect_near(bh::orbits::specific_angular_momentum(6.0, rs), 2.0 * std::sqrt(3.0), 1e-12, "L(ISCO) = 2√3 M");
    expect_near(bh::orbits::angular_velocity(6.0, rs), std::sqrt(1.0 / 216.0), 1e-15, "Ω = sqrt(M/r³)");
    expect_near(bh::orbits::thin_disk_efficiency(), 1.0 - std::sqrt(8.0 / 9.0), 1e-15, "η = 1 − sqrt(8/9) ≈ 5.7%");
    // ISCO = minimum of E(r): E is larger on both sides of r = 6M.
    expect(bh::orbits::specific_energy(5.5, rs) > bh::orbits::specific_energy(6.0, rs) &&
               bh::orbits::specific_energy(6.5, rs) > bh::orbits::specific_energy(6.0, rs),
           "specific energy has its minimum at the ISCO");
    expect(!bh::orbits::circular_orbit_exists(2.9, rs) && bh::orbits::circular_orbit_exists(3.1, rs),
           "circular orbits exist only outside the photon sphere");
}

void test_redshift_factors() {
    const double rs = 1.0;
    expect_near(bh::redshift::gravitational_factor(4.0, rs), std::sqrt(0.75), 1e-15, "static emitter g = sqrt(f)");
    expect_near(bh::redshift::gravitational_one_plus_z(4.0, rs), 1.0 / std::sqrt(0.75), 1e-15, "1+z = 1/sqrt(f)");
    // Photon with zero angular momentum about the disk axis: pure gravitational + transverse Doppler.
    const double r = 3.0;  // = 6 M (ISCO)
    const double g0 = bh::redshift::doppler_gravitational_factor(r, rs, 1.0, 0.0);
    expect_near(g0, 1.0 / bh::orbits::u_t(r, rs), 1e-15, "L_axis = 0 → g = 1/u^t");
    // Opposite angular momenta give blue/red shifts on either side of 1/u^t.
    const double gp = bh::redshift::doppler_gravitational_factor(r, rs, 1.0, +1.0);
    const double gm = bh::redshift::doppler_gravitational_factor(r, rs, 1.0, -1.0);
    expect(gp < g0 && gm > g0, "sign of L_axis selects red vs blue shift");
    expect_near(bh::redshift::doppler_gravitational_factor(r, rs, 1.0, 1.0, -1.0), gm, 1e-15,
                "reversing disk spin mirrors the shift");
    expect_near(bh::redshift::intensity_boost(2.0), 16.0, 0.0, "I_obs/I_emit = g⁴");
    // Inside the photon sphere: falls back to the static factor.
    expect_near(bh::redshift::doppler_gravitational_factor(1.2, rs, 1.0, 1.0), std::sqrt(1.0 - 1.0 / 1.2), 1e-15,
                "no circular orbit → static redshift");
}

double page_thorne_integral_numeric(double r_over_M) {
    // Direct quadrature of the Page–Thorne integral (M = 1):
    //   R(r) = (8π r³ / (3 M Ṁ)) · F,  F = −Ṁ/(4π r) · Ω'/(E−ΩL)² · ∫_{6}^{r} (E−ΩL) L' dr
    const double rs = 2.0;
    const int n = 20000;
    const double a = 6.0, b = r_over_M;
    const double h = (b - a) / n;
    auto integrand = [&](double x) {
        const double dl = (bh::orbits::specific_angular_momentum(x + 1e-6, rs) -
                           bh::orbits::specific_angular_momentum(x - 1e-6, rs)) / 2e-6;
        const double eol = bh::orbits::specific_energy(x, rs) -
                           bh::orbits::angular_velocity(x, rs) * bh::orbits::specific_angular_momentum(x, rs);
        return eol * dl;
    };
    double sum = 0.5 * (integrand(a + 1e-9) + integrand(b));
    for (int i = 1; i < n; ++i) sum += integrand(a + i * h);
    sum *= h;
    const double r = b;
    const double dOmega = -1.5 * std::pow(r, -2.5);
    const double eol2 = std::pow(bh::orbits::specific_energy(r, rs) -
                                     bh::orbits::angular_velocity(r, rs) * bh::orbits::specific_angular_momentum(r, rs),
                                 2.0);
    const double F_over_mdot = -(1.0 / (4.0 * 3.14159265358979323846 * r)) * dOmega / eol2 * sum;
    return F_over_mdot * (8.0 * 3.14159265358979323846 * r * r * r / 3.0);
}

void test_page_thorne_flux() {
    expect_near(bh::disk_emission::page_thorne_correction(6.0), 0.0, 1e-15, "flux vanishes at the ISCO");
    expect(bh::disk_emission::page_thorne_correction(5.0) == 0.0, "no emission inside the ISCO");
    for (double r : {7.0, 9.0, 12.0, 30.0}) {
        expect_near(bh::disk_emission::page_thorne_correction(r), page_thorne_integral_numeric(r), 2e-3,
                    "closed form matches Page–Thorne quadrature at r = " + std::to_string(r) + " M");
    }
    // Far from the hole R → 1 − C/x with C = √6 − (√3/2) ln((√6−√3)/(√6+√3)) ≈ 3.976
    // (the Newtonian 1 − sqrt(6M/r) is only the leading term).
    {
        const double s3 = std::sqrt(3.0), s6 = std::sqrt(6.0);
        const double C = s6 - 0.5 * s3 * std::log((s6 - s3) / (s6 + s3));
        expect_near(bh::disk_emission::page_thorne_correction(1e6), 1.0 - C / 1e3, 2e-5,
                    "far-field expansion R → 1 − C/sqrt(r/M)");
        expect(bh::disk_emission::page_thorne_correction(1e6) < 1.0, "R < 1 everywhere");
    }
    // Flux peak between 8 M and 12 M.
    double best_r = 0.0, best = 0.0;
    for (double r = 6.0; r < 40.0; r += 0.01) {
        const double F = bh::disk_emission::page_thorne_correction(r) / (r * r * r);
        if (F > best) { best = F; best_r = r; }
    }
    expect(best_r > 8.0 && best_r < 12.0, "flux peaks near r ≈ 9.6 M (got " + std::to_string(best_r) + ")");

    const double G = bh::units::G_SI, c = bh::units::C_SI;
    const double M = bh::units::SAGITTARIUS_A_MASS_KG;
    const double L_edd = bh::disk_emission::eddington_luminosity_si(M, G, c);
    expect(L_edd > 1e37 && L_edd < 1e38, "Sgr A* Eddington luminosity ≈ 5e37 W");
    const double mdot = 0.01 * bh::disk_emission::eddington_accretion_rate_si(M, G, c, bh::orbits::thin_disk_efficiency());
    const double F = bh::disk_emission::page_thorne_flux_si(9.6 * 0.5 * bh::units::LEGACY_SAGA_RS_M, M, mdot, G, c);
    const double T = bh::disk_emission::effective_temperature(F);
    expect(T > 5e4 && T < 5e5, "1% Eddington thin disk around Sgr A* peaks at ~1e5 K (got " + std::to_string(T) + ")");

    double r, g, b;
    bh::disk_emission::blackbody_rgb(6500.0, r, g, b);
    expect(std::abs(r - g) < 0.15 && std::abs(g - b) < 0.15, "6500 K is near white");
    bh::disk_emission::blackbody_rgb(2000.0, r, g, b);
    expect(r > g && g > b, "2000 K is red/orange");
    bh::disk_emission::blackbody_rgb(20000.0, r, g, b);
    expect(b > r, "20000 K is blue");
    expect_near(bh::disk_emission::tonemap_reinhard(1.0), 0.5, 1e-15, "Reinhard(1) = 0.5");
}

void test_hit_testing() {
    using bh::hit::Vec3;
    double t;
    Vec3 p;
    expect(bh::hit::segment_plane_crossing({{1.0, 1.0, 0.0}}, {{3.0, -1.0, 0.0}}, 1, t, p), "plane crossing detected");
    expect_near(t, 0.5, 1e-15, "crossing parameter interpolated");
    expect_near(p[0], 2.0, 1e-15, "crossing x interpolated");
    expect(p[1] == 0.0, "crossing lies exactly on the plane");
    expect(!bh::hit::segment_plane_crossing({{1.0, 1.0, 0.0}}, {{3.0, 2.0, 0.0}}, 1, t, p), "no crossing when same side");
    double rho;
    expect(bh::hit::point_in_annulus({{3.0, 0.0, 4.0}}, 1, 2.2, 5.2, rho) && std::abs(rho - 5.0) < 1e-15,
           "annulus test uses cylindrical radius");
    expect(!bh::hit::point_in_annulus({{1.0, 0.0, 1.0}}, 1, 2.2, 5.2, rho), "inside inner edge rejected");

    expect(bh::hit::segment_sphere_hit({{-5.0, 0.5, 0.0}}, {{5.0, 0.5, 0.0}}, {{0.0, 0.0, 0.0}}, 1.0, t), "segment hits sphere");
    expect_near(t, (5.0 - std::sqrt(0.75)) / 10.0, 1e-12, "first sphere intersection parameter");
    expect(!bh::hit::segment_sphere_hit({{-5.0, 2.0, 0.0}}, {{5.0, 2.0, 0.0}}, {{0.0, 0.0, 0.0}}, 1.0, t), "segment misses sphere");
    expect(!bh::hit::segment_sphere_hit({{2.0, 0.0, 0.0}}, {{5.0, 0.0, 0.0}}, {{0.0, 0.0, 0.0}}, 1.0, t), "sphere behind segment");
    expect(bh::hit::segment_sphere_hit({{0.5, 0.0, 0.0}}, {{5.0, 0.0, 0.0}}, {{0.0, 0.0, 0.0}}, 1.0, t) && t == 0.0,
           "segment starting inside reports t = 0");
}

// Integrate a planar ray until capture or until it is outgoing past r_stop.
// Returns +1 if escaped, -1 if captured.
int march_until_capture_or_escape(bh::PlanarRay& ray, double rs, double r_stop, bool adaptive) {
    bh::Rk45Options opt;
    opt.abs_tol = 1e-11;
    opt.rel_tol = 1e-11;
    opt.h_min = 1e-6;
    opt.h_max = 5.0;
    double h = 0.01;
    for (int i = 0; i < 2000000; ++i) {
        if (bh::planar_radius(ray) <= rs * 1.0001) return -1;
        if (adaptive) {
            h = bh::planar_rk45_step(ray, h, rs, opt).h_next;
        } else {
            bh::planar_rk4_step(ray, bh::geometric_step(bh::planar_radius(ray), rs, 0.01, 0.002, 1.0), rs);
        }
        if (bh::escape::is_outgoing_beyond_photon_sphere(bh::planar_radius(ray), bh::planar_dr(ray), rs) &&
            bh::planar_radius(ray) > r_stop) {
            return +1;
        }
    }
    return 0;
}

void test_critical_capture_escape() {
    const double rs = 1.0;
    const double r0 = 60.0;
    const double bc = bh::escape::critical_impact_parameter(rs);
    expect_near(bc, 1.5 * std::sqrt(3.0), 1e-15, "b_c = (3√3/2) rs");

    for (double b : {2.0, 2.5, 2.7, 3.2}) {
        // Start at x = −r0 heading +x, offset b along z (impact parameter at infinity ≈ b).
        bh::PlanarRay ray = bh::make_planar_ray({{-std::sqrt(r0 * r0 - b * b), 0.0, b}}, {{1.0, 0.0, 0.0}}, rs);
        const double b_eff = bh::escape::impact_parameter(ray.E, ray.L);
        const int outcome = march_until_capture_or_escape(ray, rs, r0, true);
        const bool should_capture = bh::escape::captured_from_infinity(b_eff, rs);
        expect(outcome == (should_capture ? -1 : +1),
               "b = " + std::to_string(b) + " rs (b_eff " + std::to_string(b_eff) + "): " +
                   (should_capture ? "captured" : "escaped") + " as predicted by b_c");
    }
    expect(bh::escape::will_escape_scene(10.0, 0.1, rs, 6.0) && !bh::escape::will_escape_scene(10.0, -0.1, rs, 6.0) &&
               !bh::escape::will_escape_scene(1.4, 0.1, rs, 1.0),
           "escape needs outgoing, beyond photon sphere and beyond the scene");
    expect_near(bh::escape::shadow_angular_radius(rs, 1e6) / (bc / 1e6), 1.0, 1e-5, "far shadow radius → b_c / r");
    expect_near(bh::escape::shadow_angular_radius(rs, 5.0), std::asin(bc * std::sqrt(0.8) / 5.0), 1e-15,
                "near shadow radius sin α = b_c sqrt(f)/r");
}

void test_weak_field_numeric_deflection() {
    const double rs = 1.0;
    const double b = 50.0;
    const double r0 = 4000.0;
    bh::PlanarRay ray = bh::make_planar_ray({{-std::sqrt(r0 * r0 - b * b), 0.0, b}}, {{1.0, 0.0, 0.0}}, rs);
    const int outcome = march_until_capture_or_escape(ray, rs, r0, true);
    expect(outcome == +1, "weak-field ray escapes");
    const bh::Vec3d v = bh::planar_velocity(ray);
    const double vn = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const double alpha_num = std::acos(std::clamp(v[0] / vn, -1.0, 1.0));
    const double alpha_1 = bh::weak_field::deflection_angle(rs, b);
    const double alpha_2 = bh::weak_field::deflection_angle_second_order(rs, b);
    std::cout << "  deflection numeric=" << alpha_num << " first-order=" << alpha_1 << " second-order=" << alpha_2 << "\n";
    expect(std::abs(alpha_num - alpha_1) / alpha_1 < 0.05, "numeric deflection within 5% of 2 rs / b");
    expect(std::abs(alpha_num - alpha_2) / alpha_2 < 0.01, "numeric deflection within 1% of the 2nd-order formula");
    expect(std::abs(alpha_num - alpha_2) < std::abs(alpha_num - alpha_1), "2nd-order formula is closer than 1st-order");
    expect(v[2] < 0.0, "ray is bent toward the mass");
}

void test_rk45_vs_rk4_and_planar_vs_3d() {
    const double rs = 1.0;
    // Generic ray away from the poles: planar integration must match the 3-D chart.
    const bh::Vec3d pos{{8.0, 3.0, -2.0}};
    const bh::Vec3d dir{{-0.8, -0.1, 0.55}};
    // Coordinate frame: same seeding convention as make_ray_from_cartesian (3-D chart).
    bh::PlanarRay planar = bh::make_planar_ray(pos, dir, rs, bh::DirectionFrame::Coordinate);
    bh::RayState full = bh::make_ray_from_cartesian(pos[0], pos[1], pos[2], dir[0], dir[1], dir[2], rs);
    expect_near(planar.E, full.E, 1e-12, "planar and 3-D seeds share E");
    const double dl = 0.002;
    for (int i = 0; i < 3000; ++i) {
        bh::planar_rk4_step(planar, dl, rs);
        bh::rk4_step(full, dl, rs);
    }
    const bh::Vec3d pp = bh::planar_position(planar);
    expect_near(pp[0], full.x, 1e-6, "planar x matches 3-D RK4");
    expect_near(pp[1], full.y, 1e-6, "planar y matches 3-D RK4");
    expect_near(pp[2], full.z, 1e-6, "planar z matches 3-D RK4");
    expect(std::abs(bh::planar_null_constraint(planar, rs)) < 1e-8, "planar null constraint preserved");

    // RK45 reaches the same state as fine RK4 with far fewer steps.
    bh::PlanarRay a = bh::make_planar_ray(pos, dir, rs);
    bh::PlanarRay bb = bh::make_planar_ray(pos, dir, rs);
    int steps_rk4 = 0, steps_rk45 = 0;
    double lambda_target = 6.0;
    for (double l = 0.0; l < lambda_target - 1e-12; l += dl) { bh::planar_rk4_step(a, dl, rs); ++steps_rk4; }
    bh::Rk45Options opt;
    opt.abs_tol = 1e-10;
    opt.rel_tol = 1e-10;
    double l = 0.0, h = 0.05;
    while (l < lambda_target - 1e-12) {
        const double step = std::min(h, lambda_target - l);
        bh::Rk45Options o = opt;
        o.h_max = step;
        const bh::Rk45Result r = bh::planar_rk45_step(bb, step, rs, o);
        l += r.h_used;
        h = r.h_next;
        ++steps_rk45;
    }
    const bh::Vec3d pa = bh::planar_position(a), pb = bh::planar_position(bb);
    expect_near(pa[0], pb[0], 1e-6, "RK45 x matches fine RK4");
    expect_near(pa[1], pb[1], 1e-6, "RK45 y matches fine RK4");
    expect_near(pa[2], pb[2], 1e-6, "RK45 z matches fine RK4");
    std::cout << "  steps rk4=" << steps_rk4 << " rk45=" << steps_rk45 << "\n";
    expect(steps_rk45 < steps_rk4 / 4, "RK45 needs far fewer steps than fixed RK4");
}

void test_pole_safety() {
    const double rs = 1.0;
    // Ray passing straight over the north pole: singular in the global chart, trivial in the plane.
    bh::PlanarRay ray = bh::make_planar_ray({{-20.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}, rs);
    // Give it a tilt so its plane contains the z axis.
    ray = bh::make_planar_ray({{-20.0, 0.0, 0.5}}, {{1.0, 0.0, 0.0}}, rs);
    bool finite = true;
    for (int i = 0; i < 20000 && bh::planar_radius(ray) > rs * 1.001; ++i) {
        bh::planar_rk4_step(ray, 0.01, rs);
        const bh::Vec3d p = bh::planar_position(ray);
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) { finite = false; break; }
    }
    expect(finite, "ray through the polar axis stays finite in the planar chart");

    // Purely radial ray: degenerate plane handled.
    bh::PlanarRay radial = bh::make_planar_ray({{0.0, 0.0, 10.0}}, {{0.0, 0.0, -1.0}}, rs);
    expect(radial.degenerate_radial, "radial ray flagged degenerate");
    for (int i = 0; i < 5000 && bh::planar_radius(radial) > rs * 1.001; ++i) bh::planar_rk4_step(radial, 0.005, rs);
    expect(bh::planar_radius(radial) <= rs * 1.001, "radial infalling ray reaches the horizon");
    const bh::Vec3d p = bh::planar_position(radial);
    expect(std::abs(p[0]) < 1e-9 && std::abs(p[1]) < 1e-9, "radial ray stays on its axis");
}

void test_static_observer_seeding() {
    // A unit local direction gives local photon energy 1 ⇒ E = sqrt(f(r)).
    const double rs = 1.0, r = 5.0;
    bh::PlanarRay ray = bh::make_planar_ray({{r, 0.0, 0.0}}, {{-0.6, 0.0, 0.8}}, rs);
    expect_near(ray.E, std::sqrt(1.0 - rs / r), 1e-12, "static-observer seed: E = sqrt(f) for |dir| = 1");
    expect_near(ray.state.dr, -0.6 * std::sqrt(1.0 - rs / r), 1e-12, "static-observer seed: k^r = sqrt(f) n_r");
    expect_near(bh::planar_null_constraint(ray, rs), 0.0, 1e-12, "static-observer seed is null");
    bh::PlanarRay coord = bh::make_planar_ray({{r, 0.0, 0.0}}, {{-0.6, 0.0, 0.8}}, rs, bh::DirectionFrame::Coordinate);
    expect_near(coord.state.dr, -0.6, 1e-15, "coordinate seed keeps dr = dir·e_r");
    expect_near(coord.L, ray.L, 1e-15, "both frames share L (tangential part unchanged)");

    // Redshift at a finite camera = g_inf / sqrt(f_cam).
    const double g_inf = bh::redshift::doppler_gravitational_factor(4.0, rs, 1.0, 0.7);
    expect_near(bh::redshift::doppler_gravitational_factor_at(4.0, rs, 1.0, 0.7, 5.0), g_inf / std::sqrt(0.8), 1e-15,
                "g at a static camera = g_inf / sqrt(f(r_cam))");
    expect_near(bh::redshift::doppler_gravitational_factor(4.0, rs, 1.0, 0.7, 0.5),
                bh::redshift::doppler_gravitational_factor(4.0, rs, 1.0, 0.7, 1.0), 1e-15,
                "only the sign of spin_sign matters");
}

void test_shadow_branches() {
    const double rs = 1.0;
    const double pi = 3.14159265358979323846;
    expect_near(bh::escape::shadow_angular_radius(rs, 1.5), pi / 2.0, 1e-12, "shadow = π/2 at the photon sphere");
    expect(bh::escape::shadow_angular_radius(rs, 1.2) > pi / 2.0, "shadow > π/2 inside the photon sphere");
    expect(bh::escape::shadow_angular_radius(rs, 1.0001) > 0.95 * pi, "shadow → π at the horizon");
    expect(std::abs(bh::escape::shadow_angular_radius(rs, 1.5 - 1e-9) - bh::escape::shadow_angular_radius(rs, 1.5 + 1e-9)) < 1e-6,
           "shadow radius is continuous across the photon sphere");
}

void test_rk45_invalid_state() {
    // On the polar axis the 3-D chart RHS divides by sinθ = 0 → non-finite.
    bh::RayState ray;
    ray.r = 10.0;
    ray.theta = 0.0;
    ray.phi = 0.0;
    ray.dr = -1.0;
    ray.dtheta = 0.1;
    ray.dphi = 0.3;
    ray.E = 1.0;
    const bh::RayState before = ray;
    const bh::Rk45Result res = bh::rk45_adaptive_step(ray, 0.1, 1.0);
    expect(res.h_used == 0.0 && !std::isfinite(res.error_norm), "RK45 reports an invalid state instead of stepping");
    expect(ray.r == before.r && ray.dr == before.dr, "RK45 leaves an invalid state untouched");
}

void test_kerr_analytic() {
    const double M = 1.0;
    expect_near(bh::kerr::outer_horizon(M, 0.0), 2.0, 1e-15, "a=0: r+ = 2M");
    expect_near(bh::kerr::isco_radius(M, 0.0, true), 6.0, 1e-12, "a=0: ISCO = 6M");
    expect_near(bh::kerr::photon_orbit_radius(M, 0.0, true), 3.0, 1e-12, "a=0: photon orbit = 3M");
    expect_near(bh::kerr::ergosphere_radius(M, 0.0, 0.3), 2.0, 1e-15, "a=0: ergosphere = horizon");
    expect_near(bh::kerr::thin_disk_efficiency(M, 0.0, true), bh::orbits::thin_disk_efficiency(), 1e-12,
                "a=0 efficiency matches Schwarzschild");
    expect_near(bh::kerr::outer_horizon(M, 1.0), 1.0, 1e-15, "a=1: r+ = M");
    expect_near(bh::kerr::isco_radius(M, 1.0, true), 1.0, 1e-9, "a=1 prograde ISCO = M");
    expect_near(bh::kerr::isco_radius(M, 1.0, false), 9.0, 1e-9, "a=1 retrograde ISCO = 9M");
    expect_near(bh::kerr::photon_orbit_radius(M, 1.0, true), 1.0, 1e-12, "a=1 prograde photon orbit = M");
    expect_near(bh::kerr::photon_orbit_radius(M, 1.0, false), 4.0, 1e-12, "a=1 retrograde photon orbit = 4M");
    expect_near(bh::kerr::ergosphere_equatorial(M), 2.0, 0.0, "equatorial ergosphere = 2M");
    expect(bh::kerr::thin_disk_efficiency(M, 0.998, true) > 0.3, "a=0.998 prograde efficiency > 30%");
    expect_near(bh::kerr::thin_disk_efficiency(M, 0.998, true), 0.32099, 2e-5, "a=0.998 prograde η = 32.1% (BPT)");
    expect_near(bh::kerr::thin_disk_efficiency(M, 1.0, true), 1.0 - 1.0 / std::sqrt(3.0), 1e-12,
                "a=1 prograde η = 1 − 1/√3 (finite, no 0/0)");
    expect_near(bh::kerr::thin_disk_efficiency(M, 1.0, false), 1.0 - std::sqrt(25.0 / 27.0), 1e-9,
                "a=1 retrograde η = 1 − sqrt(25/27)");
    expect_near(bh::kerr::isco_radius(M, -0.9, true), bh::kerr::isco_radius(M, 0.9, true), 0.0,
                "only |a*| matters (ISCO)");
    expect_near(bh::kerr::photon_orbit_radius(M, -0.9, true), bh::kerr::photon_orbit_radius(M, 0.9, true), 0.0,
                "only |a*| matters (photon orbit)");
    expect_near(bh::kerr::thin_disk_efficiency(M, -0.9, true), bh::kerr::thin_disk_efficiency(M, 0.9, true), 0.0,
                "only |a*| matters (efficiency)");
}

}  // namespace

int main() {
    std::cout << "bh scientific reference tests\n";
    test_photon_sphere_and_isco();
    test_units_sag_a();
    test_weak_field_one_over_b();
    test_conserved_definitions();
    test_rk4_beats_euler_null_drift();
    test_disk_factors_legacy();
    test_camera_orbit_parity();
    test_scene_params_roundtrip();
    test_circular_orbits();
    test_redshift_factors();
    test_page_thorne_flux();
    test_hit_testing();
    test_critical_capture_escape();
    test_weak_field_numeric_deflection();
    test_rk45_vs_rk4_and_planar_vs_3d();
    test_pole_safety();
    test_kerr_analytic();
    test_static_observer_seeding();
    test_shadow_branches();
    test_rk45_invalid_state();

    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "All scientific reference tests passed.\n";
    return EXIT_SUCCESS;
}
