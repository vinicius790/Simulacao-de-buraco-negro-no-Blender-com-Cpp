#!/usr/bin/env python3
"""Blender addon ⇄ C++ parity checks that run under plain python3 (no bpy).

Covers: constants mirrored from include/black_hole/*.hpp, camera / grid-warp
formulas, scene_params JSON round-trip, the bh_render_cpu CLI contract,
animation preset endpoints and the two packaged zips.

Usage: python3 tests/test_blender_addon_parity.py [--root REPO]
"""

from __future__ import annotations

import argparse
import math
import re
import sys
import types
import zipfile
from pathlib import Path


def _hdr_float(text: str, name: str) -> float:
    m = re.search(rf"\b{name}\s*=\s*([0-9][0-9.eE+-]*)", text)
    if m is None:
        raise AssertionError(f"constant {name} not found in header")
    return float(m.group(1))


def _close(a: float, b: float, rel: float = 1e-12, what: str = "") -> None:
    if not math.isclose(a, b, rel_tol=rel, abs_tol=1e-15):
        raise AssertionError(f"{what}: {a!r} != {b!r}")


def default_settings():
    """SimpleNamespace with the BHBridgeSettings attribute names / defaults."""
    import black_hole_bridge.constants as C

    return types.SimpleNamespace(
        mass_kg=C.SAG_A_MASS_KG,
        disk_inner_factor=C.DISK_INNER_FACTOR,
        disk_outer_factor=C.DISK_OUTER_FACTOR,
        disk_num=C.DISK_NUM,
        disk_thickness_m=C.DISK_THICKNESS_M,
        use_geo_units=True,
        camera_radius_m=C.CAMERA_RADIUS_M,
        camera_radius_rs=C.CAMERA_RADIUS_M / C.SAG_A_RS_M,
        camera_azimuth=C.CAMERA_AZIMUTH_RAD,
        camera_elevation=C.CAMERA_ELEVATION_RAD,
        camera_fov_y_deg=C.CAMERA_FOV_Y_DEG,
        grid_resolution=C.GRID_SIZE,
        grid_multi_plane=False,
        anim_fps=C.ANIM_FPS,
        anim_duration_s=C.ANIM_DURATION_S,
        anim_mode="TURNTABLE",
        anim_easing="LINEAR",
        anim_elev_start=C.ANIM_ELEV_START_RAD,
        anim_elev_end=C.ANIM_ELEV_END_RAD,
        anim_radius_end_rs=C.ANIM_RADIUS_END_RS,
        disk_spin_turns=C.DISK_SPIN_TURNS,
        disk_turbulence=C.DISK_TURBULENCE,
        renderer_path="",
        render_width=800,
        render_height=600,
        render_mode="LEGACY",
        render_integrator="RK4",
        render_frames=1,
        render_output_path="//bh_render/frame.png",
        render_supersample=1,
        render_timeout_s=900,
        export_path="//scene_params.json",
    )


def check_constants(root: Path, C) -> None:
    inc = root / "include/black_hole"
    disk = (inc / "disk_model.hpp").read_text(encoding="utf-8")
    units = (inc / "units.hpp").read_text(encoding="utf-8")
    cam = (inc / "camera_model.hpp").read_text(encoding="utf-8")
    schw = (inc / "schwarzschild.hpp").read_text(encoding="utf-8")

    _close(C.DISK_INNER_FACTOR, _hdr_float(disk, "LEGACY_INNER_FACTOR_RS"), what="disk inner")
    _close(C.DISK_OUTER_FACTOR, _hdr_float(disk, "LEGACY_OUTER_FACTOR_RS"), what="disk outer")
    _close(C.DISK_THICKNESS_M, _hdr_float(disk, "LEGACY_THICKNESS_M"), what="disk thickness")
    _close(C.DISK_NUM, _hdr_float(disk, "LEGACY_DISK_NUM"), what="disk_num")
    assert C.DISK_INNER_FACTOR == 2.2 and C.DISK_OUTER_FACTOR == 5.2 and C.DISK_THICKNESS_M == 1e9

    _close(C.SAG_A_MASS_KG, _hdr_float(units, "SAGITTARIUS_A_MASS_KG"), what="mass")
    _close(C.SAG_A_RS_M, _hdr_float(units, "LEGACY_SAGA_RS_M"), what="SagA_rs")
    assert C.SAG_A_MASS_KG == 8.54e36 and C.SAG_A_RS_M == 1.269e10
    _close(C.G, _hdr_float(units, "G_SI"), what="G")
    _close(C.C, _hdr_float(units, "C_SI"), what="c")

    _close(C.CAMERA_RADIUS_M, _hdr_float(cam, "LEGACY_RADIUS_M"), what="camera radius")
    _close(C.GRID_WARP_OFFSET_M, _hdr_float(cam, "GRID_WARP_OFFSET_M"), what="warp offset")
    _close(C.CAMERA_MIN_RADIUS_M, _hdr_float(cam, "MIN_RADIUS_M"), what="min radius")
    _close(C.CAMERA_MAX_RADIUS_M, _hdr_float(cam, "MAX_RADIUS_M"), what="max radius")
    _close(C.CAMERA_FOV_Y_DEG, _hdr_float(cam, "LEGACY_FOV_Y_DEG"), what="fov")
    assert C.CAMERA_RADIUS_M == 6.34194e10 and C.GRID_WARP_OFFSET_M == 3.0e10
    assert C.CAMERA_MIN_RADIUS_M == 1e10 and C.CAMERA_MAX_RADIUS_M == 1e12
    _close(C.CAMERA_ELEVATION_RAD, math.pi / 2.0, what="elevation default")

    m_ph = re.search(r"photon_sphere_radius\s*\(\s*double\s+rs\s*\)\s*\{\s*return\s+([0-9.]+)\s*\*\s*rs", schw)
    m_isco = re.search(r"isco_radius\s*\(\s*double\s+rs\s*\)\s*\{\s*return\s+([0-9.]+)\s*\*\s*rs", schw)
    assert m_ph and m_isco, "schwarzschild.hpp photon/ISCO factors not found"
    _close(C.PHOTON_SPHERE_FACTOR_RS, float(m_ph.group(1)), what="photon sphere")
    _close(C.ISCO_FACTOR_RS, float(m_isco.group(1)), what="ISCO")
    _close(C.CRITICAL_IMPACT_FACTOR_RS, 3.0 * math.sqrt(3.0) / 2.0, what="b_c")
    assert abs(C.CRITICAL_IMPACT_FACTOR_RS - 2.598) < 1e-3

    assert tuple(C.GRID_COLOR) == (0.5, 0.5, 0.5, 0.7)
    assert C.disk_color_rgb(0.5) == (1.0, 0.5, 0.2)
    assert C.disk_color_rgb(7.0) == (1.0, 1.0, 0.2)
    assert tuple(C.WINDOW_WH) == (800, 600) and tuple(C.COMPUTE_WH) == (200, 150)
    assert C.COLL_GUIDES == "BH_Guides"


def check_formulas(C) -> None:
    def cpp_warp(dist, rs, offset):
        return 2.0 * math.sqrt(rs * (dist - rs)) - offset if dist > rs else 2.0 * rs - offset

    for dist, rs, offset in ((0.0, 1.0, 2.36), (1.0, 1.0, 2.36), (2.5, 1.0, 2.36),
                             (4.0e10, 1.269e10, 3.0e10), (1.0e10, 1.269e10, 3.0e10)):
        _close(C.grid_warp_y(dist, rs, offset), cpp_warp(dist, rs, offset), what=f"warp {dist}")
    # Default offset in geo units = 3e10 / 1.269e10 * rs
    _close(C.grid_warp_y(3.0, 1.0), cpp_warp(3.0, 1.0, 3.0e10 / 1.269e10), what="warp default")

    for r, az, el in ((5.0, 0.0, math.pi / 2), (6.34194e10, 1.1, 0.4), (3.0, -2.0, 3.0)):
        e = max(0.01, min(math.pi - 0.01, el))
        expect = (r * math.sin(e) * math.cos(az), r * math.cos(e), r * math.sin(e) * math.sin(az))
        got = C.camera_position(r, az, el)
        for g, x in zip(got, expect):
            _close(g, x, what="camera_position")
    # Elevation clamp mirrors camera_model.hpp ELEVATION_MIN/MAX
    assert C.camera_position(1.0, 0.0, 0.0) == C.camera_position(1.0, 0.0, 0.01)
    _close(C.rs_from_mass(C.SAG_A_MASS_KG), 2 * C.G * C.SAG_A_MASS_KG / C.C ** 2, what="rs")


def check_blender_frame(C, grid_mesh, guides, disk_mesh) -> None:
    """C++ (Y-up) → Blender (Z-up) mapping must be a proper rotation so the
    Blender camera (up = +Z) frames the scene exactly like the C++ camera."""

    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])

    ex, ey, ez = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)
    # Columns of the rotation matrix; det = e1 · (e2 × e3) must be +1.
    c1, c2, c3 = C.cpp_to_blender(ex), C.cpp_to_blender(ey), C.cpp_to_blender(ez)
    det = sum(a * b for a, b in zip(c1, cross(c2, c3)))
    assert det == 1.0, f"cpp_to_blender is not a proper rotation (det={det})"
    assert c2 == (0.0, 0.0, 1.0), "C++ up (+Y) must map to Blender up (+Z)"
    for v in ((1.0, 2.0, 3.0), (-4.5, 0.25, 7.0)):
        assert C.blender_to_cpp(C.cpp_to_blender(v)) == v, "inverse mapping"
        w = (0.3, -1.0, 2.0)
        lhs = C.cpp_to_blender(cross(v, w))
        rhs = cross(C.cpp_to_blender(v), C.cpp_to_blender(w))
        for g, x in zip(lhs, rhs):
            _close(g, x, what="cross product preserved")
    # Camera: equatorial default sits in Blender's XY plane at height 0.
    bx, by, bz = C.camera_position_blender(5.0, 0.0, math.pi / 2)
    _close(bz, 0.0, what="equatorial camera height (Blender Z)")
    _close(math.hypot(bx, by), 5.0, what="equatorial camera radius")
    bx, by, bz = C.camera_position_blender(5.0, 0.7, 1.25)
    _close(bz, 5.0 * math.cos(1.25), what="elevated camera height = r cos(el)")

    # Generated geometry lands in the Blender disk plane (Z = 0) after mapping.
    verts, _faces, _uvs, r_in, r_out = disk_mesh.build_disk_mesh_data()
    assert all(abs(C.cpp_to_blender(v)[2]) < 1e-12 for v in verts), "disk in Blender XY"
    _close(r_in, 2.2, what="disk inner"); _close(r_out, 5.2, what="disk outer")
    ring, _ = guides.build_circle_edges(1.5, 32)
    assert all(abs(C.cpp_to_blender(v)[2]) < 1e-12 for v in ring), "guide ring in Blender XY"
    # Grid quads: (N)² faces whose edges are exactly the grid lines.
    n = 25
    faces = grid_mesh.grid_quad_faces(n)
    assert len(faces) == n * n
    gverts, gedges, _sp, _off = grid_mesh.build_warped_grid_verts_edges(grid_size=n)
    face_edges = {tuple(sorted((f[i], f[(i + 1) % 4]))) for f in faces for i in range(4)}
    assert face_edges == {tuple(sorted(e)) for e in gedges}, "quad edges == grid lines"
    # Warp axis: C++ Y becomes Blender Z (funnel height).
    _close(C.cpp_to_blender(gverts[0])[2], gverts[0][1], what="grid warp → Blender Z")

    # The locked C++ default camera is inside the disk slab; elevation 1.25 is not.
    r_def = C.CAMERA_RADIUS_M / C.SAG_A_RS_M
    assert C.camera_inside_disk(r_def, C.CAMERA_ELEVATION_RAD), "C++ default is inside the disk"
    assert not C.camera_inside_disk(r_def, 1.25), "elevation 1.25 clears the disk"
    assert not C.camera_inside_disk(6.0, math.pi / 2), "outside the outer edge"


def check_json(json_io, C) -> None:
    data = json_io.build_params_dict()
    assert data["schema"] == "black_hole.scene_params/v1"
    assert data["disk"]["inner_factor_rs"] == 2.2 and data["disk"]["outer_factor_rs"] == 5.2
    assert data["camera"]["radius_m"] == 6.34194e10
    assert data["camera"]["radius_rs"] is not None
    assert data["guides"]["photon_sphere_rs"] == 1.5 and data["guides"]["isco_rs"] == 3.0
    _close(data["guides"]["critical_impact_rs"], 3.0 * math.sqrt(3.0) / 2.0, what="json b_c")
    assert set(data["render"]) >= {"width", "height", "mode", "integrator", "frames", "supersample"}
    assert data["render"]["mode"] == "legacy" and data["render"]["integrator"] == "rk4"
    for key in ("mode", "easing", "elev_start_rad", "elev_end_rad", "radius_end_rs", "fps",
                "duration_s", "azimuth_start_rad", "azimuth_end_rad"):
        assert key in data["animation"], key
    assert data["render_baseline"]["window"] == [800, 600]

    # Round trip through a settings-like namespace.
    s = default_settings()
    s.anim_mode = "SPIRAL"
    s.anim_easing = "SINE"
    s.anim_radius_end_rs = 9.5
    s.render_mode = "RELATIVISTIC"
    s.render_integrator = "RK45"
    s.render_frames = 48
    s.render_width, s.render_height = 320, 240
    s.camera_radius_rs = 7.25
    s.camera_azimuth = 0.7
    s.disk_turbulence = 0.3
    out = json_io.params_from_settings(s)
    assert out["animation"]["mode"] == "spiral" and out["render"]["mode"] == "relativistic"
    _close(out["camera"]["radius_rs"], 7.25, what="radius_rs export")

    t = default_settings()
    out["objects"] = [{"pos_m": [4e11, 0, 0], "radius_m": 4e10}]  # C++ extra key tolerated
    out["gravity"] = False
    json_io.apply_params_to_settings(t, out)
    assert t.anim_mode == "SPIRAL" and t.anim_easing == "SINE"
    _close(t.anim_radius_end_rs, 9.5, what="radius_end")
    assert t.render_mode == "RELATIVISTIC" and t.render_integrator == "RK45"
    assert t.render_frames == 48 and (t.render_width, t.render_height) == (320, 240)
    _close(t.camera_radius_rs, 7.25, rel=1e-9, what="radius_rs import")
    _close(t.camera_azimuth, 0.7, what="azimuth import")
    _close(t.disk_turbulence, 0.3, what="turbulence import")
    _close(t.disk_inner_factor, 2.2, what="inner import")

    # 0.7.x files: legacy label + no new sections.
    u = default_settings()
    json_io.apply_params_to_settings(u, {"animation": {"mode": "turntable_azimuth"}, "render": {"mode": "bogus"}})
    assert u.anim_mode == "TURNTABLE" and u.render_mode == "LEGACY"


def check_bridge_sync(rb, C) -> None:
    """Per-frame sync helpers: the C++ orbit parameters recovered from a
    camera position must reproduce that position exactly."""
    for r, az, el in ((4.997, 0.0, math.pi / 2), (12.0, 2.5, 0.3), (18.0, -1.0, 1.4), (6.0, 3.0, 2.9)):
        pos = C.camera_position(r, az, el)
        r2, az2, el2 = rb.orbit_params_from_cpp_position(pos)
        for g, x in zip(C.camera_position(r2, az2, el2), pos):
            _close(g, x, what="orbit params round trip")
        _close(r2, r, what="radius"); _close(el2, el, what="elevation")
        # Through the Blender frame and back (what the operator does).
        r3, _az3, el3 = rb.orbit_params_from_cpp_position(C.blender_to_cpp(C.camera_position_blender(r, az, el)))
        _close(r3, r, what="radius via Blender frame"); _close(el3, el, what="elevation via Blender frame")
    _close(rb.camera_fov_y("VERTICAL", 1.0, math.radians(60.0), 4 / 3), math.radians(60.0), what="vertical fit")
    _close(rb.camera_fov_y("HORIZONTAL", math.radians(80.0), 0.0, 2.0),
           2 * math.atan(math.tan(math.radians(40.0)) / 2.0), what="horizontal fit")


def check_seamless_paths(co) -> None:
    """Periodic azimuth sampled at k/N: the frame after the last equals frame 0."""
    n = 48
    fn = co.make_path_fn("TURNTABLE", "LINEAR", 5.0, 0.3, 1.2, 9.0, 0.4, 2.7)
    last = fn((n - 1) / (n - 1), (n - 1) / n)
    _close(last[1] + 2 * math.pi / n - 2 * math.pi, 0.3, what="turntable loop closes without a duplicate frame")
    first = fn(0.0, 0.0)
    assert abs(last[1] - first[1] - 2 * math.pi) > 1e-6, "last frame is not a repeat of the first"
    spiral = co.make_path_fn("SPIRAL", "LINEAR", 5.0, 0.0, 1.2, 9.0, 0.4, 2.7)
    _close(spiral(1.0, (n - 1) / n)[2], 2.7, what="spiral elevation reaches its end value")
    # Legacy single-argument behaviour kept: t_azimuth defaults to t.
    _close(fn(0.5)[1], 0.3 + math.pi, what="t_azimuth defaults to t")


def check_render_bridge(rb) -> None:
    argv = rb.build_render_command(
        "/opt/bh/bh_render_cpu", "/tmp/scene.json", "/tmp/out/frame.png",
        800, 600, "LEGACY", "RK4", 0.0, math.pi / 2, 4.997, 60.0, frames=1, supersample=1,
    )
    assert argv == [
        "/opt/bh/bh_render_cpu", "--scene", "/tmp/scene.json", "--out", "/tmp/out/frame.png",
        "--width", "800", "--height", "600", "--mode", "legacy", "--integrator", "rk4",
        "--azimuth", "0.0", "--elevation", repr(math.pi / 2), "--radius-rs", "4.997",
        "--fov-y-deg", "60.0", "--frames", "1", "--supersample", "1",
    ], argv
    argv2 = rb.build_render_command(
        "bh_render_cpu", None, "x.bmp", 200, 150, "relativistic", "RK45",
        0.25, 1.0, 6.0, 45.0, frames=24, supersample=2, threads=8,
    )
    assert "--scene" not in argv2
    assert argv2[argv2.index("--mode") + 1] == "relativistic"
    assert argv2[argv2.index("--integrator") + 1] == "rk45"
    assert argv2[argv2.index("--frames") + 1] == "24"
    assert argv2[argv2.index("--threads") + 1] == "8"
    assert argv2[argv2.index("--supersample") + 1] == "2"
    for bad in (dict(mode="euler"), dict(integrator="rk2"), dict(frames=0)):
        kw = dict(renderer_path="r", scene_json_path=None, out_path="o.png", width=8, height=8,
                  mode="legacy", integrator="rk4", azimuth=0.0, elevation=1.0, radius_rs=5.0,
                  fov_y_deg=60.0, frames=1, supersample=1)
        kw.update(bad)
        try:
            rb.build_render_command(**kw)
        except ValueError:
            pass
        else:
            raise AssertionError(f"expected ValueError for {bad}")

    stdout = 'info: starting\n{"not": "it"}\n{"frames":1,"width":200,"height":150,' \
             '"shadow_fraction":0.12,"disk_fraction":0.3,"mode":"legacy"}\ndone\n'
    summ = rb.parse_render_summary(stdout)
    assert summ and summ["frames"] == 1 and summ["mode"] == "legacy" and summ["width"] == 200
    assert rb.parse_render_summary("nothing here") is None
    assert rb.parse_render_summary("") is None

    # Compare as Path objects: on Windows str(Path("/d/x")) is "\\d\\x".
    assert [Path(p) for p in rb.expected_frame_paths("/d/frame.png", 1)] == [Path("/d/frame.png")]
    seq = rb.expected_frame_paths("/d/frame.png", 3)
    assert [Path(p).name for p in seq] == ["frame_0000.png", "frame_0001.png", "frame_0002.png"]
    files = rb.find_sequence_files("/d/frame_0000.png", existing=["frame_0002.png", "frame_0000.png",
                                                                  "frame_0001.png", "other.png"])
    assert [Path(p).name for p in files] == ["frame_0000.png", "frame_0001.png", "frame_0002.png"]
    assert [Path(p) for p in rb.find_sequence_files("/d/single.png", existing=["single.png"])] == [Path("/d/single.png")]
    assert rb.sequence_first_number("/d/frame_0007.png") == 7
    w, h = rb.plane_size_for_fov(math.radians(60.0), 4 / 3, 10.0)
    _close(h, 2 * 10.0 * math.tan(math.radians(30.0)), what="plane h")
    _close(w / h, 4 / 3, what="plane aspect")


def check_anim(co) -> None:
    two_pi = 2.0 * math.pi
    args = dict(radius=5.0, azimuth=0.3, elevation=1.2, radius_end=12.0, elev_start=0.35,
                elev_end=math.pi - 0.35)
    r0, a0, e0 = co.camera_path_sample("TURNTABLE", 0.0, **args)
    r1, a1, e1 = co.camera_path_sample("TURNTABLE", 1.0, **args)
    assert (r0, a0, e0) == (5.0, 0.3, 1.2)
    _close(a1, 0.3 + two_pi, what="turntable end")
    assert (r1, e1) == (5.0, 1.2)

    r0, a0, e0 = co.camera_path_sample("ELEVATION_SWEEP", 0.0, **args)
    r1, a1, e1 = co.camera_path_sample("ELEVATION_SWEEP", 1.0, **args)
    assert (r0, a0, r1, a1) == (5.0, 0.3, 5.0, 0.3)
    _close(e0, 0.35, what="elev start")
    _close(e1, math.pi - 0.35, what="elev end")

    r0, a0, e0 = co.camera_path_sample("DOLLY", 0.0, **args)
    r1, a1, e1 = co.camera_path_sample("DOLLY", 1.0, **args)
    assert (r0, r1) == (5.0, 12.0) and (a0, e0, a1, e1) == (0.3, 1.2, 0.3, 1.2)
    _close(co.camera_path_sample("DOLLY", 0.5, **args)[0], 8.5, what="dolly mid")

    r0, a0, e0 = co.camera_path_sample("SPIRAL", 0.0, **args)
    r1, a1, e1 = co.camera_path_sample("SPIRAL", 1.0, **args)
    assert (r0, r1) == (5.0, 5.0)
    _close(a1 - a0, two_pi, what="spiral azimuth")
    _close(e0, 0.35, what="spiral e0")
    _close(e1, math.pi - 0.35, what="spiral e1")

    # Elevation clamp like Camera::position()
    assert co.camera_path_sample("ELEVATION_SWEEP", 0.0, 5.0, 0.0, 1.0, 5.0, -1.0, 4.0)[2] == 0.01
    try:
        co.camera_path_sample("ZIGZAG", 0.0, **args)
    except ValueError:
        pass
    else:
        raise AssertionError("unknown mode must raise")

    assert co.ease_t(0.0, "SINE") == 0.0 and co.ease_t(1.0, "SINE") == 1.0
    _close(co.ease_t(0.5, "SINE"), 0.5, what="sine mid")
    assert co.ease_t(0.25, "LINEAR") == 0.25
    fn = co.make_path_fn("TURNTABLE", "LINEAR", 5.0, 0.0, 1.0, 12.0, 0.35, 3.0)
    _close(fn(0.5)[1], math.pi, what="path fn")


def check_zips(root: Path) -> None:
    sys.path.insert(0, str(root / "blender/scripts"))
    import package_addon as pa

    addon_dir = root / pa.ADDON_REL
    files = pa.collect_addon_files(addon_dir)
    names = {p.name for p in files}
    assert "__init__.py" in names and "blender_manifest.toml" in names
    for mod in ("guides.py", "render_bridge.py", "camera_orbit.py", "json_io.py", "materials.py"):
        assert mod in names, mod
    expected = {f"{pa.PKG_NAME}/{p.relative_to(addon_dir).as_posix()}": p.read_bytes() for p in files}

    blobs = []
    for rel in pa.ZIP_OUTPUTS:
        zpath = root / rel
        assert zpath.is_file(), f"missing {rel} — run blender/scripts/package_addon.py"
        blobs.append(zpath.read_bytes())
        with zipfile.ZipFile(zpath) as zf:
            got = {i.filename: zf.read(i.filename) for i in zf.infolist()}
            assert set(got) == set(expected), f"{rel}: entries {sorted(set(got) ^ set(expected))}"
            for name, data in expected.items():
                assert got[name] == data, f"{rel}: stale {name}"
            for info in zf.infolist():
                assert info.date_time == pa.FIXED_DATE_TIME, f"{rel}: non-fixed timestamp {info.filename}"
    assert blobs[0] == blobs[1], "the two zips differ"
    assert blobs[0] == pa.build_zip_bytes(addon_dir), "zip not reproducible from folder"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    sys.path.insert(0, str(root / "blender/addons"))
    assert "bpy" not in sys.modules
    import black_hole_bridge
    import black_hole_bridge.constants as C
    from black_hole_bridge import camera_orbit, disk_mesh, grid_mesh, guides, json_io, render_bridge

    assert black_hole_bridge.bl_info["version"] == (0, 8, 0)
    manifest = (root / "blender/addons/black_hole_bridge/blender_manifest.toml").read_text("utf-8")
    assert re.search(r'^version\s*=\s*"0\.8\.0"', manifest, re.M), "manifest version"
    # Blender 4.2+ extension validation: tagline and permission reasons ≤ 64
    # chars, ending with an alphanumeric character or a closing bracket.
    for key in ("tagline", "files", "network", "clipboard", "camera", "microphone"):
        m = re.search(rf'^{key}\s*=\s*"([^"]*)"', manifest, re.M)
        if m:
            text = m.group(1)
            assert len(text) <= 64, f"manifest {key} longer than 64 chars ({len(text)})"
            assert text[-1].isalnum() or text[-1] in ")]}", f"manifest {key} must not end with punctuation"

    check_constants(root, C)
    check_formulas(C)
    check_blender_frame(C, grid_mesh, guides, disk_mesh)
    check_json(json_io, C)
    check_render_bridge(render_bridge)
    check_bridge_sync(render_bridge, C)
    check_seamless_paths(camera_orbit)
    check_anim(camera_orbit)
    check_zips(root)

    print("All Blender addon parity checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
