// Headless scientific reference tests (no OpenGL / GLEW / GLFW).
#include "black_hole/camera_model.hpp"
#include "black_hole/conserved.hpp"
#include "black_hole/disk_model.hpp"
#include "black_hole/integrator_rk4.hpp"
#include "black_hole/ray_state.hpp"
#include "black_hole/relativistic.hpp"
#include "black_hole/scene_params.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"
#include "black_hole/weak_field.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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

void test_relativistic_integration() {
    const double solar_mass_kg = 1.98847e30;
    const double mass_kg = 10.0 * solar_mass_kg;

    // Schwarzschild radius calculation through relativistic.hpp and units.hpp
    const double rs_rel = black_hole::schwarzschild_radius(mass_kg);
    const double rs_units = bh::units::schwarzschild_radius_si(mass_kg);
    const double rs_sch = bh::schwarzschild_radius_si(mass_kg);
    const double rs_expected = 2.0 * black_hole::G * mass_kg / (black_hole::c * black_hole::c);

    expect_near(rs_rel, rs_expected, 1e-6, "schwarzschild_radius matches 2GM/c^2");
    expect_near(rs_units, rs_rel, 1e-12, "units::schwarzschild_radius_si matches relativistic::schwarzschild_radius");
    expect_near(rs_sch, rs_rel, 1e-12, "schwarzschild::schwarzschild_radius_si matches relativistic::schwarzschild_radius");
    expect_near(rs_rel, 29.533e3, 10.0, "10 M_sun rs is ~29.53 km");

    // Light deflection angle through relativistic.hpp and weak_field.hpp
    const double impact_m = 10.0 * rs_rel;
    const double angle_rel = black_hole::light_deflection_angle(mass_kg, impact_m);
    const double angle_wf = bh::weak_field::deflection_angle_si(mass_kg, impact_m);
    const double angle_sch = bh::deflection_angle_si(mass_kg, impact_m);
    const double angle_expected = 2.0 * rs_rel / impact_m;

    expect_near(angle_rel, angle_expected, 1e-12, "light_deflection_angle matches 2 rs / b");
    expect_near(angle_wf, angle_rel, 1e-12, "weak_field::deflection_angle_si matches relativistic::light_deflection_angle");
    expect_near(angle_sch, angle_rel, 1e-12, "schwarzschild::deflection_angle_si matches relativistic::light_deflection_angle");

    const double angle_wf_consts = bh::weak_field::deflection_angle_si(black_hole::G, mass_kg, black_hole::c, impact_m);
    expect_near(angle_wf_consts, angle_rel, 1e-12, "weak_field with standard G and c delegates to light_deflection_angle");

    // Time dilation through relativistic.hpp and schwarzschild.hpp
    const double r_obs = 5.0 * rs_rel;
    const double dilation_rel = black_hole::time_dilation_factor(mass_kg, r_obs);
    const double dilation_sch = bh::time_dilation_factor_si(mass_kg, r_obs);
    const double dilation_geom = bh::time_dilation_factor(r_obs, rs_rel);
    const double dilation_expected = std::sqrt(1.0 - rs_rel / r_obs);

    expect_near(dilation_rel, dilation_expected, 1e-12, "time_dilation_factor matches sqrt(1 - rs/r)");
    expect_near(dilation_sch, dilation_rel, 1e-12, "schwarzschild::time_dilation_factor_si matches relativistic");
    expect_near(dilation_geom, dilation_expected, 1e-12, "schwarzschild::time_dilation_factor matches sqrt(1 - rs/r)");

    // Boundary conditions
    expect_near(black_hole::time_dilation_factor(mass_kg, rs_rel), 0.0, 0.0, "time dilation at horizon is 0");
    expect_near(black_hole::time_dilation_factor(mass_kg, 0.5 * rs_rel), 0.0, 0.0, "time dilation inside horizon is 0");
    expect_near(bh::time_dilation_factor(rs_rel, rs_rel), 0.0, 0.0, "bh::time_dilation_factor at horizon is 0");
    expect_near(bh::time_dilation_factor(0.5 * rs_rel, rs_rel), 0.0, 0.0, "bh::time_dilation_factor inside horizon is 0");
}

void test_batch_rk4_openmp() {
    const double rs = 1.0;
    const double d_lambda = 0.1;
    const std::size_t num_rays = 16;

    std::vector<bh::RayState> single_rays;
    single_rays.reserve(num_rays);
    for (std::size_t i = 0; i < num_rays; ++i) {
        const double r0 = 10.0 * rs + static_cast<double>(i) * 2.0;
        const double dx = -0.5 - 0.02 * static_cast<double>(i);
        const double dy = 0.3 + 0.01 * static_cast<double>(i);
        const double dz = 0.1;
        single_rays.push_back(bh::make_ray_from_cartesian(r0, 0.0, 0.0, dx, dy, dz, rs));
    }

    std::vector<bh::RayState> batch_rays = single_rays;

    // Step sequentially vs batch OpenMP
    for (std::size_t i = 0; i < num_rays; ++i) {
        bh::rk4_step(single_rays[i], d_lambda, rs);
    }
    bh::rk4_step_batch(batch_rays.data(), batch_rays.size(), d_lambda, rs);

    bool all_match = true;
    for (std::size_t i = 0; i < num_rays; ++i) {
        if (std::abs(single_rays[i].r - batch_rays[i].r) > 1e-12 ||
            std::abs(single_rays[i].theta - batch_rays[i].theta) > 1e-12 ||
            std::abs(single_rays[i].phi - batch_rays[i].phi) > 1e-12 ||
            std::abs(single_rays[i].dr - batch_rays[i].dr) > 1e-12 ||
            std::abs(single_rays[i].dtheta - batch_rays[i].dtheta) > 1e-12 ||
            std::abs(single_rays[i].dphi - batch_rays[i].dphi) > 1e-12) {
            all_match = false;
            break;
        }
    }
    expect(all_match, "rk4_step_batch yields identical values to single rk4_step");

    // Test integrate_rays_rk4 termination conditions
    std::vector<bh::RayState> test_rays;
    // 1) Ray aiming directly inward close to horizon (should be captured)
    test_rays.push_back(bh::make_ray_from_cartesian(1.5 * rs, 0.0, 0.0, -1.0, 0.0, 0.0, rs));
    // 2) Ray aiming directly outward (should escape)
    test_rays.push_back(bh::make_ray_from_cartesian(95.0 * rs, 0.0, 0.0, 1.0, 0.0, 0.0, rs));

    std::vector<bh::BatchRayResult> results(test_rays.size());
    bh::integrate_rays_rk4(test_rays.data(), test_rays.size(), 0.1, rs, 100, 100.0 * rs, results.data());

    expect(results[0].captured, "inward ray captured by horizon in integrate_rays_rk4");
    expect(!results[0].escaped, "inward ray not escaped");
    expect(results[0].steps_taken > 0, "inward ray recorded steps");

    expect(results[1].escaped, "outward ray escaped in integrate_rays_rk4");
    expect(!results[1].captured, "outward ray not captured");
    expect(results[1].steps_taken > 0, "outward ray recorded steps");
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
    test_relativistic_integration();
    test_batch_rk4_openmp();

    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "All scientific reference tests passed.\n";
    return EXIT_SUCCESS;
}
