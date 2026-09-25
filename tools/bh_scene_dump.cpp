// Dump default (or loaded) scene params as restricted-schema JSON.
// Linked to bh_scientific only — does not touch black_hole.cpp.
//
// Usage:
//   bh_scene_dump                     # print defaults to stdout
//   bh_scene_dump --load path.json    # load, re-serialize
//   bh_scene_dump --out path.json     # write defaults (or loaded) to file

#include "black_hole/scene_params.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string load_path;
    std::string out_path;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--load" && i + 1 < argc) {
            load_path = argv[++i];
        } else if (a == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (a == "--help" || a == "-h") {
            std::cout << "bh_scene_dump [--load in.json] [--out out.json]\n"
                         "Prints black_hole.scene_params/v1 defaults (or loaded file).\n";
            return 0;
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            return 2;
        }
    }

    bh::SceneParams params = bh::make_default_scene_params();
    std::string err;
    if (!load_path.empty()) {
        if (!bh::load_scene_params_json(load_path, params, err)) {
            std::cerr << "load failed: " << err << "\n";
            return 1;
        }
    }

    if (!out_path.empty()) {
        if (!bh::save_scene_params_json(out_path, params, err)) {
            std::cerr << "save failed: " << err << "\n";
            return 1;
        }
        std::cerr << "Wrote " << out_path << "\n";
    }

    std::cout << bh::scene_params_to_json(params);
    return 0;
}
