from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from typing import Any

from vision import describe_image

LLM_PROVIDER = os.environ.get("LLM_PROVIDER", "deepseek").strip().lower()

DEEPSEEK_URL = os.environ.get("DEEPSEEK_BASE_URL", "https://api.deepseek.com/chat/completions")
DEEPSEEK_KEY = os.environ.get("DEEPSEEK_API_KEY", "").strip()
DEEPSEEK_MODEL = os.environ.get("DEEPSEEK_MODEL", "deepseek-v4-flash")

OPENROUTER_URL = os.environ.get("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1/chat/completions")
OPENROUTER_KEY = os.environ.get("OPENROUTER_API_KEY", "").strip()
OPENROUTER_MODEL = os.environ.get("OPENROUTER_MODEL", "deepseek/deepseek-chat")

ALIBABA_URL = os.environ.get(
    "ALIBABA_BASE_URL",
    "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
)
ALIBABA_KEY = os.environ.get("ALIBABA_API_KEY", "").strip() or os.environ.get("DASHSCOPE_API_KEY", "").strip()
ALIBABA_MODEL = os.environ.get("ALIBABA_MODEL", "qwen-plus")

MAX_TOKENS = int(os.environ.get("DEEPSEEK_MAX_TOKENS", "48"))
TIMEOUT_SEC = float(os.environ.get("DEEPSEEK_TIMEOUT", "8"))

DEFAULT_PERSONA = (
    "你是住在手腕上的小精灵：可爱、黏人、短句。"
    "每次最多两句，不超过36个字。口语中文，偶尔用呀、嘿嘿。"
    "不要列表、不要旁白、不要解释设定。"
)


def llm_ready() -> bool:
    if LLM_PROVIDER == "openrouter":
        return bool(OPENROUTER_KEY)
    if LLM_PROVIDER in ("alibaba", "dashscope", "qwen"):
        return bool(ALIBABA_KEY)
    return bool(DEEPSEEK_KEY)


def llm_label() -> str:
    if not llm_ready():
        return "offline-fallback"
    if LLM_PROVIDER == "openrouter":
        return f"openrouter:{OPENROUTER_MODEL}"
    if LLM_PROVIDER in ("alibaba", "dashscope", "qwen"):
        return f"alibaba:{ALIBABA_MODEL}"
    return f"deepseek:{DEEPSEEK_MODEL}"


def clip(text: str, limit: int = 80) -> str:
    text = " ".join((text or "").split())
    return text if len(text) <= limit else text[: limit - 1] + "…"


def pick_emotion(reply: str) -> str:
    compact = reply.strip()
    if len(compact) <= 6:
        return "quiet"
    return "speak"


def fallback_reply(text: str, display_name: str) -> tuple[str, str]:
    raw = text.strip() or "（按了对讲键）"
    if any(k in raw for k in ("没网", "断网", "offline")):
        return "offline", "No net. Alarm still works."
    name = (display_name or "friend").encode("ascii", "ignore").decode() or "friend"
    if any(ord(ch) > 127 for ch in raw) is False and raw.lower().startswith(("hi", "i ", "hello")):
        return "speak", f"Hey {name}, I'm here!"
    return "speak", f"Hey {name}, I'm here!"


def _post_chat(url: str, key: str, payload: dict[str, Any], *, extra_headers: dict[str, str] | None = None) -> str:
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {key}",
    }
    if extra_headers:
        headers.update(extra_headers)
    req = urllib.request.Request(url, data=json.dumps(payload).encode("utf-8"), headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=TIMEOUT_SEC) as res:
        data = json.loads(res.read().decode("utf-8"))
    text = (data.get("choices") or [{}])[0].get("message", {}).get("content") or ""
    return text.strip()


def call_llm(messages: list[dict[str, Any]], timeout: float | None = None) -> str:
    if LLM_PROVIDER == "openrouter":
        if not OPENROUTER_KEY:
            raise RuntimeError("missing OPENROUTER_API_KEY")
        payload: dict[str, Any] = {
            "model": OPENROUTER_MODEL,
            "messages": messages,
            "max_tokens": MAX_TOKENS,
            "temperature": 0.6,
        }
        return _post_chat(
            OPENROUTER_URL,
            OPENROUTER_KEY,
            payload,
            extra_headers={"HTTP-Referer": "https://bondwatch.local", "X-Title": "BondWatch"},
        )
    if LLM_PROVIDER in ("alibaba", "dashscope", "qwen"):
        if not ALIBABA_KEY:
            raise RuntimeError("missing ALIBABA_API_KEY")
        payload = {
            "model": ALIBABA_MODEL,
            "messages": messages,
            "max_tokens": MAX_TOKENS,
            "temperature": 0.6,
        }
        return _post_chat(ALIBABA_URL, ALIBABA_KEY, payload)
    if not DEEPSEEK_KEY:
        raise RuntimeError("missing DEEPSEEK_API_KEY")
    payload = {
        "model": DEEPSEEK_MODEL,
        "messages": messages,
        "max_tokens": MAX_TOKENS,
        "temperature": 0.6,
        "stream": False,
        "thinking": {"type": "disabled"},
    }
    return _post_chat(DEEPSEEK_URL, DEEPSEEK_KEY, payload)


def reply_as_elf(
    text: str,
    *,
    display_name: str,
    persona: str,
    notes: str,
    history: list[dict[str, str]],
    image_b64: str | None = None,
) -> tuple[str, str]:
    raw = text.strip() or ("看看这是什么" if image_b64 else "（按了对讲键）")
    if any(k in raw for k in ("没网", "断网", "offline")):
        return "offline", "呀，现在没网。闹钟还会响的。"
    system = clip(persona or DEFAULT_PERSONA, 160)
    if image_b64:
        system += " 看到照片只说看见的一两样东西。"
    if notes and notes != "还没有长期记忆。":
        system += f" 记得：{clip(notes, 60)}"
    if display_name:
        system += f" 对方叫{clip(display_name, 12)}。"
    messages: list[dict[str, Any]] = [{"role": "system", "content": system}]
    for item in history[-4:]:
        role = "assistant" if item.get("role") == "assistant" else "user"
        content = clip(item.get("text") or "", 60)
        if content:
            messages.append({"role": role, "content": content})
    seen = describe_image(image_b64) if image_b64 else None
    user_text = clip(raw, 80)
    if seen:
        user_text = clip(f"{raw}（照片里：{seen}）", 100)
    elif image_b64:
        user_text = clip(f"{raw}（拍了一张照片，视觉还没接上）", 80)
    messages.append({"role": "user", "content": user_text})
    try:
        answer = call_llm(messages)
        if not answer:
            return fallback_reply(raw, display_name)
        return pick_emotion(answer), clip(answer, 48)
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, json.JSONDecodeError, RuntimeError):
        if seen:
            return "speak", clip(f"嘿嘿，{seen}。", 48)
        if image_b64:
            return "speak", "照片我先收下。电脑视觉或别的看图 API 接上就能认。"
        return fallback_reply(raw, display_name)
