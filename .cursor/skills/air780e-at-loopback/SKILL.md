---
name: air780e-at-loopback
description: >-
  Runs Hezhou (合宙) Air780E AT loopback tests over CH340 serial, including port
  discovery, echo (ATE1) verification, and wiring diagnosis. Use when the user
  asks for AT 回环/回显测试, Air780E/合宙串口调试, CH340 COM 不通, or PK/供电接线排查.
---

# 合宙 Air780E AT 回环测试

## When to use

- 用户说：AT 回环、AT 回显、合宙测通、Air780E 串口、CH340 没反应
- BondWatch 里接 4G 模组前的通断验证

## 已验证接线（用户实机 PASS，以此为准）

艾尔赛 Air780E AT 板 ↔ CH340：

| CH340 | 合宙板 | 说明 |
|---|---|---|
| 5V | 5V | 供电，PWR 灯应亮 |
| GND | G | 共地 |
| **TXD** | **TX** | **同名直连（不要交叉）** |
| **RXD** | **RX** | **同名直连（不要交叉）** |
| GND（或板内短接） | **PK** | PK 接 GND；板上可 PK↔G |

实机结论（2026-09）：`TX↔TXD`、`RX↔RXD`、`PK↔GND` 时 COM3 @ 115200 回环 PASS（`ATI` → `AirM2M_780E_V1180_LTE_AT`）。  
**不要默认交叉**；若直连不通，再试交叉作排查，但默认按上表。

右侧 `DP/DM/VB` 测 AT 时不接。CH340 跳帽优先 **3.3V**。

真机进 ESP32 时：Air780E TX/RX → GPIO17/18（仍按模组 TX→ESP RX、模组 RX→ESP TX 的 UART 逻辑）；模组 5V 来自 Hub。AT 回环阶段用 **CH340 直连模组**，不要经 ESP32。

## 回环含义（判定标准）

1. **基本通**：发 `AT` → 收到 `OK`
2. **回显回环**：发 `ATE1` → `OK`；再发 `AT` → 回包里先有回显 `AT`，再有 `OK`
3. **身份**：`ATI` 有版本字串（如 `AirM2M_780E_V1180_LTE_AT`）
4. **SIM/信号**（可选）：`AT+CPIN?`、`AT+CSQ`（无卡可 `CME ERROR: 10`）

默认波特率：**115200 8N1**。脚本会扫 `115200 / 9600 / 57600 / 921600`。

## 执行步骤（必须按序）

1. **列串口**，找 `USB-SERIAL CH340`（VID `1A86`）。确认不是 ESP32 口（ESP 会吐 BondWatch 日志）。
2. **关掉占用口的程序**（SSCOM、串口助手、PlatformIO monitor）。
3. 跑项目脚本：

```powershell
cd D:\Work\BondWatch
python tools/sscom/test_air780e_loopback.py COMx
```

4. 口能开但空回包 → 按下表排查，改完再跑。
5. 报告：`PASS`（含 ECHO-OK）/ `PARTIAL` / `FAIL` + 下一步。

## 空回包排查顺序

1. **PK → GND**
2. **5V / PWR 灯**
3. **TX/RX**：先按已验证 **直连**（TXD-TX、RXD-RX）；不通再试交叉
4. **共地** GND↔G；杜邦线插紧
5. 确认测的是合宙 CH340，不是 ESP32
6. DTR/RTS 组合再探（次要）

## 脚本位置

- `tools/sscom/test_air780e_loopback.py`
- 依赖：`pyserial`

## 报告模板

```
合宙 AT 回环 @ COMx
- 接线：5V；GND-G；TXD-TX；RXD-RX；PK-GND
- 结果：PASS ECHO-OK | PARTIAL | FAIL
- 证据：AT / ATE1 / ATI 回包
- 下一步：一条硬件动作
```
