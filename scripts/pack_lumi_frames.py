"""Pack baked lumi PNG data-URLs into RGB565 frames + a preview GIF."""
from __future__ import annotations

import json
import struct
from pathlib import Path

from PIL import Image


W = 160
H = 176
ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "cloud" / "static" / "watch" / "models" / "lumi_frames"
OUT_BIN = ROOT / "firmware" / "src" / "lumi_frames.rgb565"
OUT_GIF = ROOT / "cloud" / "static" / "watch" / "models" / "lumi_wave.gif"
OUT_HDR = ROOT / "firmware" / "include" / "lumi_frames.h"


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("cdp_json")
    args = parser.parse_args()
    payload = json.loads(Path(args.cdp_json).read_text(encoding="utf-8"))
    blob = payload["result"]["value"]
    urls = json.loads(blob)
    OUT_PNG.mkdir(parents=True, exist_ok=True)
    images: list[Image.Image] = []
    raw = bytearray()
    for i, url in enumerate(urls):
        b64 = url.split(",", 1)[1]
        import base64

        png = base64.b64decode(b64)
        img = Image.open(io_bytes(png)).convert("RGB").resize((W, H), Image.Resampling.LANCZOS)
        img.save(OUT_PNG / f"{i:02d}.png")
        images.append(img)
        for y in range(H):
            for x in range(W):
                r, g, b = img.getpixel((x, y))
                raw.extend(struct.pack("<H", rgb565(r, g, b)))
        print(f"frame {i} {img.size}")
    OUT_BIN.parent.mkdir(parents=True, exist_ok=True)
    OUT_BIN.write_bytes(raw)
    images[0].save(
        OUT_GIF,
        save_all=True,
        append_images=images[1:],
        duration=90,
        loop=0,
        optimize=True,
    )
    n = len(images)
    OUT_HDR.write_text(
        "#pragma once\n"
        "#include <stdint.h>\n\n"
        f"#define LUMI_FRAME_W {W}\n"
        f"#define LUMI_FRAME_H {H}\n"
        f"#define LUMI_FRAME_N {n}\n"
        f"#define LUMI_FRAME_BYTES ({W} * {H} * 2)\n"
        "extern const uint8_t lumi_frames[];\n"
        "extern const uint32_t lumi_frames_size;\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT_BIN} ({len(raw)} bytes) gif={OUT_GIF}")


def io_bytes(data: bytes):
    import io

    return io.BytesIO(data)


if __name__ == "__main__":
    main()
