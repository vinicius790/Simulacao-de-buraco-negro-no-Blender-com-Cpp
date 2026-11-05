"""Shared Blender material node trees matching ESTILO_VISUAL_SIMULACAO.md.

Disk: warm amber gradient vec3(1.0, r_norm, 0.2) — artistic approximation;
      real GR lensing / temperature lives in C++ geodesic.comp.
Grid: translucent grey vec4(0.5, 0.5, 0.5, 0.7).
Horizon: pure black, no specular.
Lensing: refractive photon shell modulated by deflection angle and time dilation.
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


def _set_or_create_value_node(nodes, name: str, value: float, x: float = -400.0, y: float = 0.0):
    """Ensure a ShaderNodeValue exists with the specified name and default value."""
    node = nodes.get(name)
    if node is None or node.type != "VALUE":
        node = nodes.new("ShaderNodeValue")
        node.name = name
        node.label = name
    node.location = (x, y)
    node.outputs[0].default_value = float(value)
    return node


def ensure_horizon_material(
    name: str = C.MAT_HORIZON,
    rs: float = C.RS_GEO,
    time_dilation: float = 0.0,
    deflection_angle: float = 0.0,
):
    """Black non-emissive sphere material (silhouette) exposing relativistic parameters."""
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")
    out.location = (300.0, 0.0)

    # Expose relativistic Value nodes for shader parameters / inspection / drivers
    _set_or_create_value_node(nodes, "SchwarzschildRadius", rs, x=-300.0, y=100.0)
    _set_or_create_value_node(nodes, "TimeDilation", time_dilation, x=-300.0, y=-50.0)
    _set_or_create_value_node(nodes, "DeflectionAngle", deflection_angle, x=-300.0, y=-200.0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (0.0, 0.0)
    bsdf.inputs["Base Color"].default_value = (*C.HORIZON_COLOR[:3], 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0
    bsdf.inputs["Emission Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 0.0
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    # Custom properties
    mat["bh_rs"] = float(rs)
    mat["bh_time_dilation"] = float(time_dilation)
    mat["bh_deflection_angle"] = float(deflection_angle)
    mat["bh_role"] = "horizon"
    return mat


def ensure_lensing_material(
    name: str = C.MAT_LENSING,
    rs: float = C.RS_GEO,
    deflection_angle: float = 0.2,
    time_dilation: float = 0.8944,
):
    """Refractive shell material simulating gravitational lensing and light deflection.

    IOR is driven by the deflection angle (n_eff = 1.0 + alpha * 0.15).
    Color transmission is tinted by gravitational time dilation (redshift proxy).
    """
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")
    out.location = (400.0, 0.0)

    # Expose relativistic Value nodes
    node_da = _set_or_create_value_node(nodes, "DeflectionAngle", deflection_angle, x=-500.0, y=100.0)
    _set_or_create_value_node(nodes, "SchwarzschildRadius", rs, x=-500.0, y=-50.0)
    node_td = _set_or_create_value_node(nodes, "TimeDilation", time_dilation, x=-500.0, y=-200.0)

    # Effective IOR node setup: IOR = 1.0 + DeflectionAngle * 0.15
    mult = nodes.new("ShaderNodeMath")
    mult.operation = "MULTIPLY"
    mult.location = (-260.0, 100.0)
    mult.inputs[1].default_value = 0.15
    links.new(node_da.outputs["Value"], mult.inputs[0])

    add_ior = nodes.new("ShaderNodeMath")
    add_ior.operation = "ADD"
    add_ior.location = (-80.0, 100.0)
    add_ior.inputs[0].default_value = 1.0
    links.new(mult.outputs["Value"], add_ior.inputs[1])

    # Redshift tint proxy driven by time dilation
    gamma_scale = nodes.new("ShaderNodeMapRange")
    gamma_scale.location = (-260.0, -150.0)
    gamma_scale.inputs["From Min"].default_value = 0.0
    gamma_scale.inputs["From Max"].default_value = 1.0
    gamma_scale.inputs["To Min"].default_value = 0.75
    gamma_scale.inputs["To Max"].default_value = 1.0
    links.new(node_td.outputs["Value"], gamma_scale.inputs["Value"])

    combine_col = nodes.new("ShaderNodeCombineColor")
    combine_col.location = (-80.0, -150.0)
    combine_col.inputs["Red"].default_value = 1.0
    links.new(gamma_scale.outputs["Result"], combine_col.inputs["Green"])
    links.new(gamma_scale.outputs["Result"], combine_col.inputs["Blue"])

    # Refraction BSDF
    refr = nodes.new("ShaderNodeBsdfRefraction")
    refr.location = (150.0, 0.0)
    refr.inputs["Roughness"].default_value = 0.0
    links.new(add_ior.outputs["Value"], refr.inputs["IOR"])
    links.new(combine_col.outputs["Color"], refr.inputs["Color"])

    links.new(refr.outputs["BSDF"], out.inputs["Surface"])

    # Viewport transparency / screen-space refraction hints
    mat.blend_method = "BLEND"
    if hasattr(mat, "use_screen_refraction"):
        mat.use_screen_refraction = True
    if hasattr(mat, "refraction_depth"):
        mat.refraction_depth = 0.2
    if hasattr(mat, "shadow_method"):
        mat.shadow_method = "NONE"

    mat["bh_rs"] = float(rs)
    mat["bh_deflection_angle"] = float(deflection_angle)
    mat["bh_time_dilation"] = float(time_dilation)
    mat["bh_role"] = "lensing"
    return mat


def ensure_disk_material(
    name: str = C.MAT_DISK,
    rs: float = C.RS_GEO,
    time_dilation: float = 1.0,
    deflection_angle: float = 0.0,
):
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

    # Expose relativistic Value nodes
    _set_or_create_value_node(nodes, "SchwarzschildRadius", rs, x=-500.0, y=300.0)
    _set_or_create_value_node(nodes, "TimeDilation", time_dilation, x=-500.0, y=150.0)
    _set_or_create_value_node(nodes, "DeflectionAngle", deflection_angle, x=-500.0, y=0.0)

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
    mat["bh_rs"] = float(rs)
    mat["bh_time_dilation"] = float(time_dilation)
    mat["bh_deflection_angle"] = float(deflection_angle)
    return mat


def ensure_grid_material(name: str = C.MAT_GRID):
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


def update_material_relativistic_params(
    rs: float = C.RS_GEO,
    deflection_angle: float = 0.2,
    time_dilation: float = 0.8944,
):
    """Update relativistic Value nodes and properties on all Black Hole materials."""
    if bpy is None:
        return
    for mat_name in (C.MAT_HORIZON, C.MAT_LENSING, C.MAT_DISK):
        mat = bpy.data.materials.get(mat_name)
        if mat is None or not mat.use_nodes:
            continue
        nodes = mat.node_tree.nodes
        for prop, val in (
            ("SchwarzschildRadius", rs),
            ("DeflectionAngle", deflection_angle),
            ("TimeDilation", time_dilation),
        ):
            node = nodes.get(prop)
            if node is not None and node.type == "VALUE":
                node.outputs[0].default_value = float(val)
        mat["bh_rs"] = float(rs)
        mat["bh_deflection_angle"] = float(deflection_angle)
        mat["bh_time_dilation"] = float(time_dilation)


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
