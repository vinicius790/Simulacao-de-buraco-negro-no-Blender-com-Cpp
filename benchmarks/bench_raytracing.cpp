// Headless Ray-Tracing Benchmark using OpenMP-parallelized RK4 integrator.
// Measures runtime, throughput, and geodesic outcome statistics for a 200x150 ray grid.

#include "black_hole/camera_model.hpp"
#include "black_hole/integrator_rk4.hpp"
#include "black_hole/ray_state.hpp"
#include "black_hole/schwarzschild.hpp"
#include "black_hole/units.hpp"

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::cout << "====================================================\n";
    std::cout << "   Black Hole Ray-Tracing OpenMP RK4 Benchmark\n";
    std::cout << "====================================================\n";

    // 1. Thread configuration
    int thread_count = 1;
#if defined(_OPENMP)
    thread_count = omp_get_max_threads();
    std::cout << "OpenMP Status    : ENABLED\n";
    std::cout << "Max Threads      : " << thread_count << "\n";
#else
    std::cout << "OpenMP Status    : DISABLED (single-threaded build)\n";
    std::cout << "Threads          : 1\n";
#endif

    // 2. Physical & Scene Parameters
    const double rs = bh::units::LEGACY_SAGA_RS_M; // ~1.269e10 m
    const int compute_w = 200;
    const int compute_h = 150;
    const std::size_t total_rays = static_cast<std::size_t>(compute_w * compute_h);
    const double d_lambda = 1.0e7;
    const int max_steps = 200;
    const double escape_r = 1.0e12;

    std::cout << "\n--- Simulation Configuration ---\n";
    std::cout << "Black Hole rs    : " << std::scientific << std::setprecision(4) << rs << " m (Sagittarius A*)\n";
    std::cout << "Grid Resolution  : " << std::defaultfloat << compute_w << " x " << compute_h
              << " (" << total_rays << " rays)\n";
    std::cout << "Step Size (d_λ)  : " << std::scientific << std::setprecision(2) << d_lambda << "\n";
    std::cout << "Max Step Count   : " << std::defaultfloat << max_steps << " steps/ray\n";
    std::cout << "Escape Horizon   : " << std::scientific << std::setprecision(2) << escape_r << " m\n";

    // 3. Setup Camera and Ray Grid
    bh::camera::OrbitCamera cam;
    cam.radius_m = bh::camera::LEGACY_RADIUS_M;
    cam.azimuth_rad = bh::camera::LEGACY_AZIMUTH_RAD;
    cam.elevation_rad = bh::camera::LEGACY_ELEVATION_RAD;
    cam.fov_y_deg = bh::camera::LEGACY_FOV_Y_DEG;

    double cam_x = 0.0, cam_y = 0.0, cam_z = 0.0;
    bh::camera::position(cam, cam_x, cam_y, cam_z);

    double fwd_x = 0.0, fwd_y = 0.0, fwd_z = 0.0;
    bh::camera::forward(cam, fwd_x, fwd_y, fwd_z);

    // Orthonormal camera basis (Y is up in legacy coordinates)
    // right = forward x up_world (with up_world = 0, 1, 0)
    double rx = -fwd_z;
    double ry = 0.0;
    double rz = fwd_x;
    const double r_len = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (r_len > 1.0e-12) {
        rx /= r_len;
        ry /= r_len;
        rz /= r_len;
    } else {
        rx = 1.0; ry = 0.0; rz = 0.0;
    }

    // up = right x forward
    double ux = ry * fwd_z - rz * fwd_y;
    double uy = rz * fwd_x - rx * fwd_z;
    double uz = rx * fwd_y - ry * fwd_x;
    const double u_len = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (u_len > 1.0e-12) {
        ux /= u_len;
        uy /= u_len;
        uz /= u_len;
    }

    const double aspect = bh::camera::LEGACY_ASPECT;
    const double tan_half_fov = bh::camera::tan_half_fov(cam);

    std::vector<bh::RayState> rays;
    rays.reserve(total_rays);

    for (int y = 0; y < compute_h; ++y) {
        for (int x = 0; x < compute_w; ++x) {
            const double u = (2.0 * (x + 0.5) / compute_w - 1.0) * aspect * tan_half_fov;
            const double v = (1.0 - 2.0 * (y + 0.5) / compute_h) * tan_half_fov;
            double dx = u * rx - v * ux + fwd_x;
            double dy = u * ry - v * uy + fwd_y;
            double dz = u * rz - v * uz + fwd_z;
            const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len > 1.0e-12) {
                dx /= len;
                dy /= len;
                dz /= len;
            }
            rays.push_back(bh::make_ray_from_cartesian(cam_x, cam_y, cam_z, dx, dy, dz, rs));
        }
    }

    std::vector<bh::BatchRayResult> results(total_rays);

    std::cout << "\n--- Executing Ray Tracing Benchmark ---\n";
    auto t_start = std::chrono::high_resolution_clock::now();

    bh::integrate_rays_rk4(rays.data(), rays.size(), d_lambda, rs, max_steps, escape_r, results.data());

    auto t_end = std::chrono::high_resolution_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    const double elapsed_sec = elapsed_ms / 1000.0;

    // 4. Statistics aggregation
    long long captured_count = 0;
    long long escaped_count = 0;
    long long unfinished_count = 0;
    long long total_steps = 0;

    for (std::size_t i = 0; i < total_rays; ++i) {
        total_steps += results[i].steps_taken;
        if (results[i].captured) {
            ++captured_count;
        } else if (results[i].escaped) {
            ++escaped_count;
        } else {
            ++unfinished_count;
        }
    }

    const double rays_per_sec = (elapsed_sec > 0.0) ? (static_cast<double>(total_rays) / elapsed_sec) : 0.0;
    const double us_per_ray = (total_rays > 0) ? ((elapsed_ms * 1000.0) / static_cast<double>(total_rays)) : 0.0;
    const double avg_steps_per_ray = (total_rays > 0) ? (static_cast<double>(total_steps) / static_cast<double>(total_rays)) : 0.0;
    const double steps_per_sec = (elapsed_sec > 0.0) ? (static_cast<double>(total_steps) / elapsed_sec) : 0.0;

    std::cout << "Elapsed Time     : " << std::fixed << std::setprecision(2) << elapsed_ms << " ms ("
              << std::setprecision(4) << elapsed_sec << " s)\n";
    std::cout << "Throughput       : " << std::fixed << std::setprecision(0) << rays_per_sec << " rays/sec\n";
    std::cout << "Step Throughput  : " << std::fixed << std::setprecision(0) << steps_per_sec << " steps/sec\n";
    std::cout << "Latency          : " << std::fixed << std::setprecision(2) << us_per_ray << " µs/ray\n";

    std::cout << "\n--- Ray Outcome Statistics ---\n";
    std::cout << "Captured (Event Horizon) : " << captured_count
              << " (" << std::fixed << std::setprecision(2) << (100.0 * captured_count / total_rays) << "%)\n";
    std::cout << "Escaped (Infinity)       : " << escaped_count
              << " (" << std::fixed << std::setprecision(2) << (100.0 * escaped_count / total_rays) << "%)\n";
    std::cout << "Unfinished (Max Steps)   : " << unfinished_count
              << " (" << std::fixed << std::setprecision(2) << (100.0 * unfinished_count / total_rays) << "%)\n";
    std::cout << "Total RK4 Steps Taken    : " << total_steps << "\n";
    std::cout << "Avg Steps per Ray        : " << std::fixed << std::setprecision(1) << avg_steps_per_ray << "\n";
    std::cout << "====================================================\n";

    return 0;
}
