#!/usr/bin/env python3
"""Build the style-locked BH scene and save a .blend (Blender --background).

Usage (when blender CLI is on PATH):
  blender --background --python blender/scripts/build_scene_headless.py

Optional args after -- :
  blender --background --python build_scene_headless.py -- --out /path/out.blend --bake

Steam Blender usually has no CLI on PATH — use the UI: Build Full Scene instead.
This script is bpy-only (no numpy).
"""

from __future__ import annotations

import sys
from pathlib import Path

# --- Resolve addon path and register without requiring Preferences install ---
_HERE = Path(__file__).resolve().parent
_ADDON_ROOT = _HERE.parent / "addons"
_ADDON_PKG = _ADDON_ROOT / "black_hole_bridge"
_DEFAULT_OUT = _HERE.parent / "examples" / "output" / "black_hole_sim.blend"


def _parse_argv(argv):
    out = _DEFAULT_OUT
    bake = True
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    else:
        argv = []
    i = 0
    while i < len(argv):
        if argv[i] == "--out" and i + 1 < len(argv):
            out = Path(argv[i + 1])
            i += 2
        elif argv[i] == "--bake":
            bake = True
            i += 1
        elif argv[i] == "--no-bake":
            bake = False
            i += 1
        else:
            i += 1
    return out, bake


def main():
    try:
        import bpy
    except ImportError:
        print(
            "ERROR: bpy not available. Run inside Blender:\n"
            "  blender --background --python blender/scripts/build_scene_headless.py\n"
            "Or use Steam Blender UI: sidebar Black Hole → Build Full Scene."
        )
        return 1

    out_path, do_bake = _parse_argv(sys.argv)
    out_path = out_path.resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Make package importable
    addon_parent = str(_ADDON_ROOT.resolve())
    if addon_parent not in sys.path:
        sys.path.insert(0, addon_parent)

    import black_hole_bridge
    from black_hole_bridge import scene_builder, camera_orbit, export_ops
    from black_hole_bridge import constants as C

    # Fresh scene
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # Register addon classes if needed
    if not hasattr(bpy.types.Scene, "bh_bridge"):
        black_hole_bridge.register()

    # Defaults on PropertyGroup
    s = bpy.context.scene.bh_bridge
    s.use_geo_units = True
    s.disk_inner_factor = C.DISK_INNER_FACTOR
    s.disk_outer_factor = C.DISK_OUTER_FACTOR
    s.grid_resolution = C.GRID_SIZE
    s.grid_multi_plane = True
    s.camera_radius_rs = C.CAMERA_RADIUS_M / C.SAG_A_RS_M
    s.camera_azimuth = C.CAMERA_AZIMUTH_RAD
    s.camera_elevation = C.CAMERA_ELEVATION_RAD
    s.anim_fps = C.ANIM_FPS
    s.anim_duration_s = C.ANIM_DURATION_S

    info = scene_builder.build_full_scene(bpy.context)
    print("[BH] ", info["note"])

    if do_bake:
        coll = scene_builder.ensure_collections()[C.COLL_CAMERA]
        _empty, cam = camera_orbit.ensure_orbit_rig(coll)
        radius = camera_orbit.geo_radius_from_settings(s)
        n = camera_orbit.bake_orbit_animation(
            cam,
            radius=radius,
            elevation=s.camera_elevation,
            fps=int(s.anim_fps),
            duration_s=s.anim_duration_s,
            scene=bpy.context.scene,
        )
        bpy.context.scene.camera = cam
        print(f"[BH] Baked {n} orbit frames")

    bpy.ops.wm.save_as_mainfile(filepath=str(out_path))
    print(f"[BH] Saved {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
