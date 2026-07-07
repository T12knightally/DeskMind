/*
 * dsp_ppg.cpp - PPG 信号处理 & 心率计算 实现
 */

#include "dsp_ppg.h"

// ========== 心率计算状态 ==========
static byte    rates[BPM_SAMPLE_WINDOW];  // 最近4拍的BPM值
static byte    rateIndex = 0;             // 当前写入位置
static int     rateCount = 0;             // 已记录的拍数 (<= WINDOW)
static float   instantBPM = 0;            // 瞬时心率
static int     avgBPM = 0;               // 平均心率
static int     lastRR = 0;               // 最近RR间期 (ms)
static unsigned long lastBeatTime = 0;    // 上次心跳时间
static unsigned long totalBeats = 0;      // 累计心跳数
static bool    hrValid = false;           // 心率是否有效

// ========== SpO2 计算状态 ==========
static float   irDCSum = 0;              // IR 直流累加
static float   redDCSum = 0;             // Red 直流累加
static int     dcSampleCount = 0;         // DC 累加样本数

void dsp_ppg_begin() {
    dsp_ppg_reset();
    Serial.println("[DSP] PPG 信号处理器初始化");
}

void dsp_ppg_reset() {
    memset(rates, 0, sizeof(rates));
    rateIndex = 0;
    rateCount = 0;
    instantBPM = 0;
    avgBPM = 0;
    lastRR = 0;
    lastBeatTime = 0;
    hrValid = false;
    dcSampleCount = 0;
    irDCSum = 0;
    redDCSum = 0;
}

/**
 * @brief 处理一次心跳
 *
 * 算法:
 *   1. RR间期 = 当前时间 - 上次心跳时间
 *   2. 瞬时BPM = 60000 / RR间期
 *   3. 有效性检查: BPM在 [BPM_MIN, BPM_MAX] 范围内
 *   4. 存入环形缓冲区, 计算4拍平均
 */
int dsp_ppg_onBeat(unsigned long nowMs) {
    if (lastBeatTime == 0) {
        // 第一次心跳, 记录时间但不计算BPM
        lastBeatTime = nowMs;
        totalBeats++;
        return 0;  // 首次无RR
    }

    // 计算RR间期
    int rr = (int)(nowMs - lastBeatTime);
    lastBeatTime = nowMs;

    // RR间期有效性检查 (防止噪声导致的异常值)
    // 正常范围: 273ms(220BPM) ~ 2000ms(30BPM)
    if (rr < (60000 / BPM_MAX) || rr > (60000 / BPM_MIN)) {
        return -1;  // 异常RR, 丢弃本次心跳
    }

    // 计算瞬时BPM
    float bpm = 60000.0f / (float)rr;
    instantBPM = bpm;

    // 存入环形缓冲区
    rates[rateIndex] = (byte)constrain((int)bpm, 0, 255);
    rateIndex = (rateIndex + 1) % BPM_SAMPLE_WINDOW;
    if (rateCount < BPM_SAMPLE_WINDOW) {
        rateCount++;
    }

    // 计算滑动平均BPM
    int sum = 0;
    for (int i = 0; i < rateCount; i++) {
        sum += rates[i];
    }
    avgBPM = sum / rateCount;

    lastRR = rr;
    totalBeats++;
    hrValid = true;

    return rr;  // 返回有效RR间期
}

int dsp_ppg_getLastRR() {
    return lastRR;
}

HeartRateData dsp_ppg_getHR(unsigned long nowMs) {
    HeartRateData data;
    data.instantBPM = instantBPM;
    data.avgBPM     = avgBPM;
    data.rrInterval = lastRR;
    data.valid      = hrValid;
    data.beatCount  = totalBeats;

    // 超时检测: 只标记无效，不清零（由主程序根据手指状态决定是否清零）
    if (lastBeatTime > 0 && (nowMs - lastBeatTime) > BEAT_TIMEOUT_MS) {
        data.valid = false;
    }

    return data;
}

/**
 * @brief 基于 Beer-Lambert 定律的 SpO2 近似计算
 *
 * R = (AC_red / DC_red) / (AC_ir / DC_ir)
 * SpO2 ≈ 104 - 17 * R   (经验公式, MAX30100 数据手册推荐)
 *
 * 注意: 这是简化估算, 临床精度需使用校准后的查找表
 */
SpO2Data dsp_ppg_calcSpO2(float irAC, float irDC, float redAC, float redDC) {
    SpO2Data result;
    result.valid = false;
    result.value = 0;

    // 防止除零
    if (irDC < 100 || redDC < 100 || irAC == 0) {
        return result;
    }

    float ratioAC_DC_ir  = irAC  / irDC;
    float ratioAC_DC_red = redAC / redDC;

    if (ratioAC_DC_ir < 0.001f) {
        return result;
    }

    float R = ratioAC_DC_red / ratioAC_DC_ir;

    // 经验公式 (MAX30100 参考)
    float spo2 = 104.0f - 17.0f * R;

    // 限制范围
    if (spo2 > 100) spo2 = 100;
    if (spo2 < 70)  spo2 = 70;

    result.value = (int)(spo2 + 0.5f);
    result.valid = true;

    return result;
}
