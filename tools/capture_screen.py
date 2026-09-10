#!/usr/bin/env python3
"""Capture BondWatch LCD framebuffer over USB serial (cmd 'S')."""

from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(1)


MAGIC_START = b">>>BWSHOT\n"
MAGIC_END = b"\n<<<BWSHOT\n"


def rgb565_to_bmp(path: Path, w: int, h: int, raw: bytes) -> None:
    if len(raw) < w * h * 2:
        raise ValueError(f"short frame: {len(raw)} < {w * h * 2}")
    row_pad = (4 - (w * 3) % 4) % 4
    row_size = w * 3 + row_pad
    pixel_bytes = row_size * h
    file_size = 54 + pixel_bytes

    header = bytearray()
    header += b"BM"
    header += struct.pack("<IHHI", file_size, 0, 0, 54)
    header += struct.pack("<IIIHHIIIIII", 40, w, h, 1, 24, 0, pixel_bytes, 2835, 2835, 0, 0)

    pixels = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            i = (y * w + x) * 2
            v = raw[i] | (raw[i + 1] << 8)
            r = ((v >> 11) & 0x1F) * 255 // 31
            g = ((v >> 5) & 0x3F) * 255 // 63
            b = (v & 0x1F) * 255 // 31
            pixels += bytes((b, g, r))
        pixels += b"\x00" * row_pad

    path.write_bytes(header + pixels)


def capture(port: str, baud: int, out: Path, timeout_s: float) -> None:
    ser = serial.Serial(port, baud, timeout=0.2)
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.write(b"!")
    ser.flush()

    buf = bytearray()
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            if MAGIC_START in buf and MAGIC_END in buf:
                break
        else:
            time.sleep(0.01)
    ser.close()

    start = buf.find(MAGIC_START)
    if start < 0:
        raise RuntimeError(f"no >>>BWSHOT in {len(buf)} bytes: {buf[:200]!r}")
    rest = bytes(buf[start + len(MAGIC_START) :])
    nl = rest.find(b"\n")
    if nl < 0:
        raise RuntimeError("missing size line")
    size_line = rest[:nl].decode("ascii", errors="replace").strip()
    parts = size_line.split()
    if len(parts) != 2:
        raise RuntimeError(f"bad size line: {size_line!r}")
    w, h = int(parts[0]), int(parts[1])
    payload = rest[nl + 1 :]
    end = payload.find(MAGIC_END)
    if end < 0:
        # tolerate missing leading newline in end marker
        end = payload.find(b"<<<BWSHOT\n")
        if end < 0:
            raise RuntimeError(f"no end marker; got {len(payload)} payload bytes")
        raw = payload[:end]
    else:
        raw = payload[:end]

    expect = w * h * 2
    if len(raw) < expect:
        raise RuntimeError(f"short payload {len(raw)}/{expect}")
    raw = raw[:expect]

    out.parent.mkdir(parents=True, exist_ok=True)
    if out.suffix.lower() == ".bmp":
        rgb565_to_bmp(out, w, h, raw)
        print(f"wrote {out} ({w}x{h})")
        return

    bmp = out.with_suffix(".bmp")
    rgb565_to_bmp(bmp, w, h, raw)
    try:
        from PIL import Image

        Image.open(bmp).save(out)
        bmp.unlink(missing_ok=True)
        print(f"wrote {out} ({w}x{h})")
    except ImportError:
        print(f"wrote {bmp} ({w}x{h}); pip install pillow for PNG")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="docs/device-capture.png")
    ap.add_argument("--timeout", type=float, default=60.0)
    args = ap.parse_args()
    capture(args.port, args.baud, Path(args.out), args.timeout)


if __name__ == "__main__":
    main()
