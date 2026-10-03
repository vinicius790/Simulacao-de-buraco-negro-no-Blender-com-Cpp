#include "black_hole/cpu_renderer.hpp"

#include "black_hole/camera_model.hpp"
#include "black_hole/disk_emission.hpp"
#include "black_hole/escape.hpp"
#include "black_hole/hit_testing.hpp"
#include "black_hole/orbits.hpp"
#include "black_hole/planar_geodesic.hpp"
#include "black_hole/redshift.hpp"
#include "black_hole/units.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

namespace bh {
namespace render {
namespace {

using V3 = std::array<double, 3>;

V3 sub(const V3& a, const V3& b) { return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}}; }
V3 add(const V3& a, const V3& b) { return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}}; }
V3 mul(const V3& a, double s) { return {{a[0] * s, a[1] * s, a[2] * s}}; }
double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double len(const V3& a) { return std::sqrt(dot(a, a)); }
V3 normalize(const V3& a) {
    const double l = len(a);
    return (l > 0.0) ? mul(a, 1.0 / l) : V3{{0.0, 0.0, 1.0}};
}
V3 cross(const V3& a, const V3& b) {
    return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

constexpr int DISK_AXIS = 1;  // world Y is the disk normal (legacy Y-up, disk in XZ)
constexpr double CAMERA_ON_PLANE_TOLERANCE = 1e-6;  // relative; mirrors geodesic_scientific.comp

// Beyond the disk the bending per step is tiny (∝ rs/r²): let the step grow
// with r so rays towards distant objects / the escape bound do not exhaust
// max_steps. Inside 1.05·disk_outer the regular cap applies (GPU parity).
double step_cap(const Options& opt, const GeometricScene& gs, double r) {
    return (r > 1.05 * gs.disk_outer) ? std::max(opt.step_max_rs, opt.step_k * r) : opt.step_max_rs;
}

// Deterministic hash-based starfield (only when Options::stars is true).
float star_intensity(const V3& dir) {
    const double theta = std::acos(std::clamp(dir[1], -1.0, 1.0));
    const double phi = std::atan2(dir[2], dir[0]);
    const std::uint32_t it = static_cast<std::uint32_t>(theta / 3.14159265358979323846 * 1024.0);
    const std::uint32_t ip = static_cast<std::uint32_t>((phi + 3.14159265358979323846) / (2.0 * 3.14159265358979323846) * 2048.0);
    std::uint32_t h = it * 73856093u ^ ip * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    const double u = (h & 0xffffu) / 65535.0;
    if (u > 0.9985) {
        return static_cast<float>(0.35 + 0.65 * ((h >> 16) & 0xffu) / 255.0);
    }
    return 0.0f;
}

}  // namespace

// Peak Page–Thorne effective temperature for normalisation (sampled once per render).
double peak_effective_temperature(const Options& opt) {
    const double M_kg = opt.scene.mass_kg;
    const double G = units::G_SI;
    const double c = units::C_SI;
    const double mdot = opt.mdot_edd_fraction *
                        disk_emission::eddington_accretion_rate_si(M_kg, G, c, orbits::thin_disk_efficiency());
    double best = 0.0;
    for (int i = 0; i <= 400; ++i) {
        const double r_rs = 3.0 + 7.0 * i / 400.0;  // 3 rs (ISCO) … 10 rs
        const double F = disk_emission::page_thorne_flux_si(r_rs * opt.scene.r_s_m, M_kg, mdot, G, c);
        best = std::max(best, disk_emission::effective_temperature(F));
    }
    return best > 0.0 ? best : 1.0;
}

const char* color_mode_name(ColorMode m) {
    switch (m) {
        case ColorMode::Legacy: return "legacy";
        case ColorMode::LegacyDoppler: return "relativistic";
        case ColorMode::Blackbody: return "blackbody";
    }
    return "legacy";
}

const char* integrator_name(IntegratorKind k) {
    return k == IntegratorKind::Rk45 ? "rk45" : "rk4";
}

bool parse_color_mode(const std::string& s, ColorMode& out) {
    if (s == "legacy") { out = ColorMode::Legacy; return true; }
    if (s == "relativistic" || s == "doppler") { out = ColorMode::LegacyDoppler; return true; }
    if (s == "blackbody") { out = ColorMode::Blackbody; return true; }
    return false;
}

bool parse_integrator(const std::string& s, IntegratorKind& out) {
    if (s == "rk4") { out = IntegratorKind::Rk4Geometric; return true; }
    if (s == "rk45") { out = IntegratorKind::Rk45; return true; }
    return false;
}

GeometricScene make_geometric_scene(const SceneParams& scene) {
    GeometricScene gs;
    gs.rs_m = scene.r_s_m > 0.0 ? scene.r_s_m : units::LEGACY_SAGA_RS_M;
    gs.disk_inner = scene.disk_inner_factor_rs;
    gs.disk_outer = scene.disk_outer_factor_rs;
    double bound = gs.disk_outer;
    for (const SceneObject& o : scene.objects) {
        const V3 c{{o.pos_m[0] / gs.rs_m, o.pos_m[1] / gs.rs_m, o.pos_m[2] / gs.rs_m}};
        const double R = o.radius_m / gs.rs_m;
        // Skip the black-hole marker: the horizon test handles the hole exactly.
        if (is_black_hole_marker(o, scene)) {
            continue;
        }
        if (R <= 0.0) {
            continue;
        }
        gs.spheres.push_back({c, R, o.color_rgba});
        bound = std::max(bound, len(c) + R);
    }
    gs.scene_bound = bound * 1.05 + 0.5;
    return gs;
}

CameraFrame make_camera_frame(const Options& opt, const GeometricScene& gs) {
    CameraFrame cam{};
    double px, py, pz;
    camera::position(opt.scene.camera, px, py, pz);
    cam.pos = {{px / gs.rs_m, py / gs.rs_m, pz / gs.rs_m}};
    const V3 target{{opt.scene.camera.target_x / gs.rs_m, opt.scene.camera.target_y / gs.rs_m,
                     opt.scene.camera.target_z / gs.rs_m}};
    cam.forward = normalize(sub(target, cam.pos));
    V3 up0{{0.0, 1.0, 0.0}};
    // Same world-up (+Y) as black_hole.cpp uploadCameraUBO and the Blender
    // camera; fall back only when forward ∥ Y makes the cross product vanish
    // (elevation exactly 0 or π, outside the legacy clamp).
    if (len(cross(cam.forward, up0)) < 1e-9) {
        up0 = {{0.0, 0.0, 1.0}};
    }
    cam.right = normalize(cross(cam.forward, up0));
    cam.up = cross(cam.right, cam.forward);
    cam.tan_half_fov = camera::tan_half_fov(opt.scene.camera);
    cam.aspect = static_cast<double>(opt.width) / static_cast<double>(opt.height);
    return cam;
}

std::array<double, 3> primary_ray_direction(const CameraFrame& cam, double sx, double sy) {
    const double u = (2.0 * sx - 1.0) * cam.aspect * cam.tan_half_fov;
    const double v = (1.0 - 2.0 * sy) * cam.tan_half_fov;  // +v = up (row 0 looks up)
    return normalize(add(add(mul(cam.right, u), mul(cam.up, v)), cam.forward));
}

TraceResult trace_ray(const Options& opt, const GeometricScene& gs, const V3& pos, const V3& dir) {
    TraceResult res;
    const double rs = 1.0;
    PlanarRay ray = make_planar_ray(pos, dir, rs);
    V3 prev = planar_position(ray);

    // Start inside the horizon (degenerate camera): immediate capture.
    if (planar_radius(ray) <= rs) {
        res.kind = HitKind::Horizon;
        res.hit_pos = prev;
        return res;
    }

    Rk45Options rk45;
    rk45.abs_tol = opt.rk45_tol;
    rk45.rel_tol = opt.rk45_tol;
    rk45.h_min = opt.step_min_rs * 0.1;
    rk45.h_max = opt.step_max_rs;
    double h = geometric_step(planar_radius(ray), rs, opt.step_k, opt.step_min_rs, opt.step_max_rs);

    const double Ly = ray.L_vec[DISK_AXIS];

    // A camera lying in the disk plane (legacy default elevation = π/2) is ON the
    // disk, not infinitesimally above/below it: a ray leaving the plane on its
    // first segment is not a crossing. The tolerance (1e-6·r, same as the GPU
    // shader) must absorb float32 elevations: Blender stores π/2 as
    // 1.5707963705…, cos = −4.4e−8, which with a 1e−9 threshold brought back
    // the legacy half-frame disk artifact.
    const bool camera_on_plane = std::abs(prev[DISK_AXIS]) < CAMERA_ON_PLANE_TOLERANCE * planar_radius(ray);

    for (int step = 0; step < opt.max_steps; ++step) {
        const double cap = step_cap(opt, gs, planar_radius(ray));
        if (opt.integrator == IntegratorKind::Rk45) {
            rk45.h_max = cap;
            const Rk45Result r = planar_rk45_step(ray, std::min(h, cap), rs, rk45);
            h = r.h_next;
        } else {
            h = geometric_step(planar_radius(ray), rs, opt.step_k, opt.step_min_rs, cap);
            planar_rk4_step(ray, h, rs);
        }
        res.steps = step + 1;
        const V3 cur = planar_position(ray);

        // Candidate hits along the segment prev→cur, keep the earliest.
        double best_t = 2.0;
        HitKind best_kind = HitKind::Escaped;
        V3 best_pos = cur;
        double best_rho = 0.0;
        int best_obj = -1;

        double t_h;
        if (hit::segment_sphere_hit(prev, cur, V3{{0.0, 0.0, 0.0}}, rs, t_h) || planar_radius(ray) <= rs) {
            if (!(planar_radius(ray) <= rs)) {
                best_t = t_h;
            } else {
                best_t = std::min(best_t, 1.0);
            }
            best_kind = HitKind::Horizon;
            best_pos = hit::lerp(prev, cur, std::min(best_t, 1.0));
        }

        double t_d;
        V3 disk_p;
        const bool skip_disk = camera_on_plane && step == 0;
        if (!skip_disk && hit::segment_plane_crossing(prev, cur, DISK_AXIS, t_d, disk_p)) {
            double rho;
            if (hit::point_in_annulus(disk_p, DISK_AXIS, gs.disk_inner, gs.disk_outer, rho) && t_d < best_t) {
                best_t = t_d;
                best_kind = HitKind::Disk;
                best_pos = disk_p;
                best_rho = rho;
            }
        }

        for (std::size_t i = 0; i < gs.spheres.size(); ++i) {
            double t_o;
            if (hit::segment_sphere_hit(prev, cur, gs.spheres[i].center, gs.spheres[i].radius, t_o) &&
                t_o < best_t) {
                best_t = t_o;
                best_kind = HitKind::Object;
                best_pos = hit::lerp(prev, cur, t_o);
                best_obj = static_cast<int>(i);
            }
        }

        if (best_kind != HitKind::Escaped) {
            res.kind = best_kind;
            res.hit_pos = best_pos;
            res.rho = best_rho;
            res.object_index = best_obj;
            if (best_kind == HitKind::Disk) {
                // Receiver = the static camera at |pos| (not infinity).
                res.g = redshift::doppler_gravitational_factor_at(best_rho, rs, ray.E, Ly, hit::norm3(pos),
                                                                  opt.disk_spin_sign);
            }
            return res;
        }

        if (escape::will_escape_scene(planar_radius(ray), planar_dr(ray), rs, gs.scene_bound)) {
            res.kind = HitKind::Escaped;
            res.hit_pos = cur;
            if (opt.stars) {
                // The sky must be looked up with the ASYMPTOTIC (lensed) direction,
                // not the primary pixel direction. Bending left beyond r is
                // ≲ 2 rs / r, so continue with large steps to r ≥ 1000 rs.
                for (int k = 0; k < 400 && planar_radius(ray) < 1000.0; ++k) {
                    planar_rk4_step(ray, 0.1 * planar_radius(ray), rs);
                }
            }
            res.escape_dir = normalize(planar_velocity(ray));
            return res;
        }
        prev = cur;
    }
    res.kind = HitKind::StepLimit;
    res.hit_pos = prev;
    return res;
}

std::array<float, 3> shade(const Options& opt, const GeometricScene& gs, const CameraFrame& cam,
                           const TraceResult& hit, const V3& dir) {
    std::array<float, 3> out{{0.0f, 0.0f, 0.0f}};
    switch (hit.kind) {
        case HitKind::Horizon:
        case HitKind::StepLimit:
            return out;  // black
        case HitKind::Escaped:
            if (opt.stars) {
                (void)dir;  // primary direction: unlensed — use the traced asymptote
                const float s = star_intensity(hit.escape_dir);
                out = {{s, s, s}};
            }
            return out;
        case HitKind::Object: {
            const auto& sph = gs.spheres[static_cast<std::size_t>(hit.object_index)];
            const V3 N = normalize(sub(hit.hit_pos, sph.center));
            const V3 V = normalize(sub(cam.pos, hit.hit_pos));
            const double ambient = 0.1;
            const double diff = std::max(dot(N, V), 0.0);
            // geodesic.comp writes vec4(shaded, a) and the fullscreen pass blends
            // with SRC_ALPHA over black → shaded · a.
            const double intensity = (ambient + (1.0 - ambient) * diff) * std::clamp(sph.color[3], 0.0, 1.0);
            out = {{static_cast<float>(sph.color[0] * intensity), static_cast<float>(sph.color[1] * intensity),
                    static_cast<float>(sph.color[2] * intensity)}};
            return out;
        }
        case HitKind::Disk:
            break;
    }

    // Disk shading.
    const double r_norm = hit.rho / gs.disk_outer;  // legacy: length(pos)/disk_r2 (pos on the plane)
    if (opt.color_mode == ColorMode::Legacy) {
        // geodesic.comp: color = vec4(1, r, 0.2, r) blended over the black clear colour.
        const double a = std::clamp(r_norm, 0.0, 1.0);
        out = {{static_cast<float>(1.0 * a), static_cast<float>(r_norm * a), static_cast<float>(0.2 * a)}};
        return out;
    }

    const double g = hit.g;
    const double boost = redshift::intensity_boost(g);
    if (opt.color_mode == ColorMode::LegacyDoppler) {
        const double a = std::clamp(r_norm, 0.0, 1.0);
        const double green = std::clamp(r_norm * g, 0.0, 1.0);  // blueshift → whiter, redshift → redder
        const double k = a * boost;
        out = {{static_cast<float>(disk_emission::tonemap_reinhard(1.0 * k, opt.exposure)),
                static_cast<float>(disk_emission::tonemap_reinhard(green * k, opt.exposure)),
                static_cast<float>(disk_emission::tonemap_reinhard(0.2 * k, opt.exposure))}};
        return out;
    }

    // Blackbody (Page–Thorne). Emission is zero inside the ISCO (plunging region).
    // render() fills gs.peak_temperature once per image; direct callers of
    // shade() without it get an on-the-fly value (pure function of opt).
    const double peak_T = gs.peak_temperature > 0.0 ? gs.peak_temperature : peak_effective_temperature(opt);
    const double G = units::G_SI;
    const double c = units::C_SI;
    const double mdot = opt.mdot_edd_fraction *
                        disk_emission::eddington_accretion_rate_si(opt.scene.mass_kg, G, c, orbits::thin_disk_efficiency());
    const double F = disk_emission::page_thorne_flux_si(hit.rho * gs.rs_m, opt.scene.mass_kg, mdot, G, c);
    const double T_emit = disk_emission::effective_temperature(F);
    if (T_emit <= 0.0) {
        return out;
    }
    const double T_obs = redshift::observed_temperature(g, T_emit);
    double cr, cg, cb;
    disk_emission::blackbody_rgb(T_obs, cr, cg, cb);
    const double rel = T_emit / peak_T;
    const double brightness = 4.0 * boost * rel * rel * rel * rel;  // ×4: keep the peak visible after Reinhard
    out = {{static_cast<float>(disk_emission::tonemap_reinhard(cr * brightness, opt.exposure)),
            static_cast<float>(disk_emission::tonemap_reinhard(cg * brightness, opt.exposure)),
            static_cast<float>(disk_emission::tonemap_reinhard(cb * brightness, opt.exposure))}};
    return out;
}

Image render(const Options& opt, Stats* stats) {
    Image img;
    img.width = std::max(1, opt.width);
    img.height = std::max(1, opt.height);
    img.rgb.assign(static_cast<std::size_t>(img.width) * img.height * 3, 0.0f);

    GeometricScene gs = make_geometric_scene(opt.scene);
    if (opt.color_mode == ColorMode::Blackbody) {
        gs.peak_temperature = peak_effective_temperature(opt);
    }
    const CameraFrame cam = make_camera_frame(opt, gs);
    const int S = std::max(1, opt.supersample);
    const double inv_samples = 1.0 / (S * S);

    unsigned threads = opt.threads > 0 ? static_cast<unsigned>(opt.threads) : std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;
    threads = std::min<unsigned>(threads, static_cast<unsigned>(img.height));

    std::atomic<int> next_row{0};
    std::mutex stats_mutex;
    Stats total{};

    auto worker = [&]() {
        Stats local{};
        for (;;) {
            const int y = next_row.fetch_add(1);
            if (y >= img.height) break;
            for (int x = 0; x < img.width; ++x) {
                double acc[3] = {0.0, 0.0, 0.0};
                for (int sy = 0; sy < S; ++sy) {
                    for (int sx = 0; sx < S; ++sx) {
                        const double fx = (x + (sx + 0.5) / S) / img.width;
                        const double fy = (y + (sy + 0.5) / S) / img.height;
                        const V3 dir = primary_ray_direction(cam, fx, fy);
                        const TraceResult hit = trace_ray(opt, gs, cam.pos, dir);
                        const auto c = shade(opt, gs, cam, hit, dir);
                        acc[0] += c[0];
                        acc[1] += c[1];
                        acc[2] += c[2];
                        ++local.rays;
                        local.total_steps += static_cast<std::uint64_t>(hit.steps);
                        switch (hit.kind) {
                            case HitKind::Horizon: ++local.horizon; break;
                            case HitKind::Disk:
                                ++local.disk;
                                local.min_g = std::min(local.min_g, hit.g);
                                local.max_g = std::max(local.max_g, hit.g);
                                break;
                            case HitKind::Object: ++local.object; break;
                            case HitKind::Escaped: ++local.escaped; break;
                            case HitKind::StepLimit: ++local.step_limit; break;
                        }
                    }
                }
                for (int c = 0; c < 3; ++c) {
                    img.at(x, y, c) = static_cast<float>(acc[c] * inv_samples);
                }
            }
        }
        std::lock_guard<std::mutex> lock(stats_mutex);
        total.rays += local.rays;
        total.horizon += local.horizon;
        total.disk += local.disk;
        total.object += local.object;
        total.escaped += local.escaped;
        total.step_limit += local.step_limit;
        total.total_steps += local.total_steps;
        total.min_g = std::min(total.min_g, local.min_g);
        total.max_g = std::max(total.max_g, local.max_g);
    };

    std::vector<std::thread> pool;
    for (unsigned i = 1; i < threads; ++i) pool.emplace_back(worker);
    worker();
    for (auto& t : pool) t.join();

    if (stats) *stats = total;
    return img;
}

}  // namespace render
}  // namespace bh
