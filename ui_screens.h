/*
 * ui_screens.h - 屏幕视图管理
 *
 * 管理 GC9A01 圆形屏的不同显示视图:
 *   - 主监测屏: 实时 BPM + 状态环
 *   - 压力结果屏: 压力等级 + 推荐放松方式
 *   - 异常提示屏: 传感器脱落 / 通信超时
 */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <Arduino.h>
#include "app_config.h"
#include "hrv_engine.h"

// 屏幕视图枚举
enum ScreenView {
    VIEW_MONITOR = 0,    // 主监测界面 (实时心率)
    VIEW_STRESS,         // 压力分析结果 (队友返回)
    VIEW_NO_FINGER,      // 请放置手指
    VIEW_BREATHING,      // 呼吸引导放松训练
    VIEW_PROMPT,         // 临时提示屏 (Lv3 结束 / Lv2 提醒)
};

/**
 * @brief 渲染主监测界面
 * @param bpm         平均心率 (0表示无效)
 * @param hrValid     心率是否有效
 * @param beatCount   累计心跳数
 * @param stressLevel 压力等级 (-1=无数据, 0=放松, 1=正常, 2=轻度, 3=高压)
 */
void ui_showMonitor(int bpm, bool hrValid, int beatCount, int stressLevel = -1);

/**
 * @brief 渲染压力分析结果
 * @param stressLevel  压力等级 (0-3)
 * @param relaxMethod  推荐放松方式 (0-3)
 */
void ui_showStressResult(int stressLevel, int relaxMethod);

/**
 * @brief 显示传感器脱落提示
 */
void ui_showNoFinger();

/**
 * @brief 显示通信超时提示 (队友无响应)
 * @param lastBpm  最后有效心率 (可显示在角落)
 */
void ui_showCommTimeout(int lastBpm);

/**
 * @brief 获取当前视图
 */
ScreenView ui_getCurrentView();

/**
 * @brief 切换到指定视图
 */
void ui_switchView(ScreenView view);

/**
 * @brief 获取放松方式的中文描述
 */
const char* ui_getRelaxMethodText(int methodId);

/**
 * @brief 获取压力等级的文本描述
 */
const char* ui_getStressLevelText(int level);

/**
 * @brief 渲染 HRV 7 项指标详情页 (当前 vs 静息基线 对比表)
 * @param cur      当前窗口 HRV 指标 (实时刷新)
 * @param baseline 静息基线 HRV 指标
 */
void ui_showHRVDetails(const HRVMetrics& cur, const HRVMetrics& baseline, int stressLevel = -1);

/**
 * @brief 渲染静息基线校准界面
 * @param remainingSec 剩余秒数
 * @param fingerOn     手指是否放上
 * @param complete     校准是否刚刚完成 (显示完成提示)
 */
void ui_showCalibration(int remainingSec, bool fingerOn, bool complete);

// ========== 呼吸引导 ==========

/**
 * @brief 初始化呼吸引导状态 (切换到此模式时调用一次)
 */
void ui_initBreathing();

/**
 * @brief 渲染呼吸引导动画 (每帧 ~50ms 调用)
 *
 * 自动管理阶段切换:
 *   Phase 0: 提示文字 (2s) → Phase 1
 *   Phase 1: 呼气, 圆环从中心向外扩散 (3s) → Phase 2
 *   Phase 2: 吸气, 圆环从周围向内收缩 (3s) → Phase 1
 *
 * @return 已完成的总循环数 (5 个循环后可自动退出)
 */
int  ui_showBreathing();

/**
 * @brief 检查呼吸引导的指定循环数是否已完成
 */
bool ui_isBreathingDone(int maxCycles);

// ========== 临时提示屏 ==========

#define PROMPT_LV3_DONE    0    // 呼吸训练完成, 提示走动
#define PROMPT_LV2_ALERT   1    // 轻度压力提醒, 建议冥想

/**
 * @brief 渲染临时提示屏
 * @param type  0=LV3完成/去走走, 1=LV2压力提醒/休息冥想
 */
void ui_showPrompt(int type);

#endif
