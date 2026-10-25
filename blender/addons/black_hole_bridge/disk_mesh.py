"""Annular accretion disk mesh + warm amber emission material.

Geometry matches DiskUBO: inner = 2.2 rs, outer = 5.2 rs, equatorial XZ (Y-up).
Color gradient matches geodesic.comp: vec3(1.0, r_norm, 0.2).
"""

from __future__ import annotations

import math

from . import constants as C
from . import materials

try:
    import bpy
    import bmesh
    from mathutils import Vector
except ImportError:
    bpy = None  # type: ignore
    bmesh = None  # type: ignore
    Vector = None  # type: ignore


def build_disk_mesh_data(
    rs: float = C.RS_GEO,
    inner_factor: float = C.DISK_INNER_FACTOR,
    outer_factor: float = C.DISK_OUTER_FACTOR,
    n_radial: int = 48,
    n_angular: int = 128,
):
    """Return (verts, faces, uvs) for an annular disk in the XZ plane (Y=0).

    UV: u = angle/(2π), v = (r - r_inner)/(r_outer - r_inner).
    """
    r_in = rs * inner_factor
    r_out = rs * outer_factor
    verts = []
    uvs = []
    # (n_radial+1) rings × (n_angular) verts (seam shared via wrap on faces)
    for ir in range(n_radial + 1):
        t = ir / n_radial
        r = r_in + t * (r_out - r_in)
        for ia in range(n_angular):
            a = 2.0 * math.pi * ia / n_angular
            x = r * math.cos(a)
            z = r * math.sin(a)
            y = 0.0
            verts.append((x, y, z))
            uvs.append((ia / n_angular, t))

    faces = []
    face_uvs = []
    for ir in range(n_radial):
        for ia in range(n_angular):
            ia2 = (ia + 1) % n_angular
            i0 = ir * n_angular + ia
            i1 = ir * n_angular + ia2
            i2 = (ir + 1) * n_angular + ia2
            i3 = (ir + 1) * n_angular + ia
            faces.append((i0, i1, i2, i3))
            # UV with seam unwrap: use ia/n and (ia+1)/n so last wedge goes to 1.0
            u0 = ia / n_angular
            u1 = (ia + 1) / n_angular
            v0 = ir / n_radial
            v1 = (ir + 1) / n_radial
            face_uvs.append([(u0, v0), (u1, v0), (u1, v1), (u0, v1)])

    return verts, faces, face_uvs, r_in, r_out


def build_disk(
    collection,
    rs: float = C.RS_GEO,
    inner_factor: float = C.DISK_INNER_FACTOR,
    outer_factor: float = C.DISK_OUTER_FACTOR,
    name: str = C.OBJ_DISK,
    n_radial: int = 48,
    n_angular: int = 128,
):
    """Create / replace the accretion disk object with emission material."""
    existing = bpy.data.objects.get(name)
    if existing is not None:
        mesh_old = existing.data
        bpy.data.objects.remove(existing, do_unlink=True)
        if mesh_old and mesh_old.users == 0:
            bpy.data.meshes.remove(mesh_old)

    verts, faces, face_uvs, r_in, r_out = build_disk_mesh_data(
        rs, inner_factor, outer_factor, n_radial, n_angular
    )

    mesh = bpy.data.meshes.new(name + "_Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    # UV layer
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for poly_i, poly in enumerate(mesh.polygons):
        for loop_i, loop_index in enumerate(poly.loop_indices):
            uv_layer.data[loop_index].uv = face_uvs[poly_i][loop_i]

    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)

    mat = materials.ensure_disk_material()
    if mesh.materials:
        mesh.materials[0] = mat
    else:
        mesh.materials.append(mat)

    obj.location = (0.0, 0.0, 0.0)
    obj["bh_role"] = "disk"
    obj["bh_inner_rs"] = inner_factor
    obj["bh_outer_rs"] = outer_factor
    obj["bh_r_in"] = r_in
    obj["bh_r_out"] = r_out
    obj["bh_style_note"] = (
        "artistic approximation of geodesic.comp diskColor=vec3(1,r,0.2); "
        "physics/lensing in C++"
    )
    return obj
