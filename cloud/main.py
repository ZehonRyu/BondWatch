from __future__ import annotations

import hashlib
import json
import os
import secrets
import threading
import uuid
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

import psycopg
from fastapi import Depends, FastAPI, Header, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles
from psycopg.rows import dict_row
from pydantic import BaseModel, Field

from llm import DEFAULT_PERSONA, llm_ready, reply_as_elf
from vision import vision_backend

STATIC = Path(__file__).with_name("static")
DOWNLOADS = STATIC / "downloads"
VERSION_FILE = STATIC / "version.json"
DATABASE_URL = os.environ.get(
    "DATABASE_URL",
    "postgresql://bondwatch:bondwatch@127.0.0.1:5432/bondwatch",
)
PUBLIC_URL = os.environ.get("PUBLIC_URL", "http://127.0.0.1:11111")
WATCH_SIM_PORT = int(os.environ.get("WATCH_SIM_PORT", "0") or "0")
WATCH_SIM_URL = os.environ.get("WATCH_SIM_URL", "http://192.168.1.63:11112")

app = FastAPI(title="BondWatch", version="v1")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def load_version() -> dict[str, Any]:
    if VERSION_FILE.exists():
        return json.loads(VERSION_FILE.read_text(encoding="utf-8"))
    return {"version": "1.0.0", "build": 1, "notes": ""}


def apk_path() -> Path:
    return DOWNLOADS / "bondwatch.apk"


def update_payload() -> dict[str, Any]:
    info = load_version()
    apk = apk_path()
    ready = apk.exists()
    return {
        "version": info.get("version", "1.0.0"),
        "build": int(info.get("build") or 1),
        "notes": info.get("notes") or "",
        "apk_ready": ready,
        "apk_url": f"{PUBLIC_URL}/downloads/bondwatch.apk" if ready else None,
        "apk_size": apk.stat().st_size if ready else 0,
        "download_page": f"{PUBLIC_URL}/download",
        "web_url": PUBLIC_URL,
    }


def connect() -> psycopg.Connection:
    return psycopg.connect(DATABASE_URL, row_factory=dict_row)


def hash_password(password: str, salt: str | None = None) -> str:
    salt = salt or secrets.token_hex(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), 180_000)
    return f"{salt}${digest.hex()}"


def verify_password(password: str, stored: str) -> bool:
    salt, _, _digest = stored.partition("$")
    return secrets.compare_digest(stored, hash_password(password, salt))


def seed_user(conn: psycopg.Connection, username: str, password: str, display_name: str, pair_code: str) -> str:
    user_id = f"u_{uuid.uuid4().hex[:10]}"
    conn.execute(
        "INSERT INTO users(id, username, display_name, password_hash, pair_code, created_at) VALUES(%s,%s,%s,%s,%s,%s)",
        (user_id, username, display_name, hash_password(password), pair_code, utcnow()),
    )
    conn.execute(
        "INSERT INTO memory(user_id, persona, notes) VALUES(%s,%s,%s)",
        (user_id, DEFAULT_PERSONA, "还没有长期记忆。"),
    )
    conn.execute(
        "INSERT INTO settings(user_id, volume, dnd, voice_mode, allow_proactive, daily_limit, occasions) VALUES(%s,%s,%s,%s,%s,%s,%s)",
        (user_id, 70, False, "tap", False, 3, "alarm_after,idle_pc"),
    )
    return user_id


def unique_pair_code(conn: psycopg.Connection) -> str:
    for _ in range(30):
        code = f"{secrets.randbelow(9000) + 1000}"
        if not conn.execute("SELECT 1 FROM users WHERE pair_code=%s", (code,)).fetchone():
            return code
    return secrets.token_hex(3)


def init_db() -> None:
    with connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS users (
              id TEXT PRIMARY KEY,
              username TEXT UNIQUE NOT NULL,
              display_name TEXT NOT NULL,
              password_hash TEXT NOT NULL,
              pair_code TEXT UNIQUE NOT NULL,
              created_at TIMESTAMPTZ NOT NULL
            );
            CREATE TABLE IF NOT EXISTS tokens (
              token TEXT PRIMARY KEY,
              user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
              created_at TIMESTAMPTZ NOT NULL
            );
            CREATE TABLE IF NOT EXISTS devices (
              id TEXT PRIMARY KEY,
              user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
              name TEXT NOT NULL,
              bound_at TIMESTAMPTZ NOT NULL
            );
            CREATE TABLE IF NOT EXISTS sessions (
              id TEXT PRIMARY KEY,
              user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
              holder TEXT NOT NULL,
              created_at TIMESTAMPTZ NOT NULL
            );
            CREATE TABLE IF NOT EXISTS messages (
              id TEXT PRIMARY KEY,
              session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
              role TEXT NOT NULL,
              text TEXT NOT NULL,
              emotion TEXT NOT NULL,
              created_at TIMESTAMPTZ NOT NULL
            );
            CREATE TABLE IF NOT EXISTS memory (
              user_id TEXT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
              persona TEXT NOT NULL,
              notes TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS alarms (
              id TEXT PRIMARY KEY,
              user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
              fire_at TIMESTAMPTZ NOT NULL,
              label TEXT NOT NULL,
              cached_line TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS settings (
              user_id TEXT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
              volume INTEGER NOT NULL,
              dnd BOOLEAN NOT NULL,
              voice_mode TEXT NOT NULL,
              allow_proactive BOOLEAN NOT NULL,
              daily_limit INTEGER NOT NULL,
              occasions TEXT NOT NULL
            );
            """
        )
        if not conn.execute("SELECT 1 FROM users WHERE username=%s", ("demo",)).fetchone():
            seed_user(conn, "demo", "demo123", "演示账号", "4242")
        conn.execute("UPDATE memory SET persona=%s", (DEFAULT_PERSONA,))
        conn.commit()


def current_user(authorization: str | None = Header(default=None)) -> dict[str, Any]:
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(401, "missing token")
    token = authorization.split(" ", 1)[1].strip()
    with connect() as conn:
        row = conn.execute(
            """
            SELECT u.id, u.username, u.display_name, u.pair_code, u.created_at
            FROM tokens t JOIN users u ON u.id = t.user_id
            WHERE t.token=%s
            """,
            (token,),
        ).fetchone()
    if not row:
        raise HTTPException(401, "bad token")
    return dict(row)


class AuthIn(BaseModel):
    username: str = Field(min_length=3, max_length=24)
    password: str = Field(min_length=6, max_length=72)
    display_name: str | None = None


class BindIn(BaseModel):
    device_id: str = "watch-sim-1"
    code: str
    name: str = "假手表"


class SessionIn(BaseModel):
    holder: str = Field(pattern="^(watch|phone|pc)$")
    device_id: str = "watch-sim-1"


class TurnIn(BaseModel):
    text: str = ""
    image_base64: str | None = None


class WatchBootIn(BaseModel):
    device_id: str = "watch-esp32-1"
    name: str = "BondWatch"


class TakeoverIn(BaseModel):
    action: str = Field(pattern="^(takeover|listen|release)$")
    holder: str = Field(default="pc", pattern="^(watch|phone|pc)$")


class MemoryIn(BaseModel):
    persona: str | None = None
    notes: str | None = None


class AlarmIn(BaseModel):
    minutes: int = 1
    label: str = "本地闹钟"
    cached_line: str = "到点了。"


class SettingsIn(BaseModel):
    volume: int | None = None
    dnd: bool | None = None
    voice_mode: str | None = None
    allow_proactive: bool | None = None
    daily_limit: int | None = None
    occasions: str | None = None


def _start_watch_sim() -> None:
    if WATCH_SIM_PORT <= 0:
        return
    from watchsim import serve

    threading.Thread(target=serve, args=(WATCH_SIM_PORT,), daemon=True).start()


@app.on_event("startup")
def on_startup() -> None:
    DOWNLOADS.mkdir(parents=True, exist_ok=True)
    init_db()
    _start_watch_sim()


@app.get("/health")
def health() -> dict[str, Any]:
    return {
        "ok": "bondwatch",
        "time": utcnow().isoformat(),
        "public_url": PUBLIC_URL,
        "llm": "deepseek-v4-flash" if llm_ready() else "offline-fallback",
        "vision": vision_backend(),
        "watch_sim": WATCH_SIM_URL if WATCH_SIM_PORT else None,
    }


@app.get("/v1/config")
def public_config() -> dict[str, Any]:
    payload = update_payload()
    return {
        "public_url": PUBLIC_URL,
        "apk_url": "/downloads/bondwatch.apk" if payload["apk_ready"] else None,
        "download_page": "/download",
        "version": payload["version"],
        "build": payload["build"],
        "demo": {"username": "demo", "password": "demo123"},
    }


@app.get("/v1/updates")
def updates() -> dict[str, Any]:
    return update_payload()


@app.post("/v1/auth/register")
def register(body: AuthIn) -> dict[str, Any]:
    username = body.username.strip().lower()
    if not username.isalnum():
        raise HTTPException(400, "用户名只能是字母和数字")
    with connect() as conn:
        if conn.execute("SELECT 1 FROM users WHERE username=%s", (username,)).fetchone():
            raise HTTPException(409, "用户名已存在")
        user_id = seed_user(
            conn,
            username,
            body.password,
            body.display_name or username,
            unique_pair_code(conn),
        )
        token = secrets.token_hex(24)
        conn.execute(
            "INSERT INTO tokens(token, user_id, created_at) VALUES(%s,%s,%s)",
            (token, user_id, utcnow()),
        )
        user = conn.execute(
            "SELECT id, username, display_name, pair_code FROM users WHERE id=%s",
            (user_id,),
        ).fetchone()
        conn.commit()
    return {"token": token, **dict(user)}


@app.post("/v1/auth/login")
def login(body: AuthIn) -> dict[str, Any]:
    with connect() as conn:
        user = conn.execute(
            "SELECT id, username, display_name, pair_code, password_hash FROM users WHERE username=%s",
            (body.username.strip().lower(),),
        ).fetchone()
        if not user or not verify_password(body.password, user["password_hash"]):
            raise HTTPException(401, "账号或密码不对")
        token = secrets.token_hex(24)
        conn.execute(
            "INSERT INTO tokens(token, user_id, created_at) VALUES(%s,%s,%s)",
            (token, user["id"], utcnow()),
        )
        conn.commit()
    return {
        "token": token,
        "id": user["id"],
        "username": user["username"],
        "display_name": user["display_name"],
        "pair_code": user["pair_code"],
    }


@app.get("/v1/me")
def me(user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    with connect() as conn:
        devices = conn.execute(
            "SELECT id, name, bound_at FROM devices WHERE user_id=%s",
            (user["id"],),
        ).fetchall()
    return {
        "id": user["id"],
        "username": user["username"],
        "display_name": user["display_name"],
        "pair_code": user["pair_code"],
        "devices": [dict(item) for item in devices],
    }


@app.post("/v1/devices/bind")
def bind(body: BindIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    if body.code != user["pair_code"]:
        raise HTTPException(400, "配对码不对")
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO devices(id, user_id, name, bound_at) VALUES(%s,%s,%s,%s)
            ON CONFLICT (id) DO UPDATE SET user_id=EXCLUDED.user_id, name=EXCLUDED.name, bound_at=EXCLUDED.bound_at
            """,
            (body.device_id, user["id"], body.name, utcnow()),
        )
        conn.commit()
    return {"ok": True, "device_id": body.device_id, "code": user["pair_code"]}


@app.post("/v1/sessions")
def open_session(body: SessionIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, str]:
    with connect() as conn:
        live = conn.execute(
            "SELECT id, holder FROM sessions WHERE user_id=%s ORDER BY created_at DESC LIMIT 1",
            (user["id"],),
        ).fetchone()
        if live:
            return {"session_id": live["id"], "holder": live["holder"]}
        session_id = uuid.uuid4().hex[:12]
        conn.execute(
            "INSERT INTO sessions(id, user_id, holder, created_at) VALUES(%s,%s,%s,%s)",
            (session_id, user["id"], body.holder, utcnow()),
        )
        conn.commit()
    return {"session_id": session_id, "holder": body.holder}


@app.post("/v1/watch/boot")
def watch_boot(body: WatchBootIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    """ESP32 one-shot: bind device + open/reuse a watch session."""
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO devices(id, user_id, name, bound_at) VALUES(%s,%s,%s,%s)
            ON CONFLICT (id) DO UPDATE SET user_id=EXCLUDED.user_id, name=EXCLUDED.name, bound_at=EXCLUDED.bound_at
            """,
            (body.device_id, user["id"], body.name, utcnow()),
        )
        live = conn.execute(
            "SELECT id, holder FROM sessions WHERE user_id=%s ORDER BY created_at DESC LIMIT 1",
            (user["id"],),
        ).fetchone()
        if live:
            session_id = live["id"]
            if live["holder"] != "watch":
                conn.execute("UPDATE sessions SET holder=%s WHERE id=%s", ("watch", session_id))
        else:
            session_id = uuid.uuid4().hex[:12]
            conn.execute(
                "INSERT INTO sessions(id, user_id, holder, created_at) VALUES(%s,%s,%s,%s)",
                (session_id, user["id"], "watch", utcnow()),
            )
        token = secrets.token_hex(24)
        conn.execute(
            "INSERT INTO tokens(token, user_id, created_at) VALUES(%s,%s,%s)",
            (token, user["id"], utcnow()),
        )
        conn.commit()
    return {
        "ok": True,
        "token": token,
        "session_id": session_id,
        "device_id": body.device_id,
        "display_name": user["display_name"],
        "pair_code": user["pair_code"],
    }


@app.post("/v1/sessions/{session_id}/turn")
def turn(session_id: str, body: TurnIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, str]:
    with connect() as conn:
        session = conn.execute(
            "SELECT id, holder FROM sessions WHERE id=%s AND user_id=%s",
            (session_id, user["id"]),
        ).fetchone()
        if not session:
            raise HTTPException(404, "session not found")
        mem = conn.execute("SELECT persona, notes FROM memory WHERE user_id=%s", (user["id"],)).fetchone()
        recent = conn.execute(
            """
            SELECT role, text FROM messages
            WHERE session_id=%s
            ORDER BY created_at DESC
            LIMIT 4
            """,
            (session_id,),
        ).fetchall()
    persona = mem["persona"] if mem else DEFAULT_PERSONA
    notes = mem["notes"] if mem else ""
    # Tiny Latin font on the watch: force short English for holder=watch.
    if session["holder"] == "watch":
        persona = (
            "You are a tiny wrist elf. Reply in simple English only, max 28 characters, "
            "one short sentence, cute tone. No Chinese characters."
        )
    history = list(reversed([{"role": row["role"], "text": row["text"]} for row in recent]))
    emotion, reply = reply_as_elf(
        body.text,
        display_name=user["display_name"],
        persona=persona,
        notes=notes,
        history=history,
        image_b64=body.image_base64,
    )
    if session["holder"] == "watch":
        ascii_reply = "".join(ch for ch in reply if 32 <= ord(ch) < 127).strip(" ~-_")
        reply = ascii_reply[:28] if len(ascii_reply) >= 3 else "Hi! I'm here."
        if emotion not in ("speak", "quiet", "offline", "alarm", "silent"):
            emotion = "speak"
    user_line = body.text.strip() or ("[照片]" if body.image_base64 else "")
    if body.image_base64 and not user_line.startswith("[照片]"):
        user_line = f"[照片] {user_line}".strip()
    with connect() as conn:
        if body.text.strip().startswith("记住") or "我喜欢" in body.text:
            conn.execute("UPDATE memory SET notes=%s WHERE user_id=%s", (body.text.strip(), user["id"]))
        now = utcnow()
        conn.execute(
            "INSERT INTO messages(id, session_id, role, text, emotion, created_at) VALUES(%s,%s,%s,%s,%s,%s)",
            (uuid.uuid4().hex, session_id, "user", user_line, "listen", now),
        )
        conn.execute(
            "INSERT INTO messages(id, session_id, role, text, emotion, created_at) VALUES(%s,%s,%s,%s,%s,%s)",
            (uuid.uuid4().hex, session_id, "assistant", reply, emotion, now),
        )
        conn.commit()
    return {"emotion": emotion, "text": reply, "subtitle": reply}


@app.post("/v1/sessions/{session_id}/takeover")
def takeover(session_id: str, body: TakeoverIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, str]:
    holder = body.holder if body.action == "takeover" else "watch"
    with connect() as conn:
        if not conn.execute(
            "SELECT 1 FROM sessions WHERE id=%s AND user_id=%s",
            (session_id, user["id"]),
        ).fetchone():
            raise HTTPException(404, "session not found")
        conn.execute("UPDATE sessions SET holder=%s WHERE id=%s", (holder, session_id))
        conn.commit()
    return {"session_id": session_id, "holder": holder, "action": body.action}


@app.get("/v1/memory")
def get_memory(user: dict[str, Any] = Depends(current_user)) -> dict[str, str]:
    with connect() as conn:
        row = conn.execute("SELECT persona, notes FROM memory WHERE user_id=%s", (user["id"],)).fetchone()
    return {"persona": row["persona"] if row else "", "notes": row["notes"] if row else ""}


@app.put("/v1/memory")
def put_memory(body: MemoryIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, str]:
    current = get_memory(user)
    persona = body.persona if body.persona is not None else current["persona"]
    notes = body.notes if body.notes is not None else current["notes"]
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO memory(user_id, persona, notes) VALUES(%s,%s,%s)
            ON CONFLICT (user_id) DO UPDATE SET persona=EXCLUDED.persona, notes=EXCLUDED.notes
            """,
            (user["id"], persona, notes),
        )
        conn.commit()
    return {"persona": persona, "notes": notes}


@app.get("/v1/alarms")
def get_alarms(user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    with connect() as conn:
        rows = conn.execute(
            "SELECT id, fire_at, label, cached_line FROM alarms WHERE user_id=%s ORDER BY fire_at",
            (user["id"],),
        ).fetchall()
    return {
        "items": [
            {
                "id": row["id"],
                "fire_at": row["fire_at"].isoformat(),
                "label": row["label"],
                "cached_line": row["cached_line"],
            }
            for row in rows
        ]
    }


@app.put("/v1/alarms")
def put_alarm(body: AlarmIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    item = {
        "id": uuid.uuid4().hex[:8],
        "fire_at": utcnow() + timedelta(minutes=max(1, body.minutes)),
        "label": body.label,
        "cached_line": body.cached_line,
    }
    with connect() as conn:
        conn.execute(
            "INSERT INTO alarms(id, user_id, fire_at, label, cached_line) VALUES(%s,%s,%s,%s,%s)",
            (item["id"], user["id"], item["fire_at"], item["label"], item["cached_line"]),
        )
        conn.commit()
    return {**item, "fire_at": item["fire_at"].isoformat()}


@app.delete("/v1/alarms/{alarm_id}")
def delete_alarm(alarm_id: str, user: dict[str, Any] = Depends(current_user)) -> dict[str, bool]:
    with connect() as conn:
        conn.execute("DELETE FROM alarms WHERE id=%s AND user_id=%s", (alarm_id, user["id"]))
        conn.commit()
    return {"ok": True}


@app.get("/v1/settings")
def get_settings(user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    with connect() as conn:
        row = conn.execute(
            "SELECT volume, dnd, voice_mode, allow_proactive, daily_limit, occasions FROM settings WHERE user_id=%s",
            (user["id"],),
        ).fetchone()
    if not row:
        raise HTTPException(404, "no settings")
    return dict(row)


@app.put("/v1/settings")
def put_settings(body: SettingsIn, user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    current = get_settings(user)
    merged = {
        "volume": body.volume if body.volume is not None else current["volume"],
        "dnd": body.dnd if body.dnd is not None else current["dnd"],
        "voice_mode": body.voice_mode or current["voice_mode"],
        "allow_proactive": body.allow_proactive if body.allow_proactive is not None else current["allow_proactive"],
        "daily_limit": body.daily_limit if body.daily_limit is not None else current["daily_limit"],
        "occasions": body.occasions if body.occasions is not None else current["occasions"],
    }
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO settings(user_id, volume, dnd, voice_mode, allow_proactive, daily_limit, occasions)
            VALUES(%s,%s,%s,%s,%s,%s,%s)
            ON CONFLICT (user_id) DO UPDATE SET
              volume=EXCLUDED.volume, dnd=EXCLUDED.dnd, voice_mode=EXCLUDED.voice_mode,
              allow_proactive=EXCLUDED.allow_proactive, daily_limit=EXCLUDED.daily_limit, occasions=EXCLUDED.occasions
            """,
            (
                user["id"],
                merged["volume"],
                merged["dnd"],
                merged["voice_mode"],
                merged["allow_proactive"],
                merged["daily_limit"],
                merged["occasions"],
            ),
        )
        conn.commit()
    return get_settings(user)


@app.get("/v1/history")
def history(user: dict[str, Any] = Depends(current_user)) -> dict[str, Any]:
    with connect() as conn:
        rows = conn.execute(
            """
            SELECT m.role, m.text, m.emotion, m.created_at
            FROM messages m
            JOIN sessions s ON s.id = m.session_id
            WHERE s.user_id=%s
            ORDER BY m.created_at DESC
            LIMIT 50
            """,
            (user["id"],),
        ).fetchall()
    items = list(reversed(rows))
    return {
        "items": [
            {
                "role": row["role"],
                "text": row["text"],
                "emotion": row["emotion"],
                "created_at": row["created_at"].isoformat(),
            }
            for row in items
        ]
    }


@app.api_route("/downloads/bondwatch.apk", methods=["GET", "HEAD"])
def download_apk(request: Request):
    apk = apk_path()
    if not apk.exists():
        raise HTTPException(404, "APK 还没打出来。这台电脑缺少 Android SDK。")
    headers = {
        "Cache-Control": "no-store",
        "Content-Disposition": 'attachment; filename="bondwatch.apk"',
        "Content-Length": str(apk.stat().st_size),
    }
    if request.method == "HEAD":
        return Response(
            status_code=200,
            media_type="application/vnd.android.package-archive",
            headers=headers,
        )
    return FileResponse(
        apk,
        filename="bondwatch.apk",
        media_type="application/vnd.android.package-archive",
        headers=headers,
    )


@app.get("/download")
def download_page() -> FileResponse:
    return FileResponse(STATIC / "download.html")


@app.get("/")
def home() -> FileResponse:
    return FileResponse(STATIC / "index.html")


app.mount("/static", StaticFiles(directory=STATIC), name="static")
