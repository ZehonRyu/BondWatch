"""本机视觉小服务。在电脑上跑：
  pip install opencv-python-headless
  python cloud/vision_local.py

云端会 POST http://host.docker.internal:18765/v1/see
{ "image_base64": "..." } -> { "caption": "偏亮，好像有人脸" }
"""
from __future__ import annotations

import base64
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = "0.0.0.0"
PORT = 18765


def caption_with_opencv(jpeg: bytes) -> str:
    import cv2
    import numpy as np

    arr = np.frombuffer(jpeg, dtype=np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if img is None:
        return "看不清这张图"
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    mean = float(gray.mean())
    light = "偏亮" if mean > 140 else ("偏暗" if mean < 70 else "光线正常")
    b, g, r = (float(x) for x in img.mean(axis=(0, 1)))
    if r > g + 15 and r > b + 15:
        tint = "偏红"
    elif b > r + 15 and b > g + 10:
        tint = "偏蓝"
    elif g > r + 12 and g > b + 8:
        tint = "偏绿"
    else:
        tint = "颜色普通"
    faces = 0
    cascade = getattr(cv2.data, "haarcascades", "")
    if cascade:
        detector = cv2.CascadeClassifier(cascade + "haarcascade_frontalface_default.xml")
        if not detector.empty():
            found = detector.detectMultiScale(gray, 1.1, 4, minSize=(24, 24))
            faces = len(found)
    bits = [light, tint]
    if faces:
        bits.append("好像有人脸" if faces == 1 else f"好像有{faces}张脸")
    else:
        bits.append("没看清人脸")
    return "，".join(bits)


def caption_image(b64: str) -> str:
    raw = "".join(b64.split())
    if "," in raw and raw.lower().startswith("data:"):
        raw = raw.split(",", 1)[1]
    try:
        jpeg = base64.b64decode(raw)
    except ValueError:
        return "图坏了"
    try:
        return caption_with_opencv(jpeg)
    except ImportError:
        return "本机还没装 OpenCV，先记下这张图"


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: object) -> None:
        print("[vision]", fmt % args)

    def _send(self, code: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.end_headers()

    def do_GET(self) -> None:
        if self.path == "/health":
            try:
                import cv2  # noqa: F401

                backend = "opencv"
            except ImportError:
                backend = "no-opencv"
            self._send(200, {"ok": True, "backend": backend})
            return
        self._send(404, {"detail": "not found"})

    def do_POST(self) -> None:
        if self.path != "/v1/see":
            self._send(404, {"detail": "not found"})
            return
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b"{}"
        try:
            data = json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError:
            self._send(400, {"detail": "bad json"})
            return
        self._send(200, {"caption": caption_image(str(data.get("image_base64") or ""))})


if __name__ == "__main__":
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"BondWatch vision on http://{HOST}:{PORT}/v1/see", flush=True)
    server.serve_forever()
