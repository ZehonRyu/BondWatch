const API = (window.BONDWATCH_API || "http://192.168.1.63:11111").replace(/\/$/, "");
const faces = {
  idle: { bg: "#102a44", face: "#5d9cec", label: "待机", open: false },
  listen: { bg: "#0b3d0b", face: "#2ecc71", label: "聆听", open: true },
  think: { bg: "#2a1248", face: "#c39bd3", label: "思考", open: false },
  speak: { bg: "#4a2a00", face: "#f5b041", label: "说话", open: true },
  quiet: { bg: "#2c2c2c", face: "#b0b0b0", label: "小声", open: false },
  silent: { bg: "#111111", face: "#6e6e6e", label: "无声", open: false },
  alarm: { bg: "#4a0000", face: "#e74c3c", label: "闹钟", open: true },
  offline: { bg: "#1b1b1b", face: "#7f8c8d", label: "没网", open: false },
};

const state = {
  token: "",
  sessionId: "",
  emotion: "idle",
  screenOn: true,
  dnd: false,
  lastPwr: 0,
};

const $ = (id) => document.getElementById(id);

async function api(method, path, body) {
  const res = await fetch(API + path, {
    method,
    headers: {
      "Content-Type": "application/json",
      ...(state.token ? { Authorization: `Bearer ${state.token}` } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.detail || res.statusText);
  return data;
}

function setEmotion(id, subtitle) {
  state.emotion = id;
  const look = faces[id] || faces.idle;
  $("screen").style.background = look.bg;
  $("face").style.setProperty("--skin", look.face);
  $("face").className = `elf ${id}${look.open ? " open" : ""}`;
  $("emotion").textContent = look.label;
  $("subtitle").textContent = subtitle || "";
  if (window.Elf3D) {
    window.Elf3D.setEmotion(id);
    window.Elf3D.setSkin(look.face);
    window.Elf3D.setOn(state.screenOn);
  }
}

function paintHint() {
  const el = $("hint");
  if (!el || el.dataset.offline === "1") return;
  const wide = window.isWatchLandscape ? window.isWatchLandscape() : false;
  el.textContent = wide ? "横屏 · 大脸耷拉" : "竖屏 · 全身挥手";
}

window.addEventListener("watch-orient", paintHint);

function tickClock() {
  $("clock").textContent = new Date().toTimeString().slice(0, 5);
}

async function boot() {
  try {
    const login = await api("POST", "/v1/auth/login", { username: "demo", password: "demo123" });
    state.token = login.token;
    const session = await api("POST", "/v1/sessions", { holder: "watch", device_id: "iphone-watch-sim" });
    state.sessionId = session.session_id;
    if (login.pair_code) {
      try {
        await api("POST", "/v1/devices/bind", {
          device_id: "iphone-watch-sim",
          code: login.pair_code,
          name: "iPhone 表盘模拟",
        });
      } catch (_) {}
    }
    const set = await api("GET", "/v1/settings").catch(() => ({}));
    state.dnd = Boolean(set.dnd);
    $("hint").dataset.offline = "";
    paintHint();
    setEmotion(state.dnd ? "silent" : "idle", state.dnd ? "勿扰。字幕仍在。" : "点屏幕说话");
  } catch (error) {
    $("hint").dataset.offline = "1";
    $("hint").textContent = "云端还没连上";
    setEmotion("offline", error.message || "没网");
  }
}

async function talk(imageBase64) {
  if (!state.screenOn) {
    state.screenOn = true;
    $("screen").classList.remove("off");
  }
  setEmotion("listen", imageBase64 ? "看着照片…" : "聆听中…");
  await new Promise((r) => setTimeout(r, 500));
  setEmotion("think", "思考中…");
  if (!state.sessionId) {
    setEmotion("offline", "云端还没连上");
    return;
  }
  try {
    const reply = await api("POST", `/v1/sessions/${state.sessionId}/turn`, {
      text: imageBase64 ? "看看这是什么" : "你好",
      ...(imageBase64 ? { image_base64: imageBase64 } : {}),
    });
    setEmotion(state.dnd ? "silent" : reply.emotion || "speak", reply.subtitle || reply.text || "");
    setTimeout(() => {
      setEmotion(state.dnd ? "silent" : "idle", state.dnd ? "勿扰。字幕仍在。" : "点屏幕说话");
    }, 2200);
  } catch (error) {
    setEmotion("offline", error.message);
  }
}

function fileToJpegBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("读不了这张图"));
    reader.onload = () => {
      const img = new Image();
      img.onload = () => {
        const side = 320;
        const canvas = document.createElement("canvas");
        const scale = Math.min(side / img.width, side / img.height, 1);
        canvas.width = Math.max(1, Math.round(img.width * scale));
        canvas.height = Math.max(1, Math.round(img.height * scale));
        canvas.getContext("2d").drawImage(img, 0, 0, canvas.width, canvas.height);
        resolve(canvas.toDataURL("image/jpeg", 0.35).split(",")[1]);
      };
      img.onerror = () => reject(new Error("图坏了"));
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

$("screen").addEventListener("click", () => {
  if (window.enableWatchMotion) window.enableWatchMotion();
  if (!state.screenOn) {
    state.screenOn = true;
    $("screen").classList.remove("off");
    setEmotion(state.dnd ? "silent" : "idle", "点屏幕说话");
    return;
  }
  if (state.emotion === "speak") {
    setEmotion(state.dnd ? "silent" : "idle", "已打断");
    return;
  }
  talk();
});

$("ptt").addEventListener("click", () => {
  if (state.emotion === "speak") {
    setEmotion(state.dnd ? "silent" : "idle", "已打断");
    return;
  }
  talk();
});

$("pwr").addEventListener("click", () => {
  const now = Date.now();
  if (now - state.lastPwr < 350) {
    state.dnd = !state.dnd;
    setEmotion(state.dnd ? "silent" : "idle", state.dnd ? "勿扰。字幕仍在。" : "勿扰已关");
  } else {
    state.screenOn = !state.screenOn;
    $("screen").classList.toggle("off", !state.screenOn);
    if (window.Elf3D) window.Elf3D.setOn(state.screenOn);
  }
  state.lastPwr = now;
});

$("cam").addEventListener("click", () => $("cam-file").click());
$("cam-file").addEventListener("change", async () => {
  const file = $("cam-file").files && $("cam-file").files[0];
  $("cam-file").value = "";
  if (!file) return;
  try {
    await talk(await fileToJpegBase64(file));
  } catch (error) {
    setEmotion("offline", error.message);
  }
});

document.addEventListener("touchmove", (event) => event.preventDefault(), { passive: false });
window.addEventListener("elf3d-ready", () => {
  document.body.classList.add("has-3d");
  setEmotion(state.emotion, $("subtitle").textContent);
});
setInterval(tickClock, 1000);
tickClock();
boot();
