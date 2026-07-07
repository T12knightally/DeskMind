/*
 * stress_classifier.cpp - 本地压力分类器实现
 */
#include "stress_classifier.h"
#include "wesad_hrv_model.h"
#include "app_config.h"

StressClassification stress_classify(const HRVMetrics& current, const HRVMetrics& baseline) {
    StressClassification result;
    memset(&result, 0, sizeof(result));

    if (!current.valid || !baseline.valid) {
        result.valid = false;
        return result;
    }

    // 调用 WESAD 模型
    result.probability = wesad_stress_predict_proba(
        current.rmssd,  current.sdnn,   current.meanHR,  current.medianNN,
        current.pnn50,  current.cvnn,   current.iqrNN,
        baseline.rmssd, baseline.sdnn,  baseline.meanHR, baseline.medianNN,
        baseline.pnn50, baseline.cvnn,  baseline.iqrNN
    );

    result.stressLevel = wesad_stress_level(
        current.rmssd,  current.sdnn,   current.meanHR,  current.medianNN,
        current.pnn50,  current.cvnn,   current.iqrNN,
        baseline.rmssd, baseline.sdnn,  baseline.meanHR, baseline.medianNN,
        baseline.pnn50, baseline.cvnn,  baseline.iqrNN
    );

    // 压力等级 → 推荐放松方式
    switch (result.stressLevel) {
        case STRESS_RELAXED: result.relaxMethod = RELAX_NONE;      break;
        case STRESS_NORMAL:  result.relaxMethod = RELAX_NONE;      break;
        case STRESS_MILD:    result.relaxMethod = RELAX_BREATHING; break;
        case STRESS_HIGH:    result.relaxMethod = RELAX_MOVE;      break;
        default:             result.relaxMethod = RELAX_NONE;      break;
    }

    result.valid = true;
    return result;
}
