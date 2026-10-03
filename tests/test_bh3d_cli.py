#!/usr/bin/env python3
"""BlackHole3D command-line contract.

Part 1 (no display needed): argument errors exit 2 with a message BEFORE any
window / GL context is created; --help exits 0 wherever it appears.
Part 2 (needs OpenGL 4.3, xvfb-run/Mesa is fine): runtime scientific flags
really change the captured image; the default stays the legacy baseline.
Exit 77 (CTest SKIP) for part 2 only when no GL context is available.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SKIP = 77


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

    def run(*a: str, prefix: list[str] | None = None, timeout: int = 60) -> subprocess.CompletedProcess:
        env = dict(os.environ)
        if prefix is None:
            env.pop("DISPLAY", None)  # prove the error path never touches the display
            env.pop("WAYLAND_DISPLAY", None)
        return subprocess.run((prefix or []) + [str(bh3d), *a], cwd=args.bin_dir, capture_output=True,
                              text=True, timeout=timeout, env=env)

    # ---- Part 1: parse-time contract (no display) ----
    for where in (["--help"], ["--scientific", "--help"], ["--capture", "x.png", "-h"]):
        r = run(*where)
        assert r.returncode == 0 and "--blackbody" in r.stdout, f"--help {where}: rc {r.returncode}"
    for bad, needle in ((["--exposure", "x"], "--exposure"), (["--spin-sign", "0.5"], "--spin-sign"),
                        (["--max-steps", "2.5"], "--max-steps"), (["--mdot-edd", "-1"], "--mdot-edd"),
                        (["--capture", "out.gif"], "extension"), (["--capture"], "missing value"),
                        (["--scene"], "missing value"), (["--bogus"], "unknown argument"),
                        (["--scene", "/nonexistent.json"], "scene load failed")):
        r = run(*bad)
        assert r.returncode == 2, f"{bad}: rc {r.returncode}"
        assert needle in r.stderr, f"{bad}: stderr {r.stderr!r}"
        assert "GLFW" not in r.stderr and "OpenGL" not in r.stdout, f"{bad}: GL initialised before failing"
    print("PASS: argument errors exit 2 before any GL work; --help exits 0 anywhere")

    # ---- Part 2: runtime flags change the image (needs GL) ----
    prefix: list[str] = []
    if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            print("SKIP (part 2): no display and no xvfb-run")
            return SKIP
        prefix = [xvfb, "-a", "-s", "-screen 0 1024x768x24"]
    scene = args.root / "examples/scene_relativistic_showcase.json"
    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)

        def cap(name: str, *flags: str) -> Path:
            out = t / name
            r = run("--scene", str(scene), *flags, "--capture", str(out), prefix=prefix, timeout=600)
            if r.returncode != 0:
                print(r.stdout, r.stderr)
                if "GLFW" in r.stderr or "GLEW" in r.stderr:
                    raise SystemExit(SKIP)
                raise AssertionError(f"capture failed for {flags}")
            return out

        base = cap("rel.png", "--relativistic")
        for name, flags in (("spin.png", ("--relativistic", "--spin-sign", "-1")),
                            ("expo.png", ("--relativistic", "--exposure", "6")),
                            ("bb.png", ("--blackbody",)),
                            ("bbmdot.png", ("--blackbody", "--mdot-edd", "0.5")),
                            ("steps.png", ("--relativistic", "--max-steps", "5"))):
            other = cap(name, *flags)
            assert image_diff.compare(base, other, 0, None)["max_error"] > 0, f"{flags} had no effect"
        # Reversing the spin mirrors the Doppler pattern: the bright side moves.
        w, h, a = image_diff.read_image(base)
        _, _, b = image_diff.read_image(t / "spin.png")
        def side(px, x0, x1):
            return sum(px[(y * w + x) * 3 + c] for y in range(h) for x in range(x0, x1) for c in range(3))
        assert (side(a, 0, w // 2) > side(a, w // 2, w)) != (side(b, 0, w // 2) > side(b, w // 2, w)), \
            "--spin-sign -1 must swap the Doppler-bright side"
    print("PASS: --spin-sign / --exposure / --blackbody / --mdot-edd / --max-steps change the GPU image")
    print("All BlackHole3D CLI checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
