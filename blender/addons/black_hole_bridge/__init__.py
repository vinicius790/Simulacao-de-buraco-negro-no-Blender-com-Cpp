bl_info = {
    "name": "Black Hole Bridge",
    "author": "black_hole package",
    "version": (0, 7, 0),
    "blender": (4, 0, 0),
    "location": "View3D > Sidebar > Black Hole",
    "description": (
        "Cena + animação no estilo do simulador C++/OpenGL "
        "(disco âmbar, grade cinza, órbita). Física/lensing permanece no C++."
    ),
    "category": "Physics",
    "doc_url": "",
    "tracker_url": "",
}

import math

import bpy
from bpy.props import (
    BoolProperty,
    FloatProperty,
    IntProperty,
    StringProperty,
)
from bpy.types import Panel, PropertyGroup

from . import constants as C
from . import export_ops
from . import grid_mesh


class BHBridgeSettings(PropertyGroup):
    mass_kg: FloatProperty(
        name="M (kg)",
        default=C.SAG_A_MASS_KG,
        min=1.0,
        description="Massa do buraco negro (padrão Sag A*)",
    )
    disk_inner_factor: FloatProperty(
        name="Disco interno (× rs)",
        default=C.DISK_INNER_FACTOR,
        min=1.01,
        description="Baseline legado: 2.2 × rs",
    )
    disk_outer_factor: FloatProperty(
        name="Disco externo (× rs)",
        default=C.DISK_OUTER_FACTOR,
        min=1.5,
        description="Baseline legado: 5.2 × rs",
    )
    disk_num: FloatProperty(name="disk_num", default=C.DISK_NUM, min=0.0)
    disk_thickness_m: FloatProperty(
        name="Espessura disco (m)",
        default=C.DISK_THICKNESS_M,
        min=1.0,
    )
    use_geo_units: BoolProperty(
        name="Unidades geométricas (rs=1)",
        default=True,
        description="Cena Blender com horizonte = 1 unidade (recomendado)",
    )
    camera_radius_m: FloatProperty(
        name="Raio câmara (m)",
        default=C.CAMERA_RADIUS_M,
        min=1.0,
    )
    camera_radius_rs: FloatProperty(
        name="Raio câmara (× rs)",
        default=C.CAMERA_RADIUS_M / C.SAG_A_RS_M,
        min=1.5,
        description="Padrão ≈ 5 × rs (6.34e10 / 1.269e10)",
    )
    camera_azimuth: FloatProperty(
        name="Azimute (rad)",
        default=C.CAMERA_AZIMUTH_RAD,
    )
    camera_elevation: FloatProperty(
        name="Elevação (rad)",
        default=C.CAMERA_ELEVATION_RAD,
        min=0.01,
        max=math.pi - 0.01,
    )
    camera_fov_y_deg: FloatProperty(
        name="FOV Y (°)",
        default=C.CAMERA_FOV_Y_DEG,
        min=10.0,
        max=120.0,
    )
    grid_resolution: IntProperty(
        name="Grade (N)",
        default=C.GRID_SIZE,
        min=5,
        max=80,
        description="gridSize do generateGrid C++ (padrão 25)",
    )
    grid_multi_plane: BoolProperty(
        name="Grade multi-plano",
        default=False,
        description="Se ligado, cria XZ+XY+YZ; padrão só XZ como o C++",
    )
    anim_fps: IntProperty(name="FPS", default=C.ANIM_FPS, min=1, max=120)
    anim_duration_s: FloatProperty(
        name="Duração órbita (s)",
        default=C.ANIM_DURATION_S,
        min=0.5,
        max=120.0,
    )
    export_path: StringProperty(
        name="Caminho JSON",
        default="//scene_params.json",
        subtype="FILE_PATH",
    )


class BH_PT_bridge_panel(Panel):
    bl_label = "Black Hole Bridge"
    bl_idname = "BH_PT_bridge_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Black Hole"

    def draw(self, context):
        layout = self.layout
        s = context.scene.bh_bridge
        rs = C.rs_from_mass(s.mass_kg)

        layout.label(text="Estilo travado: disco âmbar + grade cinza", icon="INFO")
        layout.label(text=f"rs ≈ {rs:.4e} m  |  cena geo rs=1")

        box = layout.box()
        box.label(text="Cena (rodar no Blender)")
        box.operator(export_ops.BH_OT_build_full_scene.bl_idname, icon="WORLD_DATA")
        row = box.row(align=True)
        row.operator(export_ops.BH_OT_rebuild_horizon.bl_idname)
        row.operator(export_ops.BH_OT_rebuild_disk.bl_idname)
        box.operator(grid_mesh.BH_OT_generate_grid_mesh.bl_idname)

        box = layout.box()
        box.label(text="Parâmetros")
        box.prop(s, "mass_kg")
        box.prop(s, "disk_inner_factor")
        box.prop(s, "disk_outer_factor")
        box.prop(s, "use_geo_units")
        if s.use_geo_units:
            box.prop(s, "camera_radius_rs")
        else:
            box.prop(s, "camera_radius_m")
        box.prop(s, "camera_azimuth")
        box.prop(s, "camera_elevation")
        box.prop(s, "camera_fov_y_deg")
        box.prop(s, "grid_resolution")
        box.prop(s, "grid_multi_plane")

        box = layout.box()
        box.label(text="Câmara / animação")
        box.operator(export_ops.BH_OT_align_camera.bl_idname, icon="OUTLINER_OB_CAMERA")
        box.prop(s, "anim_fps")
        box.prop(s, "anim_duration_s")
        box.operator(
            export_ops.BH_OT_bake_orbit_animation.bl_idname, icon="RENDER_ANIMATION"
        )

        box = layout.box()
        box.label(text="JSON ↔ C++")
        box.prop(s, "export_path")
        box.operator(export_ops.BH_OT_export_scene_params_quick.bl_idname)
        box.operator(export_ops.BH_OT_export_scene_params.bl_idname)
        box.operator(export_ops.BH_OT_import_scene_params.bl_idname)


classes = (BHBridgeSettings, BH_PT_bridge_panel) + export_ops.ALL_OPERATORS + (
    grid_mesh.BH_OT_generate_grid_mesh,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.bh_bridge = bpy.props.PointerProperty(type=BHBridgeSettings)


def unregister():
    del bpy.types.Scene.bh_bridge
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
