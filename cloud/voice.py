"""TTS (edge-tts) and optional STT helpers for BondWatch."""
from __future__ import annotations

import asyncio
import base64
import json
import os
import urllib.error
import urllib.request
import uuid
from pathlib import Path

TTS_VOICE = os.environ.get("TTS_VOICE", "zh-CN-XiaoxiaoNeural")
OPENROUTER_KEY = os.environ.get("OPENROUTER_API_KEY", "").strip()
OPENROUTER_STT_URL = os.environ.get(
    "OPENROUTER_STT_URL",
    "https://openrouter.ai/api/v1/audio/transcriptions",
)
OPENROUTER_STT_MODEL = os.environ.get("OPENROUTER_STT_MODEL", "openai/whisper-large-v3")
STT_TIMEOUT = float(os.environ.get("STT_TIMEOUT", "30"))
STT_LANGUAGE = os.environ.get("STT_LANGUAGE", "zh").strip()


def tts_ready() -> bool:
    try:
        import edge_tts  # noqa: F401

        return True
    except ImportError:
        return False


def synthesize(text: str, out_dir: Path) -> str | None:
    line = " ".join((text or "").split())
    if not line:
        return None
    try:
        import edge_tts
    except ImportError:
        return None
    out_dir.mkdir(parents=True, exist_ok=True)
    fname = f"tts_{uuid.uuid4().hex[:12]}.mp3"
    path = out_dir / fname

    async def _run() -> None:
        comm = edge_tts.Communicate(line, TTS_VOICE)
        await comm.save(str(path))

    asyncio.run(_run())
    return fname if path.is_file() and path.stat().st_size > 0 else None


def stt_ready() -> bool:
    return bool(OPENROUTER_KEY)


def transcribe_audio(audio_bytes: bytes, *, mime: str = "audio/wav") -> str:
    """STT via OpenRouter /audio/transcriptions (Whisper). Returns empty if unavailable."""
    if not OPENROUTER_KEY or not audio_bytes:
        return ""
    fmt = "wav"
    if "webm" in mime:
        fmt = "webm"
    elif "mp3" in mime or "mpeg" in mime:
        fmt = "mp3"
    elif "ogg" in mime:
        fmt = "ogg"
    b64 = base64.b64encode(audio_bytes).decode("ascii")
    payload: dict[str, object] = {
        "model": OPENROUTER_STT_MODEL,
        "input_audio": {"data": b64, "format": fmt},
    }
    if STT_LANGUAGE:
        payload["language"] = STT_LANGUAGE
    req = urllib.request.Request(
        OPENROUTER_STT_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {OPENROUTER_KEY}",
            "HTTP-Referer": "https://bondwatch.local",
            "X-Title": "BondWatch",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=STT_TIMEOUT) as res:
            data = json.loads(res.read().decode("utf-8"))
        text = data.get("text") or ""
        return " ".join(str(text).split()).strip()
    except urllib.error.HTTPError as exc:
        detail = ""
        try:
            body = json.loads(exc.read().decode("utf-8"))
            detail = str((body.get("error") or {}).get("message") or "")
        except (json.JSONDecodeError, UnicodeDecodeError, AttributeError):
            detail = str(exc)
        if exc.code == 402 or "balance" in detail.lower():
            raise RuntimeError(
                "OpenRouter 音频额度不足（需至少 $0.50）。"
                "请在本机用 http://127.0.0.1:11111 打开（浏览器免费听写），或到 OpenRouter 充值。"
            ) from exc
        raise RuntimeError(detail or f"语音识别失败 ({exc.code})") from exc
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, KeyError) as exc:
        raise RuntimeError("语音识别服务暂时不可用，请稍后重试") from exc
