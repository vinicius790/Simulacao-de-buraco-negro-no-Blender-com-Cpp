#!/usr/bin/env python3
"""Compare two images (PNG 8-bit RGB/RGBA, binary PPM P6, 24-bit BMP) — stdlib only.

Used for GPU↔CPU golden comparisons:
  BlackHole3D --scientific --capture gpu.png  vs  bh_render_cpu --out cpu.png

Prints a JSON report: mean absolute error, max error, fraction of pixels
differing by more than --pixel-tol (0–255), and an optional heat-map image.
Exit code 1 if --max-bad-fraction is exceeded.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(data: bytes) -> tuple[int, int, list[int]]:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat = 8, b""
    w = h = 0
    bit_depth = color_type = 0
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        ctype = data[pos + 4 : pos + 8]
        body = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            w, h, bit_depth, color_type = struct.unpack(">IIBB", body[:10])
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break
    if bit_depth != 8 or color_type not in (2, 6):
        raise ValueError("only 8-bit RGB/RGBA PNG supported")
    bpp = 3 if color_type == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    out: list[int] = []
    prev = bytearray(stride)
    i = 0
    for _ in range(h):
        ftype = raw[i]
        line = bytearray(raw[i + 1 : i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 0xFF
            elif ftype == 2:
                line[x] = (line[x] + b) & 0xFF
            elif ftype == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                line[x] = (line[x] + _paeth(a, b, c)) & 0xFF
        prev = line
        for x in range(w):
            out.extend(line[x * bpp : x * bpp + 3])
    return w, h, out


def read_ppm(data: bytes) -> tuple[int, int, list[int]]:
    """Binary PPM (P6, maxval 255). The header is parsed token by token
    (``#`` comments allowed) and EXACTLY one whitespace byte separates maxval
    from the pixels, so pixel bytes that happen to be whitespace (9–13, 32)
    are preserved."""
    if data[:2] != b"P6":
        raise ValueError("not a binary PPM")
    pos = 2
    fields: list[int] = []
    while len(fields) < 3:
        while pos < len(data) and (data[pos] in b" \t\r\n\x0b\x0c" or data[pos] == ord("#")):
            if data[pos] == ord("#"):
                while pos < len(data) and data[pos] not in b"\r\n":
                    pos += 1
            else:
                pos += 1
        start = pos
        while pos < len(data) and chr(data[pos]).isdigit():
            pos += 1
        if start == pos:
            raise ValueError("malformed PPM header")
        fields.append(int(data[start:pos]))
    w, h, maxval = fields
    if maxval != 255:
        raise ValueError("only maxval 255 supported")
    pos += 1  # the single whitespace byte after maxval
    pixels = data[pos : pos + w * h * 3]
    if len(pixels) != w * h * 3:
        raise ValueError("truncated PPM")
    return w, h, list(pixels)


def read_bmp(data: bytes) -> tuple[int, int, list[int]]:
    off = struct.unpack("<I", data[10:14])[0]
    w, h = struct.unpack("<ii", data[18:26])
    bpp = struct.unpack("<H", data[28:30])[0]
    if bpp != 24:
        raise ValueError("only 24-bit BMP supported")
    row = (w * 3 + 3) & ~3
    out = [0] * (w * abs(h) * 3)
    for y in range(abs(h)):
        src_y = (abs(h) - 1 - y) if h > 0 else y
        base = off + src_y * row
        for x in range(w):
            b, g, r = data[base + x * 3 : base + x * 3 + 3]
            o = (y * w + x) * 3
            out[o : o + 3] = [r, g, b]
    return w, abs(h), out


def read_image(path: Path) -> tuple[int, int, list[int]]:
    data = path.read_bytes()
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return read_png(data)
    if data[:2] == b"P6":
        return read_ppm(data)
    if data[:2] == b"BM":
        return read_bmp(data)
    raise ValueError(f"unsupported image format: {path}")


def write_ppm(path: Path, w: int, h: int, rgb: list[int]) -> None:
    path.write_bytes(f"P6\n{w} {h}\n255\n".encode() + bytes(rgb))


def compare(a: Path, b: Path, pixel_tol: int, heatmap: Path | None) -> dict:
    wa, ha, pa = read_image(a)
    wb, hb, pb = read_image(b)
    if (wa, ha) != (wb, hb):
        raise ValueError(f"size mismatch {wa}x{ha} vs {wb}x{hb}")
    n = wa * ha
    bad = 0
    total = 0
    worst = 0
    heat: list[int] = []
    for i in range(n):
        d = max(abs(pa[i * 3 + c] - pb[i * 3 + c]) for c in range(3))
        total += sum(abs(pa[i * 3 + c] - pb[i * 3 + c]) for c in range(3))
        worst = max(worst, d)
        if d > pixel_tol:
            bad += 1
        heat.extend([min(255, d * 4), 0, 0] if d > pixel_tol else [d, d, d])
    if heatmap is not None:
        write_ppm(heatmap, wa, ha, heat)
    return {
        "width": wa,
        "height": ha,
        "mean_abs_error": total / (n * 3),
        "max_error": worst,
        "pixel_tol": pixel_tol,
        "bad_fraction": bad / n,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", type=Path)
    ap.add_argument("b", type=Path)
    ap.add_argument("--pixel-tol", type=int, default=24, help="per-channel tolerance (0-255) for a 'bad' pixel")
    ap.add_argument("--max-bad-fraction", type=float, default=None, help="fail if exceeded")
    ap.add_argument("--heatmap", type=Path, default=None, help="write a PPM heat map of differences")
    args = ap.parse_args()
    report = compare(args.a, args.b, args.pixel_tol, args.heatmap)
    print(json.dumps(report))
    if args.max_bad_fraction is not None and report["bad_fraction"] > args.max_bad_fraction:
        print(f"FAIL: bad_fraction {report['bad_fraction']:.4f} > {args.max_bad_fraction}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
