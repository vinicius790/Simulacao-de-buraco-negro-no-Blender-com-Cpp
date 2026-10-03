#!/usr/bin/env python3
"""GPU ↔ CPU golden agreement for the scientific mode (needs OpenGL 4.3).

Runs
    BlackHole3D --scene S --scientific|--relativistic --capture gpu.png
    bh_render_cpu --scene S [--mode relativistic] --out cpu.png
for a few camera poses and compares the 200×150 images with tools/image_diff.py.

The GPU shader (float32) and the CPU reference (float64) implement the same
planar-RK4 algorithm, so images must agree to within 8-bit quantisation except
for rare boundary pixels.

Skips (exit 77) when no display / xvfb-run is available or the GL context
cannot be created (e.g. CI without Mesa). Mesa llvmpipe is fine.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SKIP = 77

POSES = [
    # (label, azimuth, elevation, radius_m, extra scene keys)
    ("az0_el125", 0.0, 1.25, 6.34194e10, {}),
    ("az07_el060_far", 0.7, 0.60, 8.0e10, {}),
    ("az21_el200_below", 2.1, 2.00, 6.34194e10, {}),
    # Scene FOV / target must reach the GPU too (not only bh_render_cpu).
    ("fov40_target", 0.3, 1.30, 9.0e10, {"camera": {"fov_y_deg": 40.0, "target_m": [0.0, 5.0e9, 0.0]}}),
    # Half the mass, default objects kept (as the Blender exporter writes it):
    # the black-hole marker must be skipped on both sides.
    ("half_mass", 0.0, 1.25, 6.34194e10, {"black_hole": {"mass_kg": 4.27e36, "r_s_m": 6.345e9}}),
    # "objects": [] must remove the default stars on the GPU too (az ≈ π puts
    # the default red/yellow stars behind the hole, where they would show).
    ("no_objects_az_pi", 3.14159265, 1.40, 2.2842e11, {"disk": {"inner_factor_rs": 3.0, "outer_factor_rs": 12.0}, "objects": []}),
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument("--bin-dir", type=Path, required=True)
    ap.add_argument("--max-bad-fraction", type=float, default=0.002)
    ap.add_argument("--max-mean-error", type=float, default=0.5)
    args = ap.parse_args()

    sys.path.insert(0, str(args.root / "tools"))
    import image_diff  # noqa: E402

    bh3d = args.bin_dir / "BlackHole3D"
    cpu = args.bin_dir / "bh_render_cpu"
    if not bh3d.exists() or not cpu.exists():
        print("SKIP: BlackHole3D / bh_render_cpu not built")
        return SKIP

    prefix: list[str] = []
    if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            print("SKIP: no display and no xvfb-run")
            return SKIP
        prefix = [xvfb, "-a", "-s", "-screen 0 1024x768x24"]

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        for label, az, el, radius, extra in POSES:
            scene = tmpd / f"{label}.json"
            doc = {
                "schema": "black_hole.scene_params/v1",
                "camera": {"radius_m": radius, "azimuth_rad": az, "elevation_rad": el, "fov_y_deg": 60.0},
            }
            for key, value in extra.items():
                if isinstance(value, dict):
                    doc.setdefault(key, {}).update(value)
                else:
                    doc[key] = value
            scene.write_text(json.dumps(doc))
            for mode_flag, cpu_mode in (("--scientific", "legacy"), ("--relativistic", "relativistic")):
                gpu_png = tmpd / f"{label}{mode_flag}_gpu.png"
                cpu_png = tmpd / f"{label}{mode_flag}_cpu.png"
                r = subprocess.run(prefix + [str(bh3d), "--scene", str(scene), mode_flag, "--capture", str(gpu_png)],
                                   cwd=args.bin_dir, capture_output=True, text=True, timeout=600)
                if r.returncode != 0 or not gpu_png.exists():
                    print(r.stdout, r.stderr)
                    if "GLFW" in r.stderr or "GLEW" in r.stderr or "window" in r.stderr.lower():
                        print("SKIP: OpenGL 4.3 context unavailable")
                        return SKIP
                    print(f"FAIL: GPU capture failed for {label} {mode_flag}")
                    failures += 1
                    continue
                subprocess.run([str(cpu), "--scene", str(scene), "--mode", cpu_mode, "--out", str(cpu_png), "--quiet"],
                               check=True, capture_output=True, timeout=600)
                rep = image_diff.compare(gpu_png, cpu_png, pixel_tol=24, heatmap=None)
                ok = rep["bad_fraction"] <= args.max_bad_fraction and rep["mean_abs_error"] <= args.max_mean_error
                print(("PASS" if ok else "FAIL") + f": {label} {mode_flag} " + json.dumps(rep))
                failures += 0 if ok else 1

    if failures:
        print(f"{failures} GPU/CPU agreement failure(s)")
        return 1
    print("GPU and CPU scientific renders agree.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
