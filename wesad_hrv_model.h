#pragma once
#include <math.h>

// Tiny WESAD wrist-PPG stress classifier.
// Model features: delta_rmssd_ms, delta_sdnn_ms, delta_mean_hr_bpm, delta_median_nn_ms, delta_pnn50_pct, delta_cvnn, delta_iqr_nn_ms
// Keep the user's resting baseline values in ESP32 NVS.
// Returns probability of Stress. Use >= 0.5 as Stress by default.

static const int WESAD_HRV_FEATURE_COUNT = 7;
static const float WESAD_HRV_MEAN[7] = {-0.952691083f, -9.791015f, 11.5389699f, -105.871804f, 2.2704327f, 0.0160140463f, -17.6260653f};
static const float WESAD_HRV_SCALE[7] = {66.966987f, 53.553422f, 16.9493878f, 147.503761f, 19.5696741f, 0.075718841f, 110.293998f};
static const float WESAD_HRV_COEF[7] = {-0.880641124f, 2.90804956f, 3.001286f, -1.34024388f, 2.23343701f, -1.68692327f, -1.97440273f};
static const float WESAD_HRV_INTERCEPT = 0.43893551f;

static inline float wesad_stress_predict_from_features(const float *x) {
    float logit = WESAD_HRV_INTERCEPT;
    for (int i = 0; i < WESAD_HRV_FEATURE_COUNT; ++i) {
        const float z = (x[i] - WESAD_HRV_MEAN[i]) / WESAD_HRV_SCALE[i];
        logit += WESAD_HRV_COEF[i] * z;
    }
    return 1.0f / (1.0f + expf(-logit));
}

static inline float wesad_stress_predict_proba(
    float rmssd_ms,
    float sdnn_ms,
    float mean_hr_bpm,
    float median_nn_ms,
    float pnn50_pct,
    float cvnn,
    float iqr_nn_ms,
    float rmssd_base_ms,
    float sdnn_base_ms,
    float mean_hr_base_bpm,
    float median_nn_base_ms,
    float pnn50_base_pct,
    float cvnn_base,
    float iqr_nn_base_ms
) {
    const float f0 = rmssd_ms - rmssd_base_ms;  // delta_rmssd_ms
    const float f1 = sdnn_ms - sdnn_base_ms;  // delta_sdnn_ms
    const float f2 = mean_hr_bpm - mean_hr_base_bpm;  // delta_mean_hr_bpm
    const float f3 = median_nn_ms - median_nn_base_ms;  // delta_median_nn_ms
    const float f4 = pnn50_pct - pnn50_base_pct;  // delta_pnn50_pct
    const float f5 = cvnn - cvnn_base;  // delta_cvnn
    const float f6 = iqr_nn_ms - iqr_nn_base_ms;  // delta_iqr_nn_ms
    const float x[WESAD_HRV_FEATURE_COUNT] = {f0, f1, f2, f3, f4, f5, f6};
    return wesad_stress_predict_from_features(x);
}

static inline int wesad_stress_predict(
    float rmssd_ms,
    float sdnn_ms,
    float mean_hr_bpm,
    float median_nn_ms,
    float pnn50_pct,
    float cvnn,
    float iqr_nn_ms,
    float rmssd_base_ms,
    float sdnn_base_ms,
    float mean_hr_base_bpm,
    float median_nn_base_ms,
    float pnn50_base_pct,
    float cvnn_base,
    float iqr_nn_base_ms
) {
    return wesad_stress_predict_proba(
        rmssd_ms,
        sdnn_ms,
        mean_hr_bpm,
        median_nn_ms,
        pnn50_pct,
        cvnn,
        iqr_nn_ms,
        rmssd_base_ms,
        sdnn_base_ms,
        mean_hr_base_bpm,
        median_nn_base_ms,
        pnn50_base_pct,
        cvnn_base,
        iqr_nn_base_ms
    ) >= 0.5f;
}

// ========== 4 级压力分类 ==========
// Returns 0-3 mapped to STRESS_RELAXED / STRESS_NORMAL / STRESS_MILD / STRESS_HIGH
static inline int wesad_stress_level(
    float rmssd_ms,
    float sdnn_ms,
    float mean_hr_bpm,
    float median_nn_ms,
    float pnn50_pct,
    float cvnn,
    float iqr_nn_ms,
    float rmssd_base_ms,
    float sdnn_base_ms,
    float mean_hr_base_bpm,
    float median_nn_base_ms,
    float pnn50_base_pct,
    float cvnn_base,
    float iqr_nn_base_ms
) {
    float prob = wesad_stress_predict_proba(
        rmssd_ms, sdnn_ms, mean_hr_bpm, median_nn_ms,
        pnn50_pct, cvnn, iqr_nn_ms,
        rmssd_base_ms, sdnn_base_ms, mean_hr_base_bpm, median_nn_base_ms,
        pnn50_base_pct, cvnn_base, iqr_nn_base_ms
    );

    // Discretize probability into 4 stress levels
    // 阈值对手指PPG调低 (手指信号比手腕干净, sigmoid输出更集中)
    if (prob < 0.25f)      return 0;  // STRESS_RELAXED
    else if (prob < 0.40f) return 1;  // STRESS_NORMAL
    else if (prob < 0.65f) return 2;  // STRESS_MILD
    else                   return 3;  // STRESS_HIGH
}
