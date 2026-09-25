"""Operators: Build Full Scene, rebuild parts, JSON I/O, Bake Orbit Animation."""

from __future__ import annotations

import math

from . import constants as C
from . import camera_orbit
from . import disk_mesh
from . import grid_mesh
from . import horizon
from . import json_io
from . import scene_builder

import bpy
from bpy.props import StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper, ImportHelper


class BH_OT_build_full_scene(Operator):
    bl_idname = "bh.build_full_scene"
    bl_label = "Build Full Scene"
    bl_description = (
        "Create BH_Core/Disk/Grid/Camera/Lights — amber disk vec3(1,r,0.2), "
        "grey warped grid, black horizon, orbit camera"
    )
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        info = scene_builder.build_full_scene(context)
        self.report({"INFO"}, info["note"])
        return {"FINISHED"}


class BH_OT_rebuild_disk(Operator):
    bl_idname = "bh.rebuild_disk"
    bl_label = "Rebuild Disk"
    bl_description = "Rebuild annular disk 2.2–5.2 rs with vec3(1,r,0.2) emission"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        s = context.scene.bh_bridge
        coll = scene_builder.ensure_collections()[C.COLL_DISK]
        disk_mesh.build_disk(
            coll,
            rs=C.RS_GEO,
            inner_factor=s.disk_inner_factor,
            outer_factor=s.disk_outer_factor,
        )
        self.report({"INFO"}, "Disk rebuilt (amber gradient)")
        return {"FINISHED"}


class BH_OT_rebuild_horizon(Operator):
    bl_idname = "bh.rebuild_horizon"
    bl_label = "Rebuild Horizon"
    bl_description = "Rebuild black sphere at rs_geo=1"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        coll = scene_builder.ensure_collections()[C.COLL_CORE]
        horizon.build_horizon(coll, rs=C.RS_GEO)
        self.report({"INFO"}, "Horizon rebuilt")
        return {"FINISHED"}


class BH_OT_align_camera(Operator):
    bl_idname = "bh.align_camera_from_json"
    bl_label = "Align Camera From Params"
    bl_description = "Place orbit camera from panel (C++ Camera formula)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        s = context.scene.bh_bridge
        coll = scene_builder.ensure_collections()[C.COLL_CAMERA]
        _empty, cam = camera_orbit.ensure_orbit_rig(coll)
        radius = camera_orbit.geo_radius_from_settings(s)
        camera_orbit.align_camera_from_params(
            cam,
            radius=radius,
            azimuth=s.camera_azimuth,
            elevation=s.camera_elevation,
            fov_y_deg=s.camera_fov_y_deg,
        )
        context.scene.camera = cam
        self.report(
            {"INFO"},
            f"Camera R={radius:.3f} rs az={s.camera_azimuth:.3f} el={s.camera_elevation:.3f}",
        )
        return {"FINISHED"}


class BH_OT_bake_orbit_animation(Operator):
    bl_idname = "bh.bake_orbit_animation"
    bl_label = "Bake Orbit Animation"
    bl_description = "Keyframe turntable azimuth orbit (C++ Camera style)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        s = context.scene.bh_bridge
        coll = scene_builder.ensure_collections()[C.COLL_CAMERA]
        _empty, cam = camera_orbit.ensure_orbit_rig(coll)
        radius = camera_orbit.geo_radius_from_settings(s)
        n = camera_orbit.bake_orbit_animation(
            cam,
            radius=radius,
            elevation=s.camera_elevation,
            fps=int(s.anim_fps),
            duration_s=s.anim_duration_s,
            azimuth_start=s.camera_azimuth,
            azimuth_end=s.camera_azimuth + 2.0 * math.pi,
            scene=context.scene,
        )
        context.scene.camera = cam
        self.report({"INFO"}, f"Baked {n} frames @ {int(s.anim_fps)} fps")
        return {"FINISHED"}


class BH_OT_export_scene_params(Operator, ExportHelper):
    bl_idname = "bh.export_scene_params"
    bl_label = "Export Scene Params JSON"
    bl_description = "Write DiskUBO/Camera-matching JSON"
    filename_ext = ".json"
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def invoke(self, context, event):
        s = context.scene.bh_bridge
        if s.export_path and not self.filepath:
            self.filepath = bpy.path.abspath(s.export_path)
        return super().invoke(context, event)

    def execute(self, context):
        s = context.scene.bh_bridge
        data = json_io.params_from_settings(s)
        path = self.filepath or bpy.path.abspath(s.export_path)
        json_io.write_json(path, data)
        s.export_path = path
        self.report({"INFO"}, f"Wrote {path}")
        return {"FINISHED"}


class BH_OT_import_scene_params(Operator, ImportHelper):
    bl_idname = "bh.import_scene_params"
    bl_label = "Import Scene Params JSON"
    bl_description = "Load scene_params JSON into panel"
    filename_ext = ".json"
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def execute(self, context):
        data = json_io.read_json(self.filepath)
        json_io.apply_params_to_settings(context.scene.bh_bridge, data)
        self.report({"INFO"}, f"Loaded {self.filepath}")
        return {"FINISHED"}


class BH_OT_export_scene_params_quick(Operator):
    bl_idname = "bh.export_scene_params_quick"
    bl_label = "Export Scene JSON (path)"
    bl_description = "Write JSON to the Export path field"

    def execute(self, context):
        s = context.scene.bh_bridge
        data = json_io.params_from_settings(s)
        path = bpy.path.abspath(s.export_path)
        json_io.write_json(path, data)
        self.report({"INFO"}, f"Wrote {path}")
        return {"FINISHED"}


ALL_OPERATORS = (
    BH_OT_build_full_scene,
    BH_OT_rebuild_disk,
    BH_OT_rebuild_horizon,
    BH_OT_align_camera,
    BH_OT_bake_orbit_animation,
    BH_OT_export_scene_params,
    BH_OT_import_scene_params,
    BH_OT_export_scene_params_quick,
)
