"""Camera orbit matching Camera::position() in black_hole.cpp.

  C++ (Y-up):  x = r sin(el) cos(az),  y = r cos(el),  z = r sin(el) sin(az)
  Blender:     the same point mapped by constants.cpp_to_blender → (x, −z, y)
  target = (0,0,0) always — no pan; camera up = world +Z (= C++ +Y).

Animation presets (0.8.0) are pure functions of t ∈ [0, 1] so they can be
unit-tested without bpy and mirrored by the C++ ``bh_render_cpu --frames``
azimuth sweep (TURNTABLE, frame 0 = given azimuth).
"""

from __future__ import annotations

import math
from typing import Callable, Optional, Tuple

from . import constants as C

try:
    import bpy
    from mathutils import Vector, Euler
except ImportError:
    bpy = None  # type: ignore
    Vector = None  # type: ignore
    Euler = None  # type: ignore

PathSample = Tuple[float, float, float]
# fn(t, t_azimuth) → (radius, azimuth, elevation); t_azimuth drives the periodic sweep.
PathFn = Callable[[float, Optional[float]], PathSample]


# --------------------------------------------------------------------------- #
# Pure helpers (no bpy)
# --------------------------------------------------------------------------- #
def ease_t(t: float, easing: str = "LINEAR") -> float:
    """Map normalised time t ∈ [0,1] through the chosen easing curve.

    LINEAR → t;  SINE → 0.5 − 0.5·cos(πt) (ease in-out, endpoints preserved).
    """
    t = max(0.0, min(1.0, float(t)))
    if easing == "SINE":
        return 0.5 - 0.5 * math.cos(math.pi * t)
    if easing == "LINEAR":
        return t
    raise ValueError(f"unknown easing {easing!r}; expected one of {C.ANIM_EASINGS}")


def camera_path_sample(
    mode: str,
    t: float,
    radius: float,
    azimuth: float,
    elevation: float,
    radius_end: float,
    elev_start: float,
    elev_end: float,
    t_azimuth: float | None = None,
) -> PathSample:
    """Return (radius, azimuth, elevation) at normalised time t for a preset.

    ``t_azimuth`` (defaults to ``t``) drives the PERIODIC azimuth sweep
    separately: a seamless loop of N frames samples it as k/N (frame N ≡
    frame 0, like bh_render_cpu --frames) while elevation / radius use k/(N−1)
    so they reach their end values exactly.

    TURNTABLE       azimuth sweeps +2π from ``azimuth``; radius/elevation fixed.
    ELEVATION_SWEEP elevation goes elev_start → elev_end; azimuth/radius fixed.
    DOLLY           radius goes radius → radius_end; azimuth/elevation fixed.
    SPIRAL          azimuth sweeps +2π while elevation goes elev_start → elev_end.
    Elevation is clamped to (0.01, π−0.01) like Camera::position().
    """
    t = max(0.0, min(1.0, float(t)))
    ta = t if t_azimuth is None else max(0.0, min(1.0, float(t_azimuth)))
    two_pi = 2.0 * math.pi
    if mode == "TURNTABLE":
        out = (radius, azimuth + two_pi * ta, elevation)
    elif mode == "ELEVATION_SWEEP":
        out = (radius, azimuth, elev_start + t * (elev_end - elev_start))
    elif mode == "DOLLY":
        out = (radius + t * (radius_end - radius), azimuth, elevation)
    elif mode == "SPIRAL":
        out = (radius, azimuth + two_pi * ta, elev_start + t * (elev_end - elev_start))
    else:
        raise ValueError(f"unknown anim mode {mode!r}; expected one of {C.ANIM_MODES}")
    r, az, el = out
    return (float(r), float(az), max(0.01, min(math.pi - 0.01, float(el))))


def make_path_fn(
    mode: str,
    easing: str,
    radius: float,
    azimuth: float,
    elevation: float,
    radius_end: float,
    elev_start: float,
    elev_end: float,
) -> PathFn:
    """Bind a preset + easing into ``t -> (radius, azimuth, elevation)``."""
    # Validate eagerly so bad enum values fail before any keyframe is written.
    camera_path_sample(mode, 0.0, radius, azimuth, elevation, radius_end, elev_start, elev_end)
    ease_t(0.0, easing)

    def fn(t: float, t_azimuth: float | None = None) -> PathSample:
        return camera_path_sample(
            mode, ease_t(t, easing), radius, azimuth, elevation,
            radius_end, elev_start, elev_end,
            None if t_azimuth is None else ease_t(t_azimuth, easing),
        )

    return fn


def path_fn_from_settings(s, radius: float) -> PathFn:
    """Build the path function from the BHBridgeSettings property group."""
    return make_path_fn(
        mode=s.anim_mode,
        easing=s.anim_easing,
        radius=radius,
        azimuth=float(s.camera_azimuth),
        elevation=float(s.camera_elevation),
        radius_end=float(s.anim_radius_end_rs),
        elev_start=float(s.anim_elev_start),
        elev_end=float(s.anim_elev_end),
    )


def geo_radius_from_settings(s) -> float:
    """Camera radius in Blender geo units (multiples of rs)."""
    if s.use_geo_units:
        return float(s.camera_radius_rs)
    rs = C.rs_from_mass(s.mass_kg)
    return float(s.camera_radius_m) / rs if rs > 0 else C.CAMERA_RADIUS_M / C.SAG_A_RS_M


# --------------------------------------------------------------------------- #
# bpy side
# --------------------------------------------------------------------------- #
def _set_vertical_fov(cam_data, fov_y_deg: float) -> None:
    """The C++ FOV is vertical (uploadCameraUBO tan(fov_y/2)); fit the sensor
    vertically so Blender's angle means the same thing regardless of aspect."""
    cam_data.lens_unit = "FOV"
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle_y = math.radians(fov_y_deg)


def place_camera(obj, radius: float, azimuth: float, elevation: float):
    """Set camera world location from spherical orbit params; aim at origin.

    The orbit is computed with the C++ formula (Y-up) and rotated into
    Blender's Z-up frame, so the camera's up axis (+Y local → world +Z) matches
    the C++ camera's up (+Y) and the framing is identical.
    """
    pos = C.camera_position_blender(radius, azimuth, elevation)
    obj.location = pos
    # Track-to origin via constraint preferred; also set rotation fallback
    direction = Vector((0.0, 0.0, 0.0)) - Vector(pos)
    if direction.length > 1e-12:
        quat = direction.to_track_quat("-Z", "Y")
        obj.rotation_euler = quat.to_euler()


def ensure_orbit_rig(collection, name_empty=C.OBJ_CAM_EMPTY, name_cam=C.OBJ_CAMERA):
    """Create empty pivot + camera with Track To origin."""
    # Empty (pivot at origin — azimuth encoded on camera location, not empty)
    empty = bpy.data.objects.get(name_empty)
    if empty is None:
        empty = bpy.data.objects.new(name_empty, None)
        empty.empty_display_type = "PLAIN_AXES"
        empty.empty_display_size = 0.5
        collection.objects.link(empty)
    empty.location = (0.0, 0.0, 0.0)

    cam_data = bpy.data.cameras.get(name_cam + "_Data")
    if cam_data is None:
        cam_data = bpy.data.cameras.new(name_cam + "_Data")
    _set_vertical_fov(cam_data, C.CAMERA_FOV_Y_DEG)

    cam = bpy.data.objects.get(name_cam)
    if cam is None:
        cam = bpy.data.objects.new(name_cam, cam_data)
        collection.objects.link(cam)
    else:
        cam.data = cam_data

    # Parent camera to empty (optional — we keyframe camera location directly)
    cam.parent = empty

    # Track To constraint aiming at empty (origin)
    track = None
    for c in cam.constraints:
        if c.type == "TRACK_TO" and c.name == "BH_TrackOrigin":
            track = c
            break
    if track is None:
        track = cam.constraints.new(type="TRACK_TO")
        track.name = "BH_TrackOrigin"
    track.target = empty
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"

    cam["bh_role"] = "camera"
    empty["bh_role"] = "orbit_pivot"
    return empty, cam


def align_camera_from_params(
    cam,
    radius: float,
    azimuth: float,
    elevation: float,
    fov_y_deg: float = C.CAMERA_FOV_Y_DEG,
):
    place_camera(cam, radius, azimuth, elevation)
    if cam.data:
        _set_vertical_fov(cam.data, fov_y_deg)
    cam["bh_azimuth"] = azimuth
    cam["bh_elevation"] = elevation
    cam["bh_radius"] = radius


def _clear_location_fcurves(cam) -> None:
    if cam.animation_data and cam.animation_data.action:
        action = cam.animation_data.action
        for fc in list(action.fcurves):
            if fc.data_path == "location":
                action.fcurves.remove(fc)
    cam.animation_data_clear()


def bake_camera_path(
    cam,
    frames_params_fn: PathFn,
    fps: int = C.ANIM_FPS,
    duration_s: float = C.ANIM_DURATION_S,
    scene=None,
) -> int:
    """Keyframe ``cam.location`` along ``frames_params_fn(t, t_azimuth)``:
    t = k/(N−1) ∈ [0, 1] (end values reached exactly) and t_azimuth = k/N
    (periodic azimuth: the last frame is NOT a repeat of the first, so a
    looping turntable has no hitch — same sampling as bh_render_cpu --frames).

    ``frames_params_fn`` returns (radius, azimuth, elevation) in geo units;
    the camera keeps aiming at the origin (Track To). Linear interpolation
    between frames; returns the number of frames baked.
    """
    if scene is None:
        scene = bpy.context.scene
    scene.render.fps = int(fps)
    n_frames = max(2, int(round(duration_s * fps)))
    scene.frame_start = 1
    scene.frame_end = n_frames

    _clear_location_fcurves(cam)

    for frame in range(1, n_frames + 1):
        t = (frame - 1) / (n_frames - 1)       # endpoint-inclusive (elevation, radius)
        t_az = (frame - 1) / n_frames          # periodic azimuth: seamless loop
        radius, az, el = frames_params_fn(t, t_az)
        place_camera(cam, radius, az, el)
        cam.keyframe_insert(data_path="location", frame=frame)
        cam["bh_azimuth"] = az
        cam["bh_elevation"] = el
        cam["bh_radius"] = radius

    if cam.animation_data and cam.animation_data.action:
        for fc in cam.animation_data.action.fcurves:
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"

    scene.frame_set(1)
    return n_frames


def bake_orbit_animation(
    cam,
    radius: float,
    elevation: float,
    fps: int = C.ANIM_FPS,
    duration_s: float = C.ANIM_DURATION_S,
    azimuth_start: float = 0.0,
    azimuth_end: float = 2.0 * math.pi,
    scene=None,
):
    """Keyframe camera location for a turntable (azimuth sweep), C++ orbit style.

    Elevation fixed; azimuth varies linearly — matches interactive orbit feel.
    Kept for backward compatibility (scripts/build_scene_headless.py); it is a
    thin wrapper over :func:`bake_camera_path`.
    """

    def fn(t: float, t_azimuth: float | None = None) -> PathSample:
        ta = t if t_azimuth is None else t_azimuth
        return (radius, azimuth_start + ta * (azimuth_end - azimuth_start), elevation)

    return bake_camera_path(cam, fn, fps=fps, duration_s=duration_s, scene=scene)
