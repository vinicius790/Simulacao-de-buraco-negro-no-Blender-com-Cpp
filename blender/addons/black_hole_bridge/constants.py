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

# Schwarzschild reference radii in units of rs (schwarzschild.hpp).
# r_ph = 3 M = 1.5 rs; r_ISCO = 6 M = 3 rs; shadow edge for a distant
# observer b_c = 3*sqrt(3) M = (3*sqrt(3)/2) rs ≈ 2.598 rs.
PHOTON_SPHERE_FACTOR_RS = 1.5
ISCO_FACTOR_RS = 3.0
CRITICAL_IMPACT_FACTOR_RS = 3.0 * math.sqrt(3.0) / 2.0

# bh_render_cpu (C++ headless CPU renderer) CLI vocabulary / defaults.
RENDER_MODES = ("legacy", "relativistic", "blackbody")
RENDER_INTEGRATORS = ("rk4", "rk45")
RENDER_DEFAULT_WH = WINDOW_WH  # 800x600 — compute default on the C++ side is 200x150
RENDER_FRAME_DIGITS = 4  # stem_0000.png … stem_{N-1:04d}.png
RENDER_BINARY_NAME = "bh_render_cpu"
RENDER_BINARY_CANDIDATES = (
    "build/scientific/bh_render_cpu",
    "build/sci/bh_render_cpu",
    "build/default/bh_render_cpu",
    "build/bh_render_cpu",
)

# --- Blender geometric units ---
# For usability the Blender scene uses rs_geo = 1.0 (unit sphere = horizon).
# Physical metres map via: metres = blender_units * rs_physical_m
RS_GEO = 1.0

# Disk shading (geodesic.comp): diskColor = vec3(1.0, r, 0.2), r = |pos|/disk_r2
DISK_COLOR_R = 1.0
DISK_COLOR_B = 0.2  # G channel = r_norm ∈ [inner/outer, 1]

# Grid shading (grid.frag): vec4(0.5, 0.5, 0.5, 0.7)
GRID_COLOR = (0.5, 0.5, 0.5, 0.7)

# Guide rings reuse the grid grey at lower alpha (still the locked palette).
GUIDE_COLOR = (0.5, 0.5, 0.5, 0.5)

# Horizon / background
HORIZON_COLOR = (0.0, 0.0, 0.0, 1.0)
WORLD_BG_COLOR = (0.0, 0.0, 0.0, 1.0)

# Schema
SCHEMA_ID = "black_hole.scene_params/v1"

# Animation defaults
ANIM_FPS = 24
ANIM_DURATION_S = 8.0  # one full azimuth turntable
ANIM_MODES = ("TURNTABLE", "ELEVATION_SWEEP", "DOLLY", "SPIRAL")
ANIM_EASINGS = ("LINEAR", "SINE")
ANIM_ELEV_START_RAD = 0.35
ANIM_ELEV_END_RAD = math.pi - 0.35
ANIM_RADIUS_END_RS = 12.0

# Disk spin (UV rotation) defaults — OFF by default, style lock untouched.
DISK_SPIN_TURNS = 1.0
DISK_TURBULENCE = 0.0

# Collection / object names
COLL_ROOT = "BH_Scene"
COLL_CORE = "BH_Core"
COLL_DISK = "BH_Disk"
COLL_GRID = "BH_Grid"
COLL_CAMERA = "BH_Camera"
COLL_LIGHTS = "BH_Lights"
COLL_GUIDES = "BH_Guides"

OBJ_HORIZON = "BH_Horizon"
OBJ_DISK = "BH_AccretionDisk"
OBJ_GRID = "BH_SpacetimeGrid"
OBJ_CAM_EMPTY = "BH_OrbitPivot"
OBJ_CAMERA = "BH_OrbitCamera"
OBJ_KEY_LIGHT = "BH_FillLight"
OBJ_GUIDE_PHOTON = "BH_Guide_PhotonSphere"
OBJ_GUIDE_ISCO = "BH_Guide_ISCO"
OBJ_GUIDE_CRITICAL = "BH_Guide_CriticalImpact"
OBJ_RENDER_PLANE = "BH_RenderPlane"


# Rendered thickness of the wire overlays (grid lines, guide rings) in geo
# units. Loose mesh edges are invisible to Cycles/EEVEE, so overlays are given
# real geometry (Wireframe modifier / curve bevel) of this radius.
OVERLAY_LINE_RADIUS = 0.012


def cpp_to_blender(v):
    """Map a point/vector from the C++ world frame to Blender's world frame.

    The simulator is Y-up with the disk in the XZ plane (black_hole.cpp,
    geodesic.comp). Blender is Z-up. The mapping is the proper rotation of
    +90° about X:  (x, y, z)_cpp  →  (x, −z, y)_blender.
    det = +1, so cross products (camera right = forward × up) are preserved and
    a Blender camera tracking the origin with up = +Z frames the scene exactly
    like the C++ camera with up = +Y. Pure helpers in this package keep the C++
    frame (parity tests); conversion happens only when Blender data is written.
    """
    x, y, z = v
    return (x, -z, y)


def blender_to_cpp(v):
    """Inverse of :func:`cpp_to_blender`: (x, y, z)_blender → (x, z, −y)_cpp."""
    x, y, z = v
    return (x, z, -y)


def camera_position_blender(radius: float, azimuth: float, elevation: float):
    """Camera::position() expressed in Blender's Z-up world frame."""
    return cpp_to_blender(camera_position(radius, azimuth, elevation))


def camera_inside_disk(
    radius: float,
    elevation: float,
    inner_factor: float = DISK_INNER_FACTOR,
    outer_factor: float = DISK_OUTER_FACTOR,
    thickness_geo: float = DISK_THICKNESS_M / SAG_A_RS_M,
) -> bool:
    """True if the orbit camera (geo units, rs = 1) sits inside the disk slab.

    The locked C++ default (radius ≈ 4.997 rs, elevation π/2) does: the camera
    is within the 2.2–5.2 rs annulus and on the disk plane. In OpenGL that makes
    the top half of the first frame solid yellow (float cos(π/2) ≠ 0); in
    Blender the camera sees the inside of the slab. Raise the elevation (e.g.
    1.25 rad) or the radius (> outer factor) for a clean shot.
    """
    el = max(0.01, min(math.pi - 0.01, elevation))
    rho = radius * math.sin(el)
    height = radius * math.cos(el)
    return inner_factor <= rho <= outer_factor and abs(height) <= 0.5 * thickness_geo


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
