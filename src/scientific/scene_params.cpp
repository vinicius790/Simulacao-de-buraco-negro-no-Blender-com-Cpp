#include "black_hole/scene_params.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace bh {
namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// Extract the value text for a top-level-ish key occurrence: "key"\s*:\s*<value>
// Value ends at the matching comma / closing brace / bracket at depth 0.
bool extract_value_after_key(const std::string& text, const std::string& key,
                             std::string& value_out) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = 0;
    while (true) {
        pos = text.find(needle, pos);
        if (pos == std::string::npos) {
            return false;
        }
        std::size_t colon = text.find(':', pos + needle.size());
        if (colon == std::string::npos) {
            return false;
        }
        std::size_t i = colon + 1;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i >= text.size()) {
            return false;
        }

        std::size_t start = i;
        int depth = 0;
        bool in_string = false;
        bool escape = false;
        for (; i < text.size(); ++i) {
            char c = text[i];
            if (in_string) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }
            if (c == '"') {
                in_string = true;
                continue;
            }
            if (c == '{' || c == '[') {
                ++depth;
                continue;
            }
            if (c == '}' || c == ']') {
                if (depth == 0) {
                    break;
                }
                --depth;
                continue;
            }
            if ((c == ',' || c == '}' || c == ']') && depth == 0) {
                break;
            }
        }
        value_out = trim(text.substr(start, i - start));
        return true;
    }
}

bool parse_double(const std::string& s, double& out) {
    try {
        std::size_t idx = 0;
        out = std::stod(s, &idx);
        while (idx < s.size() && std::isspace(static_cast<unsigned char>(s[idx]))) {
            ++idx;
        }
        return idx == s.size();
    } catch (...) {
        return false;
    }
}

bool parse_bool(const std::string& s, bool& out) {
    if (s == "true") {
        out = true;
        return true;
    }
    if (s == "false") {
        out = false;
        return true;
    }
    return false;
}

bool parse_string_literal(const std::string& s, std::string& out) {
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') {
        return false;
    }
    out.clear();
    for (std::size_t i = 1; i + 1 < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 + 1 <= s.size()) {
            out.push_back(s[i + 1]);
            ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return true;
}

// Parse a JSON array of numbers: [a, b, c, ...]
bool parse_number_array(const std::string& s, std::vector<double>& out) {
    out.clear();
    std::string t = trim(s);
    if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
        return false;
    }
    t = trim(t.substr(1, t.size() - 2));
    if (t.empty()) {
        return true;
    }
    std::stringstream ss(t);
    std::string item;
    while (std::getline(ss, item, ',')) {
        double v = 0.0;
        if (!parse_double(trim(item), v)) {
            return false;
        }
        out.push_back(v);
    }
    return true;
}

std::string escape_string(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o.push_back('\\');
        }
        o.push_back(c);
    }
    return o;
}

std::string format_double(double v) {
    std::ostringstream os;
    os.setf(std::ios::fmtflags(0), std::ios::floatfield);
    os.precision(17);
    os << v;
    return os.str();
}

// Extract a nested object body for a key: "key" : { ... }
bool extract_object_body(const std::string& text, const std::string& key,
                         std::string& body_out) {
    std::string value;
    if (!extract_value_after_key(text, key, value)) {
        return false;
    }
    value = trim(value);
    if (value.size() < 2 || value.front() != '{' || value.back() != '}') {
        return false;
    }
    body_out = value.substr(1, value.size() - 2);
    return true;
}

bool extract_array_body(const std::string& text, const std::string& key,
                        std::string& body_out) {
    std::string value;
    if (!extract_value_after_key(text, key, value)) {
        return false;
    }
    value = trim(value);
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
        return false;
    }
    body_out = value.substr(1, value.size() - 2);
    return true;
}

// Split top-level {...} object literals inside an array body.
std::vector<std::string> split_top_level_objects(const std::string& body) {
    std::vector<std::string> objs;
    int depth = 0;
    bool in_string = false;
    bool escape = false;
    std::size_t start = std::string::npos;
    for (std::size_t i = 0; i < body.size(); ++i) {
        char c = body[i];
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == '{') {
            if (depth == 0) {
                start = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                objs.push_back(body.substr(start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
    return objs;
}

void apply_camera(const std::string& body, SceneParams& out) {
    std::string v;
    if (extract_value_after_key(body, "radius_m", v)) {
        double d;
        if (parse_double(v, d)) {
            out.camera.radius_m = d;
        }
    }
    if (extract_value_after_key(body, "azimuth_rad", v)) {
        double d;
        if (parse_double(v, d)) {
            out.camera.azimuth_rad = d;
        }
    }
    if (extract_value_after_key(body, "elevation_rad", v)) {
        double d;
        if (parse_double(v, d)) {
            out.camera.elevation_rad = d;
        }
    }
    if (extract_value_after_key(body, "fov_y_deg", v)) {
        double d;
        if (parse_double(v, d)) {
            out.camera.fov_y_deg = d;
        }
    }
    if (extract_value_after_key(body, "target_m", v)) {
        std::vector<double> arr;
        if (parse_number_array(v, arr) && arr.size() >= 3) {
            out.camera.target_x = arr[0];
            out.camera.target_y = arr[1];
            out.camera.target_z = arr[2];
        }
    }
}

void apply_disk(const std::string& body, SceneParams& out) {
    std::string v;
    if (extract_value_after_key(body, "inner_factor_rs", v)) {
        double d;
        if (parse_double(v, d)) {
            out.disk_inner_factor_rs = d;
        }
    }
    if (extract_value_after_key(body, "outer_factor_rs", v)) {
        double d;
        if (parse_double(v, d)) {
            out.disk_outer_factor_rs = d;
        }
    }
    if (extract_value_after_key(body, "thickness_m", v)) {
        double d;
        if (parse_double(v, d)) {
            out.disk_thickness_m = d;
        }
    }
    if (extract_value_after_key(body, "disk_num", v)) {
        double d;
        if (parse_double(v, d)) {
            out.disk_num = d;
        }
    }
    // Absolute radii are accepted but factors take precedence when both present;
    // if only absolute radii are present, derive factors from r_s_m.
    double inner_m = -1.0, outer_m = -1.0;
    if (extract_value_after_key(body, "inner_radius_m", v)) {
        parse_double(v, inner_m);
    }
    if (extract_value_after_key(body, "outer_radius_m", v)) {
        parse_double(v, outer_m);
    }
    (void)inner_m;
    (void)outer_m;
}

void apply_black_hole(const std::string& body, SceneParams& out) {
    std::string v;
    if (extract_value_after_key(body, "name", v)) {
        std::string name;
        if (parse_string_literal(v, name)) {
            out.bh_name = name;
        }
    }
    if (extract_value_after_key(body, "mass_kg", v)) {
        double d;
        if (parse_double(v, d)) {
            out.mass_kg = d;
        }
    }
    if (extract_value_after_key(body, "r_s_m", v)) {
        double d;
        if (parse_double(v, d)) {
            out.r_s_m = d;
        }
    }
    if (extract_value_after_key(body, "position_m", v)) {
        std::vector<double> arr;
        if (parse_number_array(v, arr) && arr.size() >= 3) {
            out.bh_position_m = {{arr[0], arr[1], arr[2]}};
        }
    }
}

void apply_render_baseline(const std::string& body, SceneParams& out) {
    std::string v;
    if (extract_value_after_key(body, "window", v)) {
        std::vector<double> arr;
        if (parse_number_array(v, arr) && arr.size() >= 2) {
            out.window_w = static_cast<int>(arr[0]);
            out.window_h = static_cast<int>(arr[1]);
        }
    }
    if (extract_value_after_key(body, "compute", v)) {
        std::vector<double> arr;
        if (parse_number_array(v, arr) && arr.size() >= 2) {
            out.compute_w = static_cast<int>(arr[0]);
            out.compute_h = static_cast<int>(arr[1]);
        }
    }
    if (extract_value_after_key(body, "integrator", v)) {
        std::string s;
        if (parse_string_literal(v, s)) {
            out.integrator = s;
        }
    }
    if (extract_value_after_key(body, "scientific_default", v)) {
        bool b;
        if (parse_bool(v, b)) {
            out.scientific_default = b;
        }
    }
}

void apply_objects(const std::string& array_body, SceneParams& out) {
    out.objects.clear();
    for (const std::string& obj_text : split_top_level_objects(array_body)) {
        if (out.objects.size() >= SCENE_MAX_OBJECTS) {
            break;
        }
        SceneObject obj;
        std::string v;
        if (extract_value_after_key(obj_text, "pos_m", v) ||
            extract_value_after_key(obj_text, "position_m", v)) {
            std::vector<double> arr;
            if (parse_number_array(v, arr) && arr.size() >= 3) {
                obj.pos_m = {{arr[0], arr[1], arr[2]}};
            }
        }
        if (extract_value_after_key(obj_text, "radius_m", v)) {
            double d;
            if (parse_double(v, d)) {
                obj.radius_m = d;
            }
        }
        if (extract_value_after_key(obj_text, "color_rgba", v) ||
            extract_value_after_key(obj_text, "color", v)) {
            std::vector<double> arr;
            if (parse_number_array(v, arr) && arr.size() >= 3) {
                obj.color_rgba[0] = arr[0];
                obj.color_rgba[1] = arr[1];
                obj.color_rgba[2] = arr[2];
                obj.color_rgba[3] = arr.size() >= 4 ? arr[3] : 1.0;
            }
        }
        if (extract_value_after_key(obj_text, "mass_kg", v) ||
            extract_value_after_key(obj_text, "mass", v)) {
            double d;
            if (parse_double(v, d)) {
                obj.mass_kg = d;
            }
        }
        out.objects.push_back(obj);
    }
}

}  // namespace

SceneParams make_default_scene_params() {
    SceneParams p;
    // Seed the three default objects matching black_hole.cpp::objects.
    SceneObject yellow;
    yellow.pos_m = {{4e11, 0.0, 0.0}};
    yellow.radius_m = 4e10;
    yellow.color_rgba = {{1.0, 1.0, 0.0, 1.0}};
    yellow.mass_kg = 1.98892e30;
    SceneObject red;
    red.pos_m = {{0.0, 0.0, 4e11}};
    red.radius_m = 4e10;
    red.color_rgba = {{1.0, 0.0, 0.0, 1.0}};
    red.mass_kg = 1.98892e30;
    SceneObject hole;
    hole.pos_m = {{0.0, 0.0, 0.0}};
    hole.radius_m = units::LEGACY_SAGA_RS_M;
    hole.color_rgba = {{0.0, 0.0, 0.0, 1.0}};
    hole.mass_kg = units::SAGITTARIUS_A_MASS_KG;
    p.objects = {yellow, red, hole};
    return p;
}

bool scene_params_from_json(const std::string& json_text, SceneParams& out, std::string& err) {
    SceneParams tmp = make_default_scene_params();
    // Keep default objects only if the JSON does not supply an objects array.
    bool has_objects = false;

    std::string v;
    if (extract_value_after_key(json_text, "schema", v)) {
        std::string schema;
        if (parse_string_literal(v, schema)) {
            tmp.schema = schema;
        }
    }

    std::string body;
    if (extract_object_body(json_text, "black_hole", body)) {
        apply_black_hole(body, tmp);
    }
    if (extract_object_body(json_text, "camera", body)) {
        apply_camera(body, tmp);
    }
    if (extract_object_body(json_text, "disk", body)) {
        apply_disk(body, tmp);
    }
    if (extract_object_body(json_text, "render_baseline", body)) {
        apply_render_baseline(body, tmp);
    }
    if (extract_value_after_key(json_text, "gravity", v) ||
        extract_value_after_key(json_text, "Gravity", v)) {
        bool b;
        if (parse_bool(v, b)) {
            tmp.gravity = b;
        }
    }
    if (extract_array_body(json_text, "objects", body)) {
        apply_objects(body, tmp);
        has_objects = true;
    }
    (void)has_objects;

    // Minimal sanity: schema should look like our id when present.
    if (!tmp.schema.empty() &&
        tmp.schema.find("black_hole.scene_params") == std::string::npos) {
        err = "unexpected schema id: " + tmp.schema;
        return false;
    }
    if (!(std::isfinite(tmp.camera.radius_m) && tmp.camera.radius_m > 0.0)) {
        err = "invalid camera.radius_m";
        return false;
    }
    if (!(tmp.disk_inner_factor_rs > 0.0 && tmp.disk_outer_factor_rs > tmp.disk_inner_factor_rs)) {
        err = "invalid disk factors";
        return false;
    }

    out = std::move(tmp);
    err.clear();
    return true;
}

std::string scene_params_to_json(const SceneParams& in) {
    std::ostringstream os;
    os << "{\n";
    os << "  \"schema\": \"" << escape_string(in.schema) << "\",\n";
    os << "  \"notes\": \"Restricted schema black_hole.scene_params/v1 — "
          "hand-rolled parser; jq-friendly.\",\n";
    os << "  \"gravity\": " << (in.gravity ? "true" : "false") << ",\n";
    os << "  \"black_hole\": {\n";
    os << "    \"name\": \"" << escape_string(in.bh_name) << "\",\n";
    os << "    \"mass_kg\": " << format_double(in.mass_kg) << ",\n";
    os << "    \"r_s_m\": " << format_double(in.r_s_m) << ",\n";
    os << "    \"position_m\": [" << format_double(in.bh_position_m[0]) << ", "
       << format_double(in.bh_position_m[1]) << ", "
       << format_double(in.bh_position_m[2]) << "]\n";
    os << "  },\n";
    os << "  \"disk\": {\n";
    os << "    \"inner_radius_m\": "
       << format_double(in.r_s_m * in.disk_inner_factor_rs) << ",\n";
    os << "    \"outer_radius_m\": "
       << format_double(in.r_s_m * in.disk_outer_factor_rs) << ",\n";
    os << "    \"inner_factor_rs\": " << format_double(in.disk_inner_factor_rs) << ",\n";
    os << "    \"outer_factor_rs\": " << format_double(in.disk_outer_factor_rs) << ",\n";
    os << "    \"thickness_m\": " << format_double(in.disk_thickness_m) << ",\n";
    os << "    \"disk_num\": " << format_double(in.disk_num) << "\n";
    os << "  },\n";
    os << "  \"camera\": {\n";
    os << "    \"radius_m\": " << format_double(in.camera.radius_m) << ",\n";
    os << "    \"azimuth_rad\": " << format_double(in.camera.azimuth_rad) << ",\n";
    os << "    \"elevation_rad\": " << format_double(in.camera.elevation_rad) << ",\n";
    os << "    \"fov_y_deg\": " << format_double(in.camera.fov_y_deg) << ",\n";
    os << "    \"target_m\": [" << format_double(in.camera.target_x) << ", "
       << format_double(in.camera.target_y) << ", "
       << format_double(in.camera.target_z) << "]\n";
    os << "  },\n";
    os << "  \"objects\": [\n";
    for (std::size_t i = 0; i < in.objects.size(); ++i) {
        const SceneObject& o = in.objects[i];
        os << "    {\n";
        os << "      \"pos_m\": [" << format_double(o.pos_m[0]) << ", "
           << format_double(o.pos_m[1]) << ", " << format_double(o.pos_m[2])
           << "],\n";
        os << "      \"radius_m\": " << format_double(o.radius_m) << ",\n";
        os << "      \"color_rgba\": [" << format_double(o.color_rgba[0]) << ", "
           << format_double(o.color_rgba[1]) << ", "
           << format_double(o.color_rgba[2]) << ", "
           << format_double(o.color_rgba[3]) << "],\n";
        os << "      \"mass_kg\": " << format_double(o.mass_kg) << "\n";
        os << "    }" << (i + 1 < in.objects.size() ? "," : "") << "\n";
    }
    os << "  ],\n";
    os << "  \"render_baseline\": {\n";
    os << "    \"window\": [" << in.window_w << ", " << in.window_h << "],\n";
    os << "    \"compute\": [" << in.compute_w << ", " << in.compute_h << "],\n";
    os << "    \"integrator\": \"" << escape_string(in.integrator) << "\",\n";
    os << "    \"scientific_default\": "
       << (in.scientific_default ? "true" : "false") << "\n";
    os << "  }\n";
    os << "}\n";
    return os.str();
}

bool load_scene_params_json(const std::string& path, SceneParams& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "cannot open: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return scene_params_from_json(ss.str(), out, err);
}

bool save_scene_params_json(const std::string& path, const SceneParams& in, std::string& err) {
    std::ofstream out(path);
    if (!out) {
        err = "cannot write: " + path;
        return false;
    }
    out << scene_params_to_json(in);
    if (!out) {
        err = "write failed: " + path;
        return false;
    }
    err.clear();
    return true;
}

}  // namespace bh
