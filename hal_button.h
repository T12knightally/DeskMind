/*
 * hal_button.h - 板载按键管理
 * BOOT 按键 (GPIO0), 支持短按/长按识别
 */

#ifndef HAL_BUTTON_H
#define HAL_BUTTON_H

#include <Arduino.h>
#include "pins.h"
#include "app_config.h"

// 按键事件类型
enum ButtonEvent {
    BTN_NONE = 0,        // 无事件
    BTN_SHORT_PRESS,     // 短按 (< 1秒)
    BTN_LONG_PRESS,      // 长按 (>= 1秒)
    BTN_DOUBLE_CLICK     // 双击 (预留)
};

/**
 * @brief 初始化按键 (配置内部上拉)
 */
void button_begin();

/**
 * @brief 在主循环中调用, 检测按键事件
 * @return 按键事件类型 (调用后自动清除)
 */
ButtonEvent button_check();

#endif
