#!/usr/bin/env python3
"""Headless quickstart mirroring the C++ scientific helpers (no OpenGL, stdlib only).

Same formulas as include/black_hole/{schwarzschild,orbits,redshift,escape,weak_field}.hpp,
handy for quick checks or notebooks. Run: python3 examples/quickstart_scientific.py
"""

import math

G = 6.67430e-11
C = 299792458.0
M_SAGA = 8.54e36


def rs_si(mass_kg: float = M_SAGA) -> float:
    return 2.0 * G * mass_kg / (C * C)


def photon_sphere(rs: float) -> float:
    return 1.5 * rs


def isco(rs: float) -> float:
    return 3.0 * rs  # = 6 M with rs = 2M


def critical_impact(rs: float) -> float:
    return 1.5 * math.sqrt(3.0) * rs


def weak_field_alpha(rs: float, b: float) -> float:
    return 2.0 * rs / b


def weak_field_alpha2(rs: float, b: float) -> float:
    x = rs / b
    return 2.0 * x + 15.0 * math.pi / 16.0 * x * x


def orbital_speed(r: float, rs: float) -> float:
    """Speed of a circular orbit measured by a static observer: sqrt(M/(r-2M))."""
    m = 0.5 * rs
    return math.sqrt(m / (r - 2.0 * m))


def redshift_factor(r: float, rs: float, E: float, L_axis: float, spin: float = 1.0) -> float:
    """g = 1 / (u^t (1 + Ω L_axis / E)) for disk matter on a circular orbit."""
    m = 0.5 * rs
    ut = 1.0 / math.sqrt(1.0 - 3.0 * m / r)
    omega = spin * math.sqrt(m / r ** 3)
    return 1.0 / (ut * (1.0 + omega * L_axis / E))


def shadow_angle(rs: float, r_obs: float) -> float:
    f = 1.0 - rs / r_obs
    return math.asin(min(1.0, critical_impact(rs) * math.sqrt(f) / r_obs))


if __name__ == "__main__":
    rs = rs_si()
    print(f"rs        = {rs:.6e} m")
    print(f"r_photon  = {photon_sphere(rs):.6e} m")
    print(f"r_ISCO    = {isco(rs):.6e} m")
    print(f"b_c       = {critical_impact(rs):.6e} m ({critical_impact(1.0):.4f} rs)")
    print(f"alpha(b=100 rs) 1st/2nd order = {weak_field_alpha(rs, 100*rs):.6e} / {weak_field_alpha2(rs, 100*rs):.6e} rad")
    print(f"v(ISCO)   = {orbital_speed(3.0, 1.0):.3f} c")
    print(f"g(ISCO, transverse) = {redshift_factor(3.0, 1.0, 1.0, 0.0):.4f}")
    print(f"shadow at default camera (4.997 rs) = {math.degrees(shadow_angle(1.0, 4.997)):.2f} deg")
