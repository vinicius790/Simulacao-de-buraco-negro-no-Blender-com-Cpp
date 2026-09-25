"""Camera orbit matching Camera::position() in black_hole.cpp.

  x = radius * sin(elevation) * cos(azimuth)
  y = radius * cos(elevation)
  z = radius * sin(elevation) * sin(azimuth)
  target = (0,0,0) always — no pan.
"""

from __future__ import annotations

import math

from . import constants as C

try:
    import bpy
    from mathutils import Vector, Euler
except ImportError:
    bpy = None  # type: ignore
    Vector = None  # type: ignore
    Euler = None  # type: ignore


def geo_radius_from_settings(s) -> float:
    """Camera radius in Blender geo units (multiples of rs)."""
    if s.use_geo_units:
        return float(s.camera_radius_rs)
    rs = C.rs_from_mass(s.mass_kg)
    return float(s.camera_radius_m) / rs if rs > 0 else C.CAMERA_RADIUS_M / C.SAG_A_RS_M


def place_camera(obj, radius: float, azimuth: float, elevation: float):
    """Set camera world location from spherical orbit params; aim at origin."""
    pos = C.camera_position(radius, azimuth, elevation)
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
    cam_data.lens_unit = "FOV"
    cam_data.angle = math.radians(C.CAMERA_FOV_Y_DEG)

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
        cam.data.angle = math.radians(fov_y_deg)
    cam["bh_azimuth"] = azimuth
    cam["bh_elevation"] = elevation
    cam["bh_radius"] = radius


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
    """
    if scene is None:
        scene = bpy.context.scene
    scene.render.fps = int(fps)
    n_frames = max(2, int(round(duration_s * fps)))
    scene.frame_start = 1
    scene.frame_end = n_frames

    # Clear existing location fcurves for this object
    if cam.animation_data and cam.animation_data.action:
        action = cam.animation_data.action
        for fc in list(action.fcurves):
            if fc.data_path == "location":
                action.fcurves.remove(fc)

    cam.animation_data_clear()

    for frame in range(1, n_frames + 1):
        t = (frame - 1) / (n_frames - 1)
        az = azimuth_start + t * (azimuth_end - azimuth_start)
        place_camera(cam, radius, az, elevation)
        cam.keyframe_insert(data_path="location", frame=frame)
        cam["bh_azimuth"] = az

    # Linear interpolation
    if cam.animation_data and cam.animation_data.action:
        for fc in cam.animation_data.action.fcurves:
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"

    scene.frame_set(1)
    return n_frames
