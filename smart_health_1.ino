/*
 * smart_health.ino - 智能健康干预设备 (最终版)
 * MAX30102 + GC9A01 + UART
 */
#include "pins.h"
#include "app_config.h"
#include "hal_max30100.h"
#include "hal_display.h"
#include "hal_button.h"
#include "dsp_ppg.h"
#include "hrv_engine.h"
#include "comm_uart.h"
#include "ui_screens.h"
#include "stress_classifier.h"

static unsigned long lastSample = 0, lastBPMtx = 0, lastHRVtx = 0, lastUI = 0;
static unsigned long stressUntil = 0, lastCommRx = 0;
static int  sampleCnt = 0, dispMode = 0;
static int  ledBlinkCnt = 0;
static unsigned long ledBlinkNext = 0;
static SensorData g_sensor;
static HeartRateData g_hr;
static int lastView = -99, lastBPM = -99, lastStress = -2;
static unsigned long lastBeatCount = 0;
static int  lastHrvUpdateCount = -1;  // HRV 详情页刷新触发器
static HRVMetrics g_hrvLatest;
static int g_hrvUpdateCount = 0;     // HRV 更新计数 (用于触发刷新)
static int g_stressLevel = -1;
static bool breathingActive = false;   // 呼吸引导中
static unsigned long breathingStartMs = 0;

// 临时提示屏
static bool promptActive = false;
static unsigned long promptStartMs = 0;
static int  promptType = 0;
#define PROMPT_DURATION     3000    // 提示显示时长 (ms)
static unsigned long lv2CooldownUntil = 0;  // Lv2 触发冷却 (防重复)
#define LV2_COOLDOWN_MS     60000   // Lv2 冷却 60 秒

// 校准状态
static int  calibPhase = 0;       // 0=校准中, 1=显示完成提示, 2=正常
static unsigned long calibPhaseTime = 0;

void setup() {
    Serial.begin(115200); delay(300);
    Serial.println(F("\n=== SmartHealth ==="));

    Serial.println(F("[INIT] MAX30102..."));
    if (!max30100_begin()) Serial.println(F("       SENSOR FAIL"));

    Serial.println(F("[INIT] PPG & HRV..."));
    dsp_ppg_begin();
    hrv_engine_begin();
    hrv_engine_startBaseline();

    Serial.println(F("[INIT] Display..."));
    display_begin();

    button_begin();
    pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW);

    comm_uart_begin();
    lastCommRx = millis();

    Serial.println(F("[INIT] DONE\n"));
}

void loop() {
    unsigned long now = millis();

    // ---- ① 采样 (100Hz) ----
    if (now - lastSample >= SAMPLE_INTERVAL_MS) {
        lastSample = now; sampleCnt++;
        g_sensor = max30100_read();

        // 基线倒计时: 手指放上才累积时间
        bool fingerOnForTick = max30100_isOk() && (g_sensor.irRaw > 10000);
        if (fingerOnForTick) {
            hrv_engine_tickBaseline(SAMPLE_INTERVAL_MS);
        }

        if (g_sensor.beatDetected) {
            int rr = dsp_ppg_onBeat(now);
            if (rr > 0) hrv_engine_addBeat(rr, now);
            digitalWrite(PIN_LED, HIGH);
            delayMicroseconds(2000);
            digitalWrite(PIN_LED, LOW);
        }

        g_hr = dsp_ppg_getHR(now);

        if (sampleCnt % 10 == 0) {
            Serial.printf("%d,%d,%d,%d\n",
                g_sensor.irRaw, (int)g_sensor.irFiltered,
                g_hr.avgBPM, g_sensor.beatDetected ? 1 : 0);
        }
    }

    // ---- ② BPM + HRV + 压力分类 (每秒) ----
    if (now - lastBPMtx >= COMM_SEND_BPM_INTERVAL) {
        lastBPMtx = now;

        if (g_hr.valid && g_hr.avgBPM > 0)
            comm_uart_sendBPM(g_hr.avgBPM);
        else
            comm_uart_sendHeartbeat();

        // 基线就绪检测 + 状态机切换
        bool blReady = hrv_engine_isBaselineReady();
        if (blReady) {
            if (calibPhase == 0) {
                // 校准刚完成 → 进入"显示完成提示"阶段
                calibPhase = 1;
                calibPhaseTime = now;
                Serial.println("[CALIB] Phase 0→1: show complete message");
                HRVMetrics bl = hrv_engine_getBaseline();
                Serial.printf("[BASELINE] RMSSD=%.1f SDNN=%.1f HR=%.1f\n",
                              bl.rmssd, bl.sdnn, bl.meanHR);
            }
            if (calibPhase == 1 && now - calibPhaseTime >= 2000) {
                // 完成提示显示 2 秒 → 正常模式
                calibPhase = 2;
                lastView = -99;  // force redraw to mode 0
                Serial.println("[CALIB] Phase 1→2: normal mode");
            }
            // HRV + 压力分类 (Phase 1, 2 都执行)
            HRVMetrics cur = hrv_engine_compute();
            HRVMetrics bl  = hrv_engine_getBaseline();
            g_hrvLatest = cur;
            g_hrvUpdateCount++;

            // ★ HRV 诊断输出 (每 5 秒)
            {
                static int hrvDiagCnt = 0;
                if (++hrvDiagCnt % 5 == 0) {
                    Serial.printf("[HRV] valid=%d nn=%d rmssd=%.1f sdnn=%.1f\n",
                                  cur.valid, cur.nnCount, cur.rmssd, cur.sdnn);
                }
            }

            StressClassification local = stress_classify(cur, bl);
            if (local.valid) {
                g_stressLevel = local.stressLevel;
                Serial.printf("[LOCAL] Stress=%d Prob=%.2f Relax=%d\n",
                              local.stressLevel, local.probability, local.relaxMethod);

                // ★ 重度压力 → 自动触发呼吸引导 (有冷却)
                if (g_stressLevel == 3 && !breathingActive && !promptActive) {
                    breathingActive = true;
                    breathingStartMs = now;
                    ui_initBreathing();
                    Serial.println("[BREATH] Lv3 → 启动呼吸引导");
                }

                // ★ 轻度压力 → 提醒休息冥想 (有冷却)
                if (g_stressLevel == 2 && !breathingActive && !promptActive
                    && now >= lv2CooldownUntil) {
                    promptActive = true;
                    promptStartMs = now;
                    promptType = PROMPT_LV2_ALERT;
                    lv2CooldownUntil = now + LV2_COOLDOWN_MS;
                    Serial.println("[PROMPT] Lv2 → 休息冥想提醒");
                }
            }
        }
    }

    // ---- ③ UART 发送 HRV delta (每10秒) ----
    if (now - lastHRVtx >= COMM_SEND_HRV_INTERVAL) {
        lastHRVtx = now;
        if (hrv_engine_isBaselineReady()) {
            HRVDeltaFeatures d = hrv_engine_getDeltas();
            if (d.valid) comm_uart_sendHRVDeltas(d);
        }
    }

    // ---- ④ 接收队友 ----
    StressResult stress = comm_uart_checkRx();
    if (stress.isNew) {
        Serial.printf("[RX] STR:%d REL:%d\n", stress.stressLevel, stress.relaxMethod);
        ui_showStressResult(stress.stressLevel, stress.relaxMethod);
        stressUntil = now + 5000;
        lastCommRx = now;
        ledBlinkCnt = 6; ledBlinkNext = now + 100;
    }

    if (ledBlinkCnt > 0 && now >= ledBlinkNext) {
        digitalWrite(PIN_LED, ledBlinkCnt % 2);
        ledBlinkCnt--; ledBlinkNext = now + 100;
    }

    // ---- ⑤ 屏幕 (20fps) ----
    if (now - lastUI >= UI_REFRESH_MS) {
        lastUI = now;

        static bool wasFingerOn = false;
        bool sensorOK = max30100_isOk();
        bool fingerOn = sensorOK && (g_sensor.irRaw > 10000);

        if (!fingerOn && wasFingerOn) {
            Serial.println("[BPM] ✋ 手指离开，BPM归零");
            dsp_ppg_reset();
        }
        wasFingerOn = fingerOn;

        bool baselineOK = (calibPhase >= 1);   // Phase 1/2 = 基线就绪
        int curBPM = g_hr.avgBPM;

        if (ui_getCurrentView() == VIEW_STRESS && now > stressUntil)
            ui_switchView(VIEW_MONITOR);

        int vt;
        if (!sensorOK) vt = -1;
        else if (!fingerOn) vt = -2;
        else if (calibPhase == 1) vt = -4;       // 校准完成提示
        else if (calibPhase == 0) vt = -5;       // 校准中
        else if (ui_getCurrentView() == VIEW_STRESS) vt = -3;
        else vt = dispMode;

        // 校准中或完成提示时 BPM 不需刷新，直接触发
        bool needRefresh = (vt != lastView || abs(curBPM - lastBPM) >= 3
                            || g_stressLevel != lastStress
                            || (dispMode == 2 && g_hrvUpdateCount != lastHrvUpdateCount));
        if (vt == -5) {
            // 校准中：剩余秒数变化就刷新
            static int lastRemain = -1;
            int remain = hrv_engine_getBaselineRemaining();
            if (remain != lastRemain) { lastRemain = remain; needRefresh = true; }
        }

        // 呼吸引导中持续刷新 (不依赖 needRefresh，因为每帧动画都在变化)
        if (breathingActive) {
            int cycles = ui_showBreathing();
            if (cycles >= 5) {
                breathingActive = false;
                g_stressLevel = -1;
                lastView = -99;
                // ★ 呼吸完成 → 提示活动 + 冷却
                promptActive = true;
                promptStartMs = now;
                promptType = PROMPT_LV3_DONE;
                Serial.println("[BREATH] 完成 → 提示活动");
            }
        }

        // 临时提示屏 (只在首帧绘制, 静态内容无需重绘)
        if (promptActive) {
            static bool promptDrawn = false;
            if (!promptDrawn) {
                display_clear();
                ui_showPrompt(promptType);
                promptDrawn = true;
            }
            if (now - promptStartMs >= PROMPT_DURATION) {
                promptActive = false;
                promptDrawn = false;
                lastView = -99;
                Serial.println("[PROMPT] 提示结束");
            }
        }

        // ★ HRV 详情页独立刷新 (dispMode==2, 不依赖 needRefresh)
        {
            static int lastHrvRendered = -1;
            if (dispMode == 2 && !breathingActive && !promptActive
                && g_hrvUpdateCount != lastHrvRendered) {
                display_clear();
                ui_showHRVDetails(g_hrvLatest, hrv_engine_getBaseline(), g_stressLevel);
                lastHrvRendered = g_hrvUpdateCount;
                lastView = 2;
                lastStress = g_stressLevel;
            }
        }

        if (needRefresh && !breathingActive && !promptActive && dispMode != 2) {
            lastView = vt; lastBPM = curBPM; lastStress = g_stressLevel;
            lastBeatCount = g_hr.beatCount;
            lastHrvUpdateCount = g_hrvUpdateCount;

            display_clear();

            if (!sensorOK) {
                display_centerText("Sensor Error", 120, 2);
            } else if (!fingerOn) {
                if (calibPhase == 0) {
                    ui_showCalibration(hrv_engine_getBaselineRemaining(), false, false);
                } else {
                    ui_showNoFinger();
                }
            } else if (vt == -4) {
                // 校准完成提示
                ui_showCalibration(0, true, true);
            } else if (vt == -5) {
                // 校准中
                ui_showCalibration(hrv_engine_getBaselineRemaining(), true, false);
            } else if (vt == -3) {
                // stress view stays
            } else if (dispMode == 0) {
                int showBPM = (curBPM > 0) ? curBPM : 0;
                ui_showMonitor(showBPM, curBPM > 0, g_hr.beatCount, g_stressLevel);
                if (curBPM == 0) display_bottomText("Measuring...");
            } else if (dispMode == 1) {
                char b[32];
                if (g_stressLevel >= 0 && g_stressLevel <= 3) {
                    static const uint16_t scMap[] = {COLOR_STRESS_GREEN, COLOR_STRESS_YELLOW, COLOR_STRESS_ORANGE, COLOR_STRESS_RED};
                    display_centerText(ui_getStressLevelText(g_stressLevel), 25, 1, scMap[g_stressLevel]);
                }
                snprintf(b, sizeof(b), "%d BPM", curBPM > 0 ? curBPM : 0);
                display_centerText(b, 60, 4);
                snprintf(b, sizeof(b), "IR:%d", g_sensor.irRaw);
                display_centerText(b, 115, 2);
                display_centerText(baselineOK ? "Base:OK" : "Base:...", 170, 1);
            } else {
                ui_showHRVDetails(g_hrvLatest, hrv_engine_getBaseline(), g_stressLevel);
            }
        }
    }

    // ---- ⑥ 按键 ----
    ButtonEvent btn = button_check();
    if (btn == BTN_SHORT_PRESS) {
        if (breathingActive) {
            // 呼吸引导中按任意键退出
            breathingActive = false;
            g_stressLevel = -1;
            lastView = -99;
            Serial.println("[BREATH] 用户按键退出");
        } else if (ui_getCurrentView() == VIEW_STRESS) {
            ui_switchView(VIEW_MONITOR); stressUntil = 0;
        } else if (hrv_engine_isBaselineReady()) {
            // 基线就绪后才允许切换模式
            dispMode = (dispMode + 1) % 3;
            lastView = -99;
        }
    } else if (btn == BTN_LONG_PRESS) {
        dsp_ppg_reset();
        hrv_engine_startBaseline();
        calibPhase = 0;
        g_stressLevel = -1;
        digitalWrite(PIN_LED, HIGH); delay(300);
        digitalWrite(PIN_LED, LOW);
    }
}
