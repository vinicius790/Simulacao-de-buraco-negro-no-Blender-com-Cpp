"""JSON import/export matching blender/examples/scene_params_example.json
and C++ DiskUBO / Camera field names where possible.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any

from . import constants as C
from . import relativistic


def build_params_dict(
    mass_kg: float = C.SAG_A_MASS_KG,
    disk_inner_factor: float = C.DISK_INNER_FACTOR,
    disk_outer_factor: float = C.DISK_OUTER_FACTOR,
    disk_num: float = C.DISK_NUM,
    thickness_m: float = C.DISK_THICKNESS_M,
    camera_radius_m: float = C.CAMERA_RADIUS_M,
    azimuth_rad: float = C.CAMERA_AZIMUTH_RAD,
    elevation_rad: float = C.CAMERA_ELEVATION_RAD,
    fov_y_deg: float = C.CAMERA_FOV_Y_DEG,
    grid_size: int = C.GRID_SIZE,
    grid_spacing_m: float = C.GRID_SPACING_M,
    anim_fps: int = C.ANIM_FPS,
    anim_duration_s: float = C.ANIM_DURATION_S,
    use_geo_units: bool = True,
    impact_factor_rs: float = C.DEFAULT_IMPACT_FACTOR,
    observer_radius_rs: float = C.DEFAULT_OBSERVER_RADIUS_FACTOR,
) -> dict[str, Any]:
    rs = C.rs_from_mass(mass_kg)
    rel_props = relativistic.compute_relativistic_properties(
        mass_kg=mass_kg,
        impact_factor_rs=impact_factor_rs,
        radius_factor_rs=observer_radius_rs,
    )
    return {
        "schema": C.SCHEMA_ID,
        "notes": (
            "Matches DiskUBO + Camera fields used by black_hole.cpp. "
            "Blender meshes use geometric units (rs_geo=1); "
            "metres = blender_units * r_s_m. "
            "Disk color in Blender approximates geodesic.comp "
            "vec3(1.0,r,0.2) — artistic approximation, physics in C++."
        ),
        "units": {
            "blender_geo_rs": C.RS_GEO,
            "physical_rs_m": rs,
            "mapping": "blender_1_unit = r_s_m metres",
            "use_geo_units_default": use_geo_units,
        },
        "black_hole": {
            "name": "Sagittarius A*",
            "mass_kg": float(mass_kg),
            "r_s_m": float(rs),
            "position_m": [0.0, 0.0, 0.0],
            "horizon_color": list(C.HORIZON_COLOR),
        },
        "disk": {
            "inner_radius_m": float(rs * disk_inner_factor),
            "outer_radius_m": float(rs * disk_outer_factor),
            "inner_factor_rs": float(disk_inner_factor),
            "outer_factor_rs": float(disk_outer_factor),
            "thickness_m": float(thickness_m),
            "disk_num": float(disk_num),
            "plane": "XZ",
            "up_axis": "Y",
            "color_model": "geodesic.comp vec3(1.0, r=length/disk_r2, 0.2)",
            "color_inner_rgb": list(C.disk_color_rgb(disk_inner_factor / disk_outer_factor)),
            "color_outer_rgb": list(C.disk_color_rgb(1.0)),
        },
        "grid": {
            "grid_size": int(grid_size),
            "spacing_m": float(grid_spacing_m),
            "warp_offset_m": float(C.GRID_WARP_OFFSET_M),
            "warp_formula": (
                "y = 2*sqrt(rs*(dist-rs)) - offset if dist>rs else 2*rs - offset"
            ),
            "color_rgba": list(C.GRID_COLOR),
            "draw": "GL_LINES / Blender WIRE",
        },
        "camera": {
            "radius_m": float(camera_radius_m),
            "radius_rs": float(camera_radius_m / rs) if rs else None,
            "azimuth_rad": float(azimuth_rad),
            "elevation_rad": float(elevation_rad),
            "fov_y_deg": float(fov_y_deg),
            "target_m": [0.0, 0.0, 0.0],
            "position_formula": (
                "pos = (R sin(el) cos(az), R cos(el), R sin(el) sin(az))"
            ),
        },
        "animation": {
            "fps": int(anim_fps),
            "duration_s": float(anim_duration_s),
            "mode": "turntable_azimuth",
            "azimuth_start_rad": 0.0,
            "azimuth_end_rad": 2.0 * math.pi,
        },
        "render_baseline": {
            "window": list(C.WINDOW_WH),
            "compute": list(C.COMPUTE_WH),
            "integrator": "legacyEulerStep",
            "scientific_default": False,
            "world_background": list(C.WORLD_BG_COLOR),
        },
        "relativistic": {
            "backend": rel_props.get("backend", "python (pure reference)"),
            "schwarzschild_radius_m": float(rel_props["schwarzschild_radius_m"]),
            "photon_sphere_radius_m": float(rel_props["photon_sphere_m"]),
            "impact_parameter_m": float(rel_props["impact_parameter_m"]),
            "impact_factor_rs": float(rel_props["impact_factor_rs"]),
            "deflection_angle_rad": float(rel_props["deflection_angle_rad"]),
            "deflection_angle_deg": float(rel_props["deflection_angle_deg"]),
            "observer_radius_m": float(rel_props["observer_radius_m"]),
            "observer_radius_rs": float(rel_props["observer_radius_rs"]),
            "time_dilation_factor": float(rel_props["time_dilation_factor"]),
            "formulas": {
                "schwarzschild_radius": "r_s = 2 * G * M / c^2",
                "light_deflection": "alpha = 4 * G * M / (c^2 * b) = 2 * r_s / b",
                "time_dilation": "gamma = sqrt(1 - r_s / r)",
            },
        },
        "style_lock": "docs/ESTILO_VISUAL_SIMULACAO.md",
    }


def params_from_settings(s) -> dict[str, Any]:
    return build_params_dict(
        mass_kg=s.mass_kg,
        disk_inner_factor=s.disk_inner_factor,
        disk_outer_factor=s.disk_outer_factor,
        disk_num=s.disk_num,
        thickness_m=s.disk_thickness_m,
        camera_radius_m=(
            s.camera_radius_rs * C.rs_from_mass(s.mass_kg)
            if s.use_geo_units
            else s.camera_radius_m
        ),
        azimuth_rad=s.camera_azimuth,
        elevation_rad=s.camera_elevation,
        fov_y_deg=s.camera_fov_y_deg,
        grid_size=int(s.grid_resolution),
        anim_fps=int(s.anim_fps),
        anim_duration_s=s.anim_duration_s,
        use_geo_units=s.use_geo_units,
        impact_factor_rs=getattr(s, "impact_factor_rs", C.DEFAULT_IMPACT_FACTOR),
        observer_radius_rs=getattr(s, "observer_radius_rs", C.DEFAULT_OBSERVER_RADIUS_FACTOR),
    )


def apply_params_to_settings(s, data: dict[str, Any]) -> None:
    bh = data.get("black_hole", {})
    disk = data.get("disk", {})
    cam = data.get("camera", {})
    grid = data.get("grid", {})
    anim = data.get("animation", {})
    rel = data.get("relativistic", {})

    if "mass_kg" in bh:
        s.mass_kg = float(bh["mass_kg"])
    if "inner_factor_rs" in disk:
        s.disk_inner_factor = float(disk["inner_factor_rs"])
    if "outer_factor_rs" in disk:
        s.disk_outer_factor = float(disk["outer_factor_rs"])
    if "disk_num" in disk:
        s.disk_num = float(disk["disk_num"])
    if "thickness_m" in disk:
        s.disk_thickness_m = float(disk["thickness_m"])
    if "radius_m" in cam:
        s.camera_radius_m = float(cam["radius_m"])
        rs = C.rs_from_mass(s.mass_kg)
        if rs > 0:
            s.camera_radius_rs = float(cam.get("radius_rs", cam["radius_m"] / rs))
    if "impact_factor_rs" in rel and hasattr(s, "impact_factor_rs"):
        s.impact_factor_rs = float(rel["impact_factor_rs"])
    if "observer_radius_rs" in rel and hasattr(s, "observer_radius_rs"):
        s.observer_radius_rs = float(rel["observer_radius_rs"])
    if "azimuth_rad" in cam:
        s.camera_azimuth = float(cam["azimuth_rad"])
    if "elevation_rad" in cam:
        s.camera_elevation = float(cam["elevation_rad"])
    if "fov_y_deg" in cam:
        s.camera_fov_y_deg = float(cam["fov_y_deg"])
    if "grid_size" in grid:
        s.grid_resolution = int(grid["grid_size"])
    if "fps" in anim:
        s.anim_fps = int(anim["fps"])
    if "duration_s" in anim:
        s.anim_duration_s = float(anim["duration_s"])


def write_json(path: str | Path, data: dict[str, Any]) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return path


def read_json(path: str | Path) -> dict[str, Any]:
    path = Path(path)
    return json.loads(path.read_text(encoding="utf-8"))
