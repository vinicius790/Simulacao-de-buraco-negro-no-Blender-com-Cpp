#!/usr/bin/env python3
"""Golden-image guard for the historical baseline (BlackHole3D with no flags).

The default render path must stay byte-identical to the original package.
This test captures the first frame with `BlackHole3D --capture` and compares it
with docs/images/legacy_default_gpu.png:

* same renderer + GL version as recorded in legacy_default_gpu.json → EXACT
  equality (pixel tolerance 0). This is what caught a 1-ulp tanf change.
* any other driver → small tolerance (float32 arithmetic differs between GL
  implementations), still catching real regressions.

Also checks that `--scene examples/scene_params_example.json` (the documented
defaults) produces exactly the same frame as no flags.

Exit 77 (CTest SKIP) when no OpenGL 4.3 context / display is available.
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


def capture(prefix: list[str], bh3d: Path, out: Path, extra: list[str]) -> tuple[int, str]:
    r = subprocess.run(prefix + [str(bh3d), *extra, "--capture", str(out)], cwd=bh3d.parent,
                       capture_output=True, text=True, timeout=900)
    return r.returncode, r.stdout + r.stderr


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument("--bin-dir", type=Path, required=True)
    args = ap.parse_args()
    args.bin_dir = args.bin_dir.resolve()  # subprocesses run with cwd=bin_dir
    args.root = args.root.resolve()
    sys.path.insert(0, str(args.root / "tools"))
    import image_diff

    bh3d = args.bin_dir / "BlackHole3D"
    if not bh3d.exists():
        print("SKIP: BlackHole3D not built")
        return SKIP
    prefix: list[str] = []
    if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            print("SKIP: no display and no xvfb-run")
            return SKIP
        prefix = [xvfb, "-a", "-s", "-screen 0 1024x768x24"]

    meta = json.loads((args.root / "docs/images/legacy_default_gpu.json").read_text(encoding="utf-8"))
    golden = args.root / "docs/images" / meta["image"]
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "legacy.png"
        rc, log = capture(prefix, bh3d, out, [])
        if rc != 0 or not out.exists():
            print(log)
            if "GLFW" in log or "GLEW" in log or "window" in log.lower():
                print("SKIP: OpenGL 4.3 context unavailable")
                return SKIP
            print("FAIL: capture failed")
            return 1
        renderer = next((l.split(" ", 1)[1] for l in log.splitlines() if l.startswith("Renderer ")), "")
        version = next((l.split(" ", 1)[1] for l in log.splitlines() if l.startswith("OpenGL ")), "")
        same_driver = renderer == meta["renderer"] and version == meta["gl_version"]
        rep = image_diff.compare(out, golden, 0, None)
        if same_driver:
            ok = rep["max_error"] == 0
            print(("PASS" if ok else "FAIL") + f": exact golden on {renderer} / {version}: {json.dumps(rep)}")
        else:
            rep = image_diff.compare(out, golden, 8, None)
            ok = rep["bad_fraction"] <= 0.01
            print(("PASS" if ok else "FAIL") + f": tolerant golden on {renderer!r} (recorded {meta['renderer']!r}): {json.dumps(rep)}")
        if not ok:
            return 1

        out2 = Path(tmp) / "legacy_scene.png"
        rc, log = capture(prefix, bh3d, out2, ["--scene", str(args.root / "examples/scene_params_example.json")])
        rep2 = image_diff.compare(out, out2, 0, None)
        if rc != 0 or rep2["max_error"] != 0:
            print(log)
            print(f"FAIL: --scene with the documented defaults differs from no flags: {json.dumps(rep2)}")
            return 1
        print("PASS: --scene examples/scene_params_example.json == no flags (exact)")
    print("Legacy baseline golden check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
