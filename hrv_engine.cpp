/*
 * hrv_engine.cpp - HRV 计算引擎实现
 */

#include "hrv_engine.h"
#include <algorithm>   // std::sort
#include <math.h>      // fabsf

// ========== RR 间期缓冲区 (环形) ==========
static int    rrBuffer[RR_BUFFER_SIZE];    // RR间期 (ms)
static unsigned long rrTimestamps[RR_BUFFER_SIZE];  // 对应时间戳
static int    rrHead   = 0;    // 写入位置
static int    rrCount  = 0;    // 已存储数量

// ========== 基线存储 ==========
static HRVMetrics baseline;
static bool  baselineReady   = false;
static unsigned long baselineStartTime = 0;
static int   baselineBeatCount = 0;

// ========== 临时数组 (排序用, 避免栈溢出) ==========
static int tempNN[RR_BUFFER_SIZE];

// ========== 实现 ==========

void hrv_engine_begin() {
    memset(rrBuffer, 0, sizeof(rrBuffer));
    memset(rrTimestamps, 0, sizeof(rrTimestamps));
    rrHead = 0;
    rrCount = 0;

    memset(&baseline, 0, sizeof(baseline));
    baselineReady = false;
    baselineStartTime = 0;
    baselineBeatCount = 0;

    Serial.printf("[HRV] 引擎初始化 - 缓冲区 %d 条\n", RR_BUFFER_SIZE);
}

/**
 * @brief 添加一条RR记录到环形缓冲区
 */
void hrv_engine_addBeat(int rrMs, unsigned long timestamp) {
    rrBuffer[rrHead] = rrMs;
    rrTimestamps[rrHead] = timestamp;
    rrHead = (rrHead + 1) % RR_BUFFER_SIZE;
    if (rrCount < RR_BUFFER_SIZE) {
        rrCount++;
    }

    // 基线采集: 记录前N秒的RR数据
    if (!baselineReady && baselineStartTime > 0) {
        baselineBeatCount++;
    }
}

/**
 * @brief 提取滑动时间窗口内的NN间期
 *
 * @param nnArray  输出数组 (需预分配)
 * @param maxSize  数组最大容量
 * @param nowMs    当前时间
 * @param windowMs  窗口时长 (ms)
 * @return 窗口内NN间期数量
 */
static int extractWindow(int* nnArray, int maxSize, unsigned long nowMs, unsigned long windowMs) {
    int count = 0;
    unsigned long cutoff = nowMs - windowMs;

    // 从最旧的数据开始遍历环形缓冲区
    int oldestIdx = (rrCount < RR_BUFFER_SIZE) ? 0 : rrHead;
    for (int i = 0; i < rrCount; i++) {
        int idx = (oldestIdx + i) % RR_BUFFER_SIZE;
        if (rrTimestamps[idx] >= cutoff) {
            nnArray[count++] = rrBuffer[idx];
            if (count >= maxSize) break;
        }
    }

    return count;
}

/**
 * @brief 计算中位数 (需要排序)
 */
static float calcMedian(int* arr, int count) {
    if (count == 0) return 0;
    // 复制到临时数组并排序
    memcpy(tempNN, arr, count * sizeof(int));
    std::sort(tempNN, tempNN + count);

    if (count % 2 == 0) {
        return (tempNN[count/2 - 1] + tempNN[count/2]) / 2.0f;
    } else {
        return tempNN[count / 2];
    }
}

/**
 * @brief 计算 IQR (四分位距)
 */
static float calcIQR(int* arr, int count) {
    if (count < 4) return 0;
    memcpy(tempNN, arr, count * sizeof(int));
    std::sort(tempNN, tempNN + count);

    int q1Idx = count / 4;
    int q3Idx = (3 * count) / 4;
    return (float)(tempNN[q3Idx] - tempNN[q1Idx]);
}

/**
 * @brief 核心 HRV 计算
 *
 * 需要至少 30 个 NN 间期才能输出有效结果 (保证统计意义)
 *
 * 异常值过滤: 假拍 (如重搏切迹误检) 会将一次真实心跳拆成
 * "短-长"两个连续 RR 间期, 两者都明显偏离中位数。
 * 剔除偏离中位数 >30% 的 RR 及其下一个邻居 (一个假拍污染两个 RR)。
 */
HRVMetrics hrv_engine_compute() {
    HRVMetrics result;
    memset(&result, 0, sizeof(result));

    // 提取时间窗口内的NN间期
    unsigned long nowMs = millis();
    int nnCount = extractWindow(tempNN, RR_BUFFER_SIZE, nowMs, HRV_WINDOW_SEC * 1000);

    result.nnCount = nnCount;

    // 需要至少30个NN间期 (约30秒数据 @60BPM)
    if (nnCount < 30) {
        result.valid = false;
        return result;
    }

    // ========== RR 异常值过滤 ==========
    // 先求中位数 (在副本上排序, 保留 tempNN 原始顺序)
    {
        int sortedCopy[RR_BUFFER_SIZE];
        memcpy(sortedCopy, tempNN, nnCount * sizeof(int));
        std::sort(sortedCopy, sortedCopy + nnCount);

        float rawMedian;
        if (nnCount % 2 == 0) {
            rawMedian = (sortedCopy[nnCount / 2 - 1] + sortedCopy[nnCount / 2]) / 2.0f;
        } else {
            rawMedian = sortedCopy[nnCount / 2];
        }

        // 剔除偏离中位数 >30% 的 RR 及其邻居
        int rawCount = nnCount;
        int writeIdx = 0;
        for (int i = 0; i < rawCount; ) {
            float dev = fabsf((float)tempNN[i] - rawMedian) / rawMedian;
            if (dev > 0.30f) {
                // 一个假拍污染两个连续 RR (假拍前的短 RR + 假拍后的长 RR)
                i += 2;  // 跳过异常对
            } else {
                tempNN[writeIdx++] = tempNN[i];
                i++;
            }
        }
        nnCount = writeIdx;

        int removed = rawCount - nnCount;
        if (removed > 0) {
            Serial.printf("[HRV] 剔除 %d 个异常RR (原始 %d -> 保留 %d, median=%.0f)\n",
                          removed, rawCount, nnCount, rawMedian);
        }
    }

    // 过滤后至少保留 20 个正常 RR 间期
    if (nnCount < 20) {
        result.nnCount = nnCount;
        result.valid = false;
        return result;
    }

    result.nnCount = nnCount;
    result.valid = true;

    // ---- 计算 Mean NN 和 Mean HR ----
    float sumNN = 0;
    for (int i = 0; i < nnCount; i++) {
        sumNN += tempNN[i];
    }
    float meanNN = sumNN / nnCount;
    result.meanHR = 60000.0f / meanNN;

    // ---- 计算 SDNN ----
    float sumSq = 0;
    for (int i = 0; i < nnCount; i++) {
        float diff = tempNN[i] - meanNN;
        sumSq += diff * diff;
    }
    result.sdnn = sqrt(sumSq / (nnCount - 1));  // 样本标准差

    // ---- 计算 RMSSD ----
    float sumSqDiff = 0;
    for (int i = 1; i < nnCount; i++) {
        float diff = tempNN[i] - tempNN[i-1];
        sumSqDiff += diff * diff;
    }
    result.rmssd = sqrt(sumSqDiff / (nnCount - 1));

    // ---- 计算 pNN50 ----
    int countNN50 = 0;
    for (int i = 1; i < nnCount; i++) {
        if (abs(tempNN[i] - tempNN[i-1]) > 50) {
            countNN50++;
        }
    }
    result.pnn50 = (float)countNN50 / (nnCount - 1) * 100.0f;

    // ---- 计算 Median NN ----
    result.medianNN = calcMedian(tempNN, nnCount);

    // ---- 计算 CVNN ----
    result.cvnn = (meanNN > 0) ? (result.sdnn / meanNN) : 0;

    // ---- 计算 IQR NN ----
    result.iqrNN = calcIQR(tempNN, nnCount);

    return result;
}

// ========== 基线管理 ==========
static unsigned long baselineAccumMs = 0;   // 累积手指放置时间 (ms)

void hrv_engine_startBaseline() {
    baselineStartTime = millis();
    baselineBeatCount = 0;
    baselineAccumMs = 0;
    Serial.printf("[HRV] 开始采集静息基线 (%d 秒, 手指放上才计时)...\n", BASELINE_CAPTURE_SEC);
}

void hrv_engine_tickBaseline(unsigned long dtMs) {
    if (baselineReady) return;
    if (baselineAccumMs < BASELINE_CAPTURE_SEC * 1000UL) {
        baselineAccumMs += dtMs;
    }
}

int hrv_engine_getBaselineRemaining() {
    if (baselineReady) return 0;
    long remain = (long)(BASELINE_CAPTURE_SEC * 1000UL) - (long)baselineAccumMs;
    if (remain <= 0) return 0;
    return (int)((remain + 999) / 1000);  // 向上取整到秒
}

bool hrv_engine_isBaselineReady() {
    if (baselineReady) return true;

    // 累积时间足够即就绪 (不再要求心跳数量)
    if (baselineAccumMs >= BASELINE_CAPTURE_SEC * 1000UL) {
        baseline = hrv_engine_compute();
        baselineReady = true;
        Serial.println("[HRV] ✅ 静息基线采集完成!");
        if (baseline.valid) {
            Serial.printf("       RMSSD=%.1f SDNN=%.1f HR=%.1f pNN50=%.1f%%\n",
                          baseline.rmssd, baseline.sdnn,
                          baseline.meanHR, baseline.pnn50);
        } else {
            Serial.printf("       HRV 数据不足 (%d beats), 先标记就绪\n", baseline.nnCount);
        }
    }

    return baselineReady;
}

void hrv_engine_setBaseline(HRVMetrics bl) {
    baseline = bl;
    baselineReady = true;
    Serial.println("[HRV] 静息基线已手动设置");
}

HRVMetrics hrv_engine_getBaseline() {
    return baseline;
}

/**
 * @brief 计算 7 个 delta 特征
 *
 * delta = current_window - baseline
 *
 * 这是队友边缘AI模型的直接输入
 */
HRVDeltaFeatures hrv_engine_getDeltas() {
    HRVDeltaFeatures deltas;
    memset(&deltas, 0, sizeof(deltas));

    // 计算当前窗口HRV
    HRVMetrics current = hrv_engine_compute();

    if (!current.valid || !baselineReady) {
        deltas.valid = false;
        return deltas;
    }

    deltas.deltaRMSSD    = current.rmssd    - baseline.rmssd;
    deltas.deltaSDNN     = current.sdnn     - baseline.sdnn;
    deltas.deltaMeanHR   = current.meanHR   - baseline.meanHR;
    deltas.deltaMedianNN = current.medianNN - baseline.medianNN;
    deltas.deltaPNN50    = current.pnn50    - baseline.pnn50;
    deltas.deltaCVNN     = current.cvnn     - baseline.cvnn;
    deltas.deltaIQRNN    = current.iqrNN    - baseline.iqrNN;
    deltas.valid = true;

    return deltas;
}
