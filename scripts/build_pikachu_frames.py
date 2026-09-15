"""Build Pikachu watch sprite GIF from user reference mockups."""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ASSETS = Path(r"C:\Users\123\.cursor\projects\d-Work-BondWatch\assets")

REFS = [
    ASSETS / "c__Users_123_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_image-5f3d3046-984a-45df-9ef9-986871e1a3b3.png",
    ASSETS / "c__Users_123_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_image-8464ca22-45a7-4084-99b3-7573b476809d.png",
    ASSETS / "c__Users_123_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_image-6ba3a611-0bb1-4214-a6e4-bf51a681dd46.png",
]
OUT_GIF = ROOT / "scripts" / "_pikachu_pack.gif"
W, H = 160, 176
BG_IDLE = (0xFF, 0xF8, 0xEE)
BG_LISTEN = (0x05, 0x05, 0x05)


def crop_face(img: Image.Image) -> Image.Image:
    """Crop watch face card from vertical mockup."""
    w, h = img.size
    left = int(w * 0.14)
    right = int(w * 0.86)
    top = int(h * 0.22)
    bottom = int(h * 0.72)
    return img.crop((left, top, right, bottom))


def fit_canvas(src: Image.Image, bg: tuple[int, int, int], scale: float = 1.0) -> Image.Image:
    rgba = src.convert("RGBA")
    nw = max(1, int(rgba.width * scale))
    nh = max(1, int(rgba.height * scale))
    rgba = rgba.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (W, H), bg + (255,))
    ox = (W - nw) // 2
    oy = H - nh - 4
    canvas.paste(rgba, (ox, oy), rgba)
    return canvas.convert("RGB")


def blink_overlay(frame: Image.Image) -> Image.Image:
    out = frame.copy()
    draw = ImageDraw.Draw(out)
    # Soft closed-eye arcs on upper face area.
    for cx in (58, 102):
        draw.arc((cx - 12, 72, cx + 12, 88), start=0, end=180, fill=(20, 20, 20), width=3)
    return out


def main() -> None:
    loaded = [Image.open(p) for p in REFS if p.is_file()]
    if not loaded:
        raise SystemExit("missing reference PNGs")

    crops = [crop_face(im) for im in loaded]
    frames: list[Image.Image] = []

    idle = crops[0]
    listen = crops[2] if len(crops) > 2 else crops[-1]

    # Idle breathing loop (flat ref).
    for i in range(10):
        s = 1.0 + 0.025 * math.sin(i * math.pi / 5)
        frames.append(fit_canvas(idle, BG_IDLE, scale=s))

    # Blink frames.
    base = fit_canvas(idle, BG_IDLE, scale=1.0)
    frames.append(blink_overlay(base))
    frames.append(blink_overlay(base))

    idle_n = len(frames)

    # Listening loop (dark ref) — separate clip, only played while recording.
    for i in range(6):
        s = 1.0 + 0.02 * math.sin(i * math.pi / 3)
        img = fit_canvas(listen, BG_LISTEN, scale=s)
        if i % 2 == 0:
            img = ImageEnhance.Brightness(img).enhance(1.05)
        frames.append(img)

    meta = OUT_GIF.with_suffix(".meta")
    meta.write_text(f"idle={idle_n}\nlisten={len(frames) - idle_n}\n", encoding="utf-8")

    OUT_GIF.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        OUT_GIF,
        save_all=True,
        append_images=frames[1:],
        duration=120,
        loop=0,
        optimize=True,
    )
    print(f"wrote {OUT_GIF} ({len(frames)} frames)")


if __name__ == "__main__":
    main()
