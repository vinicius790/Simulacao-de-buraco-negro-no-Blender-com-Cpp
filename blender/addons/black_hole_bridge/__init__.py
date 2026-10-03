bl_info = {
    "name": "Black Hole Bridge",
    "author": "black_hole package",
    "version": (0, 8, 0),
    "blender": (4, 0, 0),
    "location": "View3D > Sidebar > Black Hole",
    "description": (
        "Cena + animação no estilo do simulador C++/OpenGL "
        "(disco âmbar, grade cinza, órbita), guias de Schwarzschild e ponte "
        "para o renderer C++ bh_render_cpu. Física/lensing permanece no C++."
    ),
    "category": "Physics",
    "doc_url": "",
    "tracker_url": "",
}

import json
import math

from . import constants as C
from . import export_ops
from . import grid_mesh
from . import guides
from . import render_bridge

try:
    import bpy
    from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
    )
    from bpy.types import Panel, PropertyGroup
except ImportError:  # headless python3: pure helpers stay importable
    bpy = None  # type: ignore

    def _no_prop(**_kw):
        return None

    BoolProperty = EnumProperty = FloatProperty = IntProperty = StringProperty = _no_prop  # type: ignore

    class Panel:  # type: ignore[no-redef]
        """Headless placeholder."""

    class PropertyGroup:  # type: ignore[no-redef]
        """Headless placeholder."""


ANIM_MODE_ITEMS = (
    ("TURNTABLE", "Turntable (azimute +2π)", "Comportamento legado: elevação fixa, azimute varre 2π"),
    ("ELEVATION_SWEEP", "Varredura de elevação", "Azimute fixo; elevação vai de início → fim"),
    ("DOLLY", "Dolly (raio)", "Azimute/elevação fixos; raio vai de 'Raio câmara' → 'Raio final'"),
    ("SPIRAL", "Espiral", "Azimute varre 2π enquanto a elevação vai de início → fim"),
)
ANIM_EASING_ITEMS = (
    ("LINEAR", "Linear", "t"),
    ("SINE", "Seno (ease in-out)", "0.5 − 0.5·cos(πt)"),
)
RENDER_MODE_ITEMS = (
    ("LEGACY", "Legado (cor vec3(1,r,0.2))", "Mesmo look do geodesic.comp: disco âmbar, sem Doppler"),
    ("RELATIVISTIC", "Relativístico (Doppler + redshift)", "Paleta legada + Doppler/redshift gravitacional + beaming g⁴"),
    ("BLACKBODY", "Corpo negro (Page–Thorne)", "Temperatura do disco fino Page–Thorne, deslocada por g; sem emissão dentro da ISCO"),
)
RENDER_INTEGRATOR_ITEMS = (
    ("RK4", "RK4", "Passo fixo"),
    ("RK45", "RK45", "Passo adaptativo (Dormand–Prince)"),
)


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

    # --- Animation ---------------------------------------------------------
    anim_fps: IntProperty(name="FPS", default=C.ANIM_FPS, min=1, max=120)
    anim_duration_s: FloatProperty(
        name="Duração (s)",
        default=C.ANIM_DURATION_S,
        min=0.5,
        max=120.0,
    )
    anim_mode: EnumProperty(
        name="Preset",
        items=ANIM_MODE_ITEMS,
        default="TURNTABLE",
        description="Trajetória da câmara; TURNTABLE = comportamento 0.7.x",
    )
    anim_easing: EnumProperty(
        name="Easing",
        items=ANIM_EASING_ITEMS,
        default="LINEAR",
        description="Curva aplicada a t∈[0,1] antes de amostrar o preset",
    )
    anim_elev_start: FloatProperty(
        name="Elevação início (rad)",
        default=C.ANIM_ELEV_START_RAD,
        min=0.01,
        max=math.pi - 0.01,
    )
    anim_elev_end: FloatProperty(
        name="Elevação fim (rad)",
        default=C.ANIM_ELEV_END_RAD,
        min=0.01,
        max=math.pi - 0.01,
    )
    anim_radius_end_rs: FloatProperty(
        name="Raio final (× rs)",
        default=C.ANIM_RADIUS_END_RS,
        min=1.5,
        description="DOLLY: raio no último frame (início = Raio câmara)",
    )

    # --- Disk spin ---------------------------------------------------------
    disk_spin_turns: FloatProperty(
        name="Voltas do disco",
        default=C.DISK_SPIN_TURNS,
        description="Rotação da textura (UV.u) ao longo do frame range; 1.0 = uma volta",
    )
    disk_glow: FloatProperty(
        name="Brilho do disco",
        default=1.0,
        min=0.0,
        max=50.0,
        description=(
            "Força de emissão do disco. 1.0 = paridade exata com o framebuffer "
            "OpenGL (view transform Raw); >1 = brilho artístico opcional"
        ),
    )
    disk_turbulence: FloatProperty(
        name="Turbulência (brilho)",
        default=C.DISK_TURBULENCE,
        min=0.0,
        max=1.0,
        description="0 = desligado (estilo travado). >0 modula só o brilho, nunca a cor",
    )

    # --- C++ CPU renderer bridge -------------------------------------------
    renderer_path: StringProperty(
        name="Renderer path",
        default="",
        subtype="FILE_PATH",
        description=(
            "Caminho para bh_render_cpu (ex.: build/scientific/bh_render_cpu). "
            "Vazio = procurar build/scientific, build/sci, build/default, build e PATH"
        ),
    )
    render_width: IntProperty(name="Largura", default=C.RENDER_DEFAULT_WH[0], min=16, max=4096)
    render_height: IntProperty(name="Altura", default=C.RENDER_DEFAULT_WH[1], min=16, max=4096)
    render_mode: EnumProperty(name="Modo", items=RENDER_MODE_ITEMS, default="LEGACY")
    render_integrator: EnumProperty(
        name="Integrador", items=RENDER_INTEGRATOR_ITEMS, default="RK4"
    )
    render_frames: IntProperty(
        name="Frames",
        default=1,
        min=1,
        max=2000,
        description=">1 varre o azimute +2π (stem_0000.png …) — turntable do C++",
    )
    render_output_path: StringProperty(
        name="Saída",
        default="//bh_render/frame.png",
        subtype="FILE_PATH",
        description="Imagem (.png/.bmp); com Frames>1 vira stem_0000.png …",
    )
    render_supersample: IntProperty(name="Supersample", default=1, min=1, max=4)
    render_stars: BoolProperty(
        name="Estrelas",
        default=False,
        description="Campo estelar procedural determinístico para raios que escapam (mostra a lente)",
    )
    render_timeout_s: IntProperty(
        name="Timeout (s)", default=900, min=10, max=86400,
        description="Tempo máximo à espera do bh_render_cpu",
    )
    export_path: StringProperty(
        name="Caminho JSON",
        default="//scene_params.json",
        subtype="FILE_PATH",
    )


# --------------------------------------------------------------------------- #
# Panels (parent + collapsible sub-panels)
# --------------------------------------------------------------------------- #
class _BHPanelMixin:
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Black Hole"


class BH_PT_bridge_panel(_BHPanelMixin, Panel):
    bl_label = "Black Hole Bridge"
    bl_idname = "BH_PT_bridge_panel"

    def draw(self, context):
        layout = self.layout
        s = context.scene.bh_bridge
        rs = C.rs_from_mass(s.mass_kg)
        layout.label(text="Estilo travado: disco âmbar + grade cinza", icon="INFO")
        layout.label(text=f"rs ≈ {rs:.4e} m  |  cena geo rs=1  |  v0.8.0")


class BH_PT_scene(_BHPanelMixin, Panel):
    bl_label = "Cena"
    bl_parent_id = "BH_PT_bridge_panel"

    def draw(self, context):
        box = self.layout
        box.operator(export_ops.BH_OT_build_full_scene.bl_idname, icon="WORLD_DATA")
        row = box.row(align=True)
        row.operator(export_ops.BH_OT_rebuild_horizon.bl_idname)
        row.operator(export_ops.BH_OT_rebuild_disk.bl_idname)
        box.operator(grid_mesh.BH_OT_generate_grid_mesh.bl_idname)


class BH_PT_params(_BHPanelMixin, Panel):
    bl_label = "Parâmetros"
    bl_parent_id = "BH_PT_bridge_panel"

    def draw(self, context):
        box = self.layout
        s = context.scene.bh_bridge
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


class BH_PT_guides(_BHPanelMixin, Panel):
    bl_label = f"Guias (fóton 1.5 rs · ISCO 3 rs · b_c {C.CRITICAL_IMPACT_FACTOR_RS:.1f} rs)"
    bl_parent_id = "BH_PT_bridge_panel"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        box = self.layout
        box.label(text="Schwarzschild: r_ph = 3M, ISCO = 6M, b_c = 3√3 M", icon="MESH_CIRCLE")
        row = box.row(align=True)
        row.operator(guides.BH_OT_build_guides.bl_idname, icon="MESH_CIRCLE")
        row.operator(guides.BH_OT_clear_guides.bl_idname, icon="X", text="")


class BH_PT_camera_anim(_BHPanelMixin, Panel):
    bl_label = "Câmara / animação"
    bl_parent_id = "BH_PT_bridge_panel"

    def draw(self, context):
        box = self.layout
        s = context.scene.bh_bridge
        box.operator(export_ops.BH_OT_align_camera.bl_idname, icon="OUTLINER_OB_CAMERA")
        box.prop(s, "anim_mode")
        box.prop(s, "anim_easing")
        row = box.row(align=True)
        row.prop(s, "anim_fps")
        row.prop(s, "anim_duration_s")
        if s.anim_mode in {"ELEVATION_SWEEP", "SPIRAL"}:
            col = box.column(align=True)
            col.prop(s, "anim_elev_start")
            col.prop(s, "anim_elev_end")
        if s.anim_mode == "DOLLY":
            box.prop(s, "anim_radius_end_rs")
        box.operator(
            export_ops.BH_OT_bake_orbit_animation.bl_idname, icon="RENDER_ANIMATION"
        )


class BH_PT_disk_spin(_BHPanelMixin, Panel):
    bl_label = "Disco (spin)"
    bl_parent_id = "BH_PT_bridge_panel"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        box = self.layout
        s = context.scene.bh_bridge
        box.prop(s, "disk_spin_turns")
        box.prop(s, "disk_glow")
        box.prop(s, "disk_turbulence")
        box.operator(export_ops.BH_OT_bake_disk_spin.bl_idname, icon="FORCE_VORTEX")
        box.label(text="Cor fixa (1, r, 0.2); turbulência só altera brilho", icon="LOCKED")


class BH_PT_render_bridge(_BHPanelMixin, Panel):
    bl_label = "Render C++ → Blender"
    bl_parent_id = "BH_PT_bridge_panel"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        box = self.layout
        s = context.scene.bh_bridge
        box.prop(s, "renderer_path")
        row = box.row(align=True)
        row.prop(s, "render_width")
        row.prop(s, "render_height")
        box.prop(s, "render_mode")
        box.prop(s, "render_integrator")
        row = box.row(align=True)
        row.prop(s, "render_frames")
        row.prop(s, "render_supersample")
        box.prop(s, "render_stars")
        box.prop(s, "render_output_path")
        box.prop(s, "render_timeout_s")
        box.operator(render_bridge.BH_OT_run_cpu_render.bl_idname, icon="RENDER_STILL")
        row = box.row(align=True)
        row.operator(
            render_bridge.BH_OT_import_render_background.bl_idname,
            icon="IMAGE_BACKGROUND",
            text="Import background",
        )
        row.operator(
            render_bridge.BH_OT_import_render_as_plane.bl_idname,
            icon="MESH_PLANE",
            text="Import as plane",
        )
        raw = context.scene.get("bh_last_render_summary")
        if raw:
            try:
                summary = json.loads(raw)
            except (TypeError, ValueError):
                summary = {}
            parts = [f"frames {summary.get('frames', '?')}"]
            if "width" in summary and "height" in summary:
                parts.append(f"{summary['width']}×{summary['height']}")
            if "shadow_fraction" in summary:
                parts.append(f"sombra {float(summary['shadow_fraction']):.3f}")
            if "disk_fraction" in summary:
                parts.append(f"disco {float(summary['disk_fraction']):.3f}")
            if "mode" in summary:
                parts.append(str(summary["mode"]))
            box.label(text="Último render: " + " · ".join(parts), icon="CHECKMARK")


class BH_PT_json(_BHPanelMixin, Panel):
    bl_label = "JSON ↔ C++"
    bl_parent_id = "BH_PT_bridge_panel"

    def draw(self, context):
        box = self.layout
        s = context.scene.bh_bridge
        box.prop(s, "export_path")
        box.operator(export_ops.BH_OT_export_scene_params_quick.bl_idname)
        box.operator(export_ops.BH_OT_export_scene_params.bl_idname)
        box.operator(export_ops.BH_OT_import_scene_params.bl_idname)
        box.label(text="BlackHole3D --scene / bh_render_cpu --scene", icon="FILE_SCRIPT")


PANELS = (
    BH_PT_bridge_panel,
    BH_PT_scene,
    BH_PT_params,
    BH_PT_guides,
    BH_PT_camera_anim,
    BH_PT_disk_spin,
    BH_PT_render_bridge,
    BH_PT_json,
)

classes = (
    (BHBridgeSettings,)
    + PANELS
    + export_ops.ALL_OPERATORS
    + (grid_mesh.BH_OT_generate_grid_mesh,)
    + guides.ALL_OPERATORS
    + render_bridge.ALL_OPERATORS
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
