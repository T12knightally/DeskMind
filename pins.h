/*
 * pins.h - GPIO 引脚定义
 * 适配 ESP32-S3 开发板 (J1 + J3 排针)
 */

#ifndef PINS_H
#define PINS_H

// ========== MAX30100 心率血氧传感器 (I2C) ==========
// 引脚选取原则: 避开 USB-OTG(19/20) 和 UART0(43/44)
#define MAX30100_SDA       21      // J3 引脚18
#define MAX30100_SCL       3       // J1 引脚13 (避开GPIO47 Octal SPI)
#define MAX30100_INT       6       // J1 引脚6  (可选, 中断)

// ========== GC9A01 圆形TFT屏幕 (SPI) ==========
#define TFT_SCLK           18      // J1 引脚11
#define TFT_MOSI           7       // J1 引脚7  (避开Flash SPI引脚)
#define TFT_MISO           -1      // 不使用
#define TFT_DC             2       // J3 引脚5
#define TFT_CS             5       // J1 引脚5
#define TFT_RST            4       // J1 引脚4
#define TFT_BL             15      // J1 引脚8 (背光PWM)

// ========== 板载外设 ==========
#define PIN_BUTTON         0       // J3 引脚14 (BOOT 按键, 低电平触发)
#define PIN_LED            48      // J3 引脚16 (板载 LED)

// ========== UART 通信 (与队友模块, Serial1) ==========
#define COMM_TX            17      // J1 引脚10 → 队友模块 RX
#define COMM_RX            16      // J1 引脚9  ← 队友模块 TX

#endif
