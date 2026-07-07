/*
 * hal_display.h - GC9A01 圆形TFT屏幕驱动封装
 * 基于 TFT_eSPI 库 (Bodmer)
 *
 * ⚠️ 使用前需配置 TFT_eSPI 库的 User_Setup.h:
 *   1. 找到 Arduino/libraries/TFT_eSPI/User_Setup_Select.h
 *   2. 注释掉所有已启用的驱动, 只保留:
 *      #include <User_Setups/Setup24_ST7789.h> 的行全部注释
 *   3. 编辑 User_Setup.h (同目录), 填入以下配置:
 *      #define GC9A01_DRIVER
 *      #define TFT_WIDTH  240
 *      #define TFT_HEIGHT 240
 *      #define TFT_MISO -1
 *      #define TFT_MOSI 23
 *      #define TFT_SCLK 18
 *      #define TFT_CS   5
 *      #define TFT_DC   2
 *      #define TFT_RST  4
 *      #define SPI_FREQUENCY  40000000
 *      #define SPI_READ_FREQUENCY 20000000
 *      #define SPI_TOUCH_FREQUENCY 2500000
 *   4. 保存, 重启 Arduino IDE
 */

#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <Arduino.h>
#include "app_config.h"

// ========== 颜色定义 (RGB565 原始值) ==========
// 不依赖 TFT_eSPI 宏，直接用十六进制数值
#define COLOR_BG            0x0000      // 黑色
#define COLOR_TEXT          0xFFFF      // 白色
#define COLOR_ACCENT        0x07E0      // 绿色
#define COLOR_STRESS_GREEN  0x07E0      // 放松 - 绿
#define COLOR_STRESS_YELLOW 0xFFE0      // 正常 - 黄
#define COLOR_STRESS_ORANGE 0xFD20      // 轻度压力 - 橙
#define COLOR_STRESS_RED    0xF800      // 高压力 - 红
#define COLOR_CIRCLE_BG     0x18E3      // 深蓝灰 (外环背景)
#define COLOR_GRAY          0x8410      // 灰色
#define COLOR_BREATH_BLUE   0x059F      // 呼吸引导环主色 (亮蓝)
#define COLOR_BREATH_DIM    0x0317      // 呼吸引导环暗色 (尾迹拖影)

/**
 * @brief 初始化 GC9A01 屏幕
 */
void display_begin();

/**
 * @brief 清屏
 */
void display_clear(uint16_t color = COLOR_BG);

/**
 * @brief 在圆形屏幕中央显示大字(用于心率等核心数据)
 * @param text   文字
 * @param y      垂直位置(像素, 从顶部算)
 * @param size   字体大小 (1-7, TFT_eSPI 标准)
 */
void display_centerText(const char* text, int y, uint8_t size = 4, uint16_t color = COLOR_TEXT);

/**
 * @brief 左对齐文字 (用于列表/详情页)
 */
void display_leftText(const char* text, int x, int y, uint8_t size = 1, uint16_t color = COLOR_TEXT);

/**
 * @brief 绘制圆形进度环 (用于压力状态显示)
 * @param x0, y0  圆心坐标
 * @param r       外径
 * @param width   环宽度
 * @param angle   填充角度 (0-360度)
 * @param color   颜色
 */
void display_drawArc(int x0, int y0, int r, int width, float angle, uint16_t color);

/**
 * @brief 在屏幕底部居中显示小字 (推荐放松方式等)
 * @param text 文字
 */
void display_bottomText(const char* text);

/**
 * @brief 绘制实心圆
 */
void display_fillCircle(int x, int y, int r, uint16_t color);

/**
 * @brief 绘制空心圆环 (1px 线宽)
 */
void display_drawCircle(int x, int y, int r, uint16_t color);

/**
 * @brief 显示传感器脱落提示
 */
void display_showNoFinger();

#endif
