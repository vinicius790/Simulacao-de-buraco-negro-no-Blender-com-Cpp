#!/usr/bin/env python3
"""Assemble rendered frames (PNG/PPM/BMP, e.g. bh_render_cpu --frames N) into an
animated GIF — standard library only.

    python3 tools/frames_to_gif.py build/renders/orbit_*.png -o orbit.gif --fps 20 --pingpong

* one global 256-colour palette built by median cut over pixels sampled from
  every frame (stable colours, no per-frame flicker);
* nearest-colour mapping through a 32×32×32 lookup cube;
* optional 4×4 ordered (Bayer) dithering (--dither) to hide palette banding
  in smooth gradients such as the accretion disk;
* GIF89a with NETSCAPE2.0 looping and variable-length LZW (spec-compliant).
"""

from __future__ import annotations

import argparse
import glob
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import image_diff  # noqa: E402  (stdlib PNG/PPM/BMP reader)


def median_cut(pixels: list[tuple[int, int, int]], n_colors: int = 256) -> list[tuple[int, int, int]]:
    boxes = [pixels]
    while len(boxes) < n_colors:
        # Split the box with the widest channel range (largest population on ties).
        best_i, best_key, best_ch = -1, (-1, -1), 0
        for i, box in enumerate(boxes):
            if len(box) < 2:
                continue
            ranges = [max(p[c] for p in box) - min(p[c] for p in box) for c in range(3)]
            ch = max(range(3), key=lambda c: ranges[c])
            key = (ranges[ch], len(box))
            if key > best_key:
                best_i, best_key, best_ch = i, key, ch
        if best_i < 0 or best_key[0] == 0:
            break
        box = sorted(boxes.pop(best_i), key=lambda p: p[best_ch])
        mid = len(box) // 2
        boxes += [box[:mid], box[mid:]]
    palette = []
    for box in boxes:
        n = len(box)
        palette.append(tuple(sum(p[c] for p in box) // n for c in range(3)))
    while len(palette) < n_colors:
        palette.append((0, 0, 0))
    return palette[:n_colors]


def build_lut(palette: list[tuple[int, int, int]]) -> bytes:
    lut = bytearray(32 * 32 * 32)
    for r in range(32):
        for g in range(32):
            for b in range(32):
                cr, cg, cb = r * 8 + 4, g * 8 + 4, b * 8 + 4
                best, best_d = 0, 1 << 30
                for i, (pr, pg, pb) in enumerate(palette):
                    d = (pr - cr) ** 2 + (pg - cg) ** 2 + (pb - cb) ** 2
                    if d < best_d:
                        best, best_d = i, d
                lut[(r << 10) | (g << 5) | b] = best
    return bytes(lut)


def lzw_encode(indices: bytes, min_code_size: int = 8) -> bytes:
    clear = 1 << min_code_size
    eoi = clear + 1
    out = bytearray()
    bit_buf = 0
    bit_len = 0

    def emit(code: int, size: int) -> None:
        nonlocal bit_buf, bit_len
        bit_buf |= code << bit_len
        bit_len += size
        while bit_len >= 8:
            out.append(bit_buf & 0xFF)
            bit_buf >>= 8
            bit_len -= 8

    code_size = min_code_size + 1
    table = {bytes([i]): i for i in range(clear)}
    next_code = eoi + 1
    emit(clear, code_size)
    w = b""
    for k in indices:
        wk = w + bytes([k])
        if wk in table:
            w = wk
            continue
        emit(table[w], code_size)
        if next_code < 4096:
            table[wk] = next_code
            next_code += 1
            if next_code > (1 << code_size) and code_size < 12:
                code_size += 1
        else:
            emit(clear, code_size)
            table = {bytes([i]): i for i in range(clear)}
            next_code = eoi + 1
            code_size = min_code_size + 1
        w = bytes([k])
    if w:
        emit(table[w], code_size)
    emit(eoi, code_size)
    if bit_len:
        out.append(bit_buf & 0xFF)
    return bytes(out)


def sub_blocks(data: bytes) -> bytes:
    out = bytearray()
    for i in range(0, len(data), 255):
        chunk = data[i : i + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("frames", nargs="+", help="frame files or glob patterns (sorted)")
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--fps", type=float, default=20.0)
    ap.add_argument("--pingpong", action="store_true", help="append the frames in reverse (seamless back-and-forth)")
    ap.add_argument("--dither", action="store_true", help="4x4 ordered dithering (reduces banding)")
    args = ap.parse_args()

    paths: list[str] = []
    for pattern in args.frames:
        paths += sorted(glob.glob(pattern)) or [pattern]
    frames = [image_diff.read_image(Path(p)) for p in paths]
    w, h = frames[0][0], frames[0][1]
    if any((f[0], f[1]) != (w, h) for f in frames):
        raise SystemExit("all frames must have the same size")
    if args.pingpong and len(frames) > 2:
        frames = frames + frames[-2:0:-1]

    sample: list[tuple[int, int, int]] = []
    step = max(1, (w * h * len(frames)) // 60000)
    k = 0
    for _, _, px in frames:
        for i in range(0, w * h, 1):
            if k % step == 0:
                sample.append((px[i * 3], px[i * 3 + 1], px[i * 3 + 2]))
            k += 1
    palette = median_cut(sample, 256)
    lut = build_lut(palette)

    delay = max(2, round(100.0 / args.fps))
    out = bytearray(b"GIF89a")
    out += struct.pack("<HHBBB", w, h, 0xF7, 0, 0)  # global table, 8 bits/channel, 256 entries
    for c in palette:
        out += bytes(c)
    out += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"  # loop forever
    bayer = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)
    for _, _, px in frames:
        if args.dither:
            # Threshold in [−7.5, +7.5] (one LUT cell = 8 levels), fixed per pixel
            # position so the pattern does not crawl between frames.
            out_idx = bytearray(w * h)
            for y in range(h):
                row = bayer[(y & 3) * 4:(y & 3) * 4 + 4]
                for x in range(w):
                    d = row[x & 3] - 7.5
                    i = (y * w + x) * 3
                    r = min(255, max(0, int(px[i] + d)))
                    g = min(255, max(0, int(px[i + 1] + d)))
                    b = min(255, max(0, int(px[i + 2] + d)))
                    out_idx[y * w + x] = lut[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)]
            idx = bytes(out_idx)
        else:
            idx = bytes(lut[((px[i] >> 3) << 10) | ((px[i + 1] >> 3) << 5) | (px[i + 2] >> 3)] for i in range(0, w * h * 3, 3))
        out += b"\x21\xF9\x04\x04" + struct.pack("<H", delay) + b"\x00\x00"  # graphic control: no disposal
        out += b"\x2C" + struct.pack("<HHHHB", 0, 0, w, h, 0)
        out += b"\x08" + sub_blocks(lzw_encode(idx, 8))
    out += b"\x3B"
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(bytes(out))
    print(f"wrote {args.out} ({len(frames)} frames, {w}x{h}, {len(out) / 1024:.0f} KiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
