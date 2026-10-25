#!/usr/bin/env python3
"""Standalone scene-parameter exporter (no Blender required)."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

G = 6.67430e-11
C = 299792458.0
DEFAULT_MASS = 8.54e36


def rs_from_mass(mass_kg: float) -> float:
    return 2.0 * G * mass_kg / (C * C)


def build_params(
    mass_kg: float,
    disk_inner_factor: float,
    disk_outer_factor: float,
    camera_radius: float,
    fov_y_deg: float,
) -> dict:
    rs = rs_from_mass(mass_kg)
    return {
        "schema": "black_hole.scene_params/v1",
        "black_hole": {
            "name": "Sagittarius A*",
            "mass_kg": mass_kg,
            "r_s_m": rs,
            "position_m": [0.0, 0.0, 0.0],
        },
        "disk": {
            "inner_radius_m": rs * disk_inner_factor,
            "outer_radius_m": rs * disk_outer_factor,
            "inner_factor_rs": disk_inner_factor,
            "outer_factor_rs": disk_outer_factor,
            "thickness_m": 1.0e9,
            "disk_num": 1.0,
        },
        "camera": {
            "radius_m": camera_radius,
            "azimuth_rad": 0.0,
            "elevation_rad": math.pi / 2.0,
            "fov_y_deg": fov_y_deg,
            "target_m": [0.0, 0.0, 0.0],
        },
        "render_baseline": {
            "window": [800, 600],
            "compute": [200, 150],
            "integrator": "legacyEulerStep",
            "scientific_default": False,
        },
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--mass", type=float, default=DEFAULT_MASS)
    p.add_argument("--disk-inner-factor", type=float, default=2.2)
    p.add_argument("--disk-outer-factor", type=float, default=5.2)
    p.add_argument("--camera-radius", type=float, default=6.34194e10)
    p.add_argument("--fov-y-deg", type=float, default=60.0)
    args = p.parse_args()

    data = build_params(
        args.mass,
        args.disk_inner_factor,
        args.disk_outer_factor,
        args.camera_radius,
        args.fov_y_deg,
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
