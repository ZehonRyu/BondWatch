const APP_VERSION = "2.0.0";
const APP_BUILD = 7;

const faces = {
  idle: { mode: "idle", face: "#ff6b6b", label: "开始对话", pill: "开始对话", open: false },
  listen: { mode: "listen", face: "#ff6b6b", label: "正在聆听...", pill: "正在聆听...", open: true },
  think: { mode: "think", face: "#ff8787", label: "思考中...", pill: "思考中...", open: false },
  speak: { mode: "idle", face: "#ff6b6b", label: "说话", pill: "说话中", open: true },
  quiet: { mode: "idle", face: "#ffb4b4", label: "小声", pill: "小声", open: false },
  silent: { mode: "idle", face: "#ccc", label: "无声", pill: "无声", open: false },
  alarm: { mode: "listen", face: "#ff4444", label: "闹钟", pill: "闹钟!", open: true },
  offline: { mode: "listen", face: "#999", label: "没网", pill: "没网", open: false },
};

const state = {
  token: localStorage.getItem("bw_token") || "",
  user: null,
  sessionId: "",
  emotion: "idle",
  screenOn: true,
  dnd: false,
  lastPwr: 0,
  faceSeq: 0,
  audioSeq: 0,
  talking: false,
  replyAudio: null,
};

function speechApi() {
  return window.SpeechRecognition || window.webkitSpeechRecognition || null;
}

function speechUsable() {
  return !!(speechApi() && window.isSecureContext);
}

function blobToBase64(blob) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result).split(",")[1] || "");
    reader.onerror = () => reject(new Error("读不了录音"));
    reader.readAsDataURL(blob);
  });
}

function listenOnce() {
  const Ctor = speechApi();
  if (!Ctor) return Promise.reject(new Error("这个浏览器不支持语音识别，请用 Chrome/Edge，或改用文字发送"));
  return new Promise((resolve, reject) => {
    const rec = new Ctor();
    rec.lang = "zh-CN";
    rec.interimResults = false;
    rec.maxAlternatives = 1;
    let done = false;
    const finish = (fn, value) => {
      if (done) return;
      done = true;
      try { rec.stop(); } catch (_) {}
      fn(value);
    };
    rec.onresult = (event) => {
      const line = event.results?.[0]?.[0]?.transcript || "";
      finish(resolve, line.trim());
    };
    rec.onerror = (event) => finish(reject, new Error(event.error || "语音识别失败"));
    rec.onend = () => {
      if (!done) finish(reject, new Error("没听到声音，再试一次"));
    };
    try {
      rec.start();
    } catch (error) {
      finish(reject, error);
    }
  });
}

async function recordOnceMs(ms = 4500) {
  if (!navigator.mediaDevices?.getUserMedia) {
    throw new Error("无法访问麦克风");
  }
  const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
  const mime = MediaRecorder.isTypeSupported("audio/webm;codecs=opus")
    ? "audio/webm;codecs=opus"
    : "audio/webm";
  const rec = new MediaRecorder(stream, { mimeType: mime });
  const chunks = [];
  rec.ondataavailable = (e) => { if (e.data?.size) chunks.push(e.data); };
  const stopped = new Promise((resolve) => { rec.onstop = resolve; });
  rec.start();
  await new Promise((r) => setTimeout(r, ms));
  rec.stop();
  await stopped;
  stream.getTracks().forEach((t) => t.stop());
  return new Blob(chunks, { type: mime });
}

function speakBrowser(text) {
  if (!window.speechSynthesis || !text) return Promise.resolve();
  return new Promise((resolve) => {
    window.speechSynthesis.cancel();
    const utter = new SpeechSynthesisUtterance(text);
    utter.lang = "zh-CN";
    utter.rate = 1.05;
    utter.onend = () => resolve();
    utter.onerror = () => resolve();
    window.speechSynthesis.speak(utter);
  });
}

async function playReplyAudio(ttsUrl, text) {
  const vol = Number($("volume").value);
  if ($("dnd").checked || vol === 0) return;
  if (ttsUrl) {
    try {
      if (state.replyAudio) {
        state.replyAudio.pause();
        state.replyAudio = null;
      }
      const audio = new Audio(`${ttsUrl}?t=${Date.now()}`);
      audio.volume = Math.max(0.05, vol / 100);
      state.replyAudio = audio;
      await audio.play();
      return;
    } catch (_) {
      /* fallback below */
    }
  }
  await speakBrowser(text);
}

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
  const screen = $("watch-screen");
  screen.classList.remove("mode-idle", "mode-listen", "mode-think");
  screen.classList.add(`mode-${look.mode}`);
  $("face").style.setProperty("--skin", look.face);
  $("face").className = `elf ${id}${look.open ? " open" : ""}`;
  $("emotion-label").textContent = subtitle ? subtitle.slice(0, 16) : look.pill;
  $("subtitle").textContent = subtitle || "";
  const rec = id === "listen";
  screen.classList.toggle("rec", rec);
  $("rec-line").classList.toggle("hidden", !rec);
  $("vu").classList.toggle("hidden", !rec);
  if (window.Elf3D) {
    window.Elf3D.setEmotion(id);
    window.Elf3D.setOn(state.screenOn);
  }
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
      try { await api("POST", "/v1/devices/bind", { device_id: "watch-esp32-1", code: me.pair_code, name: "BondWatch" }); } catch (_) {}
      try { await api("POST", "/v1/devices/bind", { device_id: "web-companion", code: me.pair_code, name: "电脑网页" }); } catch (_) {}
    }
    if (me.face && me.face.seq) {
      state.faceSeq = me.face.seq;
      setEmotion(me.face.emotion || "idle", me.face.text || "已绑定，等手表说话");
      $("bind-state").textContent = `已绑定 PAIR ${me.pair_code} · 来源 ${me.face.source || "等待"}`;
    } else {
      setEmotion(state.dnd ? "silent" : "idle", "已绑定。等手表说话，或点脸对讲");
    }
  } catch (error) {
    $("net-pill").textContent = "云端离线";
    $("net-pill").classList.add("off");
    if (String(error.message).includes("token") || String(error.message).includes("401")) {
      localStorage.removeItem("bw_token");
    }
  }
}

async function applyReply(reply) {
  const emo = $("dnd").checked || Number($("volume").value) === 0 ? "silent" : reply.emotion;
  const line = reply.subtitle || reply.text;
  setEmotion(emo, line);
  try {
    const face = await api("POST", "/v1/watch/face", {
      device_id: "web-companion",
      emotion: emo,
      text: line,
      source: "pc",
      wifi_ok: true,
    });
    state.faceSeq = face.seq || state.faceSeq;
  } catch (_) {}
  if (emo === "speak" || emo === "quiet") {
    await playReplyAudio(reply.tts_url, line);
  }
  await refreshAll();
}

async function talk(imageBase64) {
  if (state.talking) return;
  const text = $("talk-text").value.trim() || (imageBase64 ? "看看这是什么" : "你好");
  if (!state.sessionId) {
    setEmotion("offline", "还没连上会话");
    return;
  }
  if (!state.screenOn) {
    state.screenOn = true;
    $("watch-screen").classList.remove("off");
  }
  state.talking = true;
  setEmotion("listen", imageBase64 ? "看着照片…" : "聆听中…");
  await new Promise((r) => setTimeout(r, 400));
  setEmotion("think", "思考中…");
  try {
    const reply = await api("POST", `/v1/sessions/${state.sessionId}/turn`, {
      text,
      tts: !imageBase64,
      ...(imageBase64 ? { image_base64: imageBase64 } : {}),
    });
    $("talk-text").value = "";
    await applyReply(reply);
  } catch (error) {
    setEmotion("offline", error.message);
  } finally {
    state.talking = false;
  }
}

async function serverVoiceTurn() {
  const blob = await recordOnceMs(4800);
  setEmotion("think", "录音识别中…");
  return api("POST", `/v1/sessions/${state.sessionId}/turn-voice`, {
    audio_base64: await blobToBase64(blob),
    mime: blob.type || "audio/webm",
    tts: true,
  });
}

async function talkVoice() {
  if (state.talking) return;
  if (!state.sessionId) {
    setEmotion("offline", "还没连上会话");
    return;
  }
  if (!state.screenOn) {
    state.screenOn = true;
    $("watch-screen").classList.remove("off");
  }
  state.talking = true;
  let transcript = "";
  if (speechUsable()) {
    setEmotion("listen", "正在听你说…");
    try {
      transcript = await listenOnce();
    } catch (speechErr) {
      setEmotion("listen", "浏览器听不清，改用录音识别…");
      try {
        const reply = await serverVoiceTurn();
        await applyReply(reply);
        return;
      } catch (voiceErr) {
        setEmotion("offline", voiceErr.message || speechErr.message);
        return;
      } finally {
        state.talking = false;
      }
    }
    if (!transcript) {
      setEmotion("offline", "没听清，请再说一次");
      state.talking = false;
      return;
    }
    $("talk-text").value = transcript;
    setEmotion("think", `思考中…（听到：${transcript.slice(0, 18)}）`);
    try {
      const reply = await api("POST", `/v1/sessions/${state.sessionId}/turn`, {
        text: transcript,
        tts: true,
      });
      await applyReply(reply);
    } catch (error) {
      setEmotion("offline", error.message);
    } finally {
      state.talking = false;
    }
    return;
  }
  setEmotion(
    "listen",
    location.hostname === "127.0.0.1" || location.hostname === "localhost"
      ? "正在录音…"
      : "局域网地址不支持浏览器听写，正在录音识别…",
  );
  try {
    const reply = await serverVoiceTurn();
    await applyReply(reply);
  } catch (error) {
    setEmotion("offline", error.message);
  } finally {
    state.talking = false;
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

$("pair-go").onclick = async () => {
  $("auth-error").textContent = "";
  try {
    const data = await api("POST", "/v1/pair/join", {
      code: $("pair-join").value.trim(),
      holder: "pc",
      name: "电脑网页",
    });
    await afterLogin(data);
  } catch (error) {
    $("auth-error").textContent = error.message;
  }
};

async function pollAudio() {
  if (!state.token) return;
  try {
    const meta = await api("GET", "/v1/watch/audio");
    if (meta.seq && meta.seq !== state.audioSeq && meta.url) {
      state.audioSeq = meta.seq;
      const player = $("watch-audio");
      player.src = `${meta.url}?t=${Date.now()}`;
      const sec = meta.seconds ? `${meta.seconds} 秒` : `${meta.bytes} 字节`;
      $("audio-state").textContent = `收到手表录音 ${sec} · 点播放`;
      $("audio-card").classList.add("fresh");
      player.play().catch(() => {});
    }
  } catch (_) {}
}

async function pollFace() {
  if (!state.token) return;
  try {
    const face = await api("GET", "/v1/watch/face");
    if (face.seq && face.seq !== state.faceSeq && face.text) {
      state.faceSeq = face.seq;
      state.screenOn = true;
      $("watch-screen").classList.remove("off");
      setEmotion(face.emotion || "speak", face.text);
      $("bind-state").textContent = `已绑定 PAIR ${face.pair_code || ""} · ${face.source === "watch" ? "手表画面" : "同步中"}`;
    }
  } catch (_) {}
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

$("ptt").onclick = () => talkVoice();
$("talk-go").onclick = () => talk();
$("talk-start").onclick = () => talkVoice();
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
    if (state.replyAudio) {
      state.replyAudio.pause();
      state.replyAudio = null;
    }
    window.speechSynthesis?.cancel();
    setEmotion(state.dnd ? "silent" : "idle", "已打断");
    return;
  }
  talkVoice();
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
setInterval(pollFace, 1000);
setInterval(pollAudio, 1000);
tickClock();
window.addEventListener("elf3d-ready", () => document.body.classList.add("has-3d"));

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
