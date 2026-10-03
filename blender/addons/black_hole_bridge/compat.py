"""Blender-version compatibility helpers (4.0 … 5.x).

* Animation: Blender 4.4 introduced *slotted* (layered) actions and Blender 5.0
  REMOVED the legacy ``Action.fcurves`` collection. F-curves now live in the
  channel bag of the action slot assigned to the animated ID
  (``bpy_extras.anim_utils.action_get_channelbag_for_slot``).
* Node trees: ``Material.use_nodes`` / ``World.use_nodes`` are always on in
  Blender 5.0 and are deprecated (removal planned for 6.0); only touch them on
  older versions so 5.x stays warning-free.

Every function degrades gracefully and imports without bpy (headless tests).
"""

from __future__ import annotations

try:
    import bpy
except ImportError:  # headless import (py_compile / parity tests)
    bpy = None  # type: ignore


def blender_version() -> tuple:
    return tuple(bpy.app.version) if bpy is not None else (0, 0, 0)


def ensure_use_nodes(idblock) -> None:
    """Enable the node tree of a Material/World on Blender < 5.0 (always on in 5.x)."""
    if blender_version() < (5, 0, 0):
        idblock.use_nodes = True


def fcurve_collection(id_data):
    """Collection holding the F-curves animating ``id_data`` (supports iteration
    and ``.remove``), or ``None`` when it has no action.

    Blender ≥ 4.4: the channel bag of the assigned action slot.
    Blender < 4.4: the legacy ``Action.fcurves``.
    """
    ad = getattr(id_data, "animation_data", None)
    if ad is None or ad.action is None:
        return None
    action = ad.action
    slot = getattr(ad, "action_slot", None)
    if slot is not None:
        try:
            from bpy_extras import anim_utils

            get_bag = getattr(anim_utils, "action_get_channelbag_for_slot", None)
        except ImportError:
            get_bag = None
        if get_bag is not None:
            bag = get_bag(action, slot)
            if bag is not None:
                return bag.fcurves
    return getattr(action, "fcurves", None)


def fcurves(id_data) -> list:
    """List of F-curves animating ``id_data`` (empty when none)."""
    coll = fcurve_collection(id_data)
    return list(coll) if coll is not None else []


def remove_fcurves(id_data, data_path: str) -> int:
    """Remove every F-curve of ``id_data`` with the given data path. Returns count."""
    coll = fcurve_collection(id_data)
    if coll is None:
        return 0
    doomed = [fc for fc in coll if fc.data_path == data_path]
    for fc in doomed:
        coll.remove(fc)
    return len(doomed)


def set_linear(id_data, data_path: str | None = None, extrapolate: bool = False) -> int:
    """LINEAR interpolation (and optionally extrapolation) on the F-curves of
    ``id_data`` (all, or only ``data_path``). Returns the number touched."""
    n = 0
    for fc in fcurves(id_data):
        if data_path is not None and fc.data_path != data_path:
            continue
        if extrapolate:
            fc.extrapolation = "LINEAR"
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
        fc.update()
        n += 1
    return n
