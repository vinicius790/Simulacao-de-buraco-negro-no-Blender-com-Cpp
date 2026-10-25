#!/usr/bin/env python3
"""Headless quickstart mirroring the C++ scientific helpers (no OpenGL)."""

G = 6.67430e-11
C = 299792458.0
M_SAGA = 8.54e36

def rs_si(mass_kg: float = M_SAGA) -> float:
    return 2.0 * G * mass_kg / (C * C)

def photon_sphere(rs: float) -> float:
    return 1.5 * rs

def isco(rs: float) -> float:
    return 3.0 * rs  # = 6 M with rs = 2M

def weak_field_alpha(rs: float, b: float) -> float:
    return 2.0 * rs / b

if __name__ == "__main__":
    rs = rs_si()
    print(f"rs = {rs:.6e} m")
    print(f"r_photon = {photon_sphere(rs):.6e} m")
    print(f"r_ISCO = {isco(rs):.6e} m")
    print(f"alpha(b=100 rs) = {weak_field_alpha(rs, 100*rs):.6e} rad")
