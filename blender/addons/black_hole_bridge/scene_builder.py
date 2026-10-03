"""Build full BH scene collection hierarchy and objects.

Collections: BH_Core, BH_Disk, BH_Grid, BH_Camera, BH_Lights, BH_Guides
"""

from __future__ import annotations

from . import constants as C
from . import camera_orbit
from . import disk_mesh
from . import grid_mesh
from . import horizon
from . import materials

try:
    import bpy
except ImportError:
    bpy = None  # type: ignore


def _ensure_collection(name: str, parent=None):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        if parent is not None:
            parent.children.link(coll)
        else:
            bpy.context.scene.collection.children.link(coll)
    else:
        # Make sure linked under parent / scene
        if parent is not None:
            if coll.name not in [c.name for c in parent.children]:
                try:
                    parent.children.link(coll)
                except RuntimeError:
                    pass
        else:
            root = bpy.context.scene.collection
            if coll.name not in [c.name for c in root.children]:
                try:
                    root.children.link(coll)
                except RuntimeError:
                    pass
    return coll


def ensure_collections():
    root = _ensure_collection(C.COLL_ROOT)
    return {
        C.COLL_ROOT: root,
        C.COLL_CORE: _ensure_collection(C.COLL_CORE, root),
        C.COLL_DISK: _ensure_collection(C.COLL_DISK, root),
        C.COLL_GRID: _ensure_collection(C.COLL_GRID, root),
        C.COLL_CAMERA: _ensure_collection(C.COLL_CAMERA, root),
        C.COLL_LIGHTS: _ensure_collection(C.COLL_LIGHTS, root),
        C.COLL_GUIDES: _ensure_collection(C.COLL_GUIDES, root),
    }


def _ensure_fill_light(collection):
    """Dim fill only — disk is self-emissive; keep look dark like OpenGL."""
    name = C.OBJ_KEY_LIGHT
    obj = bpy.data.objects.get(name)
    if obj is None:
        light = bpy.data.lights.new(name + "_Data", type="AREA")
        light.energy = 0.5
        light.color = (1.0, 0.85, 0.7)
        obj = bpy.data.objects.new(name, light)
        collection.objects.link(obj)
    obj.location = C.cpp_to_blender((8.0, 6.0, 8.0))
    obj["bh_role"] = "fill_light"
    return obj


def build_full_scene(context=None):
    """Create complete scene from bh_bridge settings."""
    if context is None:
        context = bpy.context
    s = context.scene.bh_bridge
    colls = ensure_collections()

    rs = C.RS_GEO  # always build meshes in geo units for viewport usability
    materials.setup_world_background()
    view_transform = materials.setup_color_parity(context.scene)

    horizon.build_horizon(colls[C.COLL_CORE], rs=rs)
    disk_mesh.build_disk(
        colls[C.COLL_DISK],
        rs=rs,
        inner_factor=s.disk_inner_factor,
        outer_factor=s.disk_outer_factor,
        thickness_m=s.disk_thickness_m,
        glow=s.disk_glow,
    )
    grid_mesh.build_grid(
        colls[C.COLL_GRID],
        rs=rs,
        grid_size=int(s.grid_resolution),
        multi_plane=s.grid_multi_plane,
    )
    empty, cam = camera_orbit.ensure_orbit_rig(colls[C.COLL_CAMERA])
    radius = camera_orbit.geo_radius_from_settings(s)
    camera_orbit.align_camera_from_params(
        cam,
        radius=radius,
        azimuth=s.camera_azimuth,
        elevation=s.camera_elevation,
        fov_y_deg=s.camera_fov_y_deg,
    )
    context.scene.camera = cam
    _ensure_fill_light(colls[C.COLL_LIGHTS])

    # Cycles renders the emissive disk / beveled overlays identically on any
    # machine (EEVEE needs a GPU context, which headless Blender may lack).
    scene = context.scene
    # (RenderSettings.engine is a dynamic enum: probe by assignment.)
    try:
        scene.render.engine = "CYCLES"
    except TypeError:
        pass  # Cycles add-on disabled: keep the current engine
    scene.render.film_transparent = False

    inside = C.camera_inside_disk(
        radius,
        s.camera_elevation,
        s.disk_inner_factor,
        s.disk_outer_factor,
        s.disk_thickness_m / C.SAG_A_RS_M,
    )
    note = (
        "Meshes in geometric units (rs=1, Z-up; C++ Y-up mapped by cpp_to_blender). "
        f"Physical rs ≈ {C.rs_from_mass(s.mass_kg):.4e} m. "
        f"View transform: {view_transform} (OpenGL colour parity)."
    )
    if inside:
        note += (
            " WARNING: camera is inside the accretion disk (C++ default view) — "
            "raise Elevation (e.g. 1.25) for a clean render."
        )
    return {
        "collections": colls,
        "camera": cam,
        "radius_geo": radius,
        "rs_geo": rs,
        "camera_inside_disk": inside,
        "note": note,
    }
