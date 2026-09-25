#!/usr/bin/env python3
"""Emit SHA-256 hashes of key repository sources for regression comparison.

Does not claim scientific validation. Useful to detect accidental drift in
baseline files relative to BASELINE_SHA256.txt or a previous run.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

# Default set aligned with docs/BASELINE.md / BASELINE_SHA256.txt interests.
DEFAULT_PATHS = [
    "black_hole.cpp",
    "geodesic.comp",
    "CMakeLists.txt",
    "2D_lensing.cpp",
    "CPU-geodesic.cpp",
    "ray_tracing.cpp",
    "README.md",
    "vcpkg.json",
    "grid.vert",
    "grid.frag",
    "tests/validate_source_invariants.py",
    # Scientific (hashed only if present — see skip in main via exists check)
    "shaders/geodesic_scientific.comp",
    "src/scientific/schwarzschild.cpp",
    "src/scientific/integrator_rk4.cpp",
]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Emit SHA-256 of key sources for regression comparison.",
        epilog="Example: python3 scripts/hash_tree.py --root .",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root (default: parent of scripts/)",
    )
    parser.add_argument(
        "--paths",
        nargs="*",
        default=None,
        help="Optional explicit relative paths (default: built-in key set)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="List paths that would be hashed without reading file contents",
    )
    parser.add_argument(
        "--check-baseline-file",
        type=Path,
        default=None,
        help="Optional BASELINE_SHA256.txt to print alongside (no fail)",
    )
    args = parser.parse_args()
    root = args.root.resolve()
    rels = args.paths if args.paths is not None else DEFAULT_PATHS

    if args.dry_run:
        for rel in rels:
            p = root / rel
            status = "exists" if p.is_file() else "MISSING"
            print(f"{status:8}  {rel}")
        return 0

    missing = 0
    optional_prefixes = ("shaders/geodesic_scientific", "src/scientific/")
    for rel in rels:
        p = root / rel
        if not p.is_file():
            if rel.startswith(optional_prefixes) or "/scientific/" in rel or rel.startswith("src/scientific"):
                print(f"{'skip':64}  {rel}  (optional, not present)")
                continue
            print(f"{'MISSING':64}  {rel}", file=sys.stderr)
            missing += 1
            continue
        digest = sha256_file(p)
        print(f"{digest}  {rel}")

    if args.check_baseline_file:
        base = args.check_baseline_file
        if not base.is_file():
            base = root / base
        if base.is_file():
            print("\n--- reference file ---", file=sys.stderr)
            print(base.read_text(encoding="utf-8"), end="", file=sys.stderr)
        else:
            print(f"baseline file not found: {args.check_baseline_file}", file=sys.stderr)

    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
