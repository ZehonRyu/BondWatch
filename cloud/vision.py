from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from typing import Any

VISION_LOCAL_URL = os.environ.get("VISION_LOCAL_URL", "").strip()
VISION_API_URL = os.environ.get("VISION_API_URL", "").strip()
VISION_API_KEY = os.environ.get("VISION_API_KEY", "").strip()
VISION_MODEL = os.environ.get("VISION_MODEL", "qwen-vl-plus")


def vision_backend() -> str:
    if VISION_LOCAL_URL:
        return "opencv-local"
    if VISION_API_URL and VISION_API_KEY:
        return VISION_MODEL
    return "none"


def _clean_image(image_b64: str) -> str:
    raw = "".join(image_b64.split())
    if "," in raw and raw.lower().startswith("data:"):
        raw = raw.split(",", 1)[1]
    return raw[:80_000]


def _post_json(url: str, payload: dict[str, Any], headers: dict[str, str], timeout: float) -> dict[str, Any]:
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json", **headers},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as res:
        return json.loads(res.read().decode("utf-8"))


def describe_image(image_b64: str) -> str | None:
    raw = _clean_image(image_b64)
    if not raw:
        return None
    if VISION_LOCAL_URL:
        try:
            data = _post_json(VISION_LOCAL_URL, {"image_base64": raw}, {}, 2.5)
            caption = (data.get("caption") or "").strip()
            if caption:
                return caption[:80]
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError):
            pass
    if VISION_API_URL and VISION_API_KEY:
        try:
            data = _post_json(
                VISION_API_URL,
                {
                    "model": VISION_MODEL,
                    "max_tokens": 40,
                    "messages": [
                        {
                            "role": "user",
                            "content": [
                                {"type": "text", "text": "用一句中文说出图里最显眼的一两样东西，不超过20字。"},
                                {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{raw}"}},
                            ],
                        }
                    ],
                },
                {"Authorization": f"Bearer {VISION_API_KEY}"},
                12,
            )
            text = (data.get("choices") or [{}])[0].get("message", {}).get("content") or ""
            if text.strip():
                return text.strip()[:80]
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError):
            return None
    return None
