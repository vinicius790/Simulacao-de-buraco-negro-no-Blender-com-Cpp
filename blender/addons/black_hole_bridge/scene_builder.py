"""Build full BH scene collection hierarchy and objects.

Collections: BH_Core, BH_Disk, BH_Grid, BH_Camera, BH_Lights
"""

from __future__ import annotations

from . import constants as C
from . import camera_orbit
from . import disk_mesh
from . import grid_mesh
from . import horizon
from . import lensing
from . import materials
from . import relativistic

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
    obj.location = (8.0, 6.0, 8.0)
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

    # Calculate relativistic properties from mass and observer/impact parameters
    impact_factor = getattr(s, "impact_factor_rs", C.DEFAULT_IMPACT_FACTOR)
    observer_factor = getattr(s, "observer_radius_rs", C.DEFAULT_OBSERVER_RADIUS_FACTOR)
    rel_props = relativistic.compute_relativistic_properties(
        mass_kg=s.mass_kg,
        impact_factor_rs=impact_factor,
        radius_factor_rs=observer_factor,
    )
    if hasattr(s, "computed_rs_m"):
        s.computed_rs_m = rel_props["schwarzschild_radius_m"]
    if hasattr(s, "computed_deflection_rad"):
        s.computed_deflection_rad = rel_props["deflection_angle_rad"]
    if hasattr(s, "computed_time_dilation"):
        s.computed_time_dilation = rel_props["time_dilation_factor"]

    horizon.build_horizon(
        colls[C.COLL_CORE],
        rs=rs,
        time_dilation=rel_props["time_dilation_factor"],
        deflection_angle=rel_props["deflection_angle_rad"],
    )

    if getattr(s, "enable_lensing_sphere", True):
        lensing.build_lensing_sphere(
            colls[C.COLL_CORE],
            rs=rs,
            radius_factor=C.PHOTON_SPHERE_FACTOR,
            deflection_angle=rel_props["deflection_angle_rad"],
            time_dilation=rel_props["time_dilation_factor"],
        )
    else:
        lensing.remove_lensing_sphere()

    disk_mesh.build_disk(
        colls[C.COLL_DISK],
        rs=rs,
        inner_factor=s.disk_inner_factor,
        outer_factor=s.disk_outer_factor,
        time_dilation=rel_props["time_dilation_factor"],
        deflection_angle=rel_props["deflection_angle_rad"],
    )
    materials.update_material_relativistic_params(
        rs=rs,
        deflection_angle=rel_props["deflection_angle_rad"],
        time_dilation=rel_props["time_dilation_factor"],
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

    # Viewport shading hint via scene eevee (if present)
    scene = context.scene
    if hasattr(scene, "eevee"):
        pass
    scene.render.engine = "CYCLES" if "CYCLES" in dir(bpy.types) else scene.render.engine
    # Prefer dark film
    if hasattr(scene.render, "film_transparent"):
        scene.render.film_transparent = False

    return {
        "collections": colls,
        "camera": cam,
        "radius_geo": radius,
        "rs_geo": rs,
        "relativistic": rel_props,
        "note": (
            "Meshes in geometric units (rs=1). "
            f"Physical rs ≈ {rel_props['schwarzschild_radius_m']:.4e} m. "
            f"Relativistic: deflection={rel_props['deflection_angle_rad']:.4f} rad, "
            f"dilation={rel_props['time_dilation_factor']:.4f}. "
            "Disk shading = artistic approx of geodesic.comp; physics in C++."
        ),
    }
