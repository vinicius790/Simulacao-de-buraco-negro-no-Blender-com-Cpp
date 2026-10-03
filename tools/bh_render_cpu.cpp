// bh_render_cpu — headless CPU renderer (reference / Blender bridge).
//
// Usage:
//   bh_render_cpu [--scene scene.json] --out image.png [--width W] [--height H]
//                 [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]
//                 [--azimuth RAD] [--elevation RAD] [--radius-rs X] [--fov-y-deg D]
//                 [--frames N] [--azimuth-turns T] [--elevation-end RAD]
//                 [--threads T] [--supersample S]
//                 [--exposure E] [--gamma G] [--mdot-edd F] [--spin-sign ±1]
//                 [--stars] [--max-steps N] [--quiet]
//
// With --frames N > 1 the azimuth sweeps --azimuth-turns full turns (default 1)
// starting at --azimuth and, if --elevation-end is given, the elevation moves
// linearly from the start elevation to it (frame N−1 reaches it exactly).
// Files are written as <stem>_0000.<ext> … (Blender image sequence).
// Prints one JSON summary line on stdout when done.

#include "black_hole/cpu_renderer.hpp"
#include "black_hole/image_io.hpp"
#include "black_hole/units.hpp"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void usage() {
    std::cout <<
        "bh_render_cpu [--scene scene.json] --out image.png [--width W] [--height H]\n"
        "              [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]\n"
        "              [--azimuth RAD] [--elevation RAD] [--radius-rs X] [--fov-y-deg D]\n"
        "              [--frames N] [--azimuth-turns T] [--elevation-end RAD]\n"
        "              [--threads T] [--supersample S]\n"
        "              [--exposure E] [--gamma G] [--mdot-edd F] [--spin-sign +1|-1]\n"
        "              [--stars] [--max-steps N] [--quiet] [--help]\n";
}

// Strict finite floating-point parse (whole string must be consumed).
bool to_double(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && errno != ERANGE && std::isfinite(out);
}

// Strict integer parse with range check (no double->int UB, no silent truncation).
bool to_int(const std::string& s, int& out, long lo, long hi) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != '\0' || errno == ERANGE || v < lo || v > hi) return false;
    out = static_cast<int>(v);
    return true;
}

// Minimal JSON string escaping for the summary line (Windows paths, quotes, controls).
std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (unsigned char ch : in) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += static_cast<char>(ch);
                }
        }
    }
    return out;
}

std::string frame_path(const std::string& out, int index) {
    const std::size_t dot = out.find_last_of('.');
    const std::size_t slash = out.find_last_of("/\\");
    const bool has_ext = dot != std::string::npos && (slash == std::string::npos || dot > slash);
    const std::string stem = has_ext ? out.substr(0, dot) : out;
    const std::string ext = has_ext ? out.substr(dot) : ".png";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "_%04d", index);
    return stem + buf + ext;
}

}  // namespace

int main(int argc, char** argv) {
    bh::render::Options opt;
    std::string scene_path;
    std::string out_path;
    int frames = 1;
    bool quiet = false;
    bool have_az = false, have_el = false, have_rad = false, have_fov = false;
    double az = 0.0, el = 0.0, rad_rs = 0.0, fov = 60.0;
    double azimuth_turns = 1.0;
    bool have_el_end = false;
    double el_end = 0.0;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](std::string& v) -> bool {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << a << "\n";
                return false;
            }
            v = argv[++i];
            return true;
        };
        std::string v;
        auto bad = [&]() {
            std::cerr << "bad value for " << a << ": '" << v << "'\n";
            return 2;
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--quiet") { quiet = true; }
        else if (a == "--stars") { opt.stars = true; }
        else if (a == "--scene") { if (!need(v)) return 2; scene_path = v; }
        else if (a == "--out") { if (!need(v)) return 2; out_path = v; }
        else if (a == "--width") { if (!need(v)) return 2; if (!to_int(v, opt.width, 1, 16384)) return bad(); }
        else if (a == "--height") { if (!need(v)) return 2; if (!to_int(v, opt.height, 1, 16384)) return bad(); }
        else if (a == "--frames") { if (!need(v)) return 2; if (!to_int(v, frames, 1, 100000)) return bad(); }
        else if (a == "--azimuth-turns") { if (!need(v)) return 2; if (!to_double(v, azimuth_turns)) return bad(); }
        else if (a == "--elevation-end") { if (!need(v)) return 2; if (!to_double(v, el_end)) return bad(); have_el_end = true; }
        else if (a == "--threads") { if (!need(v)) return 2; if (!to_int(v, opt.threads, 0, 1024)) return bad(); }
        else if (a == "--supersample") { if (!need(v)) return 2; if (!to_int(v, opt.supersample, 1, 16)) return bad(); }
        else if (a == "--max-steps") { if (!need(v)) return 2; if (!to_int(v, opt.max_steps, 1, 100000000)) return bad(); }
        else if (a == "--exposure") { if (!need(v)) return 2; if (!to_double(v, opt.exposure) || opt.exposure < 0.0) return bad(); }
        else if (a == "--gamma") { if (!need(v)) return 2; if (!to_double(v, opt.gamma) || opt.gamma <= 0.0) return bad(); }
        else if (a == "--mdot-edd") { if (!need(v)) return 2; if (!to_double(v, opt.mdot_edd_fraction) || opt.mdot_edd_fraction <= 0.0) return bad(); }
        else if (a == "--spin-sign") {
            if (!need(v)) return 2;
            if (!to_double(v, opt.disk_spin_sign) || (opt.disk_spin_sign != 1.0 && opt.disk_spin_sign != -1.0)) {
                std::cerr << "bad --spin-sign (use +1 or -1): '" << v << "'\n";
                return 2;
            }
        }
        else if (a == "--azimuth") { if (!need(v)) return 2; if (!to_double(v, az)) return bad(); have_az = true; }
        else if (a == "--elevation") { if (!need(v)) return 2; if (!to_double(v, el)) return bad(); have_el = true; }
        else if (a == "--radius-rs") { if (!need(v)) return 2; if (!to_double(v, rad_rs) || rad_rs <= 0.0) return bad(); have_rad = true; }
        else if (a == "--fov-y-deg") { if (!need(v)) return 2; if (!to_double(v, fov) || fov <= 0.0 || fov >= 180.0) return bad(); have_fov = true; }
        else if (a == "--mode") {
            if (!need(v) || !bh::render::parse_color_mode(v, opt.color_mode)) {
                std::cerr << "bad --mode (legacy|relativistic|blackbody)\n";
                return 2;
            }
        } else if (a == "--integrator") {
            if (!need(v) || !bh::render::parse_integrator(v, opt.integrator)) {
                std::cerr << "bad --integrator (rk4|rk45)\n";
                return 2;
            }
        } else {
            std::cerr << "unknown argument: " << a << "\n";
            usage();
            return 2;
        }
    }

    if (out_path.empty()) {
        std::cerr << "--out is required\n";
        usage();
        return 2;
    }
    if (frames < 1) frames = 1;
    if (opt.width < 1 || opt.height < 1) {
        std::cerr << "width/height must be >= 1\n";
        return 2;
    }

    if (!scene_path.empty()) {
        std::string err;
        if (!bh::load_scene_params_json(scene_path, opt.scene, err)) {
            std::cerr << "scene load failed: " << err << "\n";
            return 1;
        }
    }
    if (have_az) opt.scene.camera.azimuth_rad = az;
    if (have_el) opt.scene.camera.elevation_rad = el;
    if (have_rad) opt.scene.camera.radius_m = rad_rs * opt.scene.r_s_m;
    if (have_fov) opt.scene.camera.fov_y_deg = fov;

    const double az0 = opt.scene.camera.azimuth_rad;
    const double el0 = opt.scene.camera.elevation_rad;
    const auto t0 = std::chrono::steady_clock::now();
    bh::render::Stats agg{};
    std::uint64_t total_steps = 0;

    for (int f = 0; f < frames; ++f) {
        if (frames > 1) {
            // Azimuth: frame N would equal frame 0 for whole turns (seamless loop).
            opt.scene.camera.azimuth_rad = az0 + 2.0 * 3.14159265358979323846 * azimuth_turns * f / frames;
            if (have_el_end) {
                opt.scene.camera.elevation_rad = el0 + (el_end - el0) * f / (frames - 1);
            }
        }
        bh::render::Stats st;
        const bh::render::Image img = bh::render::render(opt, &st);
        agg.rays += st.rays;
        agg.horizon += st.horizon;
        agg.disk += st.disk;
        agg.object += st.object;
        agg.escaped += st.escaped;
        agg.step_limit += st.step_limit;
        total_steps += st.total_steps;
        agg.min_g = std::min(agg.min_g, st.min_g);
        agg.max_g = std::max(agg.max_g, st.max_g);

        const std::string path = frames > 1 ? frame_path(out_path, f) : out_path;
        std::string err;
        if (!bh::image_io::write_image(path, img, opt.gamma, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        if (!quiet) {
            std::cerr << "[bh_render_cpu] wrote " << path << " (" << img.width << "x" << img.height
                      << ", shadow " << st.shadow_fraction() << ", disk " << st.disk_fraction() << ")\n";
        }
    }

    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::ostringstream js;
    js.precision(6);
    js << "{\"frames\":" << frames << ",\"width\":" << opt.width << ",\"height\":" << opt.height
       << ",\"mode\":\"" << bh::render::color_mode_name(opt.color_mode) << "\""
       << ",\"integrator\":\"" << bh::render::integrator_name(opt.integrator) << "\""
       << ",\"shadow_fraction\":" << agg.shadow_fraction()
       << ",\"disk_fraction\":" << agg.disk_fraction()
       << ",\"object_fraction\":" << agg.object_fraction()
       << ",\"escaped_fraction\":" << agg.escaped_fraction()
       << ",\"step_limit_rays\":" << agg.step_limit
       << ",\"mean_steps\":" << (agg.rays ? double(total_steps) / double(agg.rays) : 0.0)
       << ",\"min_g\":" << (agg.disk ? agg.min_g : 0.0)
       << ",\"max_g\":" << (agg.disk ? agg.max_g : 0.0)
       << ",\"seconds\":" << secs << ",\"out\":\"" << json_escape(out_path) << "\"}";
    std::cout << js.str() << std::endl;
    return 0;
}
