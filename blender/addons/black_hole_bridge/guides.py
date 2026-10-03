"""Schwarzschild guide rings (viewport overlays, same grey as the grid).

  BH_Guide_PhotonSphere   r = 1.5 rs  (3 M)      — unstable photon orbit
  BH_Guide_ISCO           r = 3.0 rs  (6 M)      — innermost stable circular orbit
  BH_Guide_CriticalImpact b_c = (3√3/2) rs ≈ 2.598 rs — shadow edge seen from far

Rings are generated in the C++ frame (XZ plane, Y = 0 = disk plane), rotated
into Blender's Z-up frame (disk plane = Blender XY), and built as closed curves
with a small bevel so they show up in Cycles/EEVEE renders, not only in the
viewport. Constants mirror include/black_hole/schwarzschild.hpp.
"""

import math
from typing import List, Tuple

from . import constants as C
from . import materials

try:
    import bpy
    from bpy.types import Operator
except ImportError:
    bpy = None  # type: ignore

    class Operator:  # type: ignore[no-redef]
        """Headless placeholder so the module imports without bpy."""


# (object name, radius factor in rs, meaning)
GUIDE_SPECS: Tuple[Tuple[str, float, str], ...] = (
    (
        C.OBJ_GUIDE_PHOTON,
        C.PHOTON_SPHERE_FACTOR_RS,
        "photon sphere r_ph = 3 M = 1.5 rs (schwarzschild.hpp photon_sphere_radius)",
    ),
    (
        C.OBJ_GUIDE_CRITICAL,
        C.CRITICAL_IMPACT_FACTOR_RS,
        "critical impact parameter b_c = 3*sqrt(3) M = (3*sqrt(3)/2) rs — shadow edge",
    ),
    (
        C.OBJ_GUIDE_ISCO,
        C.ISCO_FACTOR_RS,
        "ISCO r = 6 M = 3 rs (schwarzschild.hpp isco_radius)",
    ),
)


def build_circle_edges(
    radius: float, segments: int = 128
) -> Tuple[List[Tuple[float, float, float]], List[Tuple[int, int]]]:
    """Pure helper: verts + closed edge loop of a circle in the XZ plane (Y=0)."""
    segments = max(3, int(segments))
    verts = []
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        verts.append((radius * math.cos(a), 0.0, radius * math.sin(a)))
    edges = [(i, (i + 1) % segments) for i in range(segments)]
    return verts, edges


def guide_names() -> Tuple[str, ...]:
    return tuple(spec[0] for spec in GUIDE_SPECS)


def clear_guides() -> int:
    """Remove every guide object (by name or bh_role) and its orphan mesh."""
    names = set(guide_names())
    removed = 0
    for obj in list(bpy.data.objects):
        if obj.name in names or obj.get("bh_role") == "guide":
            data_old = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
            if data_old is not None and data_old.users == 0:
                if isinstance(data_old, bpy.types.Curve):
                    bpy.data.curves.remove(data_old)
                elif isinstance(data_old, bpy.types.Mesh):
                    bpy.data.meshes.remove(data_old)
            removed += 1
    return removed


def build_guides(collection, rs: float = C.RS_GEO, segments: int = 128):
    """Create / replace the three guide rings inside ``collection``."""
    clear_guides()
    mat = materials.ensure_guide_material()
    objs = []
    for name, factor, meaning in GUIDE_SPECS:
        verts, _edges = build_circle_edges(rs * factor, segments)
        curve = bpy.data.curves.new(name + "_Curve", type="CURVE")
        curve.dimensions = "3D"
        curve.bevel_depth = C.OVERLAY_LINE_RADIUS * rs
        curve.bevel_resolution = 2
        spline = curve.splines.new("POLY")
        spline.points.add(len(verts) - 1)
        for point, v in zip(spline.points, verts):
            x, y, z = C.cpp_to_blender(v)
            point.co = (x, y, z, 1.0)
        spline.use_cyclic_u = True
        curve.materials.append(mat)

        obj = bpy.data.objects.new(name, curve)
        collection.objects.link(obj)
        obj.location = (0.0, 0.0, 0.0)
        obj.show_in_front = True
        obj.color = C.GUIDE_COLOR
        obj["bh_role"] = "guide"
        obj["bh_radius_rs"] = float(factor)
        obj["bh_meaning"] = meaning
        objs.append(obj)
    return objs


class BH_OT_build_guides(Operator):
    bl_idname = "bh.build_guides"
    bl_label = "Build Guide Rings"
    bl_description = (
        "Grey wire rings: photon sphere 1.5 rs, critical impact b_c ≈ 2.6 rs, "
        "ISCO 3 rs (Schwarzschild; constants from schwarzschild.hpp)"
    )
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        from . import scene_builder

        coll = scene_builder.ensure_collections()[C.COLL_GUIDES]
        objs = build_guides(coll, rs=C.RS_GEO)
        self.report(
            {"INFO"},
            f"{len(objs)} guide rings (1.5 / {C.CRITICAL_IMPACT_FACTOR_RS:.3f} / 3.0 rs)",
        )
        return {"FINISHED"}


class BH_OT_clear_guides(Operator):
    bl_idname = "bh.clear_guides"
    bl_label = "Clear Guide Rings"
    bl_description = "Remove BH_Guide_* objects"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        n = clear_guides()
        self.report({"INFO"}, f"Removed {n} guide object(s)")
        return {"FINISHED"}


ALL_OPERATORS = (BH_OT_build_guides, BH_OT_clear_guides)
