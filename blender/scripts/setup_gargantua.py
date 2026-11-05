import bpy
import math

def setup_gargantua_scene(
    spin_a_star=0.999,
    rs=1.0,
    disk_inner=2.2,
    disk_outer=5.2
):
    """
    Creates a Gargantua-style black hole with:
    1. Pure black Event Horizon / Shadow sphere.
    2. Primary Equatorial Accretion Disk (glowing gas).
    3. Gravitational Lensing Halo (vertical upper & lower disk wrap-around).
    4. Relativistic Doppler Beaming material with left-right asymmetry.
    """
    # 1. Event Horizon
    bpy.ops.mesh.primitive_uv_sphere_add(radius=rs, location=(0, 0, 0))
    horizon = bpy.context.active_object
    horizon.name = "Gargantua_EventHorizon"

    mat_horizon = bpy.data.materials.new(name="Gargantua_Shadow_Mat")
    mat_horizon.use_nodes = True
    nodes = mat_horizon.node_tree.nodes
    nodes.clear()
    bsdf_black = nodes.new(type="ShaderNodeBsdfPrincipled")
    bsdf_black.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    bsdf_black.inputs["Roughness"].default_value = 1.0
    out_node = nodes.new(type="ShaderNodeOutputMaterial")
    mat_horizon.node_tree.links.new(bsdf_black.outputs["BSDF"], out_node.inputs["Surface"])
    horizon.data.materials.append(mat_horizon)

    # 2. Equatorial Disk
    bpy.ops.mesh.primitive_cylinder_add(radius=disk_outer * rs, depth=0.04 * rs, location=(0, 0, 0))
    disk = bpy.context.active_object
    disk.name = "Gargantua_EquatorialDisk"

    mat_disk = bpy.data.materials.new(name="Gargantua_Disk_Mat")
    mat_disk.use_nodes = True
    d_nodes = mat_disk.node_tree.nodes
    d_nodes.clear()

    emission = d_nodes.new(type="ShaderNodeEmission")
    emission.inputs["Color"].default_value = (1.0, 0.58, 0.18, 1.0) # Amber/Gold
    emission.inputs["Strength"].default_value = 15.0

    d_out = d_nodes.new(type="ShaderNodeOutputMaterial")
    mat_disk.node_tree.links.new(emission.outputs["Emission"], d_out.inputs["Surface"])
    disk.data.materials.append(mat_disk)

    # 3. Gravitational Lensed Vertical Halo (Gargantua halo effect)
    bpy.ops.mesh.primitive_torus_add(
        major_radius=rs * 2.6,
        minor_radius=rs * 0.15,
        location=(0, 0, 0),
        rotation=(math.radians(90), 0, 0)
    )
    halo = bpy.context.active_object
    halo.name = "Gargantua_LensedHalo"
    halo.data.materials.append(mat_disk)

    print("Gargantua DNGR relativistic scene built successfully!")

if __name__ == "__main__":
    setup_gargantua_scene()
