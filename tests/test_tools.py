#!/usr/bin/env python3
"""Self-tests for the stdlib-only tools in tools/ (no third-party packages).

* frames_to_gif.lzw_encode  ↔ an independent GIF-LZW decoder written here
  (variable code size, CLEAR resets, 4096-entry table limit);
* frames_to_gif end-to-end: a synthetic 3-frame animation decodes back to the
  palette-mapped pixels, GIF89a structure + NETSCAPE loop block present;
* image_diff PNG reader: all five PNG filter types (0–4) decode to the same
  pixels, plus PPM and BMP readers agree.
"""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


def lzw_decode(data: bytes, min_code_size: int) -> bytes:
    clear = 1 << min_code_size
    eoi = clear + 1
    pos = 0
    bit_buf = 0
    bit_len = 0
    code_size = min_code_size + 1
    table: list[bytes] = [bytes([i]) for i in range(clear)] + [b"", b""]
    out = bytearray()
    prev: bytes | None = None
    while True:
        while bit_len < code_size:
            if pos >= len(data):
                return bytes(out)
            bit_buf |= data[pos] << bit_len
            pos += 1
            bit_len += 8
        code = bit_buf & ((1 << code_size) - 1)
        bit_buf >>= code_size
        bit_len -= code_size
        if code == clear:
            table = [bytes([i]) for i in range(clear)] + [b"", b""]
            code_size = min_code_size + 1
            prev = None
            continue
        if code == eoi:
            return bytes(out)
        if code < len(table):
            entry = table[code]
        elif prev is not None and code == len(table):
            entry = prev + prev[:1]
        else:
            raise AssertionError(f"invalid LZW code {code}")
        out += entry
        if prev is not None and len(table) < 4096:
            table.append(prev + entry[:1])
            if len(table) == (1 << code_size) and code_size < 12:
                code_size += 1
        prev = entry


def read_gif_frames(data: bytes) -> tuple[int, int, list[tuple[int, int, int]], list[bytes], bool]:
    assert data[:6] == b"GIF89a", "GIF89a signature"
    w, h, flags = struct.unpack("<HHB", data[6:11])
    assert flags & 0x80, "global colour table"
    n_colors = 2 << (flags & 7)
    pos = 13
    palette = [tuple(data[pos + 3 * i : pos + 3 * i + 3]) for i in range(n_colors)]
    pos += 3 * n_colors
    frames: list[bytes] = []
    looping = False
    while True:
        b = data[pos]
        if b == 0x3B:
            break
        if b == 0x21:
            label = data[pos + 1]
            pos += 2
            if label == 0xFF and data[pos + 1 : pos + 12] == b"NETSCAPE2.0":
                looping = True
            while data[pos] != 0:
                pos += 1 + data[pos]
            pos += 1
            continue
        assert b == 0x2C, f"unexpected block 0x{b:02x}"
        fx, fy, fw, fh, fflags = struct.unpack("<HHHHB", data[pos + 1 : pos + 10])
        assert (fx, fy, fw, fh, fflags) == (0, 0, w, h, 0)
        pos += 10
        mcs = data[pos]
        pos += 1
        lzw = bytearray()
        while data[pos] != 0:
            n = data[pos]
            lzw += data[pos + 1 : pos + 1 + n]
            pos += 1 + n
        pos += 1
        frames.append(lzw_decode(bytes(lzw), mcs))
    return w, h, palette, frames, looping


def png_with_filter(w: int, h: int, rgb: list[int], ftype: int) -> bytes:
    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        return a if pa <= pb and pa <= pc else (b if pb <= pc else c)

    raw = bytearray()
    prev = [0] * (w * 3)
    for y in range(h):
        line = rgb[y * w * 3 : (y + 1) * w * 3]
        raw.append(ftype)
        for x in range(w * 3):
            a = line[x - 3] if x >= 3 else 0
            b = prev[x]
            c = prev[x - 3] if x >= 3 else 0
            pred = [0, a, b, (a + b) >> 1, paeth(a, b, c)][ftype]
            raw.append((line[x] - pred) & 0xFF)
        prev = line

    def chunk(t, body):
        return struct.pack(">I", len(body)) + t + body + struct.pack(">I", zlib.crc32(t + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b""))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = ap.parse_args().root.resolve()
    sys.path.insert(0, str(root / "tools"))
    import frames_to_gif
    import image_diff

    rng = random.Random(1234)
    # 1) LZW round trip, including streams long enough to hit the 4096 limit.
    for n, alphabet in ((1, 2), (500, 4), (70000, 256), (120000, 3)):
        data = bytes(rng.randrange(alphabet) for _ in range(n))
        assert lzw_decode(frames_to_gif.lzw_encode(data, 8), 8) == data, f"LZW round trip n={n}"
    print("PASS: LZW round trip (incl. CLEAR at 4096 entries)")

    # 2) PNG filters 0-4 + PPM + BMP readers agree.
    w, h = 7, 5
    rgb = [rng.randrange(256) for _ in range(w * h * 3)]
    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        for ftype in range(5):
            p = tmpd / f"f{ftype}.png"
            p.write_bytes(png_with_filter(w, h, rgb, ftype))
            assert image_diff.read_image(p) == (w, h, rgb), f"PNG filter {ftype}"
        ppm = tmpd / "x.ppm"
        image_diff.write_ppm(ppm, w, h, rgb)
        assert image_diff.read_image(ppm) == (w, h, rgb), "PPM"
        # Pixel bytes equal to whitespace right after the header must survive.
        tricky = [32, 10, 9, 13, 11, 12] + [100] * (2 * 1 * 3)
        ppm2 = tmpd / "ws.ppm"
        ppm2.write_bytes(b"P6\n# comment line\n4 1\n255\n" + bytes(tricky))
        assert image_diff.read_image(ppm2) == (4, 1, tricky), "PPM with whitespace-valued pixels"
        print("PASS: PNG filters 0-4 and PPM decode identically")

        # 3) End-to-end GIF: three frames with a few flat colours → exact.
        colours = [(0, 0, 0), (255, 200, 40), (120, 40, 10), (250, 250, 250)]
        frame_paths = []
        expected = []
        for f in range(3):
            px = []
            for y in range(12):
                for x in range(16):
                    px += colours[(x // 4 + y // 3 + f) % 4]
            p = tmpd / f"frame_{f:04d}.ppm"
            image_diff.write_ppm(p, 16, 12, px)
            frame_paths.append(str(p))
            expected.append(px)
        gif = tmpd / "anim.gif"
        subprocess.run([sys.executable, str(root / "tools/frames_to_gif.py"), *frame_paths, "-o", str(gif), "--fps", "10"],
                       check=True, capture_output=True)
        gw, gh, palette, frames, looping = read_gif_frames(gif.read_bytes())
        assert (gw, gh) == (16, 12) and len(frames) == 3 and looping
        for f, idx in enumerate(frames):
            decoded = [c for i in idx for c in palette[i]]
            assert decoded == expected[f], f"GIF frame {f} pixels"
        print("PASS: frames_to_gif end-to-end (structure, loop block, exact pixels)")

    # 4) Committed animation decodes completely.
    anim = root / "docs/images/showcase_elevation_sweep.gif"
    if anim.exists():
        gw, gh, palette, frames, looping = read_gif_frames(anim.read_bytes())
        assert looping and all(len(fr) == gw * gh for fr in frames), "showcase GIF decodes"
        print(f"PASS: docs GIF decodes ({len(frames)} frames, {gw}x{gh})")

    print("All tool self-tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
