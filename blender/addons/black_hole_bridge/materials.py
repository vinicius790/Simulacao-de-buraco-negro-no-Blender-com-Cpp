"""Shared Blender material node trees matching ESTILO_VISUAL_SIMULACAO.md.

Disk: warm amber gradient vec3(1.0, r_norm, 0.2) — artistic approximation;
      real GR lensing / temperature lives in C++ geodesic.comp.
Grid: translucent grey vec4(0.5, 0.5, 0.5, 0.7).
Guides: same grey, alpha 0.5 (photon sphere / ISCO / b_c rings).
Horizon: pure black, no specular.

Disk spin (0.8.0): an optional Mapping node shifts UV.u (angle/2π), so
keyframing Location.x by N turns rotates the disk texture N times. Optional
turbulence modulates *brightness only* (hue stays (1, r_norm, 0.2)); both are
OFF by default so the locked look is unchanged.
"""

from __future__ import annotations

import math

from . import constants as C

try:
    import bpy
except ImportError:  # allow py_compile / headless import stubs
    bpy = None  # type: ignore

DISK_SPIN_NODE = "BH_DiskSpin"
# Emission strength 1.0 + the scene's "Raw" view transform (scene_builder)
# reproduces the OpenGL framebuffer value exactly; >1 is an opt-in glow.
DISK_EMISSION_STRENGTH = 1.0


def _clear_nodes(mat):
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    return nodes, links


def _set_blended(mat) -> None:
    """Alpha-blended surface for EEVEE Legacy (4.0/4.1) and EEVEE Next (4.2+)."""
    if hasattr(mat, "blend_method"):
        mat.blend_method = "BLEND"
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "BLENDED"
    if hasattr(mat, "shadow_method"):
        mat.shadow_method = "NONE"


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


def _math_node(nodes, operation: str, label: str = ""):
    node = nodes.new("ShaderNodeMath")
    node.operation = operation
    if label:
        node.label = label
    return node


def _build_turbulence_modulation(nodes, links, uv_socket, turbulence: float):
    """Seam-free brightness modulation driven by the (rotated) disk UV.

    UV.u is angle/2π, so noise sampled on (cos 2πu, sin 2πu, v) wraps cleanly.
    Returns the socket carrying a factor in [1 − turbulence, 1 + turbulence].
    """
    sep = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv_socket, sep.inputs["Vector"])

    angle = _math_node(nodes, "MULTIPLY", "2π·u")
    angle.inputs[1].default_value = 2.0 * math.pi
    links.new(sep.outputs["X"], angle.inputs[0])

    cos_n = _math_node(nodes, "COSINE")
    sin_n = _math_node(nodes, "SINE")
    links.new(angle.outputs["Value"], cos_n.inputs[0])
    links.new(angle.outputs["Value"], sin_n.inputs[0])

    radial = _math_node(nodes, "MULTIPLY", "v·scale")
    radial.inputs[1].default_value = 3.0
    links.new(sep.outputs["Y"], radial.inputs[0])

    combine = nodes.new("ShaderNodeCombineXYZ")
    links.new(cos_n.outputs["Value"], combine.inputs["X"])
    links.new(sin_n.outputs["Value"], combine.inputs["Y"])
    links.new(radial.outputs["Value"], combine.inputs["Z"])

    noise = nodes.new("ShaderNodeTexNoise")
    noise.label = "BH turbulence (brightness only)"
    noise.inputs["Scale"].default_value = 4.0
    noise.inputs["Detail"].default_value = 3.0
    links.new(combine.outputs["Vector"], noise.inputs["Vector"])

    mod = nodes.new("ShaderNodeMapRange")
    mod.inputs["From Min"].default_value = 0.0
    mod.inputs["From Max"].default_value = 1.0
    mod.inputs["To Min"].default_value = 1.0 - turbulence
    mod.inputs["To Max"].default_value = 1.0 + turbulence
    links.new(noise.outputs["Fac"], mod.inputs["Value"])
    return mod.outputs["Result"]


def ensure_disk_material(
    name: str = "BH_Disk_Mat",
    spin: bool = False,
    turbulence: float = 0.0,
    inner_factor: float = C.DISK_INNER_FACTOR,
    outer_factor: float = C.DISK_OUTER_FACTOR,
    glow: float = DISK_EMISSION_STRENGTH,
):
    """Pure-emission disk reproducing the geodesic.comp pixel exactly.

    geodesic.comp writes vec4(1, r, 0.2, r) with r = |pos| / disk_r2 and the
    fullscreen pass blends it with SRC_ALPHA over the black clear colour, so the
    on-screen colour is (1, r, 0.2)·r. Here r comes from UV.v (radial, set by
    disk_mesh) remapped to [inner/outer, 1] using the ACTUAL disk factors, and
    the emission colour is (1, r, 0.2)·r at strength ``glow`` (1.0 = parity
    with the OpenGL framebuffer under the "Raw" view transform).

    spin=True inserts a POINT Mapping node (``BH_DiskSpin``) between TexCoord
    UV and the gradient so Location.x can be keyframed (1.0 = one full turn).
    turbulence>0 multiplies the emission *strength* by a noise factor in
    [1−t, 1+t]; the hue model is never touched.
    """
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")

    # UV: U = angle, V = radial 0 at inner → 1 at outer (set by disk_mesh)
    texcoord = nodes.new("ShaderNodeTexCoord")
    uv_socket = texcoord.outputs["UV"]
    if spin:
        mapping = nodes.new("ShaderNodeMapping")
        mapping.name = DISK_SPIN_NODE
        mapping.label = "BH disk spin (Location.x = turns)"
        mapping.vector_type = "POINT"
        links.new(uv_socket, mapping.inputs["Vector"])
        uv_socket = mapping.outputs["Vector"]

    sep = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv_socket, sep.inputs["Vector"])

    # r_norm for color G channel: map V from [0,1] to [inner/outer, 1]
    # V=0 → inner (r_norm = 2.2/5.2), V=1 → outer (r_norm = 1)
    # geodesic uses r = length(pos)/disk_r2 directly.
    map_range = nodes.new("ShaderNodeMapRange")
    map_range.inputs["From Min"].default_value = 0.0
    map_range.inputs["From Max"].default_value = 1.0
    r_inner_norm = float(inner_factor) / float(outer_factor)  # legacy 2.2/5.2 ≈ 0.423
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

    # Premultiply by alpha = r (SRC_ALPHA blend over black in black_hole.cpp).
    premul = nodes.new("ShaderNodeVectorMath")
    premul.operation = "SCALE"
    premul.label = "(1, r, 0.2) · r  (alpha blend over black)"
    links.new(combine.outputs["Color"], premul.inputs["Vector"])
    links.new(map_range.outputs["Result"], premul.inputs["Scale"])

    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Strength"].default_value = float(glow)
    links.new(premul.outputs["Vector"], emission.inputs["Color"])

    if turbulence > 0.0:
        factor = _build_turbulence_modulation(nodes, links, uv_socket, float(turbulence))
        strength = _math_node(nodes, "MULTIPLY", "emission strength")
        strength.inputs[1].default_value = float(glow)
        links.new(factor, strength.inputs[0])
        links.new(strength.outputs["Value"], emission.inputs["Strength"])

    links.new(emission.outputs["Emission"], out.inputs["Surface"])

    mat["bh_style"] = "geodesic.comp vec4(1,r,0.2,r) blended over black = (1,r,0.2)*r"
    mat["bh_inner_rs"] = float(inner_factor)
    mat["bh_outer_rs"] = float(outer_factor)
    mat["bh_glow"] = float(glow)
    mat["bh_spin"] = bool(spin)
    mat["bh_turbulence"] = float(turbulence)
    return mat


def disk_spin_node(mat):
    """Return the ``BH_DiskSpin`` Mapping node of a disk material, or None."""
    if mat is None or not mat.use_nodes or mat.node_tree is None:
        return None
    return mat.node_tree.nodes.get(DISK_SPIN_NODE)


def set_disk_spin_keyframes(mat, scene, turns_total: float) -> int:
    """Keyframe the spin Mapping Location.x: 0 at frame_start → turns_total at
    frame_end, LINEAR interpolation/extrapolation. Returns the frame span.

    Raises ValueError when the material has no spin node (build it with
    ``ensure_disk_material(spin=True)`` first).
    """
    mapping = disk_spin_node(mat)
    if mapping is None:
        raise ValueError(f"material {mat.name!r} has no {DISK_SPIN_NODE} node")
    sock = mapping.inputs["Location"]
    data_path = sock.path_from_id("default_value")
    tree = mat.node_tree

    if tree.animation_data and tree.animation_data.action:
        for fc in list(tree.animation_data.action.fcurves):
            if fc.data_path == data_path:
                tree.animation_data.action.fcurves.remove(fc)

    f0, f1 = int(scene.frame_start), int(scene.frame_end)
    if f1 <= f0:
        f1 = f0 + 1
    sock.default_value[0] = 0.0
    sock.keyframe_insert("default_value", index=0, frame=f0)
    # Seamless loop: frame_end shows turns·(n−1)/n so frame_end+1 ≡ frame_start
    # (keyframing the full turn at frame_end would repeat the first image).
    n = max(1, f1 - f0 + 1)
    sock.default_value[0] = float(turns_total) * (n - 1) / n
    sock.keyframe_insert("default_value", index=0, frame=f1)

    for fc in tree.animation_data.action.fcurves:
        if fc.data_path == data_path:
            fc.extrapolation = "LINEAR"
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
            fc.update()
    mat["bh_spin_turns"] = float(turns_total)
    scene.frame_set(f0)
    return f1 - f0


def _grey_translucent(name: str, rgba, strength: float):
    """Emission grey mixed with Transparent — shared by grid and guide rings."""
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    nodes, links = _clear_nodes(mat)
    out = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (rgba[0], rgba[1], rgba[2], 1.0)
    emission.inputs["Strength"].default_value = strength
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    mix = nodes.new("ShaderNodeMixShader")
    # Fac=0 → emission (input 1); Fac=1 → transparent (input 2): Fac = 1 − alpha
    mix.inputs["Fac"].default_value = 1.0 - rgba[3]
    links.new(emission.outputs["Emission"], mix.inputs[1])
    links.new(transparent.outputs["BSDF"], mix.inputs[2])
    links.new(mix.outputs["Shader"], out.inputs["Surface"])
    _set_blended(mat)
    return mat


def ensure_grid_material(name: str = "BH_Grid_Mat"):
    """grid.frag vec4(0.5,0.5,0.5,0.7) blended over black → 0.35 grey (Raw view)."""
    return _grey_translucent(name, C.GRID_COLOR, strength=1.0)


def ensure_guide_material(name: str = "BH_Guide_Mat"):
    """Guide rings: grid grey (0.5,0.5,0.5), low emission, alpha ≈ 0.5, BLEND."""
    mat = _grey_translucent(name, C.GUIDE_COLOR, strength=1.0)
    mat["bh_role"] = "guide_material"
    return mat


def setup_color_parity(scene) -> str:
    """Use the "Raw" view transform so emission values reach the image unchanged,
    exactly like the OpenGL framebuffer (no filmic/AgX tone curve, which would
    wash the amber disk out to near-white). Returns the transform applied.
    Users may switch to AgX/Filmic afterwards for a cinematic look."""
    vs = scene.view_settings
    for transform in ("Raw", "Standard"):
        try:
            vs.view_transform = transform
            break
        except TypeError:
            continue
    try:
        vs.look = "None"
    except TypeError:
        pass
    vs.exposure = 0.0
    vs.gamma = 1.0
    return vs.view_transform


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
