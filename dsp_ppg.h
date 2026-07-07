/*
 * dsp_ppg.h - PPG 信号处理 & 心率(BPM)计算
 *
 * 接收来自 hal_max30100 的原始/滤波数据,
 * 计算瞬时心率、滑动平均心率、SpO2 初估
 */

#ifndef DSP_PPG_H
#define DSP_PPG_H

#include <Arduino.h>
#include "app_config.h"

// ========== 心率数据结构 ==========
struct HeartRateData {
    float   instantBPM;      // 瞬时心率 (基于最近一次RR间期)
    int     avgBPM;          // 滑动平均心率 (4拍平滑)
    int     rrInterval;      // 最近一次RR间期 (ms)
    bool    valid;           // 当前BPM数据是否有效
    int     beatCount;       // 累计检测到的心跳数
};

// ========== SpO2 数据结构 ==========
struct SpO2Data {
    int     value;           // SpO2 百分比 (0-100)
    bool    valid;           // 数据是否有效
};

/**
 * @brief 初始化 PPG 信号处理器
 */
void dsp_ppg_begin();

/**
 * @brief 处理一次心跳事件, 更新心率
 * @param nowMs 当前时间 (millis())
 * @return 本次RR间期 (ms), 首次心跳返回0
 */
int dsp_ppg_onBeat(unsigned long nowMs);

/**
 * @brief 获取当前心率数据 (无新心跳时也返回最近值)
 * @param nowMs 当前时间, 用于超时检测
 * @return HeartRateData
 */
HeartRateData dsp_ppg_getHR(unsigned long nowMs);

/**
 * @brief 获取最近一次RR间期 (ms), 用于HRV计算
 */
int dsp_ppg_getLastRR();

/**
 * @brief 基于 IR/Red 比值估算 SpO2
 * @param irAC   IR通道交流分量 (滤波后)
 * @param irDC   IR通道直流分量 (原始值均值)
 * @param redAC  Red通道交流分量 (滤波后)
 * @param redDC  Red通道直流分量 (原始值均值)
 * @return SpO2Data
 */
SpO2Data dsp_ppg_calcSpO2(float irAC, float irDC, float redAC, float redDC);

/**
 * @brief 重置心率计算器 (传感器重新连接时调用)
 */
void dsp_ppg_reset();

#endif
