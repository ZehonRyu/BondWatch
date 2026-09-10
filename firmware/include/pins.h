#pragma once

// BondWatch 引脚只准这一份。改线：先改本文件，再改 diagram.json / 实物。
// 避开 GPIO0/3/45/46 和 USB 的 19/20。按键内部上拉，按下接地。
// 不要抄微雪 Wiki「ESP32S3 例程」那张脚：那是他们开发板的，会和电源键/麦/4G 打架。
//
// 真机对着模块丝印接线（当前按 2.0 ST7789 240x320；1.69 脚名略不同）：
// 1.69 LCD_DIN / 2.0 MOSI  → GPIO11
// 1.69 LCD_CLK / 2.0 SCLK  → GPIO12
// LCD_CS                   → GPIO10
// LCD_DC                   → GPIO8
// LCD_RST                  → GPIO9
// LCD_BL                   → GPIO13
// TP_SDA                   → GPIO14
// TP_SCL                   → GPIO21
// 1.69 TP_IRQ / 2.0 TP_INT → GPIO38
// TP_RST                   → GPIO39
// INMP441 WS/SCK/SD        → GPIO4 / 5 / 6；VDD=3.3V；L/R=GND
// MAX98357 BCLK/LRC/DIN    → GPIO15 / 16 / 7；VIN=3.3V（喇叭未接时可空着）
// PWR / VOL                → GPIO2 / 1，另一脚 GND（仅两颗实体键；对讲靠触屏）
// Air780E TX/RX            → GPIO17 / 18（模组 5V 来自 Hub；先别接）
// 真机 Goouuu 板：G17/G18 在左侧排针；右侧 TX0/RX0 是 USB 串口 GPIO43/44，不是 4G。
// GPIO40/41/42/47 是 Wokwi 仿真键灯，真机不要接。

#define PIN_PWR 2
#define PIN_VOL 1
// 兼容旧接线名（同一脚）：音量键，不再做实体 PTT
#define PIN_PTT PIN_VOL

#define PIN_LED 47

#define PIN_LCD_SCK 12
#define PIN_LCD_MOSI 11
#define PIN_LCD_DC 8
#define PIN_LCD_RST 9
#define PIN_LCD_CS 10
#define PIN_LCD_BLK 13

#define PIN_TP_SDA 14
#define PIN_TP_SCL 21
#define PIN_TP_INT 38
#define PIN_TP_RST 39

#define PIN_MIC_WS 4
#define PIN_MIC_SCK 5
#define PIN_MIC_SD 6

#define PIN_SPK_BCLK 15
#define PIN_SPK_LRC 16
#define PIN_SPK_DIN 7

#define PIN_4G_RX 17
#define PIN_4G_TX 18

// 仿真专用：真表 PCB 上这两脚会改成摄像头座 / 4G 状态，不要焊到量产表壳。
#define PIN_SEE 40
#define PIN_LTE 41
#define PIN_ROT 42
