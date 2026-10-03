// Structural golden tests for the headless CPU renderer (no GPU, no driver).
#include "black_hole/cpu_renderer.hpp"
#include "black_hole/escape.hpp"
#include "black_hole/image_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>
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

bool finite_image(const bh::render::Image& img) {
    for (float v : img.rgb) {
        if (!std::isfinite(v) || v < 0.0f || v > 1.0f) return false;
    }
    return true;
}

void test_legacy_structure() {
    bh::render::Options opt;
    opt.width = 96;
    opt.height = 72;
    opt.threads = 2;
    opt.scene.camera.elevation_rad = 1.25;  // slightly above the disk plane
    opt.scene.objects.clear();              // the red sphere sits at +z and would break mirror symmetry
    bh::render::Stats st;
    const bh::render::Image img = bh::render::render(opt, &st);

    expect(img.width == 96 && img.height == 72, "legacy image size");
    expect(finite_image(img), "legacy pixels finite and in [0,1]");
    expect(st.rays == 96u * 72u, "one ray per pixel");
    expect(st.step_limit == 0, "no ray hit the step limit");
    expect(st.shadow_fraction() > 0.2 && st.shadow_fraction() < 0.7, "shadow covers a plausible fraction");
    expect(st.disk_fraction() > 0.1, "disk visible");
    expect(st.escaped_fraction() > 0.01, "some rays escape");

    // Centre pixel looks at the black hole → black.
    const int cx = img.width / 2, cy = img.height / 2;
    expect(img.at(cx, cy, 0) == 0.0f && img.at(cx, cy, 1) == 0.0f && img.at(cx, cy, 2) == 0.0f,
           "centre pixel is the shadow");

    // Legacy colour model: on disk pixels R ≥ G ≥ B·… (amber ramp 1, r, 0.2 scaled by alpha).
    bool amber_ok = true;
    int disk_px = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const float r = img.at(x, y, 0), g = img.at(x, y, 1), b = img.at(x, y, 2);
            if (r > 0.05f && g > 0.0f) {
                ++disk_px;
                if (!(r >= g - 1e-6f && std::abs(b - 0.2f * r) < 1e-4f)) amber_ok = false;
            }
        }
    }
    expect(disk_px > 0 && amber_ok, "legacy disk pixels follow vec3(1, r, 0.2)·r");

    // Mirror symmetry about the vertical axis: the equatorial-plane scene is
    // symmetric under z → −z for azimuth 0 and the legacy shading has no Doppler.
    double max_diff = 0.0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width / 2; ++x) {
            for (int c = 0; c < 3; ++c) {
                max_diff = std::max(max_diff, std::abs(double(img.at(x, y, c)) - img.at(img.width - 1 - x, y, c)));
            }
        }
    }
    expect(max_diff < 2e-3, "legacy render is left/right symmetric (max diff " + std::to_string(max_diff) + ")");
}

void test_determinism_threads() {
    bh::render::Options a;
    a.width = 40;
    a.height = 30;
    a.threads = 1;
    a.scene.camera.elevation_rad = 1.1;
    bh::render::Options b = a;
    b.threads = 4;
    const bh::render::Image ia = bh::render::render(a);
    const bh::render::Image ib = bh::render::render(b);
    bool same = ia.rgb == ib.rgb;
    expect(same, "render is bit-identical across thread counts");
}

void test_relativistic_asymmetry() {
    bh::render::Options opt;
    opt.width = 96;
    opt.height = 72;
    opt.color_mode = bh::render::ColorMode::LegacyDoppler;
    opt.scene.camera.elevation_rad = 1.25;
    bh::render::Stats st;
    const bh::render::Image img = bh::render::render(opt, &st);
    expect(finite_image(img), "relativistic pixels finite");
    expect(st.min_g > 0.2 && st.min_g < 1.0, "min redshift factor in (0.2, 1)");
    expect(st.max_g > 1.0 && st.max_g < 2.0, "max blueshift factor in (1, 2)");

    double left = 0.0, right = 0.0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width / 2; ++x) {
            left += img.at(x, y, 0) + img.at(x, y, 1) + img.at(x, y, 2);
            right += img.at(img.width - 1 - x, y, 0) + img.at(img.width - 1 - x, y, 1) + img.at(img.width - 1 - x, y, 2);
        }
    }
    expect(std::abs(left - right) > 0.05 * (left + right), "Doppler beaming makes one side brighter");

    // Flipping the disk spin flips the bright side.
    opt.disk_spin_sign = -1.0;
    const bh::render::Image flipped = bh::render::render(opt);
    double left2 = 0.0, right2 = 0.0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width / 2; ++x) {
            left2 += flipped.at(x, y, 0) + flipped.at(x, y, 1) + flipped.at(x, y, 2);
            right2 += flipped.at(flipped.width - 1 - x, y, 0) + flipped.at(flipped.width - 1 - x, y, 1) +
                      flipped.at(flipped.width - 1 - x, y, 2);
        }
    }
    expect((left > right) != (left2 > right2), "reversing disk spin swaps the bright side");
}

void test_blackbody_mode() {
    bh::render::Options opt;
    opt.width = 64;
    opt.height = 48;
    opt.color_mode = bh::render::ColorMode::Blackbody;
    opt.scene.camera.elevation_rad = 1.0;
    opt.stars = true;
    bh::render::Stats st;
    const bh::render::Image img = bh::render::render(opt, &st);
    expect(finite_image(img), "blackbody pixels finite");
    double sum = 0.0;
    for (float v : img.rgb) sum += v;
    expect(sum > 1.0, "blackbody disk emits light");
}

void test_pole_camera_and_rk45() {
    bh::render::Options opt;
    opt.width = 48;
    opt.height = 36;
    opt.integrator = bh::render::IntegratorKind::Rk45;
    opt.scene.camera.elevation_rad = 0.011;  // legacy clamp: looking almost straight down the axis
    bh::render::Stats st;
    const bh::render::Image img = bh::render::render(opt, &st);
    expect(finite_image(img), "near-pole camera renders without NaN (RK45)");
    expect(st.step_limit == 0, "near-pole render hits no step limit");
    expect(st.shadow_fraction() > 0.05, "near-pole view still shows the shadow");
    expect(st.disk_fraction() > 0.15, "face-on view shows a large disk area");

    // Exactly on the axis is handled by the camera-basis fallback.
    opt.scene.camera.elevation_rad = 0.0;
    const bh::render::Image on_axis = bh::render::render(opt, &st);
    expect(finite_image(on_axis), "camera on the polar axis renders");
}

void test_shadow_size_matches_theory() {
    // Far camera, narrow FOV: shadow angular radius must match
    // sin α = b_c sqrt(f)/r_o within a few percent of the pixel scale.
    bh::render::Options opt;
    opt.width = 160;
    opt.height = 160;
    opt.scene.camera.radius_m = 60.0 * opt.scene.r_s_m;
    opt.scene.camera.elevation_rad = 0.6;
    opt.scene.camera.fov_y_deg = 12.0;
    opt.scene.disk_inner_factor_rs = 2.2;
    opt.scene.disk_outer_factor_rs = 2.21;  // practically no disk → clean shadow disc
    opt.scene.objects.clear();
    bh::render::Stats st;
    bh::render::render(opt, &st);
    const double alpha = bh::escape::shadow_angular_radius(1.0, 60.0);
    const double half_fov = 6.0 * 3.14159265358979323846 / 180.0;
    // Solid angle fraction of a small cap in a square gnomonic FOV ≈ π α² / (2 tan(half_fov))².
    const double expected = 3.14159265358979323846 * std::tan(alpha) * std::tan(alpha) /
                            (4.0 * std::tan(half_fov) * std::tan(half_fov));
    const double got = st.shadow_fraction();
    expect(std::abs(got - expected) / expected < 0.08,
           "rendered shadow area matches analytic b_c (got " + std::to_string(got) + ", expected " +
               std::to_string(expected) + ")");
}

// Bisect the critical angle between "captured" and "escaped" for rays leaving a
// static camera at r_cam, and compare with Synge's formula. This is the test
// that catches a wrong camera model (coordinate vs static-observer frame): the
// old seeding was off by ~10 % in angle at the default radius.
void test_traced_shadow_edge_vs_synge() {
    bh::render::Options opt;
    opt.max_steps = 200000;
    opt.step_k = 0.005;
    opt.step_min_rs = 0.0005;
    bh::render::GeometricScene gs;
    gs.disk_inner = 1e9;  // no disk, no objects: pure capture / escape
    gs.disk_outer = 1e9 + 1.0;
    for (double r_cam : {1.3, 3.0, 4.997, 10.0, 60.0}) {
        gs.scene_bound = 2.0 * r_cam + 10.0;
        const std::array<double, 3> pos{{r_cam * std::sin(0.6), r_cam * std::cos(0.6), 0.0}};
        const std::array<double, 3> fwd{{-pos[0] / r_cam, -pos[1] / r_cam, 0.0}};
        const std::array<double, 3> side{{0.0, 0.0, 1.0}};
        auto captured = [&](double theta) {
            const std::array<double, 3> d{{std::cos(theta) * fwd[0] + std::sin(theta) * side[0],
                                           std::cos(theta) * fwd[1] + std::sin(theta) * side[1],
                                           std::cos(theta) * fwd[2] + std::sin(theta) * side[2]}};
            return bh::render::trace_ray(opt, gs, pos, d).kind == bh::render::HitKind::Horizon;
        };
        double lo = 0.0, hi = 3.14159265358979323846;  // captured at lo, escapes at hi
        for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            (captured(mid) ? lo : hi) = mid;
        }
        const double edge = 0.5 * (lo + hi);
        const double theory = bh::escape::shadow_angular_radius(1.0, r_cam);
        expect(std::abs(edge - theory) < 2e-3 * theory,
               "traced shadow edge matches Synge at r_cam = " + std::to_string(r_cam) + " rs (traced " +
                   std::to_string(edge) + ", theory " + std::to_string(theory) + ")");
    }
}

// ---- regressions for the 0.8.0 review (renderer dimension) ------------------

double max_abs_diff(const bh::render::Image& a, const bh::render::Image& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < a.rgb.size(); ++i) m = std::max(m, double(std::abs(a.rgb[i] - b.rgb[i])));
    return m;
}

void test_float32_equatorial_camera() {
    // Blender stores π/2 as float32 (cos = −4.4e−8). The CPU must treat that
    // camera as ON the disk plane, exactly like the double default.
    bh::render::Options a;
    a.width = 64;
    a.height = 48;
    bh::render::Options b = a;
    b.scene.camera.elevation_rad = static_cast<double>(static_cast<float>(1.5707963267948966));
    const auto ia = bh::render::render(a);
    const auto ib = bh::render::render(b);
    expect(max_abs_diff(ia, ib) < 1e-3, "float32 π/2 elevation renders like the double default (no half-frame disk)");
}

void test_up_vector_near_pole() {
    // Inside the legacy elevation clamp the camera keeps world-up +Y (GPU / Blender parity).
    bh::render::Options opt;
    opt.scene.camera.elevation_rad = 0.0105;
    opt.scene.camera.azimuth_rad = 0.7;
    const auto gs = bh::render::make_geometric_scene(opt.scene);
    const auto cam = bh::render::make_camera_frame(opt, gs);
    // right must be normalize(forward × +Y): orthogonal to +Y.
    expect(std::abs(cam.right[1]) < 1e-12, "camera right ⟂ +Y at elevation 0.0105 (no up-vector switch)");
    expect(cam.up[1] > 0.0, "camera up has a +Y component");
}

void test_black_hole_marker_any_mass() {
    // Half the Sgr A* mass, default objects kept (as the Blender exporter writes it):
    // the marker must not become an opaque sphere bigger than the horizon.
    const std::string json =
        R"({"schema":"black_hole.scene_params/v1","black_hole":{"mass_kg":4.27e36,"r_s_m":6.345e9},)"
        R"("camera":{"radius_m":1.269e11,"elevation_rad":1.25}})";
    bh::render::Options opt;
    std::string err;
    expect(bh::scene_params_from_json(json, opt.scene, err), "half-mass scene parses");
    opt.width = 64;
    opt.height = 48;
    bh::render::Stats st;
    bh::render::render(opt, &st);
    expect(st.shadow_fraction() > 0.0 && st.object_fraction() == 0.0,
           "black-hole marker skipped for any mass (shadow from the horizon, not a sphere)");
    bh::SceneObject marker = bh::make_default_scene_params().objects[2];
    expect(bh::is_black_hole_marker(marker, bh::make_default_scene_params()), "default marker recognised");
    bh::SceneObject sphere = bh::make_default_scene_params().objects[0];
    expect(!bh::is_black_hole_marker(sphere, bh::make_default_scene_params()), "yellow star is not a marker");
}

void test_stars_are_lensed() {
    // Escaped rays must carry their asymptotic (bent) direction for the sky lookup.
    bh::render::Options opt;
    opt.stars = true;
    bh::render::GeometricScene gs = bh::render::make_geometric_scene(opt.scene);
    gs.disk_inner = 1e9;
    gs.disk_outer = 1e9 + 1.0;
    gs.spheres.clear();
    gs.scene_bound = 12.0;
    const std::array<double, 3> pos{{10.0, 0.0, 0.0}};
    // Aim just outside the shadow edge: strongly bent but escaping.
    const double a = bh::escape::shadow_angular_radius(1.0, 10.0) * 1.02;
    const std::array<double, 3> dir{{-std::cos(a), std::sin(a), 0.0}};
    const auto hit = bh::render::trace_ray(opt, gs, pos, dir);
    expect(hit.kind == bh::render::HitKind::Escaped, "near-critical ray escapes");
    const double cosang = hit.escape_dir[0] * dir[0] + hit.escape_dir[1] * dir[1] + hit.escape_dir[2] * dir[2];
    expect(cosang < 0.5, "escape direction is bent far from the primary direction (> 60°)");
}

void test_blackbody_render_sequence_independent() {
    // A render must not depend on previous renders with other parameters.
    bh::render::Options a;
    a.width = 40;
    a.height = 30;
    a.color_mode = bh::render::ColorMode::Blackbody;
    a.scene.camera.elevation_rad = 1.1;
    bh::render::Options b = a;
    b.scene.r_s_m *= 2.0;
    b.scene.camera.radius_m *= 2.0;
    b.threads = 1;
    const auto fresh_b = bh::render::render(b);
    a.threads = 1;
    bh::render::render(a);
    const auto again_b = bh::render::render(b);
    expect(max_abs_diff(fresh_b, again_b) == 0.0, "blackbody render independent of the previous render");
}

void test_object_alpha() {
    bh::render::Options opt;
    opt.width = 64;
    opt.height = 48;
    opt.scene.objects.clear();
    bh::SceneObject o;
    o.pos_m = {{0.0, 3.0 * opt.scene.r_s_m, 0.0}};
    o.radius_m = 0.8 * opt.scene.r_s_m;
    o.color_rgba = {{0.0, 1.0, 1.0, 1.0}};
    o.mass_kg = 1.0;
    opt.scene.objects.push_back(o);
    opt.scene.camera.elevation_rad = 1.2;
    const auto opaque = bh::render::render(opt);
    opt.scene.objects[0].color_rgba[3] = 0.4;
    const auto translucent = bh::render::render(opt);
    double ratio_min = 1e9, ratio_max = 0.0;
    for (std::size_t i = 1; i < opaque.rgb.size(); i += 3) {
        if (opaque.rgb[i] > 0.2f && opaque.rgb[i - 1] == 0.0f) {  // object pixel (no red)
            const double r = translucent.rgb[i] / opaque.rgb[i];
            ratio_min = std::min(ratio_min, r);
            ratio_max = std::max(ratio_max, r);
        }
    }
    expect(ratio_min > 0.399 && ratio_max < 0.401, "object colour is premultiplied by its alpha (rgb·a over black)");
}

void test_distant_object_no_step_limit() {
    bh::render::Options opt;
    opt.width = 40;
    opt.height = 30;
    opt.stars = true;
    opt.scene.objects.clear();
    bh::SceneObject far;
    far.pos_m = {{1e5 * opt.scene.r_s_m, 0.0, 0.0}};
    far.radius_m = opt.scene.r_s_m;
    far.color_rgba = {{1.0, 1.0, 1.0, 1.0}};
    opt.scene.objects.push_back(far);
    opt.scene.camera.elevation_rad = 1.2;
    bh::render::Stats st;
    bh::render::render(opt, &st);
    expect(st.step_limit == 0 && st.escaped > 0, "object at 1e5 rs: escaping rays still escape (no step limit)");
}

void test_crc_thread_safe() {
    std::vector<std::uint8_t> rgb(8 * 8 * 3, 7);
    std::vector<std::vector<std::uint8_t>> outs(4);
    std::vector<std::thread> pool;
    for (int i = 0; i < 4; ++i) pool.emplace_back([&, i] { outs[i] = bh::image_io::encode_png(8, 8, rgb); });
    for (auto& t : pool) t.join();
    expect(outs[0] == outs[1] && outs[1] == outs[2] && outs[2] == outs[3], "concurrent PNG encoding is consistent");
}

void test_image_io() {
    bh::render::Image img;
    img.width = 3;
    img.height = 2;
    img.rgb = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    const auto rgb8 = bh::image_io::to_rgb8(img);
    expect(rgb8.size() == 18 && rgb8[0] == 255 && rgb8[9] == 128, "to_rgb8 quantises");
    const auto png = bh::image_io::encode_png(3, 2, rgb8);
    expect(png.size() > 8 && png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G', "PNG signature");
    // IHDR chunk CRC check.
    const std::uint32_t crc_stored = (std::uint32_t(png[29]) << 24) | (std::uint32_t(png[30]) << 16) |
                                     (std::uint32_t(png[31]) << 8) | std::uint32_t(png[32]);
    expect(bh::image_io::crc32(png.data() + 12, 17) == crc_stored, "IHDR CRC32 valid");
    expect(bh::image_io::adler32(reinterpret_cast<const std::uint8_t*>("Wikipedia"), 9) == 0x11E60398u,
           "adler32 reference value");
    expect(bh::image_io::crc32(reinterpret_cast<const std::uint8_t*>("123456789"), 9) == 0xCBF43926u,
           "crc32 reference value");
    std::string err;
    bh::render::Image bad;
    expect(!bh::image_io::write_image("x.gif", bad, 1.0, err), "unknown extension rejected");
}

}  // namespace

int main() {
    std::cout << "bh cpu renderer tests\n";
    test_legacy_structure();
    test_determinism_threads();
    test_relativistic_asymmetry();
    test_blackbody_mode();
    test_pole_camera_and_rk45();
    test_shadow_size_matches_theory();
    test_traced_shadow_edge_vs_synge();
    test_float32_equatorial_camera();
    test_up_vector_near_pole();
    test_black_hole_marker_any_mass();
    test_stars_are_lensed();
    test_blackbody_render_sequence_independent();
    test_object_alpha();
    test_distant_object_no_step_limit();
    test_crc_thread_safe();
    test_image_io();
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "All CPU renderer tests passed.\n";
    return EXIT_SUCCESS;
}
