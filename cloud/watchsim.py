"""iPhone / 任意手机浏览器上的烧录表盘模拟。听 8001，宿主机映射 11112。"""
from __future__ import annotations

import mimetypes
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).with_name("static") / "watch"
PUBLIC_URL = os.environ.get("PUBLIC_URL", "http://127.0.0.1:11111")


def serve(port: int = 8001) -> None:
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: object) -> None:
            print("[watchsim]", fmt % args, flush=True)

        def _send(self, code: int, body: bytes, content_type: str) -> None:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            path = urlparse(self.path).path
            if path == "/config.js":
                api = PUBLIC_URL.rstrip("/")
                body = f'window.BONDWATCH_API="{api}";\n'.encode("utf-8")
                self._send(200, body, "application/javascript; charset=utf-8")
                return
            if path in ("/", "/index.html"):
                target = ROOT / "index.html"
            else:
                target = (ROOT / path.lstrip("/")).resolve()
                if ROOT.resolve() not in target.parents and target != ROOT.resolve():
                    self._send(404, b"not found", "text/plain")
                    return
            if not target.is_file():
                self._send(404, b"not found", "text/plain")
                return
            ctype = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
            if target.suffix == ".js":
                ctype = "application/javascript; charset=utf-8"
            elif target.suffix == ".css":
                ctype = "text/css; charset=utf-8"
            elif target.suffix == ".html":
                ctype = "text/html; charset=utf-8"
            elif target.suffix == ".glb":
                ctype = "model/gltf-binary"
            self._send(200, target.read_bytes(), ctype)

    print(f"BondWatch watch sim on 0.0.0.0:{port}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", port), Handler).serve_forever()
