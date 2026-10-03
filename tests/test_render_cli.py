#!/usr/bin/env python3
"""End-to-end contract of the bh_render_cpu command line (every flag).

* each documented flag is accepted and changes what it should;
* invalid values exit 2 with a message naming the flag (no silent failure,
  no undefined behaviour on nan / huge integers);
* the JSON summary is valid JSON even for paths with quotes / backslashes;
* sequences: naming stem_NNNN.ext (also with dots in directory names), and
  --elevation-end / --azimuth-turns reproduce single-frame renders exactly at
  the sweep endpoints;
* --help lists every flag the parser accepts (docs/CLI drift guard).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

FLAGS = ["--scene", "--out", "--width", "--height", "--mode", "--integrator", "--azimuth", "--elevation",
         "--radius-rs", "--fov-y-deg", "--frames", "--azimuth-turns", "--elevation-end", "--threads",
         "--supersample", "--exposure", "--gamma", "--mdot-edd", "--spin-sign", "--stars", "--max-steps",
         "--quiet", "--help"]


def run(binary: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run([str(binary), *args], capture_output=True, text=True, timeout=300)


def summary(cp: subprocess.CompletedProcess) -> dict:
    assert cp.returncode == 0, cp.stderr
    return json.loads(cp.stdout.strip().splitlines()[-1])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument("--bin", type=Path, required=True)
    args = ap.parse_args()
    sys.path.insert(0, str(args.root / "tools"))
    import image_diff

    b = args.bin
    src = (args.root / "tools/bh_render_cpu.cpp").read_text(encoding="utf-8")
    parsed = set(re.findall(r'a == "(--[a-z0-9-]+)"', src))
    help_text = run(b, "--help").stdout
    for flag in FLAGS:
        assert flag in parsed, f"parser does not handle {flag}"
        assert flag in help_text or flag == "--help", f"--help does not list {flag}"
    assert parsed <= set(FLAGS + ["-h"]), f"undocumented flags in parser: {parsed - set(FLAGS)}"
    print("PASS: every flag parsed and listed in --help")

    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        small = ["--width", "24", "--height", "18", "--quiet"]

        # Bad values: rc 2 + message naming the flag; nothing written.
        for flag, value in [("--width", "12abc"), ("--width", "nan"), ("--width", "0"), ("--height", "99999"),
                            ("--frames", "3e9"), ("--frames", "2.5"), ("--supersample", "50"),
                            ("--threads", "-1"), ("--exposure", "x"), ("--gamma", "0"), ("--mdot-edd", "-1"),
                            ("--radius-rs", "-3"), ("--fov-y-deg", "180"), ("--elevation", "inf"),
                            ("--spin-sign", "0.5"), ("--mode", "neon"), ("--integrator", "euler"),
                            ("--max-steps", "0")]:
            out = t / "bad.png"
            cp = run(b, "--out", str(out), flag, value, "--quiet")
            assert cp.returncode == 2, f"{flag} {value}: rc {cp.returncode}"
            assert flag.lstrip("-").split("-")[0] in cp.stderr, f"{flag} {value}: message {cp.stderr!r}"
            assert not out.exists(), f"{flag} {value}: wrote output"
        assert run(b, "--width", "8").returncode == 2, "--out is required"
        assert run(b, "--out", str(t / "x.png"), "--bogus").returncode == 2, "unknown flag"
        assert run(b, "--out", str(t / "x.gif"), *small).returncode == 1, "unsupported extension"
        print("PASS: invalid values rejected with messages")

        # JSON summary with hostile path characters.
        # Quotes and backslashes in the path (POSIX). Windows forbids '"' in names
        # but every absolute Windows path already contains backslashes.
        weird = t / ("we ird dír" if os.name == "nt" else 'we"ird\\dir')
        weird.mkdir()
        s = summary(run(b, "--out", str(weird / "a.png"), *small))
        assert s["out"] == str(weird / "a.png") and s["frames"] == 1
        for key in ("width", "height", "mode", "integrator", "shadow_fraction", "disk_fraction", "object_fraction",
                    "escaped_fraction", "step_limit_rays", "mean_steps", "min_g", "max_g", "seconds"):
            assert key in s, key
        print("PASS: JSON summary valid with quotes/backslashes in --out")

        # Formats.
        for ext in ("png", "bmp", "ppm"):
            out = t / f"f.{ext}"
            summary(run(b, "--out", str(out), *small))
            w, h, _ = image_diff.read_image(out)
            assert (w, h) == (24, 18), ext
        print("PASS: png/bmp/ppm outputs readable")

        # Sequences: naming with dotted directories and sweep endpoints.
        seq_dir = t / "d.v2"
        seq_dir.mkdir()
        s = summary(run(b, "--out", str(seq_dir / "seq.png"), "--frames", "3", "--elevation", "1.1",
                        "--elevation-end", "1.4", "--azimuth-turns", "0", *small))
        names = sorted(p.name for p in seq_dir.iterdir())
        assert names == ["seq_0000.png", "seq_0001.png", "seq_0002.png"], names
        for frame, el in ((0, "1.1"), (2, "1.4")):
            single = t / f"single_{frame}.png"
            summary(run(b, "--out", str(single), "--elevation", el, *small))
            rep = image_diff.compare(seq_dir / f"seq_{frame:04d}.png", single, 0, None)
            assert rep["max_error"] == 0, f"sweep endpoint {frame} != single render at elevation {el}"
        # Default azimuth sweep: frame k at az0 + 2πk/N.
        summary(run(b, "--out", str(t / "turn.png"), "--frames", "4", "--elevation", "1.2", *small))
        summary(run(b, "--out", str(t / "q.png"), "--elevation", "1.2", "--azimuth", repr(3.141592653589793 / 2), *small))
        rep = image_diff.compare(t / "turn_0001.png", t / "q.png", 0, None)
        assert rep["max_error"] == 0, "frame 1 of 4 is azimuth + π/2"
        print("PASS: sequence naming and sweep endpoints exact")

        # Flags that must change the image.
        base = t / "base.png"
        summary(run(b, "--out", str(base), "--elevation", "1.2", *small))
        # Stars need many escaping rays: use the showcase scene (≈35 % escape).
        show = args.root / "examples/scene_relativistic_showcase.json"
        a, c = t / "nostars.png", t / "stars.png"
        summary(run(b, "--scene", str(show), "--out", str(a), "--width", "160", "--height", "90", "--quiet"))
        summary(run(b, "--scene", str(show), "--out", str(c), "--width", "160", "--height", "90", "--stars", "--quiet"))
        assert image_diff.compare(a, c, 0, None)["max_error"] > 0, "--stars had no effect"
        for extra in (["--mode", "relativistic"], ["--mode", "blackbody"], ["--fov-y-deg", "40"],
                      ["--radius-rs", "8"], ["--gamma", "2.2"], ["--supersample", "2"], ["--spin-sign", "-1", "--mode", "relativistic"]):
            other = t / "other.png"
            summary(run(b, "--out", str(other), "--elevation", "1.2", *extra, *small))
            rep = image_diff.compare(base, other, 0, None)
            assert rep["max_error"] > 0, f"{extra} had no effect"
        # Flags that must NOT change the image (determinism).
        for extra in (["--threads", "1"], ["--threads", "3"]):
            other = t / "other.png"
            summary(run(b, "--out", str(other), "--elevation", "1.2", *extra, *small))
            assert image_diff.compare(base, other, 0, None)["max_error"] == 0, f"{extra} changed the image"
        sc = t / "scene.json"
        sc.write_text(json.dumps({"schema": "black_hole.scene_params/v1",
                                  "camera": {"radius_m": 6.34194e10, "elevation_rad": 1.2}}))
        other = t / "other.png"
        summary(run(b, "--scene", str(sc), "--out", str(other), *small))
        assert image_diff.compare(base, other, 0, None)["max_error"] == 0, "--scene camera == flags"
        rk45 = summary(run(b, "--out", str(other), "--elevation", "1.2", "--integrator", "rk45", *small))
        assert rk45["integrator"] == "rk45"
        limited = summary(run(b, "--out", str(other), "--elevation", "1.2", "--max-steps", "3", *small))
        assert limited["step_limit_rays"] > 0, "--max-steps honoured"
        print("PASS: flags change (or keep) the image as documented")

    print("All bh_render_cpu CLI checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
