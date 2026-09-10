# Regenerates offline HTML from the live canvases. Run: python export_offline_html.py
from __future__ import annotations

import json
import re
from pathlib import Path

DOCS = Path(__file__).resolve().parent
CANVAS_DIR = Path(r"C:\Users\123\.cursor\projects\d-Work-BondWatch\canvases")
BOARD_TSX = CANVAS_DIR / "bondwatch-prd-gantt.canvas.tsx"
AI_TSX = CANVAS_DIR / "bondwatch-ai-engineer.canvas.tsx"
if not BOARD_TSX.exists():
    BOARD_TSX = DOCS / "bondwatch-prd-gantt.canvas.tsx"
    AI_TSX = DOCS / "bondwatch-ai-engineer.canvas.tsx"


def extract_bracket(src: str, name: str) -> str:
    m = re.search(rf"const {re.escape(name)}(?:<[\s\S]*?>)?(?:\s*:\s*[^=]+)?\s*=\s*", src)
    if not m:
        raise SystemExit(f"missing const {name}")
    i = m.end()
    while i < len(src) and src[i] in " \t\r\n":
        i += 1
    open_ch = src[i]
    close_ch = {"[": "]", "{": "}"}[open_ch]
    depth = 0
    in_str = False
    quote = ""
    esc = False
    for j in range(i, len(src)):
        ch = src[j]
        if in_str:
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == quote:
                in_str = False
            continue
        if ch in "\"'":
            in_str = True
            quote = ch
            continue
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return src[i : j + 1]
    raise SystemExit(f"unclosed {name}")


def ts_literal_to_json(text: str):
    out = []
    i = 0
    n = len(text)
    in_str = False
    quote = ""
    esc = False
    while i < n:
        ch = text[i]
        if in_str:
            out.append(ch)
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == quote:
                in_str = False
            i += 1
            continue
        if ch in "\"'":
            in_str = True
            quote = ch
            out.append('"' if ch == "'" else ch)
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            i += 2
            while i < n and text[i] not in "\n\r":
                i += 1
            continue
        ident_start = ch.isalpha() or ch == "_"
        if ident_start:
            j = i + 1
            while j < n and (text[j].isalnum() or text[j] == "_"):
                j += 1
            word = text[i:j]
            k = j
            while k < n and text[k] in " \t\r\n":
                k += 1
            prev = "".join(out).rstrip()
            is_key = k < n and text[k] == ":" and (not prev or prev[-1] in "{,")
            if is_key:
                out.append(json.dumps(word, ensure_ascii=False))
            elif word in ("true", "false", "null"):
                out.append(word)
            else:
                out.append(json.dumps(word, ensure_ascii=False))
            i = j
            continue
        out.append(ch)
        i += 1
    raw = "".join(out)
    raw = re.sub(r",(\s*[}\]])", r"\1", raw)
    return json.loads(raw)


def grab(src: str, *names):
    return {name: ts_literal_to_json(extract_bracket(src, name)) for name in names}


CSS = r"""
    :root {
      --bg: #f3f2ee; --fg: #1c1c1c; --muted: #5c5c5c; --line: #d9d6ce;
      --card: #ffffff; --pill: #eceae3; --accent: #1f6feb;
      --warn: #9a6700; --warn-bg: #fff6db; --info: #1f4ead; --info-bg: #e8eefc;
      --danger: #9b1c1c; --ok: #0f6b4c;
    }
    * { box-sizing: border-box; }
    html, body { margin: 0; background: var(--bg); color: var(--fg);
      font: 15px/1.5 "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif; }
    .wrap { max-width: 1180px; margin: 0 auto; padding: 28px 20px 64px; }
    h1 { font-size: 28px; margin: 0 0 4px; font-weight: 650; }
    h2 { font-size: 18px; margin: 28px 0 10px; }
    h3 { font-size: 15px; margin: 22px 0 8px; }
    .sub { color: var(--muted); margin: 0 0 18px; }
    .row { display: flex; flex-wrap: wrap; gap: 8px; margin: 0 0 16px; }
    .pill { border: 1px solid var(--line); background: var(--pill); color: var(--fg);
      padding: 5px 11px; border-radius: 999px; cursor: pointer; font-size: 13px; }
    .pill.active { background: var(--fg); color: #fff; border-color: var(--fg); }
    .stats { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; margin: 0 0 16px; }
    .stat { background: var(--card); border: 1px solid var(--line); padding: 12px 14px; }
    .stat b { display: block; font-size: 22px; font-weight: 650; }
    .stat span { color: var(--muted); font-size: 12px; }
    .stat.warn b { color: var(--warn); }
    .stat.info b { color: var(--info); }
    .callout { border: 1px solid var(--line); background: var(--card); padding: 12px 14px; margin: 0 0 16px; }
    .callout.warn { background: var(--warn-bg); border-color: #ead9a0; }
    .callout.info { background: var(--info-bg); border-color: #c9d6f5; }
    .callout strong { display: block; margin-bottom: 4px; }
    table { width: 100%; border-collapse: collapse; background: var(--card); font-size: 13px; margin: 0 0 8px; }
    th, td { border: 1px solid var(--line); padding: 8px 9px; text-align: left; vertical-align: top; }
    th { background: #f7f6f2; font-weight: 600; position: sticky; top: 0; }
    tr.stripe:nth-child(even) td { background: #fafaf7; }
    tr.tone-danger td:first-child { box-shadow: inset 3px 0 0 var(--danger); }
    tr.tone-warning td:first-child { box-shadow: inset 3px 0 0 var(--warn); }
    tr.tone-info td:first-child { box-shadow: inset 3px 0 0 var(--info); }
    tr.tone-success td:first-child { box-shadow: inset 3px 0 0 var(--ok); }
    .gantt-head, .gantt-row { display: grid; grid-template-columns: 176px 1fr; gap: 8px; align-items: center; margin-bottom: 8px; }
    .gantt-axis { position: relative; height: 22px; }
    .gantt-track { position: relative; height: 28px; border: 1px solid var(--line); background: var(--card); cursor: pointer; padding: 0; width: 100%; text-align: left; }
    .tick { position: absolute; top: 0; bottom: 0; width: 1px; background: var(--line); }
    .today { position: absolute; top: 0; bottom: 0; width: 1px; background: var(--accent); }
    .bar { position: absolute; top: 5px; bottom: 5px; background: #cfd6e4; color: var(--fg); font-size: 11px; display: flex; align-items: center; padding: 0 6px; overflow: hidden; white-space: nowrap; }
    .bar.critical { background: var(--accent); color: #fff; }
    .bar.active { outline: 2px solid #111; }
    .label { font-size: 13px; }
    .label.active { font-weight: 650; }
    .axis-label { position: absolute; font-size: 12px; color: var(--muted); }
    .legend { color: var(--muted); font-size: 13px; display: flex; gap: 16px; flex-wrap: wrap; margin: 8px 0 12px; }
    .hint { color: var(--muted); font-size: 12px; margin: 0 0 8px; }
    .offline { color: var(--muted); font-size: 12px; margin-top: 36px; }
    a { color: var(--accent); }
    @media (max-width: 900px) { .stats { grid-template-columns: 1fr 1fr; } .gantt-head, .gantt-row { grid-template-columns: 1fr; } }
"""

JS_HELPERS = r"""
    function esc(s) {
      return String(s ?? "").replace(/[&<>"']/g, (c) => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
      }[c]));
    }
    function pills(items, current, attr) {
      return items.map((item) => {
        const id = typeof item === "string" ? item : item.id;
        const label = typeof item === "string" ? item : item.label;
        return `<button class="pill ${current === id ? "active" : ""}" data-${attr}="${esc(id)}">${esc(label)}</button>`;
      }).join("");
    }
    function table(headers, rows, tones, striped) {
      const body = rows.map((row, i) => {
        const tone = tones && tones[i] ? ` tone-${tones[i]}` : "";
        const stripe = striped ? " stripe" : "";
        return `<tr class="${tone}${stripe}">${row.map((c) => `<td>${esc(c)}</td>`).join("")}</tr>`;
      }).join("");
      return `<table><thead><tr>${headers.map((h) => `<th>${esc(h)}</th>`).join("")}</tr></thead><tbody>${body}</tbody></table>`;
    }
    function stats(items) {
      return `<div class="stats">${items.map((s) => `<div class="stat ${s.tone || ""}"><b>${esc(s.value)}</b><span>${esc(s.label)}</span></div>`).join("")}</div>`;
    }
    function callout(title, body, tone) {
      return `<div class="callout ${tone || ""}"><strong>${esc(title)}</strong>${esc(body)}</div>`;
    }
"""

BOARD_JS = r"""
    const GANTT_START = Date.parse("2026-08-18");
    const GANTT_END = Date.parse("2026-09-22");
    const SPAN = GANTT_END - GANTT_START;
    const state = { view: "gantt", ganttId: "g0", buyWave: "全部", softWho: "全部", area: "all", priority: "all" };
    function pct(iso) {
      const t = Date.parse(iso);
      return Math.min(100, Math.max(0, ((t - GANTT_START) / SPAN) * 100));
    }
    function todayPct() { return pct(new Date().toISOString().slice(0, 10)); }

    function renderGantt() {
      const today = todayPct();
      const selected = DATA.tasks.find((t) => t.id === state.ganttId) || DATA.tasks[0];
      const axis = DATA.months.map((m) => `<span class="axis-label" style="left:${pct(m.at)}%">${esc(m.label)}</span>`).join("");
      const rows = DATA.tasks.map((task) => {
        const left = pct(task.start);
        const width = Math.max(1.5, pct(task.end) - left);
        const active = task.id === selected.id;
        const ticks = DATA.months.slice(1).map((m) => `<span class="tick" style="left:${pct(m.at)}%"></span>`).join("");
        return `<div class="gantt-row"><div class="label ${active ? "active" : ""}">${esc(task.name)}</div>
          <button class="gantt-track" data-gantt="${esc(task.id)}" title="${esc(task.start)} 至 ${esc(task.end)}">
            ${ticks}<span class="today" style="left:${today}%"></span>
            <span class="bar ${task.critical ? "critical" : ""} ${active ? "active" : ""}" style="left:${left}%;width:${width}%">${esc(task.start.slice(5) + "–" + task.end.slice(5))}</span>
          </button></div>`;
      }).join("");
      return `${stats([
          { value: "8/21", label: "工具链 Hello", tone: "warn" },
          { value: "9/08", label: "手机假手表", tone: "info" },
          { value: "9/18", label: "正式演示" },
          { value: "9/22", label: "37 天截止" },
        ])}
        ${callout("37 天含双休：周末休息，工作日大约 26 天", "甘特轴按日历画，任务只排周一到周五。正式演示 9/18。必须项是云端 + 假手表 + 面包板 Wi-Fi。", "warn")}
        <div class="gantt-head"><div class="hint">工作流</div><div class="gantt-axis">${axis}</div></div>
        ${rows}
        <div class="legend"><span>色块：关键路径</span><span>浅块：可并行</span><span>竖线：今天 · 截止 9/22</span></div>
        ${callout(selected.name + "  ·  " + selected.start + " 至 " + selected.end, selected.note, selected.critical ? "info" : "")}
        <h3>里程碑</h3>
        ${table(["里程碑", "日期", "含义", "通过标准"], DATA.milestones, ["success","info","success","info","warning","success"], true)}`;
    }

    function renderBuy() {
      const wave = state.buyWave === "板" || state.buyWave === "工具" ? state.buyWave : "全部";
      const gear = DATA.equipment.filter((x) => wave === "全部" || x.wave === wave);
      return `${callout("货已到 8/24，采购表留底", "现在对着硬件逻辑页接线。Wi-Fi 用 S3 板载。4G、电池、GPS 先别往面包板上焊。", "warn")}
        ${stats([
          { value: "N16R8×2", label: "套件 1 + 备用 1", tone: "warn" },
          { value: "1.69+2.0", label: "先 240×280，大表 240×320", tone: "info" },
          { value: "Air780E AT", label: "约 ¥42，无板载 USB" },
          { value: "约 400–550", label: "含 Hub/烙铁/CH340（元）" },
        ])}
        <div class="row">${pills([{id:"全部",label:"全部"},{id:"板",label:"开发板"},{id:"工具",label:"基础工具"}], wave, "buy")}</div>
        ${table(["类","数量","设备","搜这个买","用来干什么","约价（元）","有则跳过 / 别买错"], gear.map((x) => [x.wave, x.qty, x.item, x.sku, x.why, x.price, x.skip]), null, true)}
        <h3>明确这次不买</h3>
        ${table(["别买","原因"], [
          ["独立 Wi-Fi 模块（ESP8266 / 另一块 ESP32）", "S3 已经有 Wi-Fi"],
          ["第二块 4G、官方 ¥299 EGT、LuatOS EPM、DTU", "贵的不双买。4G 挂了用家里 Wi-Fi。GPS 以后再说"],
          ["1.9 寸 170×320、无触摸 2inch LCD、圆屏 AMOLED、手表一体板", "分辨率或形态不对。套件 0.91 OLED 也不要当主屏"],
          ["N8R2 / 没 PSRAM 的 ESP32-S3", "必须 N16R8"],
          ["OV2640、电池、GPS 焊到当前面包板", "脚已满。镜头/定位放到下一块板"],
        ], ["danger","warning","warning","danger","warning"])}
        <h2>初期硬件还要注意什么</h2>
        <h3>实用性</h3>
        ${table(["注意","具体怎么做"], DATA.hwPractical, null, true)}
        <h3>通用性</h3>
        ${table(["原则","具体怎么做"], DATA.hwGeneric, null, true)}`;
    }

    function renderSoft() {
      const who = ["全员","硬件","客户端","云端"].includes(state.softWho) ? state.softWho : "全部";
      const apps = DATA.software.filter((x) => who === "全部" || x.who === who);
      return `${callout("今晚就装，前 5 天只追求 Hello", "客户端装 Arduino/Flutter。云端今晚开通 API。硬件今晚看接线图。", "warn")}
        ${stats([
          { value: "今晚", label: "开始下大安装包", tone: "warn" },
          { value: "Wokwi", label: "先在浏览器练 ESP32", tone: "info" },
          { value: "Arduino 2", label: "客户端必须装" },
          { value: "云 API", label: "不下载模型" },
        ])}
        <div class="row">${pills(["全部","全员","硬件","客户端","云端"].map((x) => ({id:x,label:x})), who, "soft")}</div>
        ${table(["谁装","何时","软件","怎么拿","用来干什么"], apps.map((x) => [x.who, x.when, x.name, x.get, x.why]), null, true)}`;
    }

    function renderPeople() {
      return `${callout("三人并行，每人前 5 天只做自己的 Hello", "硬件接线拍照。客户端烧录和假手表。云端交出接口表。表情可后补色块。", "warn")}
        ${table(["角色","投入","何时到位","主责","怎么找"], DATA.staff, null, true)}
        <h3>人力负荷（按自然周，周末不排）</h3>
        ${table(["角色","W1 8/18","W2 8/24","W3 8/31","W4 9/7","W5 9/14","W6 9/21"], DATA.staffLoad, null, true)}
        <h3>三人怎么交接</h3>
        ${table(["从谁到谁","交什么","怎样算交清"], [
          ["硬件 → 客户端","接线照片 + 实物已按 pins.h","能烧闪灯，键和屏有反应"],
          ["云端 → 客户端","接口约定 + 本地可跑的 FastAPI","假手表能打一轮对话，记忆能写下"],
          ["客户端 → 云端","设备或假手表连上同一套 API","情绪标签先于音频到达屏幕"],
        ], ["warning","info","success"])}
        <h3>缺人时怎么降级</h3>
        <p>缺硬件：假手表验收。缺客户端：只验 API。缺云端：没网脸和本地闹钟。</p>
        <h3>项目怎么运转</h3>
        ${table(["频率","规矩"], DATA.rules, null, true)}`;
    }

    function renderReq() {
      const rows = DATA.requirements.filter((x) =>
        (state.area === "all" || x.area === state.area) &&
        (state.priority === "all" || x.priority === state.priority)
      );
      const tones = rows.map((x) => x.priority === "P0" ? "info" : x.priority === "P1" ? "warning" : "neutral");
      return `${callout("自主开口：白名单场合才说话。闹钟只是到点开口的一种", "用户找它靠按键。它找人只走勾选过的场合，可关掉。有手机/电脑时，长句放在大屏；表上只播短句或缓存。勿扰闭嘴。本地闹钟仍响。", "info")}
        <h2>伴侣加持（已冻结）</h2>
        ${table(["场景","能做什么","边界"], DATA.companion, null, true)}
        <h2>它什么时候可以自己说话</h2>
        ${table(["场合","档","谁决定开口","在哪说","本期"], DATA.proactive, null, true)}
        <h2>自主开口铁律</h2>
        ${table(["规则","怎么执行"], DATA.proactiveRules, null, true)}
        <h2>接续协议（已冻结）</h2>
        ${table(["步骤","谁发起","行为","手表变成"], [
          ["打开 Windows","电脑","若表正在会话，询问接管或只做管理","不变，等选择"],
          ["选择接管","人","电脑成为持麦端，会话 ID 不变","旁听（默认，喇叭关）或熄屏"],
          ["选择只做管理","人","可改记忆/设置/看历史","继续持麦说话"],
          ["拿回","手表对讲键","电脑确认或超时交还","重新持麦"],
        ], ["info","success","neutral","warning"])}
        <h2>物理按键（已冻结）</h2>
        ${table(["键","短按","长按","双击","会话中"], [
          ["电源","亮屏 / 息屏","开关机","扬声器静音 + 勿扰","结束会话"],
          ["对讲","开始或结束一轮","PTT 按住说话","—","打断播报；被接管时请求拿回"],
        ])}
        <h2>需求条目</h2>
        <div class="row">${pills(DATA.areas, state.area, "area")}</div>
        <div class="row">${pills([
          {id:"all",label:"全部优先级"},{id:"P0",label:"P0 本期必做"},{id:"P1",label:"P1 真机外出前"},{id:"P2",label:"P2 后期"},
        ], state.priority, "pri")}</div>
        <p class="hint">显示 ${rows.length} / ${DATA.requirements.length} 条</p>
        ${table(["ID","优先级","需求","落在哪","验收"], rows.map((x) => [x.id, x.priority, x.need, x.where, x.accept]), tones, true)}`;
    }

    function renderFreeze() {
      return `${callout("这张表是项目的刹车", "要改范围：先改这一页，再改甘特和采购。", "warn")}
        ${table(["主题","冻结成什么","状态","明确不做"], DATA.decisions, null, true)}
        <h3>有手机/电脑时更智能；自主开口默认安静</h3>
        ${table(["场景","能做什么","边界"], DATA.companion, null, true)}
        ${table(["规则","怎么执行"], DATA.proactiveRules, null, true)}`;
    }
    function renderHw() {
      return `${callout("货已到，对着接线卡接线", "手机打开 bondwatch-wiring.html，字大、按步排。开发板侧面印着 GPIO 数字。不要抄微雪 Wiki 的 ESP32S3 例程脚。先 1.69 和两键，麦喇叭后接，4G 最后。", "warn")}
        ${stats([
          { value: "先 1.69", label: "通了再换 2.0", tone: "warn" },
          { value: "3.3V", label: "屏和麦不要接 5V", tone: "info" },
          { value: "N16R8", label: "必须这一档 S3" },
          { value: "4G 最后", label: "先 CH340 打 AT" },
        ])}
        <h2>接线顺序</h2>
        ${table(["步","接什么"], DATA.wireSteps, null, true)}
        <h2>模块丝印 → S3 GPIO</h2>
        ${table(["功能","你看到的脚","接到 S3","注意"], DATA.pins, null, true)}
        <h3>先别接</h3>
        ${table(["别接","原因"], DATA.wireSkip, ["danger","warning","warning","warning","warning","danger"])}
        <h2>真表 → 仿真 → 真机</h2>
        ${table(["功能","真表怎么接","Arduino / Wokwi","货到只换什么"], DATA.hwMap, null, true)}
        ${table(["能力","结论"], DATA.hwFeasible, null, true)}
        <p>Wokwi 仍可练：PWR/PTT/TAP。真机 GPIO6/7 改接麦和功放，不要再接灯和蜂鸣器。</p>`;
    }
    function renderIface() {
      return `${callout("三人只准认这一份", "云端字段、表情枚举、GPIO 以本页为准。4G 的 UART 先空着，屏和键通了再接。", "info")}
        <h2>云端 API（v1）</h2>
        ${table(["接口","谁调用","约定"], DATA.api, null, true)}
        <h2>表情标签（先于音频到达）</h2>
        ${table(["emotion","状态","屏幕"], DATA.emotions, null, true)}
        <h2>初值引脚（到货后只改这一张）</h2>
        <p class="hint">对着模块丝印接。避开 GPIO0/3/45/46 和 USB 的 19/20。不要抄微雪 Wiki 的 ESP32 例程脚。</p>
        ${table(["功能","你看到的脚","接到 S3","注意"], DATA.pins, null, true)}`;
    }
    function renderAccept() {
      return `${callout("只按「必须」验收；拍演日期是 9/18", "应该有、加分项做不到就口述缺口。加分做完、必须项没做，算失败。", "warn")}
        ${stats([
          { value: String(DATA.must.length), label: "必须项", tone: "warn" },
          { value: String(DATA.should.length), label: "应该有" },
          { value: String(DATA.stretch.length), label: "加分", tone: "info" },
        ])}
        <h2>必须有（不做就没过）</h2>
        ${table(["块","通过标准"], DATA.must, ["danger","danger","danger","warning"])}
        <h2>应该有</h2>
        ${table(["块","通过标准"], DATA.should)}
        <h2>加分</h2>
        ${table(["块","通过标准"], DATA.stretch)}
        <h2>演示脚本</h2>
        ${table(["步","动作"], DATA.demo, null, true)}`;
    }
    function renderRisk() {
      const tones = DATA.risks.map((_, i) => (i % 2 ? "warning" : "danger"));
      return `${callout("负责人每周看一次触发条件", "到了触发条件就执行「怎么挡」。", "warn")}
        ${table(["风险","可能","伤害","谁盯","触发条件","怎么挡"], DATA.risks, tones, true)}`;
    }

    const pages = { gantt: renderGantt, buy: renderBuy, soft: renderSoft, people: renderPeople, req: renderReq, freeze: renderFreeze, hw: renderHw, iface: renderIface, accept: renderAccept, risk: renderRisk };
    function render() {
      document.getElementById("tabs").innerHTML = pills(DATA.views, state.view, "view");
      document.getElementById("app").innerHTML = pages[state.view]();
    }
    document.body.addEventListener("click", (e) => {
      const t = e.target.closest("[data-view],[data-gantt],[data-buy],[data-soft],[data-area],[data-pri]");
      if (!t) return;
      if (t.dataset.view) state.view = t.dataset.view;
      if (t.dataset.gantt) state.ganttId = t.dataset.gantt;
      if (t.dataset.buy) state.buyWave = t.dataset.buy;
      if (t.dataset.soft) state.softWho = t.dataset.soft;
      if (t.dataset.area) state.area = t.dataset.area;
      if (t.dataset.pri) state.priority = t.dataset.pri;
      render();
    });
    render();
"""

AI_JS = r"""
    const state = { view: "nudge", bar: "全部" };
    function renderScope() {
      return `${callout("一句话", "不训练模型。把 ASR/LLM/TTS 接成 BondWatch API。自主开口也归你：默认安静、白名单、选端、可关。", "warn")}
        ${stats([
          { value: "9/02", label: "云端能对话（M1）", tone: "warn" },
          { value: "1.5 s", label: "说完到第一包音频 p50" },
          { value: "可关", label: "主动开口总开关", tone: "info" },
        ])}
        <h3>和另外两人的交接</h3>
        ${table(["方向","内容"], DATA.handoff, null, true)}
        <h3>技术约束</h3>
        ${table(["选这个","不要改成"], [
          ["FastAPI + SQLite 本机先跑","一上来 Docker/Postgres"],
          ["云厂商 ASR/LLM/TTS","自建模型"],
          ["情绪先于音频","自己发明表情 JSON"],
          ["到点播缓存句","闹钟现场调模型"],
        ], ["info","danger","warning","danger"])}`;
    }
    function renderBuild() {
      const rows = DATA.modules.filter((x) => state.bar === "全部" || x[1] === state.bar);
      const tones = rows.map((x) => x[1] === "P0" ? "info" : "neutral");
      return `<div class="row">${pills(["全部","P0","P1"].map((x)=>({id:x,label:x})), state.bar, "bar")}</div>
        ${table(["模块","优先级","具体实现","做到什么算完成"], rows, tones, true)}
        <h3>语音一帧顺序</h3>
        ${table(["顺序","谁发","内容"], [
          ["1","端","audio 帧；松键发 end"],
          ["2","云","emotion = think"],
          ["3","云","emotion = speak + reply_text"],
          ["4","云","TTS 音频帧"],
          ["5","端可选","interrupt → 立刻停 4"],
        ], ["info","warning","success","success","danger"])}`;
    }
    function renderMetric() {
      const rows = DATA.metrics.filter((x) => state.bar === "全部" || x[1] === state.bar);
      const tones = rows.map((x) => x[1] === "必须" ? "warning" : "info");
      return `${callout("怎么测延迟", "计时从上行最后一包到下行第一包 audio。家里 Wi-Fi。", "info")}
        <div class="row">${pills(["全部","必须","应该"].map((x)=>({id:x,label:x})), state.bar, "bar")}</div>
        ${table(["指标","档","测什么","数值","最晚"], rows, tones, true)}
        <h3>9/18 你要能当场做的</h3>
        ${table(["步","云端负责的部分"], [
          ["假手表说话","一轮 ASR→LLM→TTS，脸在出声前变"],
          ["静音看字幕","reply_text 有字"],
          ["记忆","现场说一个偏好，下一轮能用"],
          ["电脑接管","session_id 不变"],
          ["关掉主动说话","nudge 被拒绝，除闹钟外不出声"],
        ], null, true)}`;
    }
    function renderNudge() {
      return `${callout("自主意识 = 白名单场合，不是随时插嘴", "闹钟只是到点开口的一种。有手机/电脑时把决策和长句做完，表上只执行短句或缓存。默认安静，用户能关。", "warn")}
        <h3>策略</h3>
        ${table(["规则","实现要点"], [
          ["默认安静","没有 scene 匹配，不生成、不推送"],
          ["总开关","proactive_enabled=false → 所有 nudge 拒绝"],
          ["勿扰再判","下发前读 DND；是则不下音频"],
          ["选端","pc 在线且空闲 → pc；否则 phone；表只短句或缓存"],
          ["不抢麦","已有 holder 则推迟或改到另一端"],
          ["先生成后执行","fire_at 到达时播缓存，不现场调 LLM"],
          ["每日上限","主动次数（不含用户闹钟）默认 3"],
        ], null, true)}
        <h3>场合怎么落地</h3>
        ${table(["场合","云端做什么","档"], [
          ["到点闹钟","只同步 alarms，不调模型","P0"],
          ["闹钟后一句","设置时生成缓存字段","P1"],
          ["对话设提醒","LLM tool → 写 alarms","P1"],
          ["久未对话","看 last_session_at，可 delay","P1"],
          ["电脑空闲打开","下发短问候到 pc","P1"],
          ["记忆里的日子","读 memory 日期，本期可空着","P2"],
        ], ["info","warning","warning","warning","warning","neutral"])}`;
    }
    function renderWeek() {
      return `${callout("M1 是硬节点", "9/02 前必须文字或语音一轮 + 记忆能写。", "warn")}
        ${table(["周","主题","交出来"], DATA.weeks, null, true)}`;
    }
    function renderOut() {
      return table(["不做","原因"], DATA.out, DATA.out.map(() => "danger"), true);
    }
    const pages = { scope: renderScope, build: renderBuild, metric: renderMetric, nudge: renderNudge, week: renderWeek, out: renderOut };
    function render() {
      document.getElementById("tabs").innerHTML = pills(DATA.views, state.view, "view");
      document.getElementById("app").innerHTML = pages[state.view]();
    }
    document.body.addEventListener("click", (e) => {
      const t = e.target.closest("[data-view],[data-bar]");
      if (!t) return;
      if (t.dataset.view) { state.view = t.dataset.view; state.bar = "全部"; }
      if (t.dataset.bar) state.bar = t.dataset.bar;
      render();
    });
    render();
"""


def page(title, sub, extra_nav, data, js):
    payload = json.dumps(data, ensure_ascii=False, indent=2)
    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{title}</title>
  <style>{CSS}</style>
</head>
<body>
  <div class="wrap">
    <h1>{title}</h1>
    <p class="sub">{sub} {extra_nav}</p>
    <nav class="row" id="tabs"></nav>
    <main id="app"></main>
    <p class="offline">静态页 · 不需要联网 · 双击用浏览器打开</p>
  </div>
  <script id="board-data" type="application/json">{payload}</script>
  <script>
    const DATA = JSON.parse(document.getElementById("board-data").textContent);
    {JS_HELPERS}
    {js}
  </script>
</body>
</html>
"""


board_src = BOARD_TSX.read_text(encoding="utf-8")
ai_src = AI_TSX.read_text(encoding="utf-8")

board = {
    "views": grab(board_src, "VIEWS")["VIEWS"],
    "areas": grab(board_src, "AREAS")["AREAS"],
    "requirements": grab(board_src, "REQUIREMENTS")["REQUIREMENTS"],
    "tasks": grab(board_src, "TASKS")["TASKS"],
    "milestones": grab(board_src, "MILESTONES")["MILESTONES"],
    "equipment": grab(board_src, "EQUIPMENT")["EQUIPMENT"],
    "hwPractical": grab(board_src, "HW_PRACTICAL")["HW_PRACTICAL"],
    "hwGeneric": grab(board_src, "HW_GENERIC")["HW_GENERIC"],
    "hwMap": grab(board_src, "HW_MAP")["HW_MAP"],
    "hwFeasible": grab(board_src, "HW_FEASIBLE")["HW_FEASIBLE"],
    "wireSteps": grab(board_src, "WIRE_STEPS")["WIRE_STEPS"],
    "wireSkip": grab(board_src, "WIRE_SKIP")["WIRE_SKIP"],
    "staff": grab(board_src, "STAFF")["STAFF"],
    "staffLoad": grab(board_src, "STAFF_LOAD")["STAFF_LOAD"],
    "software": grab(board_src, "SOFTWARE")["SOFTWARE"],
    "decisions": grab(board_src, "DECISIONS")["DECISIONS"],
    "companion": grab(board_src, "COMPANION")["COMPANION"],
    "proactive": grab(board_src, "PROACTIVE")["PROACTIVE"],
    "proactiveRules": grab(board_src, "PROACTIVE_RULES")["PROACTIVE_RULES"],
    "api": grab(board_src, "API_ROWS")["API_ROWS"],
    "emotions": grab(board_src, "EMOTION_ROWS")["EMOTION_ROWS"],
    "pins": grab(board_src, "PIN_ROWS")["PIN_ROWS"],
    "must": grab(board_src, "ACCEPT_MUST")["ACCEPT_MUST"],
    "should": grab(board_src, "ACCEPT_SHOULD")["ACCEPT_SHOULD"],
    "stretch": grab(board_src, "ACCEPT_STRETCH")["ACCEPT_STRETCH"],
    "demo": grab(board_src, "DEMO_STEPS")["DEMO_STEPS"],
    "risks": grab(board_src, "RISKS")["RISKS"],
    "rules": grab(board_src, "RULES")["RULES"],
    "months": grab(board_src, "MONTHS")["MONTHS"],
}

ai = {
    "views": grab(ai_src, "VIEWS")["VIEWS"],
    "modules": grab(ai_src, "MODULES")["MODULES"],
    "metrics": grab(ai_src, "METRICS")["METRICS"],
    "weeks": grab(ai_src, "WEEKS")["WEEKS"],
    "handoff": grab(ai_src, "HANDOFF")["HANDOFF"],
    "out": grab(ai_src, "OUT")["OUT"],
}

board_html = page(
    "BondWatch 计划看板",
    "项目负责人看板。本文件可离线打开。",
    '另有 <a href="bondwatch-wiring.html">对着接线</a> · <a href="bondwatch-ai-engineer.html">云端 AI 工程师交付说明</a>。',
    board,
    BOARD_JS,
)
ai_html = page(
    "云端 AI 工程师交付说明",
    "只覆盖云端这一人。手表和假手表只做眼睛和嘴巴。",
    '返回 <a href="bondwatch-board.html">计划看板</a>。',
    ai,
    AI_JS,
)

out_board = DOCS / "bondwatch-board.html"
out_ai = DOCS / "bondwatch-ai-engineer.html"
out_board.write_text(board_html, encoding="utf-8")
out_ai.write_text(ai_html, encoding="utf-8")
print(f"board {out_board.stat().st_size} bytes, req {len(board['requirements'])}, decisions {len(board['decisions'])}")
print(f"ai {out_ai.stat().st_size} bytes, modules {len(ai['modules'])}, metrics {len(ai['metrics'])}")
