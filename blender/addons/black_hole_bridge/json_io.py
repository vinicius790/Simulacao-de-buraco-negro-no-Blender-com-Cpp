"""JSON import/export matching blender/examples/scene_params_example.json
and C++ DiskUBO / Camera field names where possible.

Schema ``black_hole.scene_params/v1`` — read by ``BlackHole3D --scene`` and
``bh_render_cpu --scene`` (restricted key set; unknown keys are ignored on both
sides). 0.8.0 adds ``guides``, ``render`` and the animation preset fields.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any

from . import constants as C

# Legacy value written by 0.7.x for animation.mode.
_LEGACY_TURNTABLE_LABEL = "turntable_azimuth"


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
    anim_mode: str = "TURNTABLE",
    anim_easing: str = "LINEAR",
    anim_elev_start_rad: float = C.ANIM_ELEV_START_RAD,
    anim_elev_end_rad: float = C.ANIM_ELEV_END_RAD,
    anim_radius_end_rs: float = C.ANIM_RADIUS_END_RS,
    disk_spin_turns: float = C.DISK_SPIN_TURNS,
    disk_turbulence: float = C.DISK_TURBULENCE,
    render_width: int = C.RENDER_DEFAULT_WH[0],
    render_height: int = C.RENDER_DEFAULT_WH[1],
    render_mode: str = "LEGACY",
    render_integrator: str = "RK4",
    render_frames: int = 1,
    render_supersample: int = 1,
) -> dict[str, Any]:
    rs = C.rs_from_mass(mass_kg)
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
            "spin_turns": float(disk_spin_turns),
            "turbulence": float(disk_turbulence),
        },
        "guides": {
            "photon_sphere_rs": C.PHOTON_SPHERE_FACTOR_RS,
            "isco_rs": C.ISCO_FACTOR_RS,
            "critical_impact_rs": C.CRITICAL_IMPACT_FACTOR_RS,
            "photon_sphere_in_M": 2.0 * C.PHOTON_SPHERE_FACTOR_RS,
            "isco_in_M": 2.0 * C.ISCO_FACTOR_RS,
            "source": "include/black_hole/schwarzschild.hpp (rs = 2 M)",
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
            "mode": str(anim_mode).lower(),
            "easing": str(anim_easing).lower(),
            "azimuth_start_rad": float(azimuth_rad),
            "azimuth_end_rad": float(azimuth_rad) + 2.0 * math.pi,
            "elev_start_rad": float(anim_elev_start_rad),
            "elev_end_rad": float(anim_elev_end_rad),
            "radius_end_rs": float(anim_radius_end_rs),
        },
        "render": {
            "width": int(render_width),
            "height": int(render_height),
            "mode": str(render_mode).lower(),
            "integrator": str(render_integrator).lower(),
            "frames": int(render_frames),
            "supersample": int(render_supersample),
            "tool": C.RENDER_BINARY_NAME,
        },
        "render_baseline": {
            "window": list(C.WINDOW_WH),
            "compute": list(C.COMPUTE_WH),
            "integrator": "legacyEulerStep",
            "scientific_default": False,
            "world_background": list(C.WORLD_BG_COLOR),
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
        anim_mode=s.anim_mode,
        anim_easing=s.anim_easing,
        anim_elev_start_rad=s.anim_elev_start,
        anim_elev_end_rad=s.anim_elev_end,
        anim_radius_end_rs=s.anim_radius_end_rs,
        disk_spin_turns=s.disk_spin_turns,
        disk_turbulence=s.disk_turbulence,
        render_width=int(s.render_width),
        render_height=int(s.render_height),
        render_mode=s.render_mode,
        render_integrator=s.render_integrator,
        render_frames=int(s.render_frames),
        render_supersample=int(s.render_supersample),
    )


def _enum_or_none(value: Any, allowed: tuple[str, ...]) -> str | None:
    """Upper-case ``value`` if it names one of ``allowed`` (else None)."""
    if not isinstance(value, str):
        return None
    v = value.strip().upper()
    if v == _LEGACY_TURNTABLE_LABEL.upper():
        v = "TURNTABLE"
    return v if v in allowed else None


def apply_params_to_settings(s, data: dict[str, Any]) -> None:
    """Copy known fields of a scene_params JSON into the settings object.

    Unknown keys (e.g. C++ ``objects``/``gravity``) are ignored; missing keys
    leave the current settings untouched.
    """
    bh = data.get("black_hole", {})
    disk = data.get("disk", {})
    cam = data.get("camera", {})
    grid = data.get("grid", {})
    anim = data.get("animation", {})
    render = data.get("render", {})

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
    if "spin_turns" in disk:
        s.disk_spin_turns = float(disk["spin_turns"])
    if "turbulence" in disk:
        s.disk_turbulence = max(0.0, float(disk["turbulence"]))
    if "radius_m" in cam:
        s.camera_radius_m = float(cam["radius_m"])
        rs = C.rs_from_mass(s.mass_kg)
        if rs > 0:
            radius_rs = cam.get("radius_rs")
            s.camera_radius_rs = float(radius_rs if radius_rs is not None else cam["radius_m"] / rs)
    elif "radius_rs" in cam and cam["radius_rs"] is not None:
        s.camera_radius_rs = float(cam["radius_rs"])
        s.camera_radius_m = float(cam["radius_rs"]) * C.rs_from_mass(s.mass_kg)
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
    mode = _enum_or_none(anim.get("mode"), C.ANIM_MODES)
    if mode is not None:
        s.anim_mode = mode
    easing = _enum_or_none(anim.get("easing"), C.ANIM_EASINGS)
    if easing is not None:
        s.anim_easing = easing
    if "elev_start_rad" in anim:
        s.anim_elev_start = float(anim["elev_start_rad"])
    if "elev_end_rad" in anim:
        s.anim_elev_end = float(anim["elev_end_rad"])
    if "radius_end_rs" in anim:
        s.anim_radius_end_rs = float(anim["radius_end_rs"])
    if "width" in render:
        s.render_width = int(render["width"])
    if "height" in render:
        s.render_height = int(render["height"])
    rmode = _enum_or_none(render.get("mode"), tuple(m.upper() for m in C.RENDER_MODES))
    if rmode is not None:
        s.render_mode = rmode
    rinteg = _enum_or_none(render.get("integrator"), tuple(i.upper() for i in C.RENDER_INTEGRATORS))
    if rinteg is not None:
        s.render_integrator = rinteg
    if "frames" in render:
        s.render_frames = max(1, int(render["frames"]))
    if "supersample" in render:
        s.render_supersample = max(1, int(render["supersample"]))


def write_json(path: str | Path, data: dict[str, Any]) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return path


def read_json(path: str | Path) -> dict[str, Any]:
    path = Path(path)
    return json.loads(path.read_text(encoding="utf-8"))
