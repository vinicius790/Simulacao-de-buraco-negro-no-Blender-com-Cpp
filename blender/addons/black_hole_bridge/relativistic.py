"""Relativistic calculations bridge for the black_hole simulator.

Exposes Schwarzschild radius, light deflection angle, and time dilation factor.
Attempts to load a compiled C++ shared library (bh_scientific) via ctypes,
with seamless fallback to pure-Python implementations matching
include/black_hole/relativistic.hpp.
"""

from __future__ import annotations

import ctypes
import math
import os
import sys
from pathlib import Path
from typing import Any, Optional, Tuple

# Physical constants matching include/black_hole/relativistic.hpp
G: float = 6.67430e-11   # Gravitational constant (m^3 kg^-1 s^-2)
C: float = 299792458.0   # Speed of light in vacuum (m s^-1)


# ---------------------------------------------------------------------------
# Pure-Python reference implementations
# ---------------------------------------------------------------------------

def _py_schwarzschild_radius(mass_kg: float) -> float:
    """Compute Schwarzschild radius rs = 2GM/c^2 in metres."""
    if mass_kg <= 0.0:
        return 0.0
    return 2.0 * G * float(mass_kg) / (C * C)


def _py_light_deflection_angle(mass_kg: float, impact_m: float) -> float:
    """Deflection angle alpha = 4GM/(c^2 b) = 2*rs/b in radians (small-angle approx)."""
    if mass_kg <= 0.0 or impact_m <= 0.0:
        return 0.0
    return 4.0 * G * float(mass_kg) / (C * C * float(impact_m))


def _py_time_dilation_factor(mass_kg: float, radius_m: float) -> float:
    """Time dilation factor gamma = sqrt(1 - rs/r) for stationary observer at radius_m."""
    if mass_kg <= 0.0 or radius_m <= 0.0:
        return 0.0
    rs = _py_schwarzschild_radius(mass_kg)
    if radius_m <= rs:
        return 0.0
    return math.sqrt(1.0 - rs / float(radius_m))


# ---------------------------------------------------------------------------
# C-library loader via ctypes
# ---------------------------------------------------------------------------

_cpp_lib: Optional[ctypes.CDLL] = None
_backend_name: str = "python"


def _candidate_library_paths() -> list[Path]:
    """Search for shared library binaries in repo and build folders."""
    here = Path(__file__).resolve().parent
    repo_root = here.parents[2]  # blender/addons/black_hole_bridge -> repo_root

    lib_names = [
        "bh_scientific.dll",
        "black_hole_bridge.dll",
        "libbh_scientific.dll",
        "libbh_scientific.so",
        "bh_scientific.so",
        "libbh_scientific.dylib",
        "bh_scientific.dylib",
    ]

    search_dirs = [
        here,
        here / "bin",
        repo_root / "build",
        repo_root / "build" / "Release",
        repo_root / "build" / "Debug",
        repo_root / "build" / "RelWithDebInfo",
        repo_root / "cmake-build-release",
        repo_root / "cmake-build-debug",
        repo_root / "bin",
    ]

    # Environment variable override if specified
    env_path = os.environ.get("BH_SCIENTIFIC_LIB")
    candidates: list[Path] = []
    if env_path:
        candidates.append(Path(env_path))

    for d in search_dirs:
        for name in lib_names:
            candidates.append(d / name)

    return candidates


def _init_ctypes_bridge() -> Tuple[Optional[ctypes.CDLL], str]:
    """Attempt loading C++ library with exports."""
    for path in _candidate_library_paths():
        if path.is_file():
            try:
                lib = ctypes.CDLL(str(path))
                # Check for exported functions
                if hasattr(lib, "bh_schwarzschild_radius") and hasattr(lib, "bh_light_deflection_angle"):
                    lib.bh_schwarzschild_radius.argtypes = [ctypes.c_double]
                    lib.bh_schwarzschild_radius.restype = ctypes.c_double
                    lib.bh_light_deflection_angle.argtypes = [ctypes.c_double, ctypes.c_double]
                    lib.bh_light_deflection_angle.restype = ctypes.c_double
                    lib.bh_time_dilation_factor.argtypes = [ctypes.c_double, ctypes.c_double]
                    lib.bh_time_dilation_factor.restype = ctypes.c_double
                    return lib, f"ctypes ({path.name})"
            except Exception:
                continue
    return None, "python (pure reference)"


_cpp_lib, _backend_name = _init_ctypes_bridge()


def get_backend() -> str:
    """Return description of the active calculation backend ('ctypes (...)' or 'python (...)')."""
    return _backend_name


# ---------------------------------------------------------------------------
# Public Relativistic API
# ---------------------------------------------------------------------------

def schwarzschild_radius(mass_kg: float) -> float:
    """Calculate Schwarzschild radius for a given mass in kg.

    Formula: rs = 2 * G * M / c^2
    """
    if _cpp_lib is not None:
        try:
            return float(_cpp_lib.bh_schwarzschild_radius(ctypes.c_double(mass_kg)))
        except Exception:
            pass
    return _py_schwarzschild_radius(mass_kg)


def light_deflection_angle(mass_kg: float, impact_m: float) -> float:
    """Calculate gravitational light deflection angle in radians.

    Formula: delta = 4 * G * M / (c^2 * b)
    """
    if _cpp_lib is not None:
        try:
            return float(_cpp_lib.bh_light_deflection_angle(
                ctypes.c_double(mass_kg), ctypes.c_double(impact_m)
            ))
        except Exception:
            pass
    return _py_light_deflection_angle(mass_kg, impact_m)


def time_dilation_factor(mass_kg: float, radius_m: float) -> float:
    """Calculate gravitational time dilation factor gamma for stationary observer at radius r.

    Formula: gamma = sqrt(1 - rs / r) if r > rs else 0.0
    """
    if _cpp_lib is not None:
        try:
            return float(_cpp_lib.bh_time_dilation_factor(
                ctypes.c_double(mass_kg), ctypes.c_double(radius_m)
            ))
        except Exception:
            pass
    return _py_time_dilation_factor(mass_kg, radius_m)


def compute_relativistic_properties(
    mass_kg: float,
    impact_factor_rs: float = 10.0,
    radius_factor_rs: float = 5.0,
    impact_m: Optional[float] = None,
    radius_m: Optional[float] = None,
) -> dict[str, Any]:
    """Compute standard set of relativistic parameters for a black hole.

    Parameters:
        mass_kg: Mass in kilograms.
        impact_factor_rs: Multiplier for impact parameter b in units of rs (default 10.0).
        radius_factor_rs: Multiplier for observer radius r in units of rs (default 5.0).
        impact_m: Explicit impact parameter in metres (overrides impact_factor_rs if provided).
        radius_m: Explicit observer radius in metres (overrides radius_factor_rs if provided).

    Returns:
        Dictionary with physical parameters and unitless factors.
    """
    rs = schwarzschild_radius(mass_kg)

    if impact_m is None:
        impact_m = rs * impact_factor_rs
    if radius_m is None:
        radius_m = rs * radius_factor_rs

    deflection_rad = light_deflection_angle(mass_kg, impact_m)
    time_dilation = time_dilation_factor(mass_kg, radius_m)
    photon_sphere_m = 1.5 * rs

    return {
        "mass_kg": float(mass_kg),
        "schwarzschild_radius_m": float(rs),
        "impact_parameter_m": float(impact_m),
        "impact_factor_rs": float(impact_m / rs) if rs > 0 else 0.0,
        "deflection_angle_rad": float(deflection_rad),
        "deflection_angle_deg": float(math.degrees(deflection_rad)),
        "observer_radius_m": float(radius_m),
        "observer_radius_rs": float(radius_m / rs) if rs > 0 else 0.0,
        "time_dilation_factor": float(time_dilation),
        "photon_sphere_m": float(photon_sphere_m),
        "backend": get_backend(),
    }
