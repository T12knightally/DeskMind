/*
 * stress_classifier.h - 本地压力分类器
 *
 * 调用 WESAD 逻辑回归模型，将 HRV 指标（当前 vs 基线）映射为 4 级压力等级。
 * 不依赖 UART 通信——完全在 ESP32 本地完成分类。
 */
#ifndef STRESS_CLASSIFIER_H
#define STRESS_CLASSIFIER_H

#include <Arduino.h>
#include "hrv_engine.h"

struct StressClassification {
    int   stressLevel;   // 0=Relaxed, 1=Normal, 2=Mild, 3=High
    float probability;   // 压力概率 (0.0 ~ 1.0)
    int   relaxMethod;   // 推荐放松方式
    bool  valid;         // 当前 HRV 和基线是否都有效
};

/**
 * @brief 运行本地压力分类
 * @param current  当前窗口的 HRV 指标
 * @param baseline 静息基线 HRV 指标
 * @return StressClassification (valid=false 表示数据不足)
 */
StressClassification stress_classify(const HRVMetrics& current, const HRVMetrics& baseline);

#endif
