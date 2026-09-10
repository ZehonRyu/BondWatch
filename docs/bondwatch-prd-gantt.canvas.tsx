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
  useHostTheme,
} from "cursor/canvas";

type Area = "all" | "product" | "keys" | "watch" | "android" | "windows" | "cloud" | "nfr" | "out";
type Priority = "P0" | "P1" | "P2";
type View = "gantt" | "buy" | "soft" | "people" | "req" | "freeze" | "hw" | "iface" | "accept" | "risk";

const VIEWS: { id: View; label: string }[] = [
  { id: "gantt", label: "甘特图" },
  { id: "buy", label: "采购清单" },
  { id: "soft", label: "软件下载" },
  { id: "people", label: "人手安排" },
  { id: "req", label: "需求表" },
  { id: "freeze", label: "冻结决策" },
  { id: "hw", label: "硬件逻辑" },
  { id: "iface", label: "接口约定" },
  { id: "accept", label: "验收演示" },
  { id: "risk", label: "风险台账" },
];

const AREAS: { id: Area; label: string }[] = [
  { id: "all", label: "全部" },
  { id: "product", label: "原则" },
  { id: "keys", label: "按键与交互" },
  { id: "watch", label: "手表" },
  { id: "android", label: "Android" },
  { id: "windows", label: "Windows" },
  { id: "cloud", label: "云端" },
  { id: "nfr", label: "非功能" },
  { id: "out", label: "本期不做" },
];

const REQUIREMENTS: {
  id: string;
  area: Area;
  priority: Priority;
  need: string;
  where: string;
  accept: string;
}[] = [
  { id: "P01", area: "product", priority: "P0", need: "AI 在远程服务器；手表只做眼睛和嘴巴", where: "全局", accept: "表端不跑大模型，断网时不能生成新对话" },
  { id: "P02", area: "product", priority: "P0", need: "Android / Windows 用来放大和管理同一伴侣，不是第二套人格", where: "全局", accept: "三端同一账号、同一记忆、同一角色" },
  { id: "P03", area: "product", priority: "P0", need: "多端必须能接续当前会话", where: "三端", accept: "电脑打开若表在说话，询问是否接管；拒绝则只做管理" },
  { id: "P04", area: "product", priority: "P0", need: "功能先行，开模放在尺寸冻结之后", where: "计划", accept: "手机假手表和开发板可戴之前，不启动开模" },
  { id: "P05", area: "product", priority: "P0", need: "要 4G 外出，同时要 Wi-Fi 作为家里和调试通道", where: "手表", accept: "两路都能连上同一后台；家里默认 Wi-Fi" },
  { id: "P06", area: "product", priority: "P0", need: "表在有手机/电脑时更智能：同一大脑；大屏负责管理、长聊、主动开口的决策和长句；表独自仍能走时、闹钟、勿扰、有网则对话", where: "全局", accept: "没手机时已同步闹钟仍响；有手机可改记忆/设场合；有电脑可接管长聊并承接主动长句" },
  { id: "P07", area: "product", priority: "P0", need: "允许一点自主开口，但默认安静：只在白名单场合说话，用户能总开关，勿扰时闭嘴", where: "全局", accept: "关掉「允许主动说话」后，除本地闹钟外不再主动出声" },

  { id: "K01", area: "keys", priority: "P0", need: "功能阶段两颗物理键 + 电容触摸；不单独做音量加减键", where: "手表", accept: "开模键位图只有电源 + 对讲；点屏对讲；音量不占用第三、四键" },
  { id: "K02", area: "keys", priority: "P0", need: "电源键：短按亮/息屏，长按开关机，双击扬声器静音并进勿扰", where: "手表", accept: "双击后喇叭为零，字幕强制开；再双击恢复" },
  { id: "K03", area: "keys", priority: "P0", need: "对讲键：短按开始/结束一轮；长按按住说话松开停；会话中短按打断播报", where: "手表", accept: "三种手感可演示；打断后立即停 TTS" },
  { id: "K04", area: "keys", priority: "P0", need: "音量在表端亮屏快捷栏调节，并在 Android / Windows 管理端可调", where: "三端", accept: "管理端改音量后，下次手表出声即生效" },
  { id: "K05", area: "keys", priority: "P1", need: "若开发板自带摇杆，可映射音量，但模具不预留第四键", where: "开发板", accept: "手板验证「戴着调音量」是否真痛，再决定后期加不加表冠" },
  { id: "K06", area: "keys", priority: "P2", need: "唤醒词本期不做；用户找它只靠点屏或按键。待机不持续上传音频", where: "手表", accept: "待机抓包无持续音频上行。唤醒词放到 35 天之后、换 IDF 再立项" },
  { id: "K07", area: "keys", priority: "P0", need: "实时对话与点按/PTT 都要有，可在管理端选默认；第一版实时按半双工", where: "手表+云", accept: "它说时不抢麦；你说或按打断才听" },
  { id: "K08", area: "keys", priority: "P0", need: "会话用超时、再见、电源键结束；结束后 4G 退回浅睡", where: "手表", accept: "结束 10 秒内模组离开全力态" },
  { id: "K09", area: "keys", priority: "P0", need: "表情始终在；低音量/勿扰/环境吵叠字幕；完全无声时表情演成脸并保留字幕", where: "手表", accept: "静音时无喇叭、有字幕、有脸；有声时优先表情" },
  { id: "K10", area: "keys", priority: "P0", need: "服务器先下情绪标签，再下语音；屏幕先于声音换表情", where: "云+表", accept: "第一包音频到达前，表情已切换" },
  { id: "K11", area: "keys", priority: "P0", need: "按键、点屏或唤醒才亮屏，不做常亮", where: "手表", accept: "待机电流不含常亮屏" },
  { id: "K12", area: "keys", priority: "P0", need: "人找 AI：点屏或按键（唤醒词本期不做）。AI 找人：白名单场合；闹钟本地响；其它主动开口可延迟且可关", where: "手表+云", accept: "没网闹钟仍响；关掉主动说话后不再搭话" },
  { id: "K13", area: "keys", priority: "P0", need: "电容触摸：点一下等于短按对讲；息屏时点一下亮屏；它说话时点一下打断", where: "手表", accept: "与对讲键同一套听-想-说；不在表上做菜单" },

  { id: "W01", area: "watch", priority: "P0", need: "聊天、能听、能说", where: "手表", accept: "一轮完整：听清、回声、表情对得上" },
  { id: "W02", area: "watch", priority: "P0", need: "本地闹钟：云端只同步设置，到点用本地铃声，不请求模型", where: "手表", accept: "飞行模式或拔卡后，已设闹钟仍响" },
  { id: "W03", area: "watch", priority: "P0", need: "勿扰本地生效：喇叭为零，只亮表情和字幕", where: "手表", accept: "断网后勿扰日程仍执行" },
  { id: "W04", area: "watch", priority: "P0", need: "断网仍走时、闹钟、没网表情和一句本地提示；对话排队", where: "手表", accept: "来网后未完成的对讲可补发或提示失败" },
  { id: "W05", area: "watch", priority: "P0", need: "被电脑接管后进入旁听或熄屏（可设，默认旁听、喇叭关）", where: "手表", accept: "旁听时麦关闭，屏幕跟会话表情和字幕；熄屏时只留振动/灯可选" },
  { id: "W06", area: "watch", priority: "P0", need: "旁听时按对讲键或点屏，向电脑请求拿回会话", where: "手表+Windows", accept: "电脑弹出确认；同意后表重新持麦" },
  { id: "W07", area: "watch", priority: "P0", need: "未绑定前屏幕显示配对二维码", where: "手表", accept: "手机扫码即可走完绑定" },
  { id: "W08", area: "watch", priority: "P1", need: "可接蓝牙耳机；有耳机时声音走耳机，没耳机且音量低走字幕", where: "手表", accept: "连接耳机后外放停" },
  { id: "W09", area: "watch", priority: "P1", need: "表端不缓存长期记忆正文", where: "手表", accept: "拆机或导出存储看不到对话记忆" },
  { id: "W10", area: "watch", priority: "P0", need: "产品要有表上摄像头。本套 DevKit 脚已满，用手机/网页「看」顶上同一条 /turn image_base64。下一版 PCB 必须留 DVP/CSI 座", where: "手表+云", accept: "假手表拍一张能短回；Arduino SEE 键能走「看着照片」状态机；真镜头不焊在这套面包板上" },
  { id: "W11", area: "watch", priority: "P0", need: "表情动画：竖屏全身挥手；横屏全屏大脸耷拉。网页 :11112 用 3D 感知手机横竖屏；真表 ESP32 自绘 2D，不跑 Three.js", where: "网页+固件", accept: "转手机出现横屏角标和大脸；Wokwi 串口 r 切换全身/大脸；真表不做屏外 3D 手" },
  { id: "W12", area: "watch", priority: "P0", need: "Arduino/Wokwi 用同一份 pins.h 把听-想-说、键、点屏、看、勿扰、闹钟、没网、4G 灯、横竖屏状态机先跑通", where: "固件", accept: "PlatformIO 能编过；Wokwi 能点 PWR/PTT/TAP/SEE；串口 h 有帮助" },

  { id: "A01", area: "android", priority: "P0", need: "手机 App 是绑定主入口：扫表上二维码或 BLE 近场", where: "Android", accept: "两种方式任选一种能把表绑到账号" },
  { id: "A02", area: "android", priority: "P0", need: "管理：人设、记忆查看/编辑、音量、亮度、勿扰日程、默认语音模式", where: "Android", accept: "改完后手表和下一次对话立刻按新设置" },
  { id: "A03", area: "android", priority: "P0", need: "放大：完整聊天历史，可在手机上继续同一段对话", where: "Android", accept: "与手表、Windows 看到同一线程" },
  { id: "A04", area: "android", priority: "P0", need: "开发期提供「假手表」小窗：用手机 4G/麦/喇叭/音量键代替真表", where: "Android", accept: "不依赖开发板即可验收聊天、记忆、闹钟、双语音、表情字幕、勿扰" },
  { id: "A05", area: "android", priority: "P1", need: "远程停用丢失的表；查看网络和流量状态", where: "Android", accept: "停用后该表证书立即失效" },
  { id: "A06", area: "android", priority: "P0", need: "管理端有「允许主动说话」总开关；场合列表可先写死两三项", where: "Android", accept: "关闭后除本地闹钟外不再主动出声；与 Windows 对齐" },
  { id: "A07", area: "android", priority: "P0", need: "假手表/网页用手机摄像头顶上表上的眼睛：压小照片发给云端，小精灵按图短回。云端接口按「表会拍照」来，不按「永远没有摄像头」来", where: "Android+云", accept: "拍一张图能收到一句看见了什么。表端拍照走同一 /turn image_base64；这套 DevKit 先不接镜头" },

  { id: "C01", area: "windows", priority: "P0", need: "同一账号登录，做大屏放大和管理，不单独做绑定向导（绑定以手机为准）", where: "Windows", accept: "未绑表的账号仍可先在电脑聊；绑表后自动出现设备" },
  { id: "C02", area: "windows", priority: "P0", need: "打开客户端时，若手表正在会话：询问是否接管", where: "Windows", accept: "弹窗含「接管」和「只做管理」；不自动抢麦" },
  { id: "C03", area: "windows", priority: "P0", need: "选择接管：电脑成为说话端；手表进入旁听或熄屏", where: "Windows+表", accept: "电脑出声，表不再占用麦；会话 ID 不变" },
  { id: "C04", area: "windows", priority: "P0", need: "选择只做管理：可看记忆、设置、历史，不插入当前会话", where: "Windows", accept: "管理操作不打断手表正在说的话" },
  { id: "C05", area: "windows", priority: "P0", need: "记忆、人设、勿扰、音量等管理能力与 Android 对齐", where: "Windows", accept: "一端改、另一端刷新可见" },
  { id: "C06", area: "windows", priority: "P1", need: "手表请求拿回时，电脑确认或超时交还", where: "Windows", accept: "交还后电脑改旁听/可关窗口，会话不断" },
  { id: "C07", area: "windows", priority: "P1", need: "电脑打开且当前无人在聊时，可在电脑上主动短问候或承接云端主动开口，不抢手腕", where: "Windows", accept: "表在勿扰或未戴时，主动内容出现在电脑；表正在持麦则不问候" },

  { id: "S01", area: "cloud", priority: "P0", need: "Windows、Android、手表共用同一套后台 API", where: "云端", accept: "一套鉴权、一套会话、一套记忆" },
  { id: "S02", area: "cloud", priority: "P0", need: "账号 + 设备身份；可吊销单台表", where: "云端", accept: "吊销后该表无法拉记忆、无法开麦" },
  { id: "S03", area: "cloud", priority: "P0", need: "会话接续：同一时间只允许一个端持麦", where: "云端", accept: "第二端想说话必须走接管协议" },
  { id: "S04", area: "cloud", priority: "P0", need: "长期记忆与短期会话分离存储", where: "云端", accept: "新会话仍能调用已写入的偏好" },
  { id: "S05", area: "cloud", priority: "P0", need: "流式语音识别和合成；回复带情绪标签", where: "云端", accept: "标签枚举固定，手表能映射到表情" },
  { id: "S06", area: "cloud", priority: "P0", need: "闹钟只同步配置，不在云端触发响铃", where: "云端", accept: "服务器宕机不影响已同步闹钟" },
  { id: "S07", area: "cloud", priority: "P1", need: "主动开口通道：白名单场合、可延迟、指定说在表/手机/电脑；勿扰和下发前再判一次", where: "云端", accept: "文档写明延迟上限；闹钟响铃不走该通道；有电脑时优先电脑说长句" },
  { id: "S08", area: "cloud", priority: "P1", need: "对话里说出提醒（如「十分钟后叫我」）可写入闹钟配置并同步到各端", where: "云+表", accept: "云端只写 alarms；到点仍由端上本地响，不回调模型" },
  { id: "S09", area: "cloud", priority: "P1", need: "设闹钟或主动场合时，可预生成一句陪伴文案缓存在端上", where: "云+表", accept: "到点播缓存句，不现场调模型。没生成过就只响铃" },
  { id: "S10", area: "cloud", priority: "P0", need: "文字/语音回合 POST /v1/sessions/{id}/turn；可选 image_base64。DeepSeek 不看图，先本地视觉摘要再喂模型", where: "云端", accept: "有图无图都返回 emotion + subtitle；表和手机走同一字段" },

  { id: "N01", area: "nfr", priority: "P0", need: "点屏或按键到开始听，用户无等待感", where: "手表", accept: "按键/点屏即按即听。唤醒词本期不做" },
  { id: "N02", area: "nfr", priority: "P0", need: "说完到第一包回复，好网络目标约 1.5 秒", where: "云+表", accept: "假手表在 Wi-Fi 下可测；4G 弱网允许更长并有思考表情" },
  { id: "N03", area: "nfr", priority: "P1", need: "待机按天计：常开只有按键、时钟、唤醒词", where: "硬件", accept: "开发板阶段给出电流分层测量，而不是只报对话耗电" },
  { id: "N04", area: "nfr", priority: "P0", need: "第一版安全底线：传输加密，表端不存记忆正文", where: "云+表", accept: "HTTPS/WSS；丢失可远程停用" },

  { id: "X01", area: "out", priority: "P2", need: "健康监测、GPS 轨迹、表盘市场、应用商店", where: "—", accept: "本期需求表不验收" },
  { id: "X04", area: "out", priority: "P2", need: "扒微信/系统日历/电脑屏幕、随时插嘴、新闻播报", where: "—", accept: "主动开口只认本产品里用户设的场合和记忆，不监控别的 App" },
  { id: "X02", area: "out", priority: "P2", need: "真全双工抢话、金属表壳天线精调、独立音量键/表冠", where: "—", accept: "手板证明刚需后再单独立项" },
  { id: "X03", area: "out", priority: "P2", need: "开模、认证、量产 BOM、运营商消费卡方案", where: "—", accept: "尺寸冻结后的下一阶段" },
  { id: "X05", area: "out", priority: "P2", need: "把 OV2640 焊到当前 DevKit 杜邦线上", where: "—", accept: "本期不验收真镜头出图。产品摄像头在下一版带座 PCB" },
];

const GANTT_START = Date.parse("2026-08-18");
const GANTT_END = Date.parse("2026-09-22");
const SPAN = GANTT_END - GANTT_START;

type GanttTask = {
  id: string;
  name: string;
  start: string;
  end: string;
  critical?: boolean;
  later?: boolean;
  note: string;
};

const TASKS: GanttTask[] = [
  { id: "g0", name: "工具链第一次跑通", start: "2026-08-18", end: "2026-08-21", critical: true, note: "日历 D1–D4。python 起 API、Arduino/Wokwi 全功能状态机、flutter doctor。D2 已能编过固件：键/点屏/看/勿扰/闹钟/没网/横竖屏。" },
  { id: "g1", name: "一次下单（含备用）", start: "2026-08-18", end: "2026-08-19", note: "D1–D2。小智散件 + 备用 N16R8 + 微雪 1.69/2.0 各一块 + 艾尔赛 Air780E AT+天线 + Hub/CH340。不买第二块 4G、官方 EGT、独立 Wi-Fi、GPS、电池、OV2640。" },
  { id: "g2", name: "云端最小对话", start: "2026-08-20", end: "2026-09-02", critical: true, note: "工作日约 D3–D12。FastAPI：登录、一轮语音/文字、记忆、情绪标签。" },
  { id: "g3", name: "Flutter 假手表 + iPhone 表盘", start: "2026-08-24", end: "2026-09-08", critical: true, note: "Android 假手表：聊、表情、字幕、闹钟、勿扰、绑定、手机看图。iPhone Safari :11112 只模拟 240×280 烧录脸：竖屏全身挥手，横屏大脸耷拉。" },
  { id: "g4", name: "开箱：灯、屏、串口", start: "2026-08-24", end: "2026-08-31", critical: true, note: "货已到 8/24。Hub+S3 烧现有固件，再按硬件逻辑页丝印表接 1.69 和两键。不要抄微雪 Wiki 的 ESP32 例程脚。" },
  { id: "g5", name: "麦、喇叭、两键", start: "2026-08-31", end: "2026-09-10", critical: true, note: "半双工。啸叫就拉开距离、它说时关麦。" },
  { id: "g6", name: "Air780E 能上网", start: "2026-09-02", end: "2026-09-14", note: "艾尔赛 AT 模块。先 Hub 5V + CH340 打 AT，再 UART 17/18 共地。加分项，不要挡假手表。本套不接 GPS。" },
  { id: "g7", name: "板子 Wi-Fi 连云端", start: "2026-09-07", end: "2026-09-17", critical: true, note: "硬件必须项：家里 Wi-Fi 一轮对话和表情。" },
  { id: "g8", name: "Windows 接管简版", start: "2026-09-08", end: "2026-09-18", note: "假手表能聊再开。编不过就网页兜底，不算失败。" },
  { id: "g9", name: "演示与收尾", start: "2026-09-17", end: "2026-09-22", critical: true, note: "正式演示锁定 9/18 周五。9/21–9/22 只修问题。周末 9/19–9/20 休息。" },
];

const MILESTONES = [
  ["M0", "8/21 五", "工具链 Hello", "Arduino/Wokwi 状态机能点；python 起 API；flutter doctor 无红叉"],
  ["M1", "9/02 三", "云端能对话", "文字或语音一轮 + 记忆能写；有图走同一 /turn"],
  ["M2", "9/08 二", "手机假手表能用", "现有手机：聊、表情、闹钟、勿扰、看图；iPhone :11112 能转横屏"],
  ["M3", "9/17 四", "面包板能听能说", "DevKit+屏+麦+喇叭，Wi-Fi 一轮"],
  ["M4", "9/18 五", "正式演示", "按验收页 8 步拍完。这是 37 天里真正要过的一天"],
  ["M5", "9/22 二", "工期截止", "含双休的第 37 天。只收尾，不新开功能"],
];

const EQUIPMENT: {
  wave: string;
  qty: string;
  item: string;
  sku: string;
  why: string;
  price: string;
  skip: string;
}[] = [
  { wave: "板", qty: "1+1", item: "小智散件 + 备用 N16R8", sku: "轩特佳/小智 DIY 套件（散件）含 N16R8、INMP441、MAX98357、喇叭、400 孔面包板×2、跳线、Type-C 数据线。再加一块焊好排针的 N16R8", why: "听和说 + 主控。套件里那块干活，另买一块备用。套件 0.91 OLED 不要当手表屏", price: "套件约 60 + 板约 32", skip: "不要全套成品/带外壳。不要 N8R2/N8R8。不要独立 Wi-Fi 板" },
  { wave: "板", qty: "1+1", item: "电容触摸彩屏（两种尺寸）", sku: "微雪 1.69inch Touch LCD Module（240×280 ST7789+CST816）×1 先点亮；微雪 2inch Capacitive Touch LCD（240×320）×1 大表。都必须引出 TP_SDA/SCL/INT/RST", why: "固件先按 240×280 写。2.0 只是高度多 40 像素，宽仍是 240。同款没备一份：坏了换另一种尺寸", price: "1.69 约 86；2.0 约 75", skip: "不要 1.9 寸 170×320。不要无触摸的 2inch LCD Module。不要圆屏/AMOLED/一体板" },
  { wave: "板", qty: "3+", item: "数字麦克风", sku: "INMP441 I2S，焊好排针，兼容芯片可。套件已有 1，另加备用", why: "静电和 L/R 接反最常见", price: "已在车", skip: "不要模拟麦" },
  { wave: "板", qty: "套件含", item: "功放 + 小喇叭", sku: "散件里的 MAX98357 + 8Ω 喇叭。缺了在轩特佳补", why: "喇叭线易焊断，功放脚易接反", price: "套件内", skip: "不要大功率外放" },
  { wave: "板", qty: "20–25", item: "轻触开关", sku: "6×6 mm 一包。不要只拍 1 颗", why: "电源 + 对讲仍要实体键。脚掰断立刻换", price: "几元", skip: "—" },
  { wave: "板", qty: "1", item: "4G 模块（AT）", sku: "艾尔赛 Air780E 带天线（AT固件），约 ¥42。确认 Nano 卡槽、出厂 AT、排针 5V/GND/TX/RX 已焊。没有板载 USB 是正常的", why: "外出上网。家里 Wi-Fi 就是容错。GPS 本套不接", price: "约 42", skip: "不要 EPM/EGH/EGG LuatOS。不要官方 ¥299 EGT。不要 DTU/银尔达。不要 A7670E。不要裸 LGA" },
  { wave: "板", qty: "后买", item: "物联网卡 / 电池 / GPS", sku: "卡先用闲置 Nano 手机卡。电池 3.7V + TP4056 对话通了再买。GPS 以后换 EGT 或另加 UART", why: "本套脚已满，GPS 不焊这套板。对话跑通前全程 USB", price: "这次 0", skip: "这次别买电池、GPS 板、第二块 4G" },
  { wave: "工具", qty: "1", item: "恒温烙铁套装", sku: "德力西 60W 可调温套装。已有则跳过", why: "焊排针和喇叭线。工具不买两套", price: "约 67", skip: "已有则不买" },
  { wave: "工具", qty: "家里有则跳", item: "数字万用表", sku: "带通断蜂鸣。查虚接比量电压更快", why: "一套够", price: "40–100", skip: "已有则不买" },
  { wave: "工具", qty: "1–2", item: "USB 转 TTL", sku: "CH340G，能切 3.3V，带杜邦线。4G 没 USB，电脑先试 AT", why: "日常烧录走 S3 的 Type-C。TTL 只给 4G 打 AT 或重刷", price: "约 3–10/个", skip: "不要 PL2303。不要 CH341A 烧录座" },
  { wave: "工具", qty: "套件含", item: "面包板和线", sku: "散件已有 400 孔×2、跳线、一根 Type-C 数据线。不够再补公母杜邦一包", why: "虚接是停工第一名。屏和 I2S 优先短跳线", price: "套件内", skip: "—" },
  { wave: "工具", qty: "1", item: "供电 USB Hub", sku: "USB 3.0，独立 5V 3A 墙上电源。S3 烧录和 Air780E 5V 必须分口", why: "4G 发射电流大，总线取电会把 S3 复位", price: "约 41", skip: "不要没变压器的总线 Hub" },
  { wave: "工具", qty: "套件1+再买1", item: "Type-C 数据线", sku: "必须能传数据。套件已有 1 根，再买 1 根备用。Hub 自带线不能烧板", why: "烧 S3。只充电线长得一样", price: "约 10–20", skip: "不要仅充电线。4G 模块没有 Micro-USB" },
];

const HW_PRACTICAL = [
  ["两块板必须分口供电", "S3 走自己的 Type-C。Air780E AT 模块没有 USB，用 Hub 的 5V+GND 供电，和 S3 只共地。笔记本直供，4G 一发射整板复位。"],
  ["电池往后放", "能对话之前全程 USB。电池引入电压跌落和充电噪声，新手极难查。这次先别买电池。"],
  ["4G 先接天线再上电", "艾尔赛模块先插天线。没天线发射会伤模组。天线离开麦克风 10cm 以上。出厂 AT：CH340 拨 3.3V 发 AT 应回 OK。"],
  ["麦和喇叭分两端", "面包板上把 INMP441 和喇叭尽量远离。软件上它说时关掉麦（半双工），比调回声消除实在。"],
  ["I2S、SPI 线要短", "杜邦线一长，屏花、音断。音频不稳就改焊到洞洞板，别在面包板上死磕。"],
  ["屏选 3.3V 电容触摸，I2C 单独接", "ESP32 是 3.3V。CST816 的 SDA/SCL 不是屏 SPI 那组 SCL/SDA。INT 必须接，RST 建议接。"],
  ["按键避开捆绑脚", "GPIO0 / 3 / 45 / 46 不要做电源或对讲。用错会无法下载或一按就复位。"],
  ["引脚表只准有一份", "写在 pins.h，改线先改文件再改实物。每改一次拍一张接线照片，发给 AI 时连文件一起贴。"],
  ["COM 口号会变", "换口、换线编号就变。程序里不要写死 COM3。"],
  ["备用件怎么用", "易坏件各备一份：线虚接先换线，板烧了换备用 S3。4G 板不双买，家里 Wi-Fi 顶上。贵的工具（烙铁、表、Hub）只买一套。"],
];

const HW_GENERIC = [
  ["只买 2.54mm + 3.3V 标准件", "SPI 屏、I2S 麦、I2S 功放、UART 4G。不买手表 FPC、异形排线，坏了找不到替件。"],
  ["网卡可替换", "应用层只认「有网」。先走 DevKit 的 Wi-Fi，Air780E 当第二个 Modem。换模组只换 AT，不改聊天逻辑。"],
  ["驱动三层分开", "显示 / 音频 / 网络三个模块。换一块 ST7789 只改显示文件，不要把引脚写进对话代码。"],
  ["4G 这次买，先独立跑", "艾尔赛 Air780E AT 一次下单。货到先 Hub 5V + CH340 打 AT，不要焊到 DevKit 背面。卡用闲置手机 Nano SIM。"],
  ["DevKit 保持官方形", "抄小智/乐鑫默认脚，不发明私有脚。AI 和例程才能对得上。"],
  ["外壳和 PCB 都往后", "35 天的形态就是面包板。洞洞板只为声学稳定，不是做手表胚。摄像头座留给下一版 PCB，不在这套杜邦线上硬挤。"],
];

const STAFF = [
  ["硬件", "1.0 全程", "货已到 8/24，按丝印表接线", "接线、安装、供电、面包板、屏麦喇叭键。4G 先独立打 AT", "不写业务代码。只按硬件逻辑页和 pins.h。每改一次拍照。线虚接先换备用件"],
  ["客户端", "1.0 全程", "D1 装工具链，货到烧录", "Arduino 固件编写和烧录、Flutter 假手表、Windows 大屏、设备连云端", "Android 先于 Windows。引脚只认接口页。货到先闪灯，再叠屏麦喇叭"],
  ["云端 AI", "1.0 全程", "D1 开通 API 并起服务", "FastAPI、鉴权、会话、记忆、ASR/LLM/TTS、情绪标签", "先把接口表写进仓库。客户端和固件都按这一份对接。Key 不进 Git"],
];

const STAFF_LOAD = [
  ["硬件", "0.5", "1.0", "1.0", "1.0", "1.0", "0.5"],
  ["客户端", "1.0", "1.0", "1.0", "1.0", "1.0", "0.5"],
  ["云端 AI", "1.0", "1.0", "1.0", "1.0", "0.8", "0.5"],
];

const SOFTWARE: {
  who: string;
  when: string;
  name: string;
  get: string;
  why: string;
}[] = [
  { who: "全员", when: "D1 今晚", name: "Git for Windows", get: "git-scm.com，安装时勾选加入 PATH", why: "三人共用同一仓库" },
  { who: "云端", when: "D1 今晚", name: "Python 3.12", get: "python.org，务必勾选 Add python.exe to PATH", why: "FastAPI 云端。客户端若装 ESP-IDF 也需要 Python" },
  { who: "客户端", when: "D1 今晚", name: "Android Studio", get: "developer.android.google.cn 国内镜像。装完 SDK、打开一次模拟器或真机", why: "Flutter 依赖 Android SDK；假手表要装到现有手机" },
  { who: "客户端", when: "D1 今晚", name: "Flutter SDK", get: "flutter.cn 稳定版，解压后把 flutter\\bin 加 PATH，跑 flutter doctor", why: "Android 假手表和 Windows 大屏同一套代码，35 天来不及写两套客户端" },
  { who: "客户端", when: "D1 今晚", name: "Visual Studio 2022 Build Tools", get: "visualstudio.microsoft.com，只勾「使用 C++ 的桌面开发」", why: "Flutter Windows 编译需要。体积大，今晚就开始下" },
  { who: "客户端", when: "D1", name: "手机 USB 调试", get: "不用下软件：手机开发者选项打开 USB 调试；Windows 若认不出再装厂商驱动", why: "假手表装到现有安卓机" },
  { who: "客户端", when: "D1 今晚", name: "Arduino IDE 2", get: "arduino.cc 装 2.x。开发板管理器加 esp32 by Espressif 3.x。板选 ESP32S3 Dev Module", why: "固件编写和烧录。今晚必须装，货到烧闪灯" },
  { who: "客户端", when: "D1 今晚", name: "ESP-IDF 5.3 Windows 安装包", get: "dl.espressif.com 的 esp-idf-tools-setup。先装上，D35 前能不用就不用", why: "以后做唤醒词才需要。新手前三周用 Arduino，避免两套环境一起炸" },
  { who: "客户端", when: "D1 今晚", name: "串口助手或合宙 Luatools", get: "任意串口助手即可。出厂已是 AT 不必刷机。Luatools 在 docs.openluat.com", why: "艾尔赛模块没 USB。CH340 接 5V/GND/TX/RX，发 AT 应回 OK" },
  { who: "客户端", when: "D1", name: "CH340 驱动", get: "插 CH340G 后设备管理器要有 COM 口。板上跳帽拨 3.3V", why: "4G 试 AT 或重刷才用。日常烧录走 S3 的 Type-C。不要 PL2303/CH341A" },
  { who: "客户端", when: "板子没到也能练", name: "Wokwi（浏览器）", get: "wokwi.com，选 ESP32-S3，用 Arduino 代码。不用安装", why: "在电脑上试闪灯、按键、串口打印。货没到也能练手，代码以后几乎能原样烧进 DevKit" },
  { who: "硬件", when: "板子没到也能练", name: "Falstad Circuit（浏览器）", get: "falstad.com/circuit ，英文页，点 Circuits 里的开关、上拉电阻例子", why: "搞懂按键为什么要上拉、3.3V 和 5V 不能混。不模拟整块手表" },
  { who: "硬件", when: "接线前画一张", name: "立创 EDA（浏览器）", get: "lceda.cn 免费账号。只画原理图，不布板", why: "把 pins.h 画成图：屏、麦、喇叭、键、4G 的 UART。发给客户端和云端时比纯文字清楚" },
  { who: "客户端", when: "D14 起", name: "MQTTX", get: "mqttx.app 下载 Windows 版", why: "Air780E 连上服务器前，先用电脑冒充设备发一条 MQTT，确认云端通" },
  { who: "硬件", when: "D1 起", name: "小智面包板文档（网页）", get: "不用装软件。搜「虾哥小智 ESP32-S3 N16R8 面包板」对照接线。把照片发给客户端", why: "接线资料最多。引脚不要自己发明" },
  { who: "云端", when: "D1", name: "大模型 + 语音 API", get: "不下载。开火山引擎（豆包 + 语音）或阿里云百炼，开通 LLM、ASR、TTS，把 Key 放到本地环境变量", why: "云端对话和手表听说都走远程。35 天不要自己部署模型" },
];

const DECISIONS = [
  ["产品形态", "AI 在云端；手表只做眼睛和嘴巴", "已冻", "不做端侧大模型"],
  ["双端定位", "Android / Windows 放大和管理同一伴侣，必须能接续", "已冻", "电脑打开询问接管；表旁听或熄屏"],
  ["伴侣加持", "手机和电脑让同一只表更聪明：记忆、管理、长聊、主动开口的决策和长内容。表负责随时叫、本地闹、短句", "已冻", "不另做第二个人格；不把手机功能搬进表里"],
  ["绑定", "手机 App 扫码或 BLE；Windows 不做绑定向导", "已冻", "—"],
  ["按键", "电源 + 对讲两颗实体键；点屏对讲；音量走软件；双击电源静音/勿扰", "已冻", "不做独立音量键；表上不做触屏菜单"],
  ["语音", "点屏或短按对讲一轮 + 长按 PTT + 半双工实时；用户找它靠点屏或按键。它找人只走白名单场合，可关", "已冻", "不做唤醒词；不做随时闲聊插嘴"],
  ["自主开口", "默认安静。白名单场合才说话。有手机/电脑时：想清楚+生成长句在大屏；表上只播短句或缓存句。勿扰闭嘴（本地闹钟除外）", "已冻", "不读微信/系统日历/屏幕；不现场为闹钟调模型；每天有次数上限"],
  ["屏幕", "固件先按 1.69 寸 240×280 ST7789+CST816。大表可选微雪 2.0 寸 240×320（宽仍 240）。表情优先；点一下=短按对讲", "已冻", "不要 1.9 寸 170×320、圆屏/AMOLED、无触摸 2inch LCD、套件 0.91 OLED 当主屏"],
  ["联网", "家里用 S3 板载 Wi-Fi；外出用艾尔赛 Air780E AT+天线（约 ¥42）。软件先走 Wi-Fi", "已冻", "不另买独立 Wi-Fi；不买第二块 4G；不买官方 ¥299 EGT/LuatOS EPM；本套不接 GPS"],
  ["硬件路线", "小智散件 N16R8 + 微雪触摸屏 + INMP441/MAX98357 + 艾尔赛 Air780E AT 独立模块", "已冻", "不买手表一体板；不买 DTU；电池/GPS 后买"],
  ["摄像头", "产品要有表上镜头。35 天这套 DevKit GPIO 已满，用手机/网页「看」顶上同一条看图链路。下一版板必须留 DVP/CSI 座，固件只换取图，不换云端协议", "已冻", "现在不买 OV2640 焊到面包板；不是产品取消摄像头"],
  ["看图", "DeepSeek 只吃文字。图先走本机 OpenCV，或任意 OpenAI 兼容视觉 API，再把一句话摘要喂给它", "已冻", "不把 OpenCV 塞进云端镜像；不指望 DeepSeek 直接看图"],
  ["软件栈", "FastAPI + Flutter；Arduino 点亮；IDF 后置", "已冻", "不并行第二套客户端"],
  ["表盘模拟", "iPhone 等手机浏览器打开 :11112，只显示 240×280 烧录脸。真表是 ESP32 自绘，不是安卓/iOS", "已冻", "11112 只模拟表盘效果，不假装手表里跑安卓"],
  ["3D 表情", "网页/手机 :11112：竖屏全身挥手，横屏全屏大脸耷拉，能感知手机横屏。真表 ESP32 自绘 2D，不跑 Three.js", "已冻", "真表不做屏外 3D 手；要类似效果就预渲染序列"],
  ["人手", "硬件接线安装 / 客户端编写烧录（固件+Android+Windows） / 云端 AI 实现，三人并行", "已冻", "不要让一个人同时扛三摊；假手表已带动画，真表固件眨眼张嘴即可"],
  ["工期", "2026-08-18 至 09-22，日历 37 天（含双休）。周末不排活，实际约 26 个工作日。正式演示 9/18", "已冻", "周末不加班赶新功能"],
];

const COMPANION = [
  ["仅手表 · 有网", "听、说、表情、字幕；用云端大脑。主动开口只播已缓存的短句", "随时叫一声。不编长文、不改记忆、不做复杂决策"],
  ["仅手表 · 断网", "走时、本地闹钟、勿扰、没网脸；已缓存的到点短句仍可播", "新对话和现场生成做不到。已设闹钟必须仍响"],
  ["手表 + 手机", "绑定、记忆、人设、场合白名单、总开关；主动开口可在手机说长一点", "决策和生成在有网时做完。不读微信、系统日历、健康"],
  ["手表 + 电脑", "长聊接管；空闲时可在电脑主动问候；长内容留在大屏", "打开时若表在聊则询问接管。不抢持麦，不读电脑屏幕"],
  ["闹钟 / 到点", "配置来自手机、电脑或对话 → 云端只存 → 端上本地响", "到点不请求模型。没手机、断网、云挂了，已同步的仍响"],
];

const PROACTIVE = [
  ["到点闹钟", "P0", "用户设的时间", "表本地铃", "必须。不调模型"],
  ["闹钟后一句陪伴", "P1", "设闹钟时云端预生成，缓存在端", "表短句（约 8 秒）", "没缓存就只响铃"],
  ["对话写成提醒", "P1", "「十分钟后叫我」→ alarms", "到点仍本地", "应该有"],
  ["久未对话", "P1", "云端看上次会话时间", "电脑优先，否则手机；表最多一句", "可延迟，适配 4G 浅睡"],
  ["电脑打开且空闲", "P1", "Windows 事件", "只在电脑说", "表持麦则不问候"],
  ["记忆里的日子", "P2", "记忆字段（考试、生日）", "有网才生成；先大屏", "本期可不做"],
  ["勿扰结束 / 回到有网", "P1", "端上日程或重连", "短提示或补发失败", "回来有网已在断网需求里"],
  ["微信 / 日历 / 新闻 / 看你屏幕插嘴", "不做", "—", "—", "白名单以外一律闭嘴"],
];

const PROACTIVE_RULES = [
  ["默认安静", "没有勾选的场合，不开口。宁可漏，不要烦"],
  ["总开关", "「允许主动说话」。关掉后只剩用户找它，加上本地闹钟"],
  ["勿扰 / 静音", "主动开口为零。最多表情。本地闹钟仍可响（用户自己设的）"],
  ["不抢麦", "已有持麦端时，主动内容去另一端或推迟，绝不双开喇叭"],
  ["大屏加持", "想清楚、生成长句、选场合，都在手机/电脑在线时做。表只执行短句或缓存"],
  ["表上短", "手腕主动出声 ≤ 约 8 秒。更长的去手机或电脑"],
  ["每天上限", "主动开口（不含用户点的闹钟）建议 ≤ 3 次，可在设置改"],
  ["先生成后执行", "到点不现场喊模型。4G 浅睡只收已经想好的一句"],
];

const API_ROWS = [
  ["POST /v1/auth/login", "手机/电脑登录", "返回 token 和 user_id"],
  ["POST /v1/devices/bind", "扫码绑定设备", "body: device_id, code"],
  ["POST /v1/sessions", "开一轮对话", "返回 session_id；同时只允许一个持麦端"],
  ["POST /v1/sessions/{id}/turn", "一轮文字/可选照片", "body: text, image_base64? 返回 emotion + subtitle。表和手机同一字段"],
  ["POST /v1/sessions/{id}/takeover", "电脑接管或手表拿回", "body: action=takeover|listen|release"],
  ["WS /v1/sessions/{id}/audio", "上行 PCM/opus，下行 TTS 流 + emotion", "先到 emotion，再到音频"],
  ["GET/PUT /v1/memory", "长期记忆读写", "三端同一份"],
  ["GET/PUT /v1/alarms", "闹钟配置", "只同步；响铃在端上"],
  ["PUT /v1/settings", "音量、勿扰、默认语音模式、允许主动说话、每日上限、场合勾选", "改完各端刷新"],
  ["POST /v1/nudges", "创建一次主动开口（可 delay、指定 watch/phone/pc）", "P1。勿扰则拒绝。到点闹钟不走这条"],
];

const EMOTION_ROWS = [
  ["idle", "待机", "呼吸 + 眨眼"],
  ["listen", "聆听", "耳朵动、嘴轻张"],
  ["think", "思考", "歪头晃"],
  ["speak", "说话", "嘴一张一合"],
  ["quiet", "小声/勿扰", "表情 + 字幕"],
  ["silent", "完全无声", "脸 + 字幕，喇叭为零"],
  ["alarm", "闹钟", "晃脸，本地铃"],
  ["offline", "没网", "发灰闪一下 + 短字"],
];

const PIN_ROWS = [
  ["电源键", "6×6 一脚", "GPIO2", "另一脚 GND。不要用 GPIO0"],
  ["对讲键", "6×6 一脚", "GPIO1", "另一脚 GND。点屏和短按对讲同一套逻辑"],
  ["屏电源", "VCC", "3V3", "只接 S3 的 3.3V，不要接 5V。2.0 的 3V3 脚空着"],
  ["屏地", "GND", "GND", "必须和 S3 共地"],
  ["屏 MOSI", "1.69: LCD_DIN　2.0: MOSI", "GPIO11", "有的线标 SDA，那是屏 SPI，不是触摸"],
  ["屏 SCK", "1.69: LCD_CLK　2.0: SCLK", "GPIO12", "不是触摸的 SCL"],
  ["屏 CS", "LCD_CS", "GPIO10", "片选，低有效"],
  ["屏 DC", "LCD_DC", "GPIO8", "数据/命令"],
  ["屏 RST", "LCD_RST", "GPIO9", "复位，低有效"],
  ["屏背光", "LCD_BL", "GPIO13", "也可先接 3V3 常亮"],
  ["触摸 SDA", "TP_SDA", "GPIO14", "CST816 I2C。必须 3.3V"],
  ["触摸 SCL", "TP_SCL", "GPIO21", "不要和屏 SPI 接到同一组线"],
  ["触摸 INT", "1.69: TP_IRQ　2.0: TP_INT", "GPIO38", "按下为低"],
  ["触摸 RST", "TP_RST", "GPIO39", "上电先拉低再拉高"],
  ["麦电源", "INMP441 VDD", "3V3", "不要接 5V"],
  ["麦地 / 声道", "GND、L/R", "GND", "L/R 接地=左声道"],
  ["麦 WS", "WS / LRCK / WS", "GPIO4", "I2S 字选择"],
  ["麦 SCK", "SCK / BCLK", "GPIO5", "I2S 时钟"],
  ["麦 SD", "SD / DOUT", "GPIO6", "I2S 数据入。真机这里不是灯"],
  ["功放电源", "MAX98357 VIN", "3V3", "GND 共地。SD 使能脚空着或接 3V3"],
  ["喇叭 BCLK", "BCLK", "GPIO15", "MAX98357"],
  ["喇叭 LRC", "LRC / LRCLK / WS", "GPIO16", "MAX98357"],
  ["喇叭 DIN", "DIN", "GPIO7", "I2S 数据出。真机这里不是蜂鸣器"],
  ["喇叭本体", "OUT+ / OUT-", "功放喇叭焊盘", "不要接到 S3 GPIO"],
  ["4G 电源", "Air780E 5V、GND", "Hub 5V、S3 GND", "只共地，不要用 S3 的 3.3V 给 4G 供电。先别接"],
  ["4G → ESP RX", "模组 TX", "GPIO17", "交叉：模组 TX 接 ESP RX。天线先装再上电"],
  ["4G ← ESP TX", "模组 RX", "GPIO18", "模组 RX 接 ESP TX。先用 CH340 打 AT"],
];

const WIRE_STEPS = [
  ["0", "Hub 插墙。S3 的 Type-C 插 Hub。电脑只连 Hub。烧现有固件，串口出现 BondWatch"],
  ["1", "两颗 6×6：GPIO2→GND 电源，GPIO1→GND 对讲。短按应能息屏"],
  ["2", "只接微雪 1.69（12 根：VCC/GND + 6 根屏 + 4 根触摸）。上电应出脸。2.0 先放着"],
  ["3", "INMP441：VDD→3V3，GND 和 L/R→GND，WS→4，SCK→5，SD→6。线尽量短"],
  ["4", "MAX98357：VIN→3V3，BCLK→15，LRC→16，DIN→7。喇叭焊到功放 OUT+/OUT-，拉开麦"],
  ["5", "4G 最后：先天线+Nano 卡+Hub 5V+CH340 打 AT。OK 后再 TX/RX 交叉接到 17/18"],
];

const WIRE_SKIP = [
  ["微雪 Wiki「ESP32S3 例程」那张脚", "那是他们一体板的脚，会把 GPIO2 接到背光、GPIO6 接到触摸，和 pins.h 冲突"],
  ["套件 0.91 OLED", "不是手表屏，空着"],
  ["2.0 屏（先）", "1.69 点亮后再换，脚相同。MISO、SD_CS 空着"],
  ["GPIO40 / 41 / 42 / 47", "Wokwi 的 SEE、4G灯、ROT、TALK。真机不要接"],
  ["OV2640、GPS、电池", "本套不接。对话通了再谈电池"],
  ["Air780E 的 5V 接到 S3 3V3", "会烧板。4G 吃 Hub 5V，UART 才接到 S3"],
];

const HW_MAP = [
  ["电源键", "GPIO2 轻触接地", "同脚 PWR", "同脚"],
  ["对讲键", "GPIO1 轻触接地", "同脚 PTT", "同脚"],
  ["点屏", "CST816 INT GPIO38", "TAP 拉低同一脚", "换成 CST816 芯片，软件仍看下降沿"],
  ["彩屏", "ST7789 240×280 SPI（可换 2.0 240×320 同脚）", "同脚 ILI9341", "先接微雪 1.69 Touch；大表换 2.0 Capacitive Touch"],
  ["触摸 I2C", "SDA14 SCL21 RST39", "只做复位，不仿协议", "接 CST816"],
  ["麦克风", "INMP441 I2S 4/5/6", "GPIO6 = MIC 灯（听亮说灭）", "I2S 真采麦；半双工逻辑不变"],
  ["喇叭", "MAX98357 I2S 15/16/7", "GPIO7 = 蜂鸣器", "I2S 真放音"],
  ["看 / 摄像头", "下一版 DVP/CSI 座", "SEE GPIO40，只置「有图」", "驱动出 JPEG，仍走 /turn image_base64"],
  ["家里网", "S3 板载 Wi-Fi", "默认有网；串口 o/n", "真连云端 HTTPS"],
  ["外出网", "艾尔赛 Air780E AT UART 17/18", "串口 4，GPIO41 灯=注网", "Hub 5V + 天线 + Nano 卡；AT 注网后当第二个 Modem"],
  ["横竖屏", "以后方向/重力", "串口 r；网页感知手机旋转", "真表 2D 自绘，不跑 Three.js"],
  ["云端对话", "Wi-Fi 或 4G 上 HTTPS", "固件内 mockTurn，字段同 /turn", "换成真 HTTP，状态机不改"],
];

const HW_FEASIBLE = [
  ["听-想-说状态机、键、点屏、勿扰、息屏、闹钟、没网脸", "已在 Arduino 编过，Wokwi 可点；真机同一份 pins.h"],
  ["半双工：听时关喇叭、说时关麦", "仿真用灯/蜂鸣器；真机 GPIO6/7 改接 I2S 麦和功放"],
  ["看图链路（有图标志 → 短回）", "真机先用手机「看」。不要焊摄像头"],
  ["I2S 真录音/放音、CST816、Air780E", "货已到。按下面顺序接：先 1.69+两键，再麦喇叭，4G 最后"],
];

const ACCEPT_MUST = [
  ["云端", "用手机或电脑完成一轮文字或语音对话，记忆能写下再读出"],
  ["假手表", "现有安卓机：表情状态、字幕、本地闹钟、勿扰、能绑定账号"],
  ["看图", "手机或网页拍一张，能收到看见了什么；走 /turn image_base64"],
  ["表盘模拟", "iPhone Safari 打开局域网 :11112，竖屏全身挥手，横屏大脸耷拉"],
  ["面包板", "DevKit+触摸屏+麦+喇叭+两键，点屏或 PTT 完成一轮半双工对话"],
  ["安全", "Key 不进 Git；HTTPS/WSS；表端不存长期记忆正文"],
];

const ACCEPT_SHOULD = [
  ["Windows", "打开时若假手表在聊，询问是否接管"],
  ["4G", "艾尔赛 Air780E AT 独立注网；CH340 能打出 AT/OK，再用 MQTTX 看一条上行"],
  ["字幕策略", "静音后强制字幕"],
  ["对话设闹钟", "对假手表说「1 分钟后叫我」，配置同步，到点本地响"],
  ["主动开口", "关掉总开关后不再主动出声；电脑空闲可短问候（有则演示）"],
];

const ACCEPT_STRETCH = [
  ["唤醒词", "ESP-SR 本地唤醒"],
  ["4G 拼主控", "UART 让 DevKit 走蜂窝上网"],
  ["精致 Windows", "旁听画面、拿回确认做完整"],
  ["久未说话主动开口", "超过设定时长后电脑或手机先说，表最多一句"],
];

const DEMO_STEPS = [
  ["1", "打开假手表或 :11112，看待机表情（竖屏挥手）"],
  ["2", "点屏或按对讲，出现聆听 → 思考 → 说话；MIC 亮时喇叭关"],
  ["3", "把手机横过来（或 Wokwi 串口 r），大脸耷拉"],
  ["4", "点「看」或 SEE，走看着照片再短回"],
  ["5", "把音量打到静音或双击电源，字幕出现、喇叭为零"],
  ["6", "设一个 1 分钟闹钟，断网或关云端仍响"],
  ["7", "面包板重复步骤 2（Wi-Fi）；展示 pins.h 和接线照片一致"],
  ["8", "电脑打开询问接管（有则演示）；加分：Air780E 注网"],
];

const RISKS = [
  ["安装耗过 5 天", "高", "高", "客户端", "D5 仍无 Hello", "砍 Windows 和 IDF，只保 Arduino + 假手表调试"],
  ["面包板声学啸叫", "高", "中", "硬件", "一对讲就自激", "拉开麦和喇叭；客户端它说时关麦；改焊洞洞板"],
  ["USB 一发射就掉口", "高", "高", "硬件", "4G 注网或播音时 COM 消失", "上带电源的 Hub；两板分口供电"],
  ["物联网卡不能出网", "中", "中", "硬件", "AT 注网成功但连不上服务器", "先用手机热点给 DevKit；换可公网出站的卡"],
  ["Flutter Windows 编不过", "中", "低", "客户端", "D20 后还在斗 VS Build Tools", "D35 用手机 + 浏览器管理页兜底"],
  ["范围反弹（开模/唤醒词/圆屏）", "高", "高", "负责人", "有人说「顺便做了吧」", "先改冻结决策表，否则不准进甘特"],
  ["API 费用或审核", "中", "中", "云端 AI", "Key 欠费或内容被拦", "设月度额度；失败时端上显示 offline 脸"],
  ["缺任一角色一周", "中", "高", "负责人", "硬件/客户端/云端有人缺席", "缺硬件：假手表验收；缺客户端：只验 API；缺云端：端上显示没网脸"],
  ["工期紧、只有约 26 个工作日", "高", "高", "负责人", "有人想加唤醒词/开模/圆屏", "先改冻结决策。必须项保假手表+面包板 Wi-Fi"],
  ["主动开口太烦", "中", "高", "云端 AI", "用户关掉主动或差评吵", "默认安静；白名单；每日上限；勿扰闭嘴；有电脑先电脑说"],
  ["引脚各改各的", "高", "中", "全员", "有人生成了另一套 GPIO", "只准改接口约定和 pins.h 这一张表，硬件拍照存档"],
  ["把「现在不焊镜头」当成产品取消摄像头", "中", "高", "负责人", "采购或需求写成永远没有眼睛", "冻结：产品要镜头；本套板用手机顶上；下一版留 DVP 座"],
  ["把网页 3D 当成真表能力", "中", "中", "客户端", "有人要 ESP32 跑 Three.js 或屏外手", "真表只自绘 2D；3D 只在 :11112"],
];

const RULES = [
  ["每天（工作日）", "硬件、客户端、云端各交一个可见进展（接线照片 / 录屏 20 秒 / 接口可调用）。没有进展就写卡在哪。周末不要求"],
  ["问 AI", "硬件带接线照片；客户端带完整日志和 pins.h；云端带完整报错。不要只说「不行了」"],
  ["改需求", "先改「冻结决策」页，再改甘特。口头答应不算数"],
  ["联调", "接口以本看板「接口约定」为准，三边禁止私自加字段"],
  ["密钥", "火山/阿里 Key 只放云端本机环境变量，禁止提交仓库"],
];

const MONTHS = [
  { label: "8/18", at: "2026-08-18" },
  { label: "8/24", at: "2026-08-24" },
  { label: "8/31", at: "2026-08-31" },
  { label: "9/7", at: "2026-09-07" },
  { label: "9/14", at: "2026-09-14" },
  { label: "9/21", at: "2026-09-21" },
];

function pct(iso: string): number {
  const t = Date.parse(iso);
  return Math.min(100, Math.max(0, ((t - GANTT_START) / SPAN) * 100));
}

function GanttChart({
  selected,
  onSelect,
}: {
  selected: string;
  onSelect: (id: string) => void;
}) {
  const theme = useHostTheme();
  const today = pct("2026-08-19");
  const selectedTask = TASKS.find((task) => task.id === selected);

  return (
    <Stack gap={12}>
      <div style={{ display: "grid", gridTemplateColumns: "176px 1fr", gap: 8, alignItems: "center" }}>
        <Text size="small" tone="tertiary">
          工作流
        </Text>
        <div style={{ position: "relative", height: 22 }}>
          {MONTHS.map((month) => (
            <span
              key={month.label}
              style={{
                position: "absolute",
                left: `${pct(month.at)}%`,
                color: theme.text.tertiary,
                fontSize: 12,
              }}
            >
              {month.label}
            </span>
          ))}
        </div>
      </div>
      {TASKS.map((task) => {
        const left = pct(task.start);
        const right = pct(task.end);
        const width = Math.max(1.5, right - left);
        const active = selected === task.id;
        const fill = task.later
          ? theme.fill.tertiary
          : task.critical
            ? theme.accent.primary
            : theme.fill.primary;
        const color = task.critical && !task.later ? theme.text.onAccent : theme.text.primary;
        return (
          <div
            key={task.id}
            style={{ display: "grid", gridTemplateColumns: "176px 1fr", gap: 8, alignItems: "center" }}
          >
            <Text size="small" weight={active ? "semibold" : "normal"}>
              {task.name}
            </Text>
            <button
              type="button"
              onClick={() => onSelect(task.id)}
              title={`${task.start} 至 ${task.end}`}
              style={{
                position: "relative",
                height: 28,
                padding: 0,
                border: `1px solid ${theme.stroke.tertiary}`,
                background: theme.bg.editor,
                cursor: "pointer",
                textAlign: "left",
              }}
            >
              {MONTHS.slice(1).map((month) => (
                <span
                  key={month.label}
                  style={{
                    position: "absolute",
                    top: 0,
                    bottom: 0,
                    left: `${pct(month.at)}%`,
                    width: 1,
                    background: theme.stroke.tertiary,
                  }}
                />
              ))}
              <span
                style={{
                  position: "absolute",
                  top: 0,
                  bottom: 0,
                  left: `${today}%`,
                  width: 1,
                  background: theme.accent.primary,
                }}
              />
              <span
                style={{
                  position: "absolute",
                  top: 5,
                  bottom: 5,
                  left: `${left}%`,
                  width: `${width}%`,
                  background: fill,
                  outline: active ? `2px solid ${theme.accent.control}` : "none",
                  display: "flex",
                  alignItems: "center",
                  paddingLeft: 6,
                  paddingRight: 6,
                  overflow: "hidden",
                  color,
                  fontSize: 11,
                  whiteSpace: "nowrap",
                }}
              >
                {task.start.slice(5)}–{task.end.slice(5)}
              </span>
            </button>
          </div>
        );
      })}
      <Row gap={16} wrap>
        <Text size="small" tone="secondary">
          色块：关键路径
        </Text>
        <Text size="small" tone="secondary">
          浅块：可并行
        </Text>
        <Text size="small" tone="secondary">
          竖线：今天 2026-08-19（日历 D2）· 截止 9/22（日历 D37，含双休）
        </Text>
      </Row>
      {selectedTask ? (
        <Callout tone={selectedTask.later ? "neutral" : selectedTask.critical ? "info" : "neutral"} title={`${selectedTask.name}  ·  ${selectedTask.start} 至 ${selectedTask.end}`}>
          {selectedTask.note}
        </Callout>
      ) : (
        <Text tone="secondary">点击甘特条查看该段在做什么。</Text>
      )}
    </Stack>
  );
}

export default function BondWatchPrdGantt() {
  const [view, setView] = useCanvasState<View>("view", "gantt");
  const [area, setArea] = useCanvasState<Area>("req-area", "all");
  const [priority, setPriority] = useCanvasState<Priority | "all">("req-pri", "all");
  const [ganttId, setGanttId] = useCanvasState<string>("gantt-id", "g0");
  const [buyWave, setBuyWave] = useCanvasState<string>("buy-wave", "全部");
  const [softWho, setSoftWho] = useCanvasState<string>("soft-who", "全部");

  const rows = REQUIREMENTS.filter(
    (item) => (area === "all" || item.area === area) && (priority === "all" || item.priority === priority),
  );
  const waveOk = buyWave === "板" || buyWave === "工具" ? buyWave : "全部";
  const gear = EQUIPMENT.filter((item) => waveOk === "全部" || item.wave === waveOk);
  const softOk = ["全员", "硬件", "客户端", "云端"].includes(softWho) ? softWho : "全部";
  const apps = SOFTWARE.filter((item) => softOk === "全部" || item.who === softOk);

  return (
    <Stack gap={24}>
      <Stack gap={8}>
        <H1>BondWatch 计划看板</H1>
        <Text tone="secondary">
          项目负责人看板。今天 2026-08-19，日历 D2。必须项：云端 + 假手表 + 面包板 Wi-Fi。表盘 :11112 和 Arduino 仿真用来把状态机先做对。
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

      {view === "gantt" ? (
        <Stack gap={20}>
          <Grid columns={4} gap={16}>
            <Stat value="8/24" label="今天工作日" tone="warning" />
            <Stat value="9/08" label="手机假手表" tone="info" />
            <Stat value="9/18" label="正式演示" />
            <Stat value="9/22" label="37 天截止" />
          </Grid>
          <Callout tone="warning" title="37 天含双休：周末休息，工作日大约 26 天">
            甘特轴按日历画，周六日不断开，但任务只排周一到周五。正式演示 9/18。硬件已到：本周先把 1.69 和两键接亮。必须项是云端 + 假手表 + 面包板 Wi-Fi。
          </Callout>
          <GanttChart selected={ganttId} onSelect={setGanttId} />
          <H3>里程碑</H3>
          <Table
            headers={["里程碑", "日期", "含义", "通过标准"]}
            rows={MILESTONES}
            rowTone={["success", "info", "success", "info", "warning", "success"]}
            striped
          />
        </Stack>
      ) : null}

      {view === "buy" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="货已到 8/24，采购表留底">
            现在对着硬件逻辑页接线。Wi-Fi 用 S3 板载。4G、电池、GPS 先别往面包板上焊。
          </Callout>
          <Grid columns={4} gap={16}>
            <Stat value="N16R8×2" label="套件 1 + 备用 1" tone="warning" />
            <Stat value="1.69+2.0" label="先 240×280，大表 240×320" tone="info" />
            <Stat value="Air780E AT" label="约 ¥42，无板载 USB" />
            <Stat value="约 400–550" label="含 Hub/烙铁/CH340（元）" />
          </Grid>
          <Row gap={8} wrap>
            {["全部", "板", "工具"].map((wave) => (
              <span key={wave}>
                <Pill active={waveOk === wave} onClick={() => setBuyWave(wave)}>
                  {wave === "全部" ? "全部" : wave === "板" ? "开发板" : "基础工具"}
                </Pill>
              </span>
            ))}
          </Row>
          <Table
            headers={["类", "数量", "设备", "搜这个买", "用来干什么", "约价（元）", "有则跳过 / 别买错"]}
            rows={gear.map((item) => [item.wave, item.qty, item.item, item.sku, item.why, item.price, item.skip])}
            striped
            stickyHeader
          />
          <H3>明确这次不买</H3>
          <Table
            headers={["别买", "原因"]}
            rows={[
              ["独立 Wi-Fi 模块（ESP8266 / 另一块 ESP32）", "S3 已经有 Wi-Fi。再买一块只会双电台、双供电、引脚打架"],
              ["第二块 4G、官方 ¥299 EGT、LuatOS EPM、DTU", "贵的不双买。4G 挂了用家里 Wi-Fi。GPS 以后再说"],
              ["1.9 寸 170×320、无触摸 2inch LCD、圆屏 AMOLED、手表一体板", "分辨率或形态不对。套件 0.91 OLED 也不要当主屏"],
              ["N8R2 / 没 PSRAM 的 ESP32-S3", "语音缓存不够。必须 N16R8"],
              ["OV2640、电池、GPS 焊到当前面包板", "脚已满。镜头/定位放到下一块板；电池对话通了再接"],
            ]}
            rowTone={["danger", "warning", "warning", "danger", "warning"]}
          />
          <H2>初期硬件还要注意什么</H2>
          <H3>实用性（不注意就会停工）</H3>
          <Table headers={["注意", "具体怎么做"]} rows={HW_PRACTICAL.map((row) => [row[0], row[1]])} striped />
          <H3>通用性（现在这么搭，以后才拆得开）</H3>
          <Table headers={["原则", "具体怎么做"]} rows={HW_GENERIC.map((row) => [row[0], row[1]])} striped />
        </Stack>
      ) : null}

      {view === "people" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="三人并行，每人前 5 天只做自己的 Hello">
            硬件：货已到，按硬件逻辑页丝印表接线拍照，不写业务代码。客户端：Arduino/Wokwi 状态机已能点；假手表先于 Windows。云端：开通语音和大模型 API，看图走 /turn image_base64。
          </Callout>
          <Table
            headers={["角色", "投入", "何时到位", "主责", "怎么找"]}
            rows={STAFF}
            striped
          />
          <H3>人力负荷（按自然周，周末不排）</H3>
          <Table
            headers={["角色", "W1 8/18", "W2 8/24", "W3 8/31", "W4 9/7", "W5 9/14", "W6 9/21"]}
            rows={STAFF_LOAD}
            striped
          />
          <H3>三人怎么交接</H3>
          <Table
            headers={["从谁到谁", "交什么", "怎样算交清"]}
            rows={[
              ["硬件 → 客户端", "接线照片 + 实物已按 pins.h", "能烧闪灯，键和屏有反应"],
              ["云端 → 客户端", "接口约定 + 本地可跑的 FastAPI", "假手表能打一轮对话，记忆能写下"],
              ["客户端 → 云端", "设备或假手表连上同一套 API", "情绪标签先于音频到达屏幕"],
            ]}
            rowTone={["warning", "info", "success"]}
          />
          <H3>缺人时怎么降级</H3>
          <Text>
            缺硬件：D35 用手机假手表验收，板子后补。缺客户端：只验云端 API。缺云端：端上只显示没网脸和本地闹钟。表情用动画色块顶上。
          </Text>
          <H3>项目怎么运转</H3>
          <Table headers={["频率", "规矩"]} rows={RULES} striped />
        </Stack>
      ) : null}

      {view === "soft" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="今晚就装，前 5 天只追求 Hello">
            Arduino IDE 2 由客户端装。板子改成通用 DevKit 后，Arduino 和小智教程是同一条路。ESP-IDF 今晚开始下，唤醒词放到 35 天之后。VS Build Tools 和 Android Studio 体积大，先丢下去下。云端今晚开通 API。硬件今晚看接线图。
          </Callout>
          <Callout tone="info" title="电路可以先在浏览器里试，不必先装 LTspice">
            板子没到：客户端用 Wokwi 跑 ESP32 程序，硬件用 Falstad 练上拉和 3.3V。接线前：硬件用立创 EDA 画一张原理图当 pins.h。不要用 Multisim/Proteus 去仿真整块手表，模块电路不是那样设计的。
          </Callout>
          <Grid columns={4} gap={16}>
            <Stat value="今晚" label="开始下大安装包" tone="warning" />
            <Stat value="Wokwi" label="先在浏览器练 ESP32" tone="info" />
            <Stat value="Arduino 2" label="客户端必须装" />
            <Stat value="云 API" label="不下载模型" />
          </Grid>
          <Row gap={8} wrap>
            {["全部", "全员", "硬件", "客户端", "云端"].map((who) => (
              <span key={who}>
                <Pill active={softOk === who} onClick={() => setSoftWho(who)}>
                  {who === "全部" ? "全部" : who}
                </Pill>
              </span>
            ))}
          </Row>
          <Table
            headers={["谁装", "何时", "软件", "怎么拿", "用来干什么"]}
            rows={apps.map((item) => [item.who, item.when, item.name, item.get, item.why])}
            striped
            stickyHeader
          />
          <H3>一点点尝试的顺序</H3>
          <Table
            headers={["第几步", "用什么", "试什么", "和真机的关系"]}
            rows={[
              ["1", "Falstad", "开关 + 上拉电阻、LED + 限流电阻、3.3V 电源", "搞懂按键怎么接，避免把 5V 接到 ESP32"],
              ["2", "Wokwi ESP32-S3", "PWR/PTT/TAP/SEE、听-想-说、勿扰、闹钟、没网、串口 r 横竖屏", "同一份 Arduino 代码，货到几乎能直接烧"],
              ["3", "立创 EDA 原理图", "把屏、麦、喇叭、键、4G 的 UART 画成一张图", "就是以后的 pins.h，发给 AI 对线"],
              ["4", "真机 USB 闪灯", "DevKit 插上，烧 Wokwi 里跑通的那份", "第一次真硬件"],
              ["5", "真机加屏、触摸、键", "对着原理图接 ST7789、CST816 和两颗键", "仍先不要麦和 4G"],
              ["6", "真机加麦喇叭", "半双工：按键说话、松开播放", "声学问题模拟器做不到，必须真机"],
              ["7", "Air780E AT + MQTTX", "Hub 5V 供电，CH340 先打 AT，再接到 GPIO17/18", "4G 和射频无法在 Wokwi 里仿真"],
            ]}
            rowTone={["info", "info", "info", "success", "success", "warning", "warning"]}
          />
          <H3>装完怎么确认</H3>
          <Table
            headers={["命令或动作", "通过标准"]}
            rows={[
              ["git --version", "打出版本号"],
              ["python --version", "3.12.x"],
              ["flutter doctor", "Android toolchain 和 Visual Studio 尽量无红叉"],
              ["Arduino 开发板管理器能搜到 esp32", "能新建 ESP32-S3 工程"],
              ["打开 ESP-IDF PowerShell 跑 idf.py --version", "5.3 或以上"],
              ["插 USB-TTL，设备管理器有 COM 口", "CH340/CP2102 驱动成功"],
              ["CH340 发 AT 回 OK", "艾尔赛模块出厂 AT，不必先刷机"],
            ]}
          />
          <H3>本期不要装</H3>
          <Table
            headers={["别装", "原因"]}
            rows={[
              ["Docker、PostgreSQL、Redis、Nginx", "先本地 SQLite + FastAPI 直接跑"],
              ["LTspice、Multisim、Proteus、Keil", "过重。我们用的是现成模块，不是从运放画手表"],
              ["自己部署的大模型 / Ollama / 语音模型", "手表算力不够，电脑也会拖垮 35 天"],
              ["第二套 Windows 框架（WPF/Electron）", "已经用 Flutter 覆盖 Windows"],
            ]}
            rowTone={["warning", "danger", "danger", "warning"]}
          />
        </Stack>
      ) : null}

      {view === "freeze" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="这张表是项目的刹车">
            口头答应不算数。要改范围：先改这一页，再改甘特和采购。35 天里最常见的翻车是「顺便把唤醒词/开模/圆屏做了」。
          </Callout>
          <Table
            headers={["主题", "冻结成什么", "状态", "明确不做"]}
            rows={DECISIONS}
            striped
          />
          <H3>有手机/电脑时更智能；自主开口默认安静</H3>
          <Table headers={["场景", "能做什么", "边界"]} rows={COMPANION} striped />
          <Table headers={["规则", "怎么执行"]} rows={PROACTIVE_RULES} striped />
        </Stack>
      ) : null}

      {view === "hw" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="货已到，对着接线卡接线">
            手机打开 docs/bondwatch-wiring.html，字大、按步排。开发板侧面印着 GPIO 数字。不要抄微雪 Wiki 的 ESP32S3 例程脚。先 1.69 和两键，麦喇叭后接，4G 最后。
          </Callout>
          <Grid columns={4} gap={16}>
            <Stat value="先 1.69" label="通了再换 2.0" tone="warning" />
            <Stat value="3.3V" label="屏和麦不要接 5V" tone="info" />
            <Stat value="N16R8" label="必须这一档 S3" />
            <Stat value="4G 最后" label="先 CH340 打 AT" />
          </Grid>
          <H2>接线顺序</H2>
          <Table headers={["步", "接什么"]} rows={WIRE_STEPS} striped />
          <H2>模块丝印 → S3 GPIO</H2>
          <Table headers={["功能", "你看到的脚", "接到 S3", "注意"]} rows={PIN_ROWS} striped stickyHeader />
          <H3>先别接</H3>
          <Table headers={["别接", "原因"]} rows={WIRE_SKIP} rowTone={["danger", "warning", "warning", "warning", "warning", "danger"]} />
          <H2>真表 → 仿真 → 真机</H2>
          <Table headers={["功能", "真表怎么接", "Arduino / Wokwi", "货到只换什么"]} rows={HW_MAP} striped stickyHeader />
          <Table headers={["能力", "结论"]} rows={HW_FEASIBLE} striped />
          <Text>
            Wokwi 仍可练：PWR/PTT/TAP。真机 GPIO6/7 改接麦和功放，不要再接灯和蜂鸣器。
          </Text>
        </Stack>
      ) : null}

      {view === "iface" ? (
        <Stack gap={16}>
          <Callout tone="info" title="三人只准认这一份">
            云端字段、表情枚举、GPIO 都写在这里。有人生成了另一套引脚或另一套 JSON，以本页为准改回去。4G 的 UART 先空着，屏和键通了再接。
          </Callout>
          <H2>云端 API（v1）</H2>
          <Table headers={["接口", "谁调用", "约定"]} rows={API_ROWS} striped />
          <H2>表情标签（先于音频到达）</H2>
          <Table headers={["emotion", "状态", "屏幕"]} rows={EMOTION_ROWS} striped />
          <H2>初值引脚（到货后只改这一张）</H2>
          <Text tone="secondary">
            对着模块丝印接。避开 GPIO0/3/45/46 和 USB 的 19/20。改线：先改本表，再改 pins.h，再改实物，拍一张照。不要抄微雪 Wiki 的 ESP32 例程脚。
          </Text>
          <Table headers={["功能", "你看到的脚", "接到 S3", "注意"]} rows={PIN_ROWS} striped stickyHeader />
          <H2>半双工</H2>
          <Text>
            听的时候关喇叭、说的时候关麦。它说时点屏或短按 PTT 立刻停 TTS。应用层不抢麦。
          </Text>
        </Stack>
      ) : null}

      {view === "accept" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="只按「必须」验收；拍演日期是 9/18">
            37 天截止是 9/22。正式演示放在 9/18 周五，按下面 8 步走一遍。应该有、加分项做不到就口述缺口。加分做完、必须项没做，算失败。
          </Callout>
          <Grid columns={3} gap={16}>
            <Stat value={String(ACCEPT_MUST.length)} label="必须项" tone="warning" />
            <Stat value={String(ACCEPT_SHOULD.length)} label="应该有" />
            <Stat value={String(ACCEPT_STRETCH.length)} label="加分" tone="info" />
          </Grid>
          <H2>必须有（不做就没过）</H2>
          <Table headers={["块", "通过标准"]} rows={ACCEPT_MUST} rowTone={["danger", "danger", "danger", "info", "danger", "warning"]} />
          <H2>应该有</H2>
          <Table headers={["块", "通过标准"]} rows={ACCEPT_SHOULD} />
          <H2>加分</H2>
          <Table headers={["块", "通过标准"]} rows={ACCEPT_STRETCH} />
          <H2>演示脚本（按这个顺序拍）</H2>
          <Table headers={["步", "动作"]} rows={DEMO_STEPS} striped />
        </Stack>
      ) : null}

      {view === "risk" ? (
        <Stack gap={16}>
          <Callout tone="warning" title="负责人每周看一次触发条件">
            风险不是写着吓人，是到了触发条件就执行「怎么挡」。不要等 D34 再发现 Windows 编不过。
          </Callout>
          <Table
            headers={["风险", "可能", "伤害", "谁盯", "触发条件", "怎么挡"]}
            rows={RISKS}
            rowTone={["danger", "warning", "danger", "warning", "info", "danger", "warning", "danger", "warning", "warning", "danger", "danger", "warning"]}
            striped
            stickyHeader
          />
        </Stack>
      ) : null}

      {view === "req" ? (
        <Stack gap={16}>
          <Callout tone="info" title="自主开口：白名单场合才说话。闹钟只是到点开口的一种">
            用户找它靠点屏或按键。它找人只走勾选过的场合，可总开关关掉。有手机/电脑时：想清楚和长句放在大屏；表上只播短句或到点缓存。勿扰时闭嘴。本地闹钟仍响。
          </Callout>
          <H2>伴侣加持（已冻结）</H2>
          <Table headers={["场景", "能做什么", "边界"]} rows={COMPANION} striped />
          <H2>它什么时候可以自己说话</H2>
          <Table headers={["场合", "档", "谁决定开口", "在哪说", "本期"]} rows={PROACTIVE} striped />
          <H2>自主开口铁律</H2>
          <Table headers={["规则", "怎么执行"]} rows={PROACTIVE_RULES} striped />
          <H2>接续协议（已冻结）</H2>
          <Table
            headers={["步骤", "谁发起", "行为", "手表变成"]}
            rows={[
              ["打开 Windows", "电脑", "若表正在会话，询问「接管」或「只做管理」", "不变，等选择"],
              ["选择接管", "人", "电脑成为持麦端，会话 ID 不变", "旁听（默认，喇叭关）或熄屏"],
              ["选择只做管理", "人", "可改记忆/设置/看历史，不插入当前对话", "继续持麦说话"],
              ["拿回", "手表对讲键或点屏", "电脑确认或超时交还", "重新持麦；电脑改旁听"],
            ]}
            rowTone={["info", "success", "neutral", "warning"]}
          />
          <H2>物理按键和触摸（已冻结）</H2>
          <Table
            headers={["键", "短按", "长按", "双击", "会话中"]}
            rows={[
              ["电源", "亮屏 / 息屏", "开关机", "扬声器静音 + 勿扰（字幕强制开）", "结束会话，模组退浅睡"],
              ["对讲", "开始或结束一轮点按对话", "按住说话，松开停止（PTT）", "—", "打断 AI 播报；被接管时请求拿回"],
              ["点屏", "等于短按对讲；息屏时亮屏", "—", "—", "打断播报；不打开菜单"],
              ["看", "手机/网页拍照；仿真 SEE 键", "—", "—", "同一 /turn image_base64；真镜头下一版板"],
            ]}
          />
          <H2>需求条目</H2>
          <Row gap={8} wrap>
            {AREAS.map((item) => (
              <span key={item.id}>
                <Pill active={area === item.id} onClick={() => setArea(item.id)}>
                  {item.label}
                </Pill>
              </span>
            ))}
          </Row>
          <Row gap={8} wrap>
            <Pill active={priority === "all"} onClick={() => setPriority("all")}>
              全部优先级
            </Pill>
            <Pill active={priority === "P0"} onClick={() => setPriority("P0")}>
              P0 本期必做
            </Pill>
            <Pill active={priority === "P1"} onClick={() => setPriority("P1")}>
              P1 真机外出前
            </Pill>
            <Pill active={priority === "P2"} onClick={() => setPriority("P2")}>
              P2 后期
            </Pill>
          </Row>
          <Text size="small" tone="tertiary">
            显示 {rows.length} / {REQUIREMENTS.length} 条
          </Text>
          <Table
            headers={["ID", "优先级", "需求", "落在哪", "验收"]}
            rows={rows.map((item) => [item.id, item.priority, item.need, item.where, item.accept])}
            rowTone={rows.map((item) => (item.priority === "P0" ? "info" : item.priority === "P1" ? "warning" : "neutral"))}
            striped
            stickyHeader
          />
        </Stack>
      ) : null}
    </Stack>
  );
}

