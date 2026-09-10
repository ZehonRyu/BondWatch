const APP_VERSION = "1.0.1";
const APP_BUILD = 2;

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
  token: localStorage.getItem("bw_token") || "",
  user: null,
  sessionId: "",
  emotion: "idle",
  screenOn: true,
  dnd: false,
  lastPwr: 0,
};

const $ = (id) => document.getElementById(id);

async function api(method, path, body) {
  const res = await fetch(path, {
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
  $("watch-screen").style.background = look.bg;
  $("face").style.setProperty("--skin", look.face);
  $("face").className = `elf ${id}${look.open ? " open" : ""}`;
  $("emotion-label").textContent = look.label;
  $("subtitle").textContent = subtitle || "";
}

function tickClock() {
  const now = new Date();
  $("clock").textContent = now.toTimeString().slice(0, 5);
}

async function afterLogin(data) {
  state.token = data.token;
  localStorage.setItem("bw_token", data.token);
  state.user = data;
  $("auth").classList.add("hidden");
  $("app").classList.remove("hidden");
  $("hello").textContent = `你好，${data.display_name}`;
  $("pair-code").textContent = data.pair_code;
  await refreshAll();
}

async function refreshAll() {
  try {
    const me = await api("GET", "/v1/me");
    state.user = me;
    $("hello").textContent = `你好，${me.display_name}`;
    $("pair-code").textContent = me.pair_code;
    $("bind-state").textContent = me.devices?.length ? `已绑定 ${me.devices.length} 台设备` : "尚未绑定设备";
    $("net-pill").textContent = "云端已连接";
    $("net-pill").classList.remove("off");
    const mem = await api("GET", "/v1/memory");
    $("persona").value = mem.persona || "";
    $("notes").value = mem.notes || "";
    const set = await api("GET", "/v1/settings");
    $("volume").value = set.volume;
    $("vol-label").textContent = set.volume;
    $("dnd").checked = set.dnd;
    $("proactive").checked = set.allow_proactive;
    state.dnd = set.dnd;
    const alarms = await api("GET", "/v1/alarms");
    $("alarm-list").innerHTML = (alarms.items || [])
      .map((item) => `<li>${item.label} · ${new Date(item.fire_at).toLocaleString()}</li>`)
      .join("") || "<li>还没有闹钟</li>";
    const hist = await api("GET", "/v1/history");
    $("log").innerHTML = (hist.items || [])
      .map((item) => `<div class="bubble ${item.role === "user" ? "me" : ""}">${item.text}</div>`)
      .join("");
    const session = await api("POST", "/v1/sessions", { holder: "pc", device_id: "web-companion" });
    state.sessionId = session.session_id;
    if (me.pair_code) {
      try { await api("POST", "/v1/devices/bind", { device_id: "web-companion", code: me.pair_code, name: "网页伴侣" }); } catch (_) {}
    }
    setEmotion(state.dnd ? "silent" : "idle", state.dnd ? "勿扰。字幕仍在。" : "点我或按对讲");
  } catch (error) {
    $("net-pill").textContent = "云端离线";
    $("net-pill").classList.add("off");
    if (String(error.message).includes("token") || String(error.message).includes("401")) {
      localStorage.removeItem("bw_token");
    }
  }
}

async function talk(imageBase64) {
  const text = $("talk-text").value.trim() || (imageBase64 ? "看看这是什么" : "你好");
  if (!state.screenOn) {
    state.screenOn = true;
    $("watch-screen").classList.remove("off");
  }
  setEmotion("listen", imageBase64 ? "看着照片…" : "聆听中…");
  await new Promise((r) => setTimeout(r, 600));
  setEmotion("think", "思考中…");
  try {
    const reply = await api("POST", `/v1/sessions/${state.sessionId}/turn`, {
      text,
      ...(imageBase64 ? { image_base64: imageBase64 } : {}),
    });
    const emo = $("dnd").checked || Number($("volume").value) === 0 ? "silent" : reply.emotion;
    setEmotion(emo, reply.subtitle || reply.text);
    $("talk-text").value = "";
    await refreshAll();
    setTimeout(() => setEmotion($("dnd").checked ? "silent" : "idle", "点我或按对讲"), 2200);
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

$("auth-go").onclick = async () => {
  $("auth-error").textContent = "";
  const mode = document.querySelector(".tab.on").dataset.mode;
  const body = {
    username: $("username").value.trim(),
    password: $("password").value,
    display_name: $("display-name").value.trim() || undefined,
  };
  try {
    const data = await api("POST", mode === "register" ? "/v1/auth/register" : "/v1/auth/login", body);
    await afterLogin(data);
  } catch (error) {
    $("auth-error").textContent = error.message;
  }
};

document.querySelectorAll(".tab").forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll(".tab").forEach((item) => item.classList.remove("on"));
    btn.classList.add("on");
    $("name-row").classList.toggle("hidden", btn.dataset.mode !== "register");
  };
});

document.querySelectorAll(".nav-btn").forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll(".nav-btn").forEach((item) => item.classList.remove("on"));
    document.querySelectorAll(".page").forEach((item) => item.classList.remove("on"));
    btn.classList.add("on");
    $(`page-${btn.dataset.page}`).classList.add("on");
  };
});

$("ptt").onclick = () => talk();
$("talk-go").onclick = () => talk();
$("cam").onclick = () => $("cam-file").click();
$("cam-file").onchange = async () => {
  const file = $("cam-file").files && $("cam-file").files[0];
  $("cam-file").value = "";
  if (!file) return;
  try {
    await talk(await fileToJpegBase64(file));
  } catch (error) {
    setEmotion("offline", error.message);
  }
};
$("watch-screen").onclick = () => {
  if (!state.screenOn) {
    state.screenOn = true;
    $("watch-screen").classList.remove("off");
    setEmotion(state.dnd ? "silent" : "idle", "点我或按对讲");
    return;
  }
  if (state.emotion === "speak") {
    setEmotion(state.dnd ? "silent" : "idle", "已打断");
    return;
  }
  talk();
};
$("pwr").onclick = () => {
  const now = Date.now();
  if (now - state.lastPwr < 350) {
    $("dnd").checked = !$("dnd").checked;
    $("dnd").dispatchEvent(new Event("change"));
  } else {
    state.screenOn = !state.screenOn;
    $("watch-screen").classList.toggle("off", !state.screenOn);
  }
  state.lastPwr = now;
};

$("save-memory").onclick = async () => {
  await api("PUT", "/v1/memory", { persona: $("persona").value, notes: $("notes").value });
  await refreshAll();
};
$("add-alarm").onclick = async () => {
  await api("PUT", "/v1/alarms", { minutes: 1, label: "本地闹钟", cached_line: "到点了。" });
  await refreshAll();
};
$("volume").oninput = () => { $("vol-label").textContent = $("volume").value; };
$("volume").onchange = () => api("PUT", "/v1/settings", { volume: Number($("volume").value) });
$("dnd").onchange = async () => {
  state.dnd = $("dnd").checked;
  await api("PUT", "/v1/settings", { dnd: state.dnd });
  setEmotion(state.dnd ? "silent" : "idle", state.dnd ? "勿扰。字幕仍在。" : "勿扰已关");
};
$("proactive").onchange = () => api("PUT", "/v1/settings", { allow_proactive: $("proactive").checked });
$("logout").onclick = () => {
  localStorage.removeItem("bw_token");
  location.reload();
};

setInterval(tickClock, 1000);
tickClock();

$("reload-app").onclick = () => location.reload();

if ("serviceWorker" in navigator) {
  navigator.serviceWorker.register("/static/sw.js?v=" + APP_VERSION).catch(() => {});
}

(async () => {
  const cfg = await api("GET", `/v1/updates?t=${Date.now()}`).catch(() => ({ web_url: location.origin }));
  const downloadHref = cfg.download_page || "/download";
  $("public-url").href = downloadHref;
  $("public-url").textContent = downloadHref;
  if (cfg.apk_url) {
    $("download-box").insertAdjacentHTML("beforeend", `<p><a href="${cfg.apk_url}">下载 Android APK</a></p>`);
  }
  if ((cfg.build || 0) > APP_BUILD) {
    $("update-text").textContent = `网页有新版本 v${cfg.version}：${cfg.notes || "点刷新即可"}`;
    $("update-banner").classList.remove("hidden");
  }
  if (state.token) {
    try {
      const me = await api("GET", "/v1/me");
      await afterLogin({ token: state.token, ...me });
    } catch (_) {
      localStorage.removeItem("bw_token");
    }
  }
})();
