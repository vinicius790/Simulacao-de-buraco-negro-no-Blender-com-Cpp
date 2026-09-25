#pragma once
// Orbit-camera math mirroring black_hole.cpp::Camera (tests / Blender parity).
//
// Convention (legacy visual baseline):
//   - Target fixed at the black-hole centre (0,0,0).
//   - Y-up world; accretion disk lies in the XZ plane.
//   - elevation ∈ (0.01, π−0.01); elevation = π/2 is the equatorial view.
//   - position = (r sin e cos a,  r cos e,  r sin e sin a)
//   - Default radius 6.34194e10 m, FOV Y = 60°, aspect = 800/600.
//
// Grid-warp companion (CPU generateGrid in black_hole.cpp) — documented here
// so camera_model / ESTILO stay consistent:
//   for each mass, dist = sqrt(dx^2+dz^2);
//   if dist > rs:  deltaY = 2*sqrt(rs*(dist-rs));  y += deltaY - 3e10;
//   else:          y += 2*rs - 3e10;   // deep pit inside the horizon

#include <algorithm>
#include <cmath>

namespace bh {
namespace camera {

inline constexpr double PI = 3.14159265358979323846;

/// Default orbit radius used by black_hole.cpp Camera::radius.
inline constexpr double LEGACY_RADIUS_M = 6.34194e10;

/// Default azimuth (radians).
inline constexpr double LEGACY_AZIMUTH_RAD = 0.0;

/// Default elevation (radians) — equatorial side-on view.
inline constexpr double LEGACY_ELEVATION_RAD = PI / 2.0;

/// Vertical FOV in degrees (uploadCameraUBO uses tan(radians(60/2))).
inline constexpr double LEGACY_FOV_Y_DEG = 60.0;

/// Window aspect used for the compute UBO (WIDTH/HEIGHT = 800/600).
inline constexpr double LEGACY_ASPECT = 800.0 / 600.0;

/// Elevation clamp matching Camera::processMouseMove / position().
inline constexpr double ELEVATION_MIN = 0.01;
inline constexpr double ELEVATION_MAX = PI - 0.01;

/// Zoom clamps from Camera::minRadius / maxRadius.
inline constexpr double MIN_RADIUS_M = 1.0e10;
inline constexpr double MAX_RADIUS_M = 1.0e12;

/// Constant subtracted from grid warp heights in generateGrid (metres).
inline constexpr double GRID_WARP_OFFSET_M = 3.0e10;

struct OrbitCamera {
    double radius_m = LEGACY_RADIUS_M;
    double azimuth_rad = LEGACY_AZIMUTH_RAD;
    double elevation_rad = LEGACY_ELEVATION_RAD;
    double fov_y_deg = LEGACY_FOV_Y_DEG;
    double target_x = 0.0;
    double target_y = 0.0;
    double target_z = 0.0;
};

inline double clamp_elevation(double elevation_rad) {
    return std::clamp(elevation_rad, ELEVATION_MIN, ELEVATION_MAX);
}

inline double clamp_radius(double radius_m) {
    return std::clamp(radius_m, MIN_RADIUS_M, MAX_RADIUS_M);
}

/// World-space camera position mirroring Camera::position().
inline void position(const OrbitCamera& cam,
                     double& out_x, double& out_y, double& out_z) {
    const double e = clamp_elevation(cam.elevation_rad);
    const double a = cam.azimuth_rad;
    const double r = cam.radius_m;
    out_x = r * std::sin(e) * std::cos(a);
    out_y = r * std::cos(e);
    out_z = r * std::sin(e) * std::sin(a);
}

/// tan(½ FOV_Y) as uploaded to the Camera UBO.
inline double tan_half_fov(const OrbitCamera& cam) {
    const double half = cam.fov_y_deg * 0.5 * (PI / 180.0);
    return std::tan(half);
}

/// Forward unit vector from camera position toward target (legacy: always origin).
inline void forward(const OrbitCamera& cam,
                    double& out_x, double& out_y, double& out_z) {
    double px, py, pz;
    position(cam, px, py, pz);
    const double dx = cam.target_x - px;
    const double dy = cam.target_y - py;
    const double dz = cam.target_z - pz;
    const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len <= 0.0) {
        out_x = 0.0;
        out_y = 0.0;
        out_z = -1.0;
        return;
    }
    out_x = dx / len;
    out_y = dy / len;
    out_z = dz / len;
}

/// Grid warp Δy for a single mass at cylindrical distance `dist` (metres).
/// Mirrors black_hole.cpp::generateGrid. Aesthetic, not an isometric embedding.
inline double grid_warp_delta_y(double dist, double rs) {
    if (dist > rs) {
        return 2.0 * std::sqrt(rs * (dist - rs)) - GRID_WARP_OFFSET_M;
    }
    // Deep pit inside / at the horizon (legacy: 2*sqrt(rs*rs) - 3e10).
    return 2.0 * rs - GRID_WARP_OFFSET_M;
}

}  // namespace camera
}  // namespace bh
