/*
 * hal_display.cpp - GC9A01 显示驱动 (匹配成功的 test_basic 模式)
 */
#include "hal_display.h"
#include "pins.h"
#include <TFT_eSPI.h>

// 和 test_basic 一样用 static 全局对象 (不 new)
static TFT_eSPI g_tft;

#define CX (SCREEN_WIDTH / 2)
#define CY (SCREEN_HEIGHT / 2)

void display_begin() {
    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    g_tft.begin();
    g_tft.setRotation(0);
    g_tft.fillScreen(COLOR_BG);
    g_tft.setTextColor(COLOR_TEXT, COLOR_BG);

    Serial.println("[GC9A01] OK");
}

void display_clear(uint16_t color) {
    g_tft.fillScreen(color);
}

void display_centerText(const char* text, int y, uint8_t size, uint16_t color) {
    g_tft.setTextSize(size);
    g_tft.setTextColor(color, COLOR_BG);
    g_tft.setCursor(CX - (strlen(text) * size * 6) / 2, y);
    g_tft.print(text);
}

void display_leftText(const char* text, int x, int y, uint8_t size, uint16_t color) {
    g_tft.setTextSize(size);
    g_tft.setTextColor(color, COLOR_BG);
    g_tft.setCursor(x, y);
    g_tft.print(text);
}

void display_drawArc(int x0, int y0, int r, int width, float angle, uint16_t color) {
    float startAngle = -90;
    int segments = max(2, (int)(angle / 3.0));

    for (int i = 0; i < segments; i++) {
        float a1 = startAngle + (angle * i / segments);
        float a2 = startAngle + (angle * (i + 1) / segments);
        float rad1 = a1 * PI / 180.0;
        float rad2 = a2 * PI / 180.0;

        for (int w = 0; w < width; w++) {
            int rr = r - w;
            if (rr <= 0) continue;
            int x1 = x0 + rr * cos(rad1);
            int y1 = y0 + rr * sin(rad1);
            int x2 = x0 + rr * cos(rad2);
            int y2 = y0 + rr * sin(rad2);
            g_tft.drawLine(x1, y1, x2, y2, color);
        }
    }
}

void display_bottomText(const char* text) {
    g_tft.setTextSize(1);
    int tw = strlen(text) * 6;
    g_tft.setCursor(CX - tw / 2, SCREEN_HEIGHT - 16);
    g_tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    g_tft.print(text);
}

void display_fillCircle(int x, int y, int r, uint16_t color) {
    g_tft.fillCircle(x, y, r, color);
}

void display_drawCircle(int x, int y, int r, uint16_t color) {
    g_tft.drawCircle(x, y, r, color);
}

void display_showNoFinger() {
    g_tft.fillScreen(COLOR_BG);
    g_tft.setTextColor(COLOR_TEXT, COLOR_BG);
    g_tft.setTextSize(2);
    g_tft.setCursor(CX - 60, CY - 10);
    g_tft.print("Place Finger");
}
