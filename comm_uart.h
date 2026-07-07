/*
 * comm_uart.h - UART 通信模块
 *
 * 与队友模块通过 Serial1 通信:
 *   发送 (每秒):   BPM:<bpm>\n                  实时心率
 *   发送 (每10秒): HRV:<d1>,<d2>,<d3>,<d4>,<d5>,<d6>,<d7>\n  7个delta特征
 *   接收:         STR:<level>,REL:<method>\n   压力分析结果
 *
 * HRV 7特征顺序: deltaRMSSD, deltaSDNN, deltaMeanHR,
 *                deltaMedianNN, deltaPNN50, deltaCVNN, deltaIQRNN
 */

#ifndef COMM_UART_H
#define COMM_UART_H

#include <Arduino.h>
#include "app_config.h"
#include "hrv_engine.h"

// 压力分析结果
struct StressResult {
    int   stressLevel;
    int   relaxMethod;
    bool  isNew;
    unsigned long timestamp;
};

/**
 * @brief 初始化 UART 通信
 */
void comm_uart_begin();

/**
 * @brief 发送实时 BPM (每秒)
 */
void comm_uart_sendBPM(int bpm);

/**
 * @brief 发送 7 个 HRV delta 特征 (每10秒)
 *        队友边缘AI模型的直接输入
 */
void comm_uart_sendHRVDeltas(const HRVDeltaFeatures& deltas);

/**
 * @brief 发送心跳包 (无有效数据时)
 */
void comm_uart_sendHeartbeat();

/**
 * @brief 检查并解析接收缓冲区
 *        在主循环中高频调用
 * @return 解析后的结果 (isNew=true 表示新数据)
 */
StressResult comm_uart_checkRx();

/**
 * @brief 获取最近一次接收的结果
 */
StressResult comm_uart_getLastResult();

#endif
