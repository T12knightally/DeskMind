# Smart Health — 智能健康监测与呼吸引导设备

基于 ESP32-S3 和 MAX30102 的指端 PPG 健康监测设备，实现心率测量、HRV 分析和四级压力分类，并依据压力等级自动触发呼吸引导放松训练。

## 硬件

- **主控**：ESP32-S3（双核 240MHz，8MB PSRAM）
- **传感器**：MAX30102（100Hz 采样率，I2C 400kHz）
- **屏幕**：GC9A01 圆形 TFT（240×240，SPI 40MHz）
- **按键**：BOOT 键（GPIO0）
- **LED**：板载 LED（GPIO48）

## 引脚连接

| 功能 | 引脚 |
|------|------|
| I2C SDA | GPIO21 |
| I2C SCL | GPIO3 |
| SPI SCLK | GPIO18 |
| SPI MOSI | GPIO7 |
| SPI DC | GPIO2 |
| SPI CS | GPIO5 |
| SPI RST | GPIO4 |
| TFT BL | GPIO15 |
| BUTTON | GPIO0 |
| LED | GPIO48 |

## 软件架构

```
pins.h                  GPIO 引脚定义
app_config.h            全局参数配置

hal_max30100.cpp        传感器驱动（I2C + DC 滤波 + 峰值检测）
hal_display.cpp         屏幕驱动（TFT_eSPI 封装）
hal_button.cpp          按键驱动（50ms 去抖）

dsp_ppg.cpp             PPG 信号处理与心率计算
hrv_engine.cpp          HRV 引擎（60s 窗口 + 异常过滤 + 7 项指标）
wesad_hrv_model.h       WESAD 压力分类模型（逻辑回归 + sigmoid）
stress_classifier.cpp   压力分类器封装

comm_uart.cpp           UART 通信（预留扩展接口）
ui_screens.cpp          多屏 UI 与呼吸引导动画

smart_health_1.ino      主程序（时间分片调度 + 状态机）
```

## 功能

1. **心率测量**：30–220 BPM，4 拍滑动平均，±2 BPM 精度
2. **HRV 分析**：RMSSD、SDNN、MeanHR、MedianNN、pNN50、CVNN、IQRNN 共 7 项时域指标，60 秒滑动窗口
3. **压力分类**：基于 WESAD 逻辑回归模型，四级离散化（放松/正常/轻度压力/高压力），每秒更新
4. **呼吸引导**：Lv3 高压力自动触发，5 循环约 30 秒，8 组同心环动画
5. **休息提醒**：Lv2 轻度压力弹出冥想休息建议，60 秒冷却

## 依赖库

- [SparkFun MAX3010x Sensor Library](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)（需配置 User_Setup.h，详见 `TFT_ESPI_SETUP.txt`）

## 编译

1. 安装 Arduino IDE 和 ESP32 开发板支持
2. 安装上述两个依赖库
3. 按照 `TFT_ESPI_SETUP.txt` 配置 TFT_eSPI 的 `User_Setup.h`
4. 打开 `smart_health_1.ino`，选择 ESP32S3 Dev Module，编译烧录

## 使用

| 操作 | 效果 |
|------|------|
| 开机 | 自动初始化传感器，开始 60 秒基线采集 |
| 手指放置 | 屏幕显示倒计时，完成后进入监测模式 |
| 短按按键 | 循环切换：监测屏 → 调试屏 → HRV 详情页 |
| 长按按键 | 重新校准基线 |

## 参考资料

- Schmidt P, et al. Introducing WESAD, a Multimodal Dataset for Wearable Stress and Affect Detection. ICMI '18, 2018.
- Task Force of ESC/NASPE. Heart Rate Variability: Standards of Measurement. European Heart Journal, 1996.
