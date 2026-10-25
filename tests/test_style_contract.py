#!/usr/bin/env python3
"""Style-contract assertions complementary to validate_source_invariants.py.

Locks visual-bible numbers that Blender / scene JSON / scientific helpers must
match: disk factors 2.2 / 5.2, SagA_rs literal, grid warp, disk colour.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def require(text: str, pattern: str, description: str) -> None:
    if re.search(pattern, text, re.MULTILINE | re.DOTALL) is None:
        raise AssertionError(f"Missing style contract: {description}\npattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    cpp = (root / "black_hole.cpp").read_text(encoding="utf-8")
    shader = (root / "geodesic.comp").read_text(encoding="utf-8")
    disk_hdr = (root / "include/black_hole/disk_model.hpp").read_text(encoding="utf-8")
    cam_hdr = (root / "include/black_hole/camera_model.hpp").read_text(encoding="utf-8")
    example = (root / "examples/scene_params_example.json").read_text(encoding="utf-8")
    grid_frag = (root / "grid.frag").read_text(encoding="utf-8")

    # Locked disk factors (visual baseline).
    require(cpp, r"SagA\.r_s\s*\*\s*2\.2f", "CPU disk inner factor 2.2")
    require(cpp, r"SagA\.r_s\s*\*\s*5\.2f", "CPU disk outer factor 5.2")
    require(disk_hdr, r"LEGACY_INNER_FACTOR_RS\s*=\s*2\.2", "disk_model inner 2.2")
    require(disk_hdr, r"LEGACY_OUTER_FACTOR_RS\s*=\s*5\.2", "disk_model outer 5.2")
    require(example, r'"inner_factor_rs"\s*:\s*2\.2', "example JSON inner 2.2")
    require(example, r'"outer_factor_rs"\s*:\s*5\.2', "example JSON outer 5.2")

    # SagA_rs literal must remain in the production shader.
    require(shader, r"const\s+float\s+SagA_rs\s*=\s*1\.269e10", "SagA_rs string in geodesic.comp")

    # Disk colour aesthetic: vec3(1.0, r, 0.2)
    require(shader, r"vec3\s*\(\s*1\.0\s*,\s*r\s*,\s*0\.2\s*\)", "disk colour vec3(1,r,0.2)")

    # Grid warp formula pieces (see docs/ESTILO_VISUAL_SIMULACAO.md).
    require(
        cpp,
        r"2\.0\s*\*\s*sqrt\s*\(\s*r_s\s*\*\s*\(\s*dist\s*-\s*r_s\s*\)\s*\)",
        "grid warp deltaY = 2*sqrt(rs*(dist-rs))",
    )
    require(cpp, r"3e10f", "grid warp offset 3e10")
    require(cam_hdr, r"GRID_WARP_OFFSET_M\s*=\s*3\.0e10", "camera_model grid warp offset")

    # Window / compute defaults.
    require(cpp, r"int\s+WIDTH\s*=\s*800", "window width 800")
    require(cpp, r"int\s+HEIGHT\s*=\s*600", "window height 600")
    require(cpp, r"int\s+COMPUTE_WIDTH\s*=\s*200", "compute width 200")
    require(cpp, r"int\s+COMPUTE_HEIGHT\s*=\s*150", "compute height 150")

    # Grid line colour (comment says blue; value is grey translucent).
    require(
        grid_frag,
        r"vec4\s*\(\s*0\.5\s*,\s*0\.5\s*,\s*0\.5\s*,\s*0\.7\s*\)",
        "grid line colour 0.5/0.5/0.5/0.7",
    )

    print("All style-contract assertions passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
