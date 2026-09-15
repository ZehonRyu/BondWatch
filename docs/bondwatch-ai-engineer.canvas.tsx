import {
  Callout,
  Grid,
  H1,
  H2,
  H3,
  Pill,
  Row,
  Stack,
  Stat,
  Table,
  Text,
  useCanvasState,
} from "cursor/canvas";

type View = "scope" | "build" | "metric" | "nudge" | "week" | "out";

const VIEWS: { id: View; label: string }[] = [
  { id: "scope", label: "职责边界" },
  { id: "build", label: "要实现什么" },
  { id: "metric", label: "指标怎么过" },
  { id: "nudge", label: "自主开口" },
  { id: "week", label: "按周交付" },
  { id: "out", label: "明确不做" },
];

const MODULES = [
  ["账号与鉴权", "P0", "POST /v1/auth/login；JWT 或随机 token；所有业务接口校验", "无 token 或过期一律 401。Key 只在环境变量"],
  ["设备绑定", "P0", "POST /v1/devices/bind：校验 device_id + 短码；账号下挂设备", "绑成功后该设备能开会话。Windows 不走绑定向导，但登录后能看到已绑设备"],
  ["会话与持麦", "P0", "POST /v1/sessions；同一时刻只一个 holder；takeover / listen / release", "第二端想说话必须 takeover。管理操作不打断当前 TTS"],
  ["语音流水线", "P0", "MQTT：bw/{device_id}/up/audio 上行 PCM 16kHz 单声道；STT → LLM → TTS；dn/meta 先发 emotion，再 dn/audio", "半双工：TTS 播放中忽略新音频，直到 up/ctrl interrupt"],
  ["情绪标签", "P0", "下行先 JSON emotion，再音频。枚举固定：think / speak（云端必发）；idle/listen/quiet/silent/alarm/offline 由端上 FSM 本地切", "第一包音频到达前，客户端已经收到 emotion。禁止音频比标签早"],
  ["表情结构化输出", "P1", "/turn 与 dn/meta 扩展 expression（happy/shy/surprised/sad/neutral）+ action（wave/nod/bounce/still/peek）。LLM JSON mode，白名单校验", "说「好开心呀」→ expression=happy, action=wave。旧客户端忽略新字段仍可用"],
  ["长期记忆", "P0", "GET/PUT /v1/memory；与短期对话分开存。每轮把记忆摘要注入 LLM", "写入「我叫小王」后，新会话能叫出小王。三端读到同一份"],
  ["人设", "P0", "系统提示词 + 可编辑人设字段（随 settings 或 memory 的 persona）", "改人设后下一轮回复口吻变化，不需要重启服务"],
  ["闹钟同步", "P0", "GET/PUT /v1/alarms 只存配置（时间、重复、文案）", "云端宕机或断网，端上已同步的闹钟仍响。云端绝不在到点调模型"],
  ["设置", "P0", "PUT /v1/settings：音量、勿扰、默认语音模式、proactive_enabled", "关掉 proactive 后，除用户点的对话和本地闹钟外，云端不得再下发主动 TTS"],
  ["主动开口策略", "P0", "白名单场合 + 选端（watch/phone/pc）+ 勿扰再判 + 每日上限。有 pc 在线则长句走 pc", "策略用规则，不要每分钟喊一次大模型闲聊"],
  ["主动开口通道", "P1", "POST /v1/nudges：scene、text 或 generate、target、fire_at（可 delay）", "勿扰返回 423。闹钟响铃不走 nudges"],
  ["预生成缓存句", "P1", "设闹钟或 nudge 时生成一句 TTS/文本，随配置下发到端", "到点播缓存，不现场调模型。没有缓存就只响铃"],
  ["对话设闹钟", "P1", "LLM 把「十分钟后叫我」写成 alarms 记录，同步到各端", "只写配置。到点仍本地响"],
  ["打断", "P0", "up/ctrl interrupt：停 TTS 生成和 MQTT 推流", "从收到 interrupt 到音频流结束 ≤ 200ms"],
  ["字幕文本", "P0", "下行带 transcript（用户）和 reply_text（助手），给小声/勿扰叠字幕", "静音策略在端上；云端只要把字送出去"],
  ["健康检查", "P0", "GET /health；日志打 request_id；token/费用可查", "客户端连不上时能区分：服务挂了 vs 模型超时"],
  ["设备吊销", "P1", "吊销后该 device 不能拉记忆、不能开麦", "9/18 前有则演示，没有就口述"],
];

const METRICS = [
  ["服务能起", "必须", "本机 uvicorn 起 FastAPI，GET /health 返回 200", "< 100 ms", "8/21"],
  ["接口表落地", "必须", "仓库 README 或 openapi.json 与看板「接口约定」一致，字段名不许私自改", "0 处和看板冲突", "8/21"],
  ["文字一轮", "必须", "登录 → 开会话 → 发一句中文 → 收回复，记忆可写可读", "p95 < 3 s（Wi-Fi，不含冷启动）", "9/02"],
  ["语音一轮", "必须", "用户说完（VAD/松键）到第一包 TTS 音频", "p50 ≤ 1.5 s，p95 ≤ 3 s（假手表 + 家里 Wi-Fi）", "9/02"],
  ["情绪先于声音", "必须", "MQTT dn/meta 先带 emotion=think 或 speak，然后才是 dn/audio 帧", "100% 轮次标签早于音频 ≥ 80 ms", "9/02"],
  ["记忆跨会话", "必须", "会话 A 写入偏好，会话 B 回复中用到", "手工 5 条用例全过", "9/02"],
  ["单持麦", "必须", "已有 holder 时第二端开麦被拒，必须走 takeover", "对打 20 次，0 次双持麦", "9/08"],
  ["打断 TTS", "必须", "播放中发 interrupt", "流在 200 ms 内停，不再出新音频包", "9/08"],
  ["密钥安全", "必须", "git grep 无 sk-/AKIA/火山 key；.env 在 gitignore", "扫描 0 命中", "全程"],
  ["主动总开关", "必须", "proactive_enabled=false 时发 nudge", "拒绝，0 次主动 TTS", "9/08"],
  ["勿扰不主动", "必须", "DND 开着时 fire nudge", "不下发音频；闹钟通道除外", "9/08"],
  ["识别能用", "应该", "10 句日常中文短句（1–8 秒）", "≥ 8 句意图正确（不必逐字）", "9/08"],
  ["费用可控", "应该", "日志记 input/output tokens；设月额度，超额返回 offline 语义", "额度到达后端上能收到错误而不是瞎回", "9/18"],
  ["TLS", "应该", "对外 WSS/HTTPS；局域网演示可用 HTTP，文档写明", "演示当天假手表能连上所用地址", "9/18"],
];

const WEEKS = [
  ["W1 8/18–8/21", "Hello", "Python 3.12、FastAPI、/health、开通火山或阿里 LLM+ASR+TTS、Key 进环境变量、把接口表写进仓库"],
  ["W2 8/24–8/28", "文字闭环", "login、session、文字对话、SQLite 记忆 PUT/GET、系统人设。给客户端一份可 curl 的示例"],
  ["W3 8/31–9/02", "语音闭环", "WS 音频、ASR、LLM、TTS 流、emotion 先下。M1：一轮语音 + 记忆能写。达标 1.5s/3s"],
  ["W4 9/03–9/08", "对接假手表", "持麦/接管、settings、alarms、proactive_enabled、interrupt。陪客户端联调，不改字段名"],
  ["W5 9/09–9/17", "稳住 + 主动开口", "修超时弱网。有余力做 nudges + 预生成缓存句 + 选端（电脑优先）"],
  ["W6 9/18", "演示", "按验收 8 步能走通云端部分。只修问题，不新开模型或向量库"],
];

const HANDOFF = [
  ["交给客户端", "Base URL、登录示例、MQTT 主题（up/audio、up/ctrl、dn/meta、dn/audio）、expression+action 字段（P1）、settings.proactive_enabled、nudges 字段"],
  ["向客户端收", "假手表能连；PCM 采样率约定（建议 16 kHz / 16bit / mono）；松键或 VAD 结束帧"],
  ["不收硬件的", "GPIO、接线照片、4G AT。云端只认「有没有网、音频在不在 WS 里」"],
];

const OUT = [
  ["端侧大模型 / Ollama / 本地语音模型", "表上算力不够，电脑也会拖垮 37 天"],
  ["自己训练或微调", "用豆包/百炼现成接口"],
  ["唤醒词", "本期冻结不做；那是客户端以后接 ESP-SR"],
  ["闹钟响铃、勿扰执行", "只同步配置；到点是端上的事"],
  ["Flutter / Arduino / 接线", "那是客户端和硬件"],
  ["真全双工、服务器回声消除", "半双工：它说时不抢麦"],
  ["向量数据库、RAG 平台、Docker 全家桶", "SQLite + 一份记忆摘要够演示"],
  ["主动搭话推送当成闲聊机器人", "只做白名单场合。没有勾选就闭嘴"],
  ["读取手机日历、微信、电脑屏幕", "智能来自同一云端大脑 + 大屏管理，不靠扒系统通知"],
  ["到点现场调模型", "闹钟和缓存句在端上执行。生成放在有网有大屏的时候"],
];

export default function BondWatchAiEngineer() {
  const [view, setView] = useCanvasState<View>("ai-view", "metric");
  const [modBar, setModBar] = useCanvasState<string>("ai-mod", "全部");
  const [metBar, setMetBar] = useCanvasState<string>("ai-met", "全部");

  const modules = MODULES.filter((row) => modBar === "全部" || row[1] === modBar);
  const metrics = METRICS.filter((row) => metBar === "全部" || row[1] === metBar);

  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>云端 AI 工程师交付说明</H1>
        <Text tone="secondary">
          只覆盖这一人：把「大脑」做成可调用的服务。手表和假手表只做眼睛和嘴巴。
        </Text>
      </Stack>

      <Row gap={8} wrap>
        {VIEWS.map((item) => (
          <span key={item.id}>
            <Pill active={view === item.id} onClick={() => setView(item.id)}>
              {item.label}
            </Pill>
          </span>
        ))}
      </Row>

      {view === "scope" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="一句话">
            你不训练模型。你把 ASR、LLM、TTS 接成 BondWatch API。自主开口也归你做策略：默认安静、白名单、选端、可关。不在手表上养一个不停说话的人格。
          </Callout>
          <Grid columns={3} gap={16}>
            <Stat value="9/02" label="云端能对话（M1）" tone="warning" />
            <Stat value="1.5 s" label="说完到第一包音频 p50" />
            <Stat value="一份" label="三端同一记忆同一角色" tone="info" />
          </Grid>
          <H3>和另外两人的交接</H3>
          <Table headers={["方向", "内容"]} rows={HANDOFF} striped />
          <H3>技术约束（已冻）</H3>
          <Table
            headers={["选这个", "不要改成"]}
            rows={[
              ["FastAPI + SQLite 本机先跑", "一上来 Docker/Postgres/Redis"],
              ["云厂商 ASR/LLM/TTS", "自建模型"],
              ["HTTPS/WSS 对外；局域网演示可 HTTP", "把 Key 写进仓库或固件"],
              ["emotion 枚举固定 + expression/action 白名单", "自由发明动画名或跳过 FSM 本地切 listen/think"],
            ]}
            rowTone={["info", "danger", "warning", "info"]}
          />
        </Stack>
      ) : null}

      {view === "build" ? (
        <Stack gap={16}>
          <Row gap={8} wrap>
            {["全部", "P0", "P1"].map((item) => (
              <span key={item}>
                <Pill active={modBar === item} onClick={() => setModBar(item)}>
                  {item === "全部" ? "全部" : item}
                </Pill>
              </span>
            ))}
          </Row>
          <Table
            headers={["模块", "优先级", "具体实现", "做到什么算完成"]}
            rows={modules}
            rowTone={modules.map((row) => (row[1] === "P0" ? "info" : "neutral"))}
            striped
            stickyHeader
          />
          <H3>语音一帧里必须有的顺序</H3>
          <Table
            headers={["顺序", "谁发", "内容"]}
            rows={[
              ["1", "端", "audio 帧；松键或静音结束发 end"],
              ["2", "云", "emotion = think（ASR 已出、LLM 未完）"],
              ["3", "云", "emotion = speak，并带 reply_text"],
              ["4", "云", "TTS 音频帧（PCM 或厂商编码，和客户端书面约定一种）"],
              ["5", "端可选", "interrupt → 云立刻停 4"],
            ]}
            rowTone={["info", "warning", "success", "success", "danger"]}
          />
        </Stack>
      ) : null}

      {view === "metric" ? (
        <Stack gap={16}>
          <Callout tone="info" title="怎么测延迟">
            用假手表或 curl/脚本，在家里 Wi-Fi 测。计时从「上行最后一包 / end 帧」到「下行第一包 audio」。4G 弱网允许更长，但必须先下 think，不能黑屏干等。
          </Callout>
          <Row gap={8} wrap>
            {["全部", "必须", "应该"].map((item) => (
              <span key={item}>
                <Pill active={metBar === item} onClick={() => setMetBar(item)}>
                  {item}
                </Pill>
              </span>
            ))}
          </Row>
          <Table
            headers={["指标", "档", "测什么", "数值", "最晚"]}
            rows={metrics}
            rowTone={metrics.map((row) => (row[1] === "必须" ? "warning" : "info"))}
            striped
            stickyHeader
          />
          <H3>9/18 演示时你要能当场做的</H3>
          <Table
            headers={["步", "云端负责的部分"]}
            rows={[
              ["假手表说话", "一轮 ASR→LLM→TTS，脸在出声前就变"],
              ["静音看字幕", "reply_text 有字；喇叭是客户端关的"],
              ["记忆", "现场说一个偏好，下一轮能用"],
              ["电脑接管", "持麦从假手表切到 Windows，session_id 不变"],
              ["关掉主动说话", "再触发 nudge，云端拒绝，表除闹钟外不出声"],
            ]}
            striped
          />
        </Stack>
      ) : null}

      {view === "nudge" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="自主意识 = 白名单场合，不是随时插嘴">
            闹钟只是「到点开口」的一种。真正要做的是：在有手机/电脑时把决策和长句做完，表上只执行短句或缓存。默认安静，用户能关。
          </Callout>
          <H3>你要实现的策略</H3>
          <Table
            headers={["规则", "实现要点"]}
            rows={[
              ["默认安静", "没有 scene 匹配，不生成、不推送"],
              ["总开关", "settings.proactive_enabled=false → 所有 nudge 拒绝"],
              ["勿扰再判", "下发前读 DND；是则不下音频"],
              ["选端", "pc 在线且空闲 → target=pc；否则 phone；表只短句或缓存"],
              ["不抢麦", "已有 session holder 则推迟或改到另一端"],
              ["先生成后执行", "fire_at 到达时播缓存，不现场调 LLM"],
              ["每日上限", "主动次数（不含用户闹钟）默认 3"],
            ]}
            striped
          />
          <H3>场合怎么落地</H3>
          <Table
            headers={["场合", "云端做什么", "档"]}
            rows={[
              ["到点闹钟", "只同步 alarms，不调模型", "P0"],
              ["闹钟后一句", "设置时生成 text/audio 缓存字段", "P1"],
              ["对话设提醒", "LLM tool → 写 alarms", "P1"],
              ["久未对话", "看 last_session_at，可 delay 推 nudge", "P1"],
              ["电脑空闲打开", "Windows 上报 online；你下发短问候到 pc", "P1"],
              ["记忆里的日子", "读 memory 日期字段，本期可空着", "P2"],
            ]}
            rowTone={["info", "warning", "warning", "warning", "warning", "neutral"]}
          />
        </Stack>
      ) : null}

      {view === "week" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="M1 是你的硬节点">
            9/02 前必须文字或语音一轮 + 记忆能写。假手表可以还没接完，但客户端要能对着你的服务 curl 通。
          </Callout>
          <Table headers={["周", "主题", "交出来"]} rows={WEEKS} striped />
          <H2>每天可见进展（工作日）</H2>
          <Text>
            交一个：健康检查截图、一轮对话录屏、或接口变更说明（对照接口页）。没有进展就写卡在厂商 API、超时还是字段。
          </Text>
        </Stack>
      ) : null}

      {view === "out" ? (
        <Stack gap={16}>
          <Table headers={["不做", "原因"]} rows={OUT} rowTone={OUT.map(() => "danger" as const)} striped />
        </Stack>
      ) : null}
    </Stack>
  );
}
