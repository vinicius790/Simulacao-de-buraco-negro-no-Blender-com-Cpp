"""Shared Blender material node trees matching ESTILO_VISUAL_SIMULACAO.md.

Disk: warm amber gradient vec3(1.0, r_norm, 0.2) — artistic approximation;
      real GR lensing / temperature lives in C++ geodesic.comp.
Grid: translucent grey vec4(0.5, 0.5, 0.5, 0.7).
Horizon: pure black, no specular.
"""

from __future__ import annotations

from . import constants as C

try:
    import bpy
except ImportError:  # allow py_compile / headless import stubs
    bpy = None  # type: ignore


def _clear_nodes(mat):
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    return nodes, links


def ensure_horizon_material(name: str = "BH_Horizon_Mat"):
    """Black non-emissive sphere material (silhouette)."""
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*C.HORIZON_COLOR[:3], 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0
    bsdf.inputs["Emission Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 0.0
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def ensure_disk_material(name: str = "BH_Disk_Mat"):
    """Emission disk with radial amber gradient matching geodesic.comp.

    Color = (1.0, r_norm, 0.2) where r_norm comes from Generated/Object
    radial distance mapped via UV.v (radial) or a procedural length node.
    Labeled artistic approximation — physics in C++.
    """
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")

    # UV: U = angle, V = radial 0 at inner → 1 at outer (set by disk_mesh)
    texcoord = nodes.new("ShaderNodeTexCoord")
    sep = nodes.new("ShaderNodeSeparateXYZ")
    links.new(texcoord.outputs["UV"], sep.inputs["Vector"])

    # r_norm for color G channel: map V from [0,1] to [inner/outer, 1]
    # V=0 → inner (r_norm = 2.2/5.2), V=1 → outer (r_norm = 1)
    # geodesic uses r = length(pos)/disk_r2 directly.
    map_range = nodes.new("ShaderNodeMapRange")
    map_range.inputs["From Min"].default_value = 0.0
    map_range.inputs["From Max"].default_value = 1.0
    r_inner_norm = C.DISK_INNER_FACTOR / C.DISK_OUTER_FACTOR  # ≈ 0.423
    map_range.inputs["To Min"].default_value = r_inner_norm
    map_range.inputs["To Max"].default_value = 1.0
    links.new(sep.outputs["Y"], map_range.inputs["Value"])  # UV.v

    # Combine RGB: R=1, G=r_norm, B=0.2
    combine = nodes.new("ShaderNodeCombineColor")
    if hasattr(combine, "mode"):
        combine.mode = "RGB"
    combine.inputs["Red"].default_value = C.DISK_COLOR_R
    combine.inputs["Blue"].default_value = C.DISK_COLOR_B
    links.new(map_range.outputs["Result"], combine.inputs["Green"])

    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Strength"].default_value = 12.0
    links.new(combine.outputs["Color"], emission.inputs["Color"])

    # Slight Principled mix for viewport friendliness + pure emission for Cycles
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (1.0, 0.6, 0.2, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 8.0
    links.new(combine.outputs["Color"], bsdf.inputs["Emission Color"])
    if "Emission Color" not in bsdf.inputs:
        # older naming
        pass
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0

    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = 0.35
    links.new(bsdf.outputs["BSDF"], mix.inputs[1])
    links.new(emission.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs["Shader"], out.inputs["Surface"])

    mat["bh_style"] = "geodesic.comp vec3(1.0,r,0.2) artistic approximation"
    return mat


def ensure_grid_material(name: str = "BH_Grid_Mat"):
    """Translucent grey wireframe material — grid.frag vec4(0.5,0.5,0.5,0.7)."""
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    rgb = C.GRID_COLOR
    emission.inputs["Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    emission.inputs["Strength"].default_value = 0.7
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = 1.0 - rgb[3]  # alpha 0.7 → keep 70% emission
    links.new(emission.outputs["Emission"], mix.inputs[1])
    links.new(transparent.outputs["BSDF"], mix.inputs[2])
    # Invert: Fac=0 → emission; we want mostly emission with some transparency
    mix.inputs["Fac"].default_value = 0.3
    links.new(mix.outputs["Shader"], out.inputs["Surface"])
    mat.blend_method = "BLEND"
    if hasattr(mat, "shadow_method"):
        mat.shadow_method = "NONE"
    return mat


def setup_world_background(world=None, color=None):
    """Near-black world background (escaped rays → vec4(0))."""
    if world is None:
        world = bpy.context.scene.world
        if world is None:
            world = bpy.data.worlds.new("BH_World")
            bpy.context.scene.world = world
    color = color or C.WORLD_BG_COLOR
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputWorld")
    bg = nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (*color[:3], 1.0)
    bg.inputs["Strength"].default_value = 0.0
    links.new(bg.outputs["Background"], out.inputs["Surface"])
    return world
