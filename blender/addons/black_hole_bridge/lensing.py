"""Refractive lensing sphere at the photon sphere (r = 1.5 * rs).

Simulates the optical lensing distortion of the background spacetime grid and
accretion disk using refractive shader parameters driven by general relativity.
"""

from __future__ import annotations

from . import constants as C
from . import materials

try:
    import bpy
    import bmesh
except ImportError:
    bpy = None  # type: ignore
    bmesh = None  # type: ignore


def remove_lensing_sphere(name: str = C.OBJ_LENSING):
    """Safely unlink and remove the lensing sphere object if present."""
    if bpy is None:
        return
    existing = bpy.data.objects.get(name)
    if existing is not None:
        mesh_old = existing.data
        bpy.data.objects.remove(existing, do_unlink=True)
        if mesh_old and mesh_old.users == 0:
            bpy.data.meshes.remove(mesh_old)


def build_lensing_sphere(
    collection,
    rs: float = C.RS_GEO,
    radius_factor: float = C.PHOTON_SPHERE_FACTOR,
    deflection_angle: float = 0.2,
    time_dilation: float = 0.8944,
    name: str = C.OBJ_LENSING,
    segments: int = 64,
    rings: int = 32,
):
    """Create or rebuild the refractive lensing shell at r = rs * radius_factor."""
    if bpy is None or bmesh is None:
        return None

    remove_lensing_sphere(name)

    mesh = bpy.data.meshes.new(name + "_Mesh")
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)

    sphere_radius = rs * radius_factor

    bm = bmesh.new()
    bmesh.ops.create_uvsphere(
        bm,
        u_segments=segments,
        v_segments=rings,
        radius=sphere_radius,
    )
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()

    mat = materials.ensure_lensing_material(
        rs=rs,
        deflection_angle=deflection_angle,
        time_dilation=time_dilation,
    )
    if mesh.materials:
        mesh.materials[0] = mat
    else:
        mesh.materials.append(mat)

    obj.location = (0.0, 0.0, 0.0)
    obj["bh_rs"] = float(rs)
    obj["bh_radius"] = float(sphere_radius)
    obj["bh_radius_factor"] = float(radius_factor)
    obj["bh_deflection_angle"] = float(deflection_angle)
    obj["bh_time_dilation"] = float(time_dilation)
    obj["bh_role"] = "lensing_sphere"

    # Make smooth shaded
    for poly in mesh.polygons:
        poly.use_smooth = True

    return obj
