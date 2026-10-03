#pragma once
// Headless, deterministic, multithreaded CPU renderer for the Schwarzschild scene.
//
// Purpose
//   * golden-image / structural regression tests that need no GPU or driver;
//   * the Blender "render bridge" (bh_render_cpu → PNG → camera background);
//   * a double-precision reference for the GPU shaders.
//
// Physics
//   * null geodesics integrated in each ray's orbital plane (planar_geodesic.hpp),
//     pole-safe, RK4 with geometric step or adaptive RK45;
//   * continuous segment hit tests (hit_testing.hpp) for disk / horizon / spheres;
//   * proof-based escape criterion (escape.hpp);
//   * optional relativistic disk shading: Doppler + gravitational redshift
//     (redshift.hpp) and Page–Thorne blackbody (disk_emission.hpp).
//
// Visual conventions follow the legacy baseline exactly in ColorMode::Legacy:
//   world Y-up, disk in XZ (2.2–5.2 rs), disk colour vec3(1, r_norm, 0.2) with
//   alpha r_norm composited over black, black horizon, Lambert-lit spheres.
//   Row 0 of the output image is the TOP of the frame.

#include "black_hole/scene_params.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bh {
namespace render {

enum class ColorMode { Legacy, LegacyDoppler, Blackbody };
enum class IntegratorKind { Rk4Geometric, Rk45 };
enum class HitKind { Escaped, Horizon, Disk, Object, StepLimit };

struct Options {
    int width = 200;             // legacy compute width
    int height = 150;            // legacy compute height
    int supersample = 1;         // S×S regular sub-pixel grid
    int threads = 0;             // 0 → hardware_concurrency
    ColorMode color_mode = ColorMode::Legacy;
    IntegratorKind integrator = IntegratorKind::Rk4Geometric;
    SceneParams scene = make_default_scene_params();

    // Integration controls (in units of rs).
    int max_steps = 20000;
    double step_k = 0.02;        // geometric step: dλ = clamp(step_k · r, step_min, step_max)
    double step_min_rs = 0.005;
    double step_max_rs = 2.0;
    double rk45_tol = 1e-8;

    // Scientific shading controls.
    double exposure = 2.0;             // pre-tonemap multiplier (LegacyDoppler / Blackbody)
    double gamma = 1.0;                // output gamma (1.0 = linear like the legacy texture)
    double disk_spin_sign = 1.0;       // +1 counter-clockwise about +Y
    double mdot_edd_fraction = 0.01;   // Blackbody: Ṁ as a fraction of Eddington
    bool stars = false;                // deterministic procedural starfield for escaped rays
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<float> rgb;  // 3 floats per pixel, row-major, row 0 = top

    float& at(int x, int y, int c) { return rgb[(static_cast<std::size_t>(y) * width + x) * 3 + c]; }
    float at(int x, int y, int c) const { return rgb[(static_cast<std::size_t>(y) * width + x) * 3 + c]; }
};

struct Stats {
    std::uint64_t rays = 0;
    std::uint64_t horizon = 0;
    std::uint64_t disk = 0;
    std::uint64_t object = 0;
    std::uint64_t escaped = 0;
    std::uint64_t step_limit = 0;
    std::uint64_t total_steps = 0;
    double min_g = 1e300;
    double max_g = 0.0;

    double shadow_fraction() const { return rays ? double(horizon) / double(rays) : 0.0; }
    double disk_fraction() const { return rays ? double(disk) / double(rays) : 0.0; }
    double object_fraction() const { return rays ? double(object) / double(rays) : 0.0; }
    double escaped_fraction() const { return rays ? double(escaped) / double(rays) : 0.0; }
};

struct TraceResult {
    HitKind kind = HitKind::Escaped;
    std::array<double, 3> hit_pos{{0.0, 0.0, 0.0}};  // rs units, world frame
    double rho = 0.0;        // cylindrical radius of a disk hit (rs)
    double g = 1.0;          // redshift factor of a disk hit
    int object_index = -1;
    int steps = 0;
    std::array<double, 3> escape_dir{{0.0, 0.0, 0.0}};  // asymptotic direction of an escaped ray
};

/// Scene converted to geometric units (rs = 1) in the world frame.
struct GeometricScene {
    double rs_m = 1.0;
    double disk_inner = 2.2;
    double disk_outer = 5.2;
    double scene_bound = 6.0;  // radius beyond which nothing can be hit
    double peak_temperature = 0.0;  // Blackbody normalisation (K); 0 = compute on demand
    struct Sphere {
        std::array<double, 3> center;
        double radius;
        std::array<double, 4> color;
    };
    std::vector<Sphere> spheres;  // the black hole itself is NOT included (horizon test)
};

double peak_effective_temperature(const Options& opt);  // Page–Thorne T_eff max over 3–10 rs

GeometricScene make_geometric_scene(const SceneParams& scene);

/// Trace a single ray (position/direction in rs units, world frame).
TraceResult trace_ray(const Options& opt, const GeometricScene& gs,
                      const std::array<double, 3>& pos, const std::array<double, 3>& dir);

/// Camera basis in the legacy convention (right, up, forward) and position in rs.
struct CameraFrame {
    std::array<double, 3> pos;
    std::array<double, 3> right;
    std::array<double, 3> up;
    std::array<double, 3> forward;
    double tan_half_fov;
    double aspect;
};
CameraFrame make_camera_frame(const Options& opt, const GeometricScene& gs);

/// Direction for normalised image coordinates (sx, sy ∈ [0,1], sy = 0 at top).
std::array<double, 3> primary_ray_direction(const CameraFrame& cam, double sx, double sy);

/// Shade a trace result into linear RGB (before gamma).
std::array<float, 3> shade(const Options& opt, const GeometricScene& gs, const CameraFrame& cam,
                           const TraceResult& hit, const std::array<double, 3>& dir);

/// Render the full image. Deterministic for a given Options regardless of thread count.
Image render(const Options& opt, Stats* stats = nullptr);

const char* color_mode_name(ColorMode m);
const char* integrator_name(IntegratorKind k);
bool parse_color_mode(const std::string& s, ColorMode& out);
bool parse_integrator(const std::string& s, IntegratorKind& out);

}  // namespace render
}  // namespace bh
