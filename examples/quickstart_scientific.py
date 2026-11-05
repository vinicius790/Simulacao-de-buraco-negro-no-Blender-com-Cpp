#!/usr/bin/env python3
"""Headless quickstart mirroring the C++ scientific helpers and relativistic utilities (no OpenGL)."""

from __future__ import annotations

import math
import sys
from pathlib import Path

# Add bridge path to import relativistic utilities if available
_bridge_dir = Path(__file__).resolve().parents[1] / "blender" / "addons"
if str(_bridge_dir) not in sys.path:
    sys.path.insert(0, str(_bridge_dir))

try:
    from black_hole_bridge.relativistic import (
        schwarzschild_radius,
        light_deflection_angle,
        time_dilation_factor,
        G,
        C,
    )
except (ImportError, ValueError):
    G = 6.67430e-11
    C = 299792458.0

    def schwarzschild_radius(mass_kg: float) -> float:
        return 2.0 * G * float(mass_kg) / (C * C) if mass_kg > 0.0 else 0.0

    def light_deflection_angle(mass_kg: float, impact_m: float) -> float:
        if mass_kg <= 0.0 or impact_m <= 0.0:
            return 0.0
        return 2.0 * schwarzschild_radius(mass_kg) / float(impact_m)

    def time_dilation_factor(mass_kg: float, radius_m: float) -> float:
        if mass_kg <= 0.0 or radius_m <= 0.0:
            return 0.0
        rs = schwarzschild_radius(mass_kg)
        return math.sqrt(1.0 - rs / float(radius_m)) if radius_m > rs else 0.0

M_SAGA = 8.54e36


def rs_si(mass_kg: float = M_SAGA) -> float:
    return schwarzschild_radius(mass_kg)


def photon_sphere(rs: float) -> float:
    return 1.5 * rs


def isco(rs: float) -> float:
    return 3.0 * rs  # = 6 M with rs = 2M


def weak_field_alpha(rs: float, b: float) -> float:
    return 2.0 * rs / b


if __name__ == "__main__":
    rs = rs_si()
    b = 100.0 * rs
    alpha = light_deflection_angle(M_SAGA, b)
    gamma_isco = time_dilation_factor(M_SAGA, isco(rs))

    print(f"rs = {rs:.6e} m")
    print(f"r_photon = {photon_sphere(rs):.6e} m")
    print(f"r_ISCO = {isco(rs):.6e} m")
    print(f"alpha(b=100 rs) = {alpha:.6e} rad")
    print(f"time dilation at ISCO (r=3 rs) = {gamma_isco:.6f}")
