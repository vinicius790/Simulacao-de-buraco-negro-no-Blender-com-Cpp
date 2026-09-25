"""Shared constants mirroring black_hole.cpp / geodesic.comp / grid.frag.

Visual style is locked by docs/ESTILO_VISUAL_SIMULACAO.md — do not invent palettes.
"""

from __future__ import annotations

import math

# Physical constants (SI)
G = 6.67430e-11
C = 299792458.0

# Sagittarius A* baseline (black_hole.cpp / geodesic.comp)
SAG_A_MASS_KG = 8.54e36
SAG_A_RS_M = 1.269e10  # geodesic.comp SagA_rs; also ≈ 2GM/c² for mass above

# Disk (DiskUBO / uploadDiskUBO)
DISK_INNER_FACTOR = 2.2
DISK_OUTER_FACTOR = 5.2
DISK_NUM = 2.0
DISK_THICKNESS_M = 1.0e9

# Camera (Camera struct)
CAMERA_RADIUS_M = 6.34194e10
CAMERA_AZIMUTH_RAD = 0.0
CAMERA_ELEVATION_RAD = math.pi / 2.0
CAMERA_FOV_Y_DEG = 60.0
CAMERA_MIN_RADIUS_M = 1.0e10
CAMERA_MAX_RADIUS_M = 1.0e12

# Grid (generateGrid)
GRID_SIZE = 25  # → (gridSize+1)² vertices
GRID_SPACING_M = 1.0e10
GRID_WARP_OFFSET_M = 3.0e10  # subtracted in C++ y warp

# Render baseline
WINDOW_WH = (800, 600)
COMPUTE_WH = (200, 150)

# --- Blender geometric units ---
# For usability the Blender scene uses rs_geo = 1.0 (unit sphere = horizon).
# Physical metres map via: metres = blender_units * rs_physical_m
RS_GEO = 1.0

# Disk shading (geodesic.comp): diskColor = vec3(1.0, r, 0.2), r = |pos|/disk_r2
DISK_COLOR_R = 1.0
DISK_COLOR_B = 0.2  # G channel = r_norm ∈ [inner/outer, 1]

# Grid shading (grid.frag): vec4(0.5, 0.5, 0.5, 0.7)
GRID_COLOR = (0.5, 0.5, 0.5, 0.7)

# Horizon / background
HORIZON_COLOR = (0.0, 0.0, 0.0, 1.0)
WORLD_BG_COLOR = (0.0, 0.0, 0.0, 1.0)

# Schema
SCHEMA_ID = "black_hole.scene_params/v1"

# Animation defaults
ANIM_FPS = 24
ANIM_DURATION_S = 8.0  # one full azimuth turntable

# Collection / object names
COLL_ROOT = "BH_Scene"
COLL_CORE = "BH_Core"
COLL_DISK = "BH_Disk"
COLL_GRID = "BH_Grid"
COLL_CAMERA = "BH_Camera"
COLL_LIGHTS = "BH_Lights"

OBJ_HORIZON = "BH_Horizon"
OBJ_DISK = "BH_AccretionDisk"
OBJ_GRID = "BH_SpacetimeGrid"
OBJ_CAM_EMPTY = "BH_OrbitPivot"
OBJ_CAMERA = "BH_OrbitCamera"
OBJ_KEY_LIGHT = "BH_FillLight"


def rs_from_mass(mass_kg: float) -> float:
    return 2.0 * G * mass_kg / (C * C)


def camera_position(radius: float, azimuth: float, elevation: float):
    """Match Camera::position() in black_hole.cpp (Y-up, target origin)."""
    elev = max(0.01, min(math.pi - 0.01, elevation))
    x = radius * math.sin(elev) * math.cos(azimuth)
    y = radius * math.cos(elev)
    z = radius * math.sin(elev) * math.sin(azimuth)
    return (x, y, z)


def disk_color_rgb(r_norm: float):
    """geodesic.comp: vec3(1.0, r, 0.2) with r = |pos|/disk_r2 clamped to [0,1]."""
    g = max(0.0, min(1.0, float(r_norm)))
    return (DISK_COLOR_R, g, DISK_COLOR_B)


def grid_warp_y(dist: float, rs: float, offset: float | None = None) -> float:
    """Didactic Schwarzschild warp from generateGrid (black_hole.cpp).

    if dist > rs:
        y = 2*sqrt(rs*(dist - rs)) - offset
    else:
        y = 2*rs - offset   # sharp pit inside horizon (C++: 2*sqrt(rs*rs))

    In geo units use rs=1 and offset ≈ GRID_WARP_OFFSET_M / SAG_A_RS_M ≈ 2.36.
    """
    if offset is None:
        offset = GRID_WARP_OFFSET_M / SAG_A_RS_M * rs
    if dist > rs:
        return 2.0 * math.sqrt(rs * (dist - rs)) - offset
    return 2.0 * rs - offset
