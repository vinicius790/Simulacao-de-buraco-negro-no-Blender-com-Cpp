#pragma once
// Scene-parameter JSON I/O for the black_hole package (additive; not wired into
// the default black_hole.cpp render loop).
//
// Schema: black_hole.scene_params/v1  (jq-friendly, flat-enough nesting).
// Parser: hand-rolled / nlohmann-free — only the RESTRICTED key set below.
// See examples/scene_params_example.json and blender/examples/scene_params_example.json.
//
// Loaded fields:
//   camera.radius_m / azimuth_rad / elevation_rad / fov_y_deg / target_m[3]
//   disk.inner_factor_rs / outer_factor_rs / thickness_m / disk_num
//   gravity (bool) — maps to the legacy Gravity flag (G key / RMB)
//   objects[] — optional list of {pos_m[3], radius_m, color_rgba[4], mass_kg}
//
// Unknown keys are ignored. Missing keys keep SceneParams defaults (legacy).

#include "black_hole/camera_model.hpp"
#include "black_hole/disk_model.hpp"
#include "black_hole/units.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace bh {

inline constexpr std::size_t SCENE_MAX_OBJECTS = 16;
inline constexpr const char* SCENE_SCHEMA_ID = "black_hole.scene_params/v1";

struct SceneObject {
    std::array<double, 3> pos_m{{0.0, 0.0, 0.0}};
    double radius_m = 0.0;
    std::array<double, 4> color_rgba{{1.0, 1.0, 1.0, 1.0}};
    double mass_kg = 0.0;
};

struct SceneParams {
    std::string schema = SCENE_SCHEMA_ID;

    // Black hole (Sag A* defaults).
    std::string bh_name = "Sagittarius A*";
    double mass_kg = units::SAGITTARIUS_A_MASS_KG;
    double r_s_m = units::LEGACY_SAGA_RS_M;
    std::array<double, 3> bh_position_m{{0.0, 0.0, 0.0}};

    // Camera (mirrors black_hole.cpp::Camera defaults).
    camera::OrbitCamera camera{};

    // Disk factors (geometric annulus; Novikov–Thorne NOT implemented).
    double disk_inner_factor_rs = disk::LEGACY_INNER_FACTOR_RS;
    double disk_outer_factor_rs = disk::LEGACY_OUTER_FACTOR_RS;
    double disk_thickness_m = disk::LEGACY_THICKNESS_M;
    double disk_num = disk::LEGACY_DISK_NUM;

    // Legacy Gravity toggle (default OFF — geodesic integration still runs;
    // Gravity gates N-body / object motion in the CPU loop when enabled).
    bool gravity = false;

    // Optional scene objects (up to SCENE_MAX_OBJECTS).
    std::vector<SceneObject> objects;

    // Render baseline metadata (informational; not applied by the GL binary yet).
    int window_w = 800;
    int window_h = 600;
    int compute_w = 200;
    int compute_h = 150;
    std::string integrator = "legacyEulerStep";
    bool scientific_default = false;
};

/// Fill SceneParams with the locked visual-baseline defaults.
SceneParams make_default_scene_params();

/// Load from a UTF-8 JSON file. Returns false on I/O or parse failure;
/// `err` receives a short message. On failure `out` is left unchanged.
bool load_scene_params_json(const std::string& path, SceneParams& out, std::string& err);

/// Save SceneParams as pretty-printed restricted-schema JSON.
bool save_scene_params_json(const std::string& path, const SceneParams& in, std::string& err);

/// Serialize to a JSON string (pretty).
std::string scene_params_to_json(const SceneParams& in);

/// Parse from a JSON string. Same contract as load_scene_params_json.
bool scene_params_from_json(const std::string& json_text, SceneParams& out, std::string& err);

/// True if `o` is the scene's black-hole MARKER object (the third default object
/// in black_hole.cpp, drawn as a black sphere by the legacy shader). Physically
/// correct paths skip it: the horizon test handles the hole exactly. The test
/// does not depend on scene.r_s_m alone (a marker sized for another mass would
/// otherwise become an opaque sphere that enlarges the shadow):
/// centred on the hole AND (radius within 5 % of scene r_s, or of its own
/// 2GM/c², or mass ≥ half the hole mass).
bool is_black_hole_marker(const SceneObject& o, const SceneParams& scene);

/// Derived geometric annulus for the current disk factors / rs.
inline disk::Annulus scene_disk_annulus(const SceneParams& p) {
    return disk::legacy_annulus(p.r_s_m, p.disk_inner_factor_rs, p.disk_outer_factor_rs,
                                p.disk_thickness_m);
}

}  // namespace bh
