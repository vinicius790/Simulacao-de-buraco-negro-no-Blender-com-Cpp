#!/usr/bin/env python3
"""Deterministic zip builder for the Black Hole Bridge addon.

Writes two byte-identical archives:

    blender/addons/black_hole_bridge.zip
    blender/black_hole_bridge.zip

Each contains ``black_hole_bridge/<file>`` for every ``*.py`` plus
``blender_manifest.toml`` in the addon folder (sorted, ``__pycache__``
excluded) with fixed 1980-01-01 timestamps, so re-running the script on an
unchanged tree yields the same bytes (checked by tests/test_blender_addon_parity.py).

Usage:
    python3 blender/scripts/package_addon.py [--root REPO] [--check]
"""

from __future__ import annotations

import argparse
import hashlib
import io
import sys
import zipfile
from pathlib import Path
from typing import List

PKG_NAME = "black_hole_bridge"
ADDON_REL = Path("blender/addons") / PKG_NAME
ZIP_OUTPUTS = (Path("blender/addons") / f"{PKG_NAME}.zip", Path("blender") / f"{PKG_NAME}.zip")
FIXED_DATE_TIME = (1980, 1, 1, 0, 0, 0)
MANIFEST = "blender_manifest.toml"


def collect_addon_files(addon_dir: Path) -> List[Path]:
    """Sorted ``*.py`` + manifest inside ``addon_dir`` (no ``__pycache__``)."""
    files = [
        p
        for p in addon_dir.rglob("*")
        if p.is_file()
        and "__pycache__" not in p.parts
        and (p.suffix == ".py" or p.name == MANIFEST)
    ]
    return sorted(files, key=lambda p: p.relative_to(addon_dir).as_posix())


def build_zip_bytes(addon_dir: Path) -> bytes:
    """Return the deterministic archive bytes for ``addon_dir``."""
    buf = io.BytesIO()
    # ZIP_STORED: DEFLATE output differs between zlib builds (e.g. zlib-ng), so
    # stored entries are the only byte-reproducible choice across platforms.
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_STORED) as zf:
        for path in collect_addon_files(addon_dir):
            arcname = f"{PKG_NAME}/{path.relative_to(addon_dir).as_posix()}"
            info = zipfile.ZipInfo(arcname, date_time=FIXED_DATE_TIME)
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 3  # Unix, regardless of host OS
            info.external_attr = 0o644 << 16
            zf.writestr(info, path.read_bytes())
    return buf.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--check", action="store_true", help="verify zips instead of writing")
    args = parser.parse_args()
    root = args.root.resolve()
    addon_dir = root / ADDON_REL
    if not (addon_dir / "__init__.py").is_file():
        print(f"ERROR: addon folder not found: {addon_dir}", file=sys.stderr)
        return 2

    data = build_zip_bytes(addon_dir)
    digest = hashlib.sha256(data).hexdigest()
    status = 0
    for rel in ZIP_OUTPUTS:
        out = root / rel
        if args.check:
            ok = out.is_file() and out.read_bytes() == data
            print(f"{'OK ' if ok else 'STALE'} {rel}")
            status |= 0 if ok else 1
        else:
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(data)
            print(f"Wrote {rel} ({len(data)} bytes)")
    print(f"sha256 {digest}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
