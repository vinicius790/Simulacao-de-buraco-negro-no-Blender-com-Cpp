#!/usr/bin/env python3
"""Shader contract: std140 layouts and bindings must agree between C++ and GLSL.

Checks
  * geodesic.comp (legacy) and shaders/geodesic.comp mirror are byte-identical;
  * grid.vert / grid.frag mirrors are byte-identical;
  * every UBO binding used by black_hole.cpp exists in the shader it loads;
  * SciParams (binding 4) field order/count matches SciParamsUBOData in C++;
  * geodesic_scientific.comp keeps the honesty markers (Kerr NOT implemented)
    and the corrected Christoffel factor r*f;
  * the legacy shader still carries NO scientific code (no SciParams, no binding 4).
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def fail(msg: str) -> None:
    raise AssertionError(msg)


def require(text: str, pattern: str, desc: str) -> None:
    if re.search(pattern, text, re.MULTILINE | re.DOTALL) is None:
        fail(f"Missing shader contract: {desc}\npattern: {pattern}")


def forbid(text: str, pattern: str, desc: str) -> None:
    if re.search(pattern, text, re.MULTILINE | re.DOTALL) is not None:
        fail(f"Forbidden in shader: {desc}\npattern: {pattern}")


def strip_comments(glsl: str) -> str:
    glsl = re.sub(r"/\*.*?\*/", "", glsl, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", glsl)


def block_fields(glsl: str, block: str) -> list[str]:
    m = re.search(rf"uniform\s+{block}\s*\{{(?P<body>.*?)\}}", glsl, re.DOTALL)
    if not m:
        fail(f"uniform block {block} not found")
    body = re.sub(r"//[^\n]*", "", m.group("body"))
    return [f.strip() for f in body.split(";") if f.strip()]


def cpp_struct_fields(cpp: str, struct: str) -> list[str]:
    m = re.search(rf"struct\s+alignas\(16\)\s+{struct}\s*\{{(?P<body>.*?)\}};", cpp, re.DOTALL)
    if not m:
        fail(f"C++ struct {struct} not found")
    body = re.sub(r"//[^\n]*", "", m.group("body"))
    return [f.strip() for f in body.split(";") if f.strip()]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = ap.parse_args().root.resolve()

    legacy = (root / "geodesic.comp").read_text(encoding="utf-8")
    sci = (root / "shaders/geodesic_scientific.comp").read_text(encoding="utf-8")
    cpp = (root / "black_hole.cpp").read_text(encoding="utf-8")

    for name in ("geodesic.comp", "grid.vert", "grid.frag"):
        if (root / name).read_bytes() != (root / "shaders" / name).read_bytes():
            fail(f"shaders/{name} mirror diverged from root canonical copy")

    for shader, label in ((legacy, "geodesic.comp"), (sci, "geodesic_scientific.comp")):
        for binding in (1, 2, 3):
            require(shader, rf"binding\s*=\s*{binding}\s*\)\s*uniform", f"{label} binding {binding}")
        require(shader, r"binding\s*=\s*0\s*,\s*rgba8\)", f"{label} output image binding 0 rgba8")
        require(shader, r"local_size_x\s*=\s*16\s*,\s*local_size_y\s*=\s*16", f"{label} 16x16 workgroup")

    forbid(strip_comments(legacy), r"SciParams", "legacy shader must not contain scientific UBO")
    forbid(strip_comments(legacy), r"binding\s*=\s*4", "legacy shader must not use binding 4")

    require(sci, r"binding\s*=\s*4\s*\)\s*uniform\s+SciParams", "scientific SciParams at binding 4")
    require(cpp, r"glBindBufferBase\(GL_UNIFORM_BUFFER,\s*4,\s*sciUBO\)", "C++ binds SciParams to 4")
    require(cpp, r'"geodesic_scientific\.comp"', "C++ can load the scientific shader")
    require(cpp, r'"geodesic\.comp"', "C++ still loads the legacy shader by default")

    glsl_fields = block_fields(sci, "SciParams")
    cpp_fields = cpp_struct_fields(cpp, "SciParamsUBOData")
    if len(glsl_fields) != len(cpp_fields):
        fail(f"SciParams field count GLSL {len(glsl_fields)} != C++ {len(cpp_fields)}")
    type_map = {"int": "std::int32_t", "float": "float"}
    for g, c in zip(glsl_fields, cpp_fields):
        gt, gn = g.split()[:2]
        ct, cn = c.rsplit(None, 1)
        if type_map.get(gt) != ct.strip():
            fail(f"SciParams type mismatch: GLSL '{g}' vs C++ '{c}'")
        if gn != cn:
            fail(f"SciParams name/order mismatch: GLSL '{gn}' vs C++ '{cn}'")
    require(cpp, r"sizeof\(SciParamsUBOData\)\s*==\s*32", "SciParams static_assert size 32")

    require(sci, r"Kerr / spin: NOT implemented", "scientific shader honesty marker")
    require(sci, r"r\s*\*\s*f\s*\*\s*dphi\s*\*\s*dphi", "corrected Christoffel term r*f*dphi^2")
    require(sci, r"void\s+rk4\s*\(", "scientific RK4 integrator")
    forbid(strip_comments(sci), r"legacyEulerStep", "scientific shader must not reuse the legacy integrator")

    print("All shader-contract assertions passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
