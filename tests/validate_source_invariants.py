#!/usr/bin/env python3
"""Static regression guard for the legacy visual/physics baseline.

This test intentionally does not claim scientific correctness. It prevents
accidental edits to the historical constants and integration behavior while
safe engineering refactors are performed around the baseline.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def require(text: str, pattern: str, description: str) -> None:
    if re.search(pattern, text, re.MULTILINE | re.DOTALL) is None:
        raise AssertionError(f"Missing invariant: {description}\npattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()

    cpp = (root / "black_hole.cpp").read_text(encoding="utf-8")
    shader = (root / "geodesic.comp").read_text(encoding="utf-8")

    # Rendering and compute resolution baseline.
    require(cpp, r"int\s+WIDTH\s*=\s*800", "window width = 800")
    require(cpp, r"int\s+HEIGHT\s*=\s*600", "window height = 600")
    require(cpp, r"int\s+COMPUTE_WIDTH\s*=\s*200", "compute width = 200")
    require(cpp, r"int\s+COMPUTE_HEIGHT\s*=\s*150", "compute height = 150")
    require(cpp, r"glDrawArrays\(GL_TRIANGLE_STRIP,\s*0,\s*6\)", "legacy fullscreen topology")
    require(cpp, r"glBlendFunc\(GL_SRC_ALPHA,\s*GL_ONE_MINUS_SRC_ALPHA\)", "legacy blend function")

    # Legacy physical/visual constants must not drift unnoticed.
    require(shader, r"const\s+float\s+SagA_rs\s*=\s*1\.269e10", "legacy Schwarzschild radius constant")
    require(shader, r"const\s+float\s+D_LAMBDA\s*=\s*1e7", "legacy integration step")
    require(shader, r"const\s+double\s+ESCAPE_R\s*=\s*1e30", "legacy escape radius")
    require(shader, r"60000\s*:\s*60000", "legacy 60,000 step count")
    require(cpp, r"SagA\.r_s\s*\*\s*2\.2f", "legacy disk inner radius")
    require(cpp, r"SagA\.r_s\s*\*\s*5\.2f", "legacy disk outer radius")

    # The default baseline intentionally remains the historical single-stage
    # Euler integrator. Scientific corrections must be added as a separate mode.
    require(shader, r"void\s+legacyEulerStep\s*\(", "explicitly named legacy integrator")
    legacy_fn = re.search(
        r"void\s+legacyEulerStep\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        shader,
        re.DOTALL,
    )
    if not legacy_fn:
        raise AssertionError("Could not parse legacyEulerStep")
    rhs_calls = len(re.findall(r"\bgeodesicRHS\s*\(", legacy_fn.group("body")))
    if rhs_calls != 1:
        raise AssertionError(f"Legacy integrator changed: expected 1 RHS evaluation, found {rhs_calls}")

    # Engineering corrections expected in the upgraded package.
    require(
        cpp,
        r"GL_SHADER_IMAGE_ACCESS_BARRIER_BIT\s*\|\s*GL_TEXTURE_FETCH_BARRIER_BIT",
        "imageStore -> texture fetch synchronization",
    )
    require(shader, r"vec4\s+mass\[16\]", "std140-safe mass array stride")
    require(shader, r"int\s+moving", "std140-safe moving flag")

    print("All static baseline invariants passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
