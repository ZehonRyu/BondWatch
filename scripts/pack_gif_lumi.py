"""Pack a GIF (or PNG) into Lumi RGB565 firmware frames + web preview."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image, ImageSequence

W = 160
H = 176
BG = (0xFF, 0xF8, 0xEE)  # #fff8ee — cream card background
ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "cloud" / "static" / "watch" / "models" / "lumi_frames"
OUT_BIN = ROOT / "firmware" / "src" / "lumi_frames.rgb565"
OUT_GIF = ROOT / "cloud" / "static" / "watch" / "models" / "lumi_wave.gif"
OUT_SRC = ROOT / "cloud" / "static" / "watch" / "models" / "lumi_source.gif"
OUT_HDR = ROOT / "firmware" / "include" / "lumi_frames.h"


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def fit_bottom(img: Image.Image, w: int, h: int, bg: tuple[int, int, int]) -> Image.Image:
    src = img.convert("RGBA")
    scale = min(w / src.width, h / src.height)
    nw = max(1, int(src.width * scale))
    nh = max(1, int(src.height * scale))
    src = src.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (w, h), bg + (255,))
    ox = (w - nw) // 2
    oy = h - nh
    canvas.paste(src, (ox, oy), src)
    return canvas.convert("RGB")


def load_frames(path: Path) -> tuple[list[Image.Image], list[int]]:
    im = Image.open(path)
    frames: list[Image.Image] = []
    durations: list[int] = []
    for frame in ImageSequence.Iterator(im):
        rgba = frame.convert("RGBA")
        frames.append(fit_bottom(rgba, W, H, BG))
        durations.append(max(40, int(frame.info.get("duration") or im.info.get("duration") or 90)))
    if not frames:
        raise SystemExit(f"no frames in {path}")
    if len(frames) == 1:
        durations = [90]
    return frames, durations


def main() -> None:
    parser = argparse.ArgumentParser(description="Pack GIF/PNG into Lumi RGB565 frames")
    parser.add_argument("source", type=Path, help="GIF or PNG path")
    parser.add_argument("--duration", type=int, default=0, help="ms per frame if GIF has no timing")
    args = parser.parse_args()
    src = args.source.expanduser().resolve()
    if not src.is_file():
        raise SystemExit(f"missing {src}")

    frames, durations = load_frames(src)
    if args.duration > 0:
        durations = [args.duration] * len(frames)

    OUT_PNG.mkdir(parents=True, exist_ok=True)
    raw = bytearray()
    for i, img in enumerate(frames):
        img.save(OUT_PNG / f"{i:02d}.png")
        for y in range(H):
            for x in range(W):
                r, g, b = img.getpixel((x, y))
                raw.extend(struct.pack("<H", rgb565(r, g, b)))
        print(f"frame {i} {img.size} dur={durations[i % len(durations)]}ms")

    OUT_BIN.parent.mkdir(parents=True, exist_ok=True)
    OUT_BIN.write_bytes(raw)
    dur = durations[0] if len(set(durations)) == 1 else durations
    frames[0].save(
        OUT_GIF,
        save_all=True,
        append_images=frames[1:],
        duration=dur,
        loop=0,
        optimize=True,
    )
    OUT_SRC.write_bytes(src.read_bytes())

    n = len(frames)
    meta_path = src.with_suffix(".meta")
    idle_n = n
    listen_n = 0
    if meta_path.is_file():
        for line in meta_path.read_text(encoding="utf-8").splitlines():
            if line.startswith("idle="):
                idle_n = int(line.split("=", 1)[1])
            elif line.startswith("listen="):
                listen_n = int(line.split("=", 1)[1])
    if listen_n <= 0 and idle_n >= n:
        idle_n = n
    listen_off = min(idle_n, max(0, n - 1))
    idle_n = listen_off if listen_off > 0 else n
    listen_n = min(listen_n, max(1, n - listen_off))
    if listen_off + listen_n > n:
        listen_n = max(1, n - listen_off)
    asm = ROOT / "firmware" / "src" / "lumi_frames.S"
    asm.write_text(
        f"/* auto-generated: {n} frames, {len(raw)} bytes */\n"
        '    .section .rodata.lumi_frames,"a"\n'
        "    .global lumi_frames\n"
        "    .align 4\n"
        "lumi_frames:\n"
        '    .incbin "firmware/src/lumi_frames.rgb565"\n'
        "    .size lumi_frames, .-lumi_frames\n",
        encoding="utf-8",
    )
    OUT_HDR.write_text(
        "#pragma once\n"
        "#include <stdint.h>\n\n"
        f"#define LUMI_FRAME_W {W}\n"
        f"#define LUMI_FRAME_H {H}\n"
        f"#define LUMI_FRAME_N {n}\n"
        f"#define LUMI_IDLE_N {idle_n}\n"
        f"#define LUMI_LISTEN_OFF {listen_off}\n"
        f"#define LUMI_LISTEN_N {listen_n}\n"
        f"#define LUMI_FRAME_MS {durations[0]}\n"
        f"#define LUMI_FRAME_BYTES ({W} * {H} * 2)\n"
        "extern const uint8_t lumi_frames[];\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT_BIN} ({len(raw)} bytes, {n} frames)")
    print(f"preview {OUT_GIF}")


if __name__ == "__main__":
    main()
