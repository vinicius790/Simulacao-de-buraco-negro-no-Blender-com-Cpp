"""Black horizon sphere at r = rs (geo units: rs_geo = 1.0)."""

from __future__ import annotations

from . import constants as C
from . import materials

try:
    import bpy
    import bmesh
except ImportError:
    bpy = None  # type: ignore
    bmesh = None  # type: ignore


def build_horizon(
    collection,
    rs: float = C.RS_GEO,
    name: str = C.OBJ_HORIZON,
    segments: int = 64,
    rings: int = 32,
):
    """Create / replace a UV sphere of radius ``rs`` with black material."""
    # Remove existing
    existing = bpy.data.objects.get(name)
    if existing is not None:
        mesh_old = existing.data
        bpy.data.objects.remove(existing, do_unlink=True)
        if mesh_old and mesh_old.users == 0:
            bpy.data.meshes.remove(mesh_old)

    mesh = bpy.data.meshes.new(name + "_Mesh")
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)

    bm = bmesh.new()
    bmesh.ops.create_uvsphere(
        bm,
        u_segments=segments,
        v_segments=rings,
        radius=rs,
    )
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()

    mat = materials.ensure_horizon_material()
    if mesh.materials:
        mesh.materials[0] = mat
    else:
        mesh.materials.append(mat)

    obj.location = (0.0, 0.0, 0.0)
    obj["bh_rs"] = rs
    obj["bh_role"] = "horizon"
    return obj
