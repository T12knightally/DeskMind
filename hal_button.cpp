/*
 * hal_button.cpp - 按键驱动实现
 */

#include "hal_button.h"

// ========== 按键状态 ==========
static bool          btnPressed     = false;
static unsigned long btnPressStart  = 0;
static ButtonEvent   pendingEvent   = BTN_NONE;
static unsigned long lastReleaseTime = 0;

void button_begin() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);   // BOOT键: 外部无上拉, 使用内部上拉
    Serial.println("[BUTTON] 按键初始化 (GPIO0, 上拉模式)");
}

ButtonEvent button_check() {
    ButtonEvent result = pendingEvent;
    pendingEvent = BTN_NONE;  // 事件消费后清除

    bool state = (digitalRead(PIN_BUTTON) == LOW);  // LOW = 按下

    if (state && !btnPressed) {
        // 按下沿
        btnPressed = true;
        btnPressStart = millis();
    }
    else if (!state && btnPressed) {
        // 释放沿
        btnPressed = false;
        unsigned long duration = millis() - btnPressStart;

        if (duration >= DEBOUNCE_MS) {
            if (duration >= LONG_PRESS_MS) {
                pendingEvent = BTN_LONG_PRESS;
            } else {
                pendingEvent = BTN_SHORT_PRESS;
            }
        }
        lastReleaseTime = millis();
    }

    return result;
}
