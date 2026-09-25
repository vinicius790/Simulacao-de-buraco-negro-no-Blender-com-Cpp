"""Warped spacetime grid matching black_hole.cpp generateGrid aesthetic.

Wireframe (edges), grey translucent material (grid.frag), Y-dip warp:

    if dist > rs:
        y = 2*sqrt(rs*(dist - rs)) - offset
    else:
        y = 2*rs - offset

Multi-plane: primary XZ equatorial sheet (matching C++), plus optional
XY / YZ didactic sheets at lower opacity via separate objects.
"""

from __future__ import annotations

import math

from . import constants as C
from . import materials

try:
    import bpy
    from bpy.types import Operator
except ImportError:
    bpy = None  # type: ignore
    Operator = object  # type: ignore


def build_warped_grid_verts_edges(
    rs: float = C.RS_GEO,
    grid_size: int = C.GRID_SIZE,
    spacing: float | None = None,
    offset: float | None = None,
    plane: str = "XZ",
):
    """Build verts + edges for one warped Cartesian sheet.

    plane:
      "XZ" — C++ default (Y warped), world X/Z vary
      "XY" — Z warped (didactic)
      "YZ" — X warped (didactic)

    Formula documented in constants.grid_warp_y / ESTILO_VISUAL_SIMULACAO.md.
    """
    if spacing is None:
        # C++ spacing 1e10 m ≈ 0.788 rs; in geo units use ~0.8 rs
        spacing = (C.GRID_SPACING_M / C.SAG_A_RS_M) * rs
    if offset is None:
        offset = (C.GRID_WARP_OFFSET_M / C.SAG_A_RS_M) * rs

    n = grid_size + 1
    verts = []
    for j in range(n):
        for i in range(n):
            a = (i - grid_size / 2) * spacing
            b = (j - grid_size / 2) * spacing
            dist = math.hypot(a, b)
            w = C.grid_warp_y(dist, rs, offset)
            if plane == "XZ":
                # C++: (worldX, y_warp, worldZ)
                verts.append((a, w, b))
            elif plane == "XY":
                verts.append((a, b, w))
            elif plane == "YZ":
                verts.append((w, a, b))
            else:
                raise ValueError(f"unknown plane {plane}")

    edges = []
    for j in range(grid_size):
        for i in range(grid_size):
            idx = j * n + i
            edges.append((idx, idx + 1))
            edges.append((idx, idx + n))
        # last column vertical? already covered by i loop for horizontals;
        # close right edge of row
        idx = j * n + grid_size
        edges.append((idx, idx + n))
    # last row horizontals
    for i in range(grid_size):
        idx = grid_size * n + i
        edges.append((idx, idx + 1))

    return verts, edges, spacing, offset


def build_grid(
    collection,
    rs: float = C.RS_GEO,
    grid_size: int = C.GRID_SIZE,
    multi_plane: bool = True,
    name: str = C.OBJ_GRID,
):
    """Create / replace warped grid object(s) in BH_Grid collection."""
    # Clear previous grid objects with our prefix
    to_remove = [
        o
        for o in list(bpy.data.objects)
        if o.name == name or o.name.startswith(name + "_")
    ]
    for o in to_remove:
        mesh_old = o.data
        bpy.data.objects.remove(o, do_unlink=True)
        if mesh_old and mesh_old.users == 0:
            bpy.data.meshes.remove(mesh_old)

    mat = materials.ensure_grid_material()
    planes = ["XZ", "XY", "YZ"] if multi_plane else ["XZ"]
    objs = []
    for plane in planes:
        verts, edges, spacing, offset = build_warped_grid_verts_edges(
            rs=rs, grid_size=grid_size, plane=plane
        )
        suffix = "" if plane == "XZ" else f"_{plane}"
        mesh_name = name + suffix + "_Mesh"
        obj_name = name + suffix
        mesh = bpy.data.meshes.new(mesh_name)
        mesh.from_pydata(verts, edges, [])
        mesh.update()
        obj = bpy.data.objects.new(obj_name, mesh)
        collection.objects.link(obj)
        if mesh.materials:
            mesh.materials[0] = mat
        else:
            mesh.materials.append(mat)
        # Prefer wireframe display
        obj.display_type = "WIRE"
        obj.show_wire = True
        obj["bh_role"] = "grid"
        obj["bh_plane"] = plane
        obj["bh_grid_size"] = grid_size
        obj["bh_spacing"] = spacing
        obj["bh_warp_offset"] = offset
        obj["bh_warp_formula"] = (
            "y = 2*sqrt(rs*(dist-rs)) - offset  if dist>rs; "
            "else 2*rs - offset  (black_hole.cpp generateGrid)"
        )
        objs.append(obj)
    return objs


class BH_OT_generate_grid_mesh(Operator):
    bl_idname = "bh.generate_grid_mesh"
    bl_label = "Rebuild Grid"
    bl_description = (
        "Create warped spacetime grid (C++ generateGrid aesthetic, grey wire)"
    )
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        from . import scene_builder

        s = context.scene.bh_bridge
        coll = scene_builder.ensure_collections()[C.COLL_GRID]
        rs = C.RS_GEO if s.use_geo_units else C.rs_from_mass(s.mass_kg)
        # When not geo units, still scale grid into manageable Blender units
        if not s.use_geo_units:
            # Build in geo then note mapping; prefer geo for viewport
            rs = C.RS_GEO
        objs = build_grid(
            coll,
            rs=rs,
            grid_size=int(s.grid_resolution),
            multi_plane=s.grid_multi_plane,
        )
        self.report(
            {"INFO"},
            f"Grid rebuilt ({len(objs)} plane(s), size={s.grid_resolution}, rs_geo={rs})",
        )
        return {"FINISHED"}
