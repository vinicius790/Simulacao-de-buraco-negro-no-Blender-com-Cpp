#!/usr/bin/env python3
"""Byte-compile every Blender addon module and helper script (no bpy needed).

Usage: python3 tests/test_blender_addon_compile.py [--root REPO]
Exit code is non-zero when any file fails to compile.
"""

from __future__ import annotations

import argparse
import py_compile
import sys
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    sources = sorted(
        p
        for folder in (root / "blender/addons/black_hole_bridge", root / "blender/scripts")
        for p in folder.rglob("*.py")
        if "__pycache__" not in p.parts
    )
    if not sources:
        print("ERROR: no Python sources found under blender/", file=sys.stderr)
        return 2

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i, src in enumerate(sources):
            cfile = Path(tmp) / f"{i}.pyc"
            try:
                py_compile.compile(str(src), cfile=str(cfile), doraise=True)
            except py_compile.PyCompileError as exc:
                failures += 1
                print(f"FAIL {src.relative_to(root)}\n{exc.msg}", file=sys.stderr)
    print(f"Compiled {len(sources)} files, {failures} failure(s).")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
