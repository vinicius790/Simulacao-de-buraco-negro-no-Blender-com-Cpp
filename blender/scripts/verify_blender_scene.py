import bpy
import json
import math
import sys
from pathlib import Path

def main():
    report = {
        "blender_version": bpy.app.version_string,
        "scene_name": bpy.context.scene.name,
        "objects": {},
        "materials": {},
        "frames": [bpy.context.scene.frame_start, bpy.context.scene.frame_end],
        "status": "PASS"
    }

    for obj in bpy.data.objects:
        report["objects"][obj.name] = {
            "type": obj.type,
            "location": [round(v, 4) for v in obj.location],
            "visible": obj.visible_get()
        }

    for mat in bpy.data.materials:
        node_names = [n.name for n in mat.node_tree.nodes] if mat.node_tree else []
        report["materials"][mat.name] = {
            "use_nodes": mat.use_nodes,
            "nodes": node_names
        }

    expected_objects = ["BH_EventHorizon", "BH_AccretionDisk", "BH_Camera", "BH_SpacetimeGrid"]
    for exp in expected_objects:
        if exp not in report["objects"]:
            report["status"] = "WARNING_MISSING_OBJECT"

    print("=== BLENDER SCENE VERIFICATION REPORT ===")
    print(json.dumps(report, indent=2))
    print("=========================================")

if __name__ == "__main__":
    main()
