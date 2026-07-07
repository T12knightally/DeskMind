/*
 * hrv_engine.h - HRV (心率变异性) 计算引擎
 *
 * 从 RR 间期序列计算 7 项 HRV 指标, 并管理静息基线:
 *
 *   特征          | 缩写     | 含义
 *   ─────────────┼─────────┼────────────────────
 *   RMSSD        | rmssd   | 相邻NN间期差值的均方根
 *   SDNN         | sdnn    | NN间期标准差
 *   Mean HR      | mean_hr | 平均心率 (BPM)
 *   Median NN    | med_nn  | NN间期中位数
 *   pNN50        | pnn50   | 相邻间期差>50ms的百分比
 *   CVNN         | cvnn    | 变异系数 (SDNN/MeanNN)
 *   IQR NN       | iqr_nn  | NN间期四分位距 (Q3-Q1)
 *
 * 队友边缘AI模型输入 = 当前窗口值 - 静息基线值 (7个delta)
 */

#ifndef HRV_ENGINE_H
#define HRV_ENGINE_H

#include <Arduino.h>
#include "app_config.h"

// ========== HRV 指标结构 ==========
struct HRVMetrics {
    float rmssd;       // 相邻NN间期差值的均方根 (ms)
    float sdnn;        // NN间期标准差 (ms)
    float meanHR;      // 平均心率 (BPM)
    float medianNN;    // NN间期中位数 (ms)
    float pnn50;       // 相邻间期差>50ms的百分比 (%)
    float cvnn;        // 变异系数 (SDNN / MeanNN)
    float iqrNN;       // NN间期四分位距 (ms)
    bool  valid;       // 数据是否足够 (至少需要 ~30个RR间期)
    int   nnCount;     // 窗口内有效NN间期数量
};

// ========== HRV Delta 特征 (队友模型输入) ==========
struct HRVDeltaFeatures {
    float deltaRMSSD;
    float deltaSDNN;
    float deltaMeanHR;
    float deltaMedianNN;
    float deltaPNN50;
    float deltaCVNN;
    float deltaIQRNN;
    bool  valid;       // 当前HRV和基线是否都有效
};

/**
 * @brief 初始化 HRV 引擎
 */
void hrv_engine_begin();

/**
 * @brief 记录一次心跳 (RR间期)
 * @param rrMs 本次RR间期 (ms)
 * @param timestamp 心跳时间戳 (millis)
 */
void hrv_engine_addBeat(int rrMs, unsigned long timestamp);

/**
 * @brief 计算当前滑动窗口的 HRV 指标
 * @return HRVMetrics
 */
HRVMetrics hrv_engine_compute();

/**
 * @brief 开始采集静息基线 (清空之前基线, 重新记录)
 */
void hrv_engine_startBaseline();

/**
 * @brief 为基线倒计时累加时间 (仅在手指放上时调用)
 * @param dtMs 本次累加时长 (ms)，通常为 SAMPLE_INTERVAL_MS
 */
void hrv_engine_tickBaseline(unsigned long dtMs);

/**
 * @brief 获取基线倒计时剩余秒数
 * @return 剩余秒数 (0 = 已完成)
 */
int hrv_engine_getBaselineRemaining();

/**
 * @brief 检查基线是否已采集完成
 * @return true 基线已就绪
 */
bool hrv_engine_isBaselineReady();

/**
 * @brief 计算 7 个 delta 特征 (当前 - 基线)
 * @return HRVDeltaFeatures (valid=false 表示基线或当前值无效)
 */
HRVDeltaFeatures hrv_engine_getDeltas();

/**
 * @brief 获取静息基线值
 */
HRVMetrics hrv_engine_getBaseline();

/**
 * @brief 手动设置基线 (用于测试或从外部加载)
 */
void hrv_engine_setBaseline(HRVMetrics baseline);

#endif
