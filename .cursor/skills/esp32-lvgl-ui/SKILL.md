---
name: esp32-lvgl-ui
description: >-
  BondWatch ESP32-S3 LVGL UI and 2D Lumi sprite animation pitfalls and workflows.
  Use when flashing watch firmware, replacing GIF/sprite assets, fixing garbled/noisy
  face on ST7789, bw_ui.cpp/LVGL issues, lumi_frames.rgb565, pack_gif_lumi, or ESP32
  UI display problems.
---

# BondWatch ESP32 LVGL / 2D 精灵 UI

## When to use

- 表盘中间花屏、彩噪、静态雪花
- 换 GIF/素材后表上没变化或动画错乱
- 改 `lumi_frames.rgb565` / `bw_ui.cpp` / LVGL 表情
- 烧录后 UI 异常排查

## 硬件与栈（冻结）

| 项 | 值 |
|---|---|
| 屏 | ST7789 240×320，SPI |
| UI | LVGL 8.3（`BW_USE_LVGL=1`），主路径 `firmware/src/bw_ui.cpp` |
| 表情 | **2D RGB565 精灵**，160×176，**不跑 Three.js/WebGL** |
| 真表素材 | `firmware/src/lumi_frames.rgb565` → `.incbin` 进 Flash |
| 3D | 仅 Web `:11112` 预览 / 可选烘焙，不进固件 |

## 踩坑清单（必读）

### 1. 换素材后固件没嵌入新二进制（最常见）

**现象**：花屏、彩噪、或播几帧后变雪花。

**原因**：`lumi_frames.S` 用 `.incbin` 嵌入 rgb565，**PlatformIO 不会在仅改 `.rgb565` 时自动重编 `.S`**，Flash 里仍是旧帧数/旧数据；若 `lumi_frames.h` 里 `LUMI_FRAME_N` 已变大，超出部分读到垃圾 Flash。

**必做**：
1. 用 `scripts/pack_gif_lumi.py` 打包（它会**重写** `lumi_frames.S` 并带帧数注释，触发重编）
2. 或手动改 `lumi_frames.S` 第一行注释后 `pio run -t clean`
3. 烧录前验证 map：`firmware.map` 里 `.rodata.lumi_frames` 大小 = `LUMI_FRAME_N × 56320`

```
56320 = 160 × 176 × 2  （每帧字节）
21 帧 → 0x120c00 = 1182720 bytes
12 帧 → 0xa5000  = 675840 bytes   ← 若头文件写 21 但 map 仍是这个就错了
```

### 2. 帧数头文件与二进制不一致

**检查**：
```powershell
python -c "from pathlib import Path; b=Path('firmware/src/lumi_frames.rgb565').stat().st_size; print('frames', b//56320)"
# 必须等于 firmware/include/lumi_frames.h 的 LUMI_FRAME_N
```

### 3. LVGL 换帧必须刷新 img 源

**现象**：只 invalidate 不更新，动画卡住或残影。

**做法**（`bw_ui.cpp` `lumiSetFrame`）：
```cpp
lumiDsc.data = lumi_frames + (uint32_t)lumiIdx * LUMI_FRAME_BYTES;
lv_img_cache_invalidate_src(&lumiDsc);
lv_img_set_src(imgLumi, &lumiDsc);
```

`lumiDsc.header.cf = LV_IMG_CF_TRUE_COLOR`，`w/h` 必须与 `LUMI_FRAME_W/H` 一致。

### 4. incbin 路径不能乱改

`.S` 里必须是（相对工程根）：
```asm
.incbin "firmware/src/lumi_frames.rgb565"
```
改成 `"lumi_frames.rgb565"` 会 **Assembler Error: file not found**。

### 5. 花屏仍可能是 RGB565 字节序

UI 按钮颜色正常、仅图片花屏时，先确认坑 1 已排除。仍花屏再试打包时交换高低字节，或试 `lv_conf.h` 的 `LV_COLOR_16_SWAP`（改此项会影响全 UI，需整屏目测）。

当前 `LV_COLOR_16_SWAP 0`，打包脚本用小端 `<H` 写 rgb565。

### 6. 烧录端口

- **COM1** = 主板 UART，**不要烧**
- ESP32 通常是 **CH340 `1A86:7523`**
- 合宙 Air780E 也是 CH340，**别烧错板**

```powershell
python -m platformio device list
python -m platformio run -t upload --upload-port COMx
```

失败：按住 **BOOT** → 点 **RESET** → 再 upload。

### 7. 文档 canvas 双份

`docs/export_offline_html.py` 优先读 `canvases/bondwatch-*.canvas.tsx`，不是 `docs/` 下同名文件。改看板要改 **canvases** 或两处同步。

## 换 GIF / 精灵标准流程

```powershell
cd D:\Work\BondWatch

# 1. 打包（GIF 或 PNG 序列源）
python scripts/pack_gif_lumi.py "C:\path\to\anim.gif"

# 2. 核对帧数
python -c "from pathlib import Path; b=Path('firmware/src/lumi_frames.rgb565').stat().st_size; print(b, 'bytes', b//56320, 'frames')"

# 3. 编译（素材大改建议 clean）
python -m platformio run -t clean
python -m platformio run

# 4. 验证 map 嵌入大小（可选）
# 在 .pio/build/esp32-s3-devkitc-1/firmware.map 搜 rodata.lumi_frames

# 5. 烧录
python -m platformio run -t upload --upload-port COMx
```

**素材规格**：160×176，RGB565，背景 `#102a44`，角色底对齐。Web 预览：`cloud/static/watch/models/lumi_wave.gif`。

**单帧 GIF**：`LUMI_FRAME_N=1` 合法，表上为静态图。

## 相关文件

| 文件 | 作用 |
|---|---|
| `scripts/pack_gif_lumi.py` | GIF→rgb565 + 头文件 + 重写 `.S` |
| `firmware/src/lumi_frames.S` | Flash 嵌入（auto-generated 注释） |
| `firmware/include/lumi_frames.h` | `LUMI_FRAME_N/W/H/MS/BYTES` |
| `firmware/src/bw_ui.cpp` | LVGL 主页、精灵播放 |
| `firmware/include/lv_conf.h` | `LV_COLOR_DEPTH 16` |
| `docs/bondwatch-2d-animation.html` | 2D 方案全文 |

## 报告模板（给用户）

```
ESP32 UI / 精灵
- 素材：xxx.gif，N 帧，rgb565 xxx bytes
- map 嵌入：0x?????? bytes（应等于 rgb565 文件大小）
- 操作：pack → clean build → upload COMx
- 结果：正常动画 / 仍花屏 → 下一步
```
