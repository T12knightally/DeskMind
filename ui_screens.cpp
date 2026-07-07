/*
 * ui_screens.cpp - 屏幕视图实现
 */

#include "ui_screens.h"
#include "hal_display.h"

static ScreenView currentView = VIEW_MONITOR;

// ========== 放松方式文本映射 ==========
static const char* relaxTexts[] = {
    "No Action",         // RELAX_NONE
    "Take a Break",      // RELAX_REST
    "Breath Training",   // RELAX_BREATHING
    "Stand & Move"       // RELAX_MOVE
};

// ========== 压力等级文本映射 ==========
static const char* stressTexts[] = {
    "Lv0 Relaxed",       // STRESS_RELAXED
    "Lv1 Normal",        // STRESS_NORMAL
    "Lv2 Mild",          // STRESS_MILD
    "Lv3 High"           // STRESS_HIGH
};

// ========== 压力等级对应颜色 ==========
static uint16_t stressColors[] = {
    COLOR_STRESS_GREEN,   // 放松
    COLOR_STRESS_YELLOW,  // 正常
    COLOR_STRESS_ORANGE,  // 轻度压力
    COLOR_STRESS_RED      // 高压力
};

// ========== 实现 ==========

ScreenView ui_getCurrentView() {
    return currentView;
}

void ui_switchView(ScreenView view) {
    if (view != currentView) {
        display_clear();
        currentView = view;
    }
}

const char* ui_getRelaxMethodText(int methodId) {
    methodId = constrain(methodId, 0, 3);
    return relaxTexts[methodId];
}

const char* ui_getStressLevelText(int level) {
    level = constrain(level, 0, 3);
    return stressTexts[level];
}

// ================================================================
// 主监测界面
//   - 中央: 大号 BPM 数字
//   - 下方: "BPM" 标签
//   - 外环: 状态环 (基于BPM比例)
//   - 底部: 心率状态小字
// ================================================================
void ui_showMonitor(int bpm, bool hrValid, int beatCount, int stressLevel) {
    ui_switchView(VIEW_MONITOR);

    char buf[16];

    // ★ 顶部: 压力等级小字
    if (stressLevel >= 0 && stressLevel <= 3) {
        uint16_t sc = stressColors[stressLevel];
        display_centerText(stressTexts[stressLevel], 35, 1, sc);
    }

    // 中央大号BPM
    if (hrValid && bpm > 0) {
        snprintf(buf, sizeof(buf), "%d", bpm);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    display_centerText(buf, 80, 6);   // 大字 BPM (上移到 80, 给压力文字腾地方)

    // "BPM" 标签
    display_centerText("BPM", 145, 2);

    // 状态环 (120° ~ 360° 对应心率 50~150)
    if (hrValid && bpm >= 30) {
        float ratio = constrain((float)(bpm - 50) / 100.0f, 0.0f, 1.0f);
        float arcAngle = ratio * 300.0f;  // 最大300度
        // 颜色根据BPM变化: <60蓝, 60-100绿, >100黄
        uint16_t arcColor = COLOR_ACCENT;
        if (bpm < 60)  arcColor = 0x059F;   // 蓝色
        if (bpm > 100) arcColor = 0xFFE0;   // 黄色
        display_drawArc(120, 120, 105, 10, arcAngle, arcColor);

        // 心率状态文字
        const char* status;
        if (bpm < 60)      status = "Low";
        else if (bpm < 85) status = "Rest";
        else if (bpm < 105) status = "Normal";
        else               status = "Active";

        display_centerText(status, 180, 2);
    }

    // 底部: 心跳计数
    if (beatCount > 0) {
        snprintf(buf, sizeof(buf), "Beats: %d", beatCount);
        display_bottomText(buf);
    }
}

// ================================================================
// 压力分析结果界面
//   - 中央偏上: 压力等级 (带颜色环)
//   - 下方: 推荐放松方式
// ================================================================
void ui_showStressResult(int stressLevel, int relaxMethod) {
    ui_switchView(VIEW_STRESS);

    stressLevel = constrain(stressLevel, 0, 3);
    uint16_t color = stressColors[stressLevel];

    // 压力等级文字 (中央偏上)
    display_centerText(ui_getStressLevelText(stressLevel), 60, 3);

    // 彩色压力环 (根据等级填充不同角度)
    int arcFill = (stressLevel + 1) * 75;  // 75° / 150° / 225° / 300°
    display_drawArc(120, 120, 100, 15, arcFill, color);

    // 分隔线效果 (简单居中文本)
    display_centerText("---", 120, 2);

    // 推荐放松方式
    const char* relaxText = ui_getRelaxMethodText(relaxMethod);
    display_centerText("Recommend:", 150, 2);
    display_centerText(relaxText, 175, 2);
}

// ================================================================
// 传感器脱落提示
// ================================================================
void ui_showNoFinger() {
    ui_switchView(VIEW_NO_FINGER);
    display_showNoFinger();
}

// ================================================================
// 通信超时提示 (队友模块无响应)
// ================================================================
void ui_showCommTimeout(int lastBpm) {
    char buf[32];
    snprintf(buf, sizeof(buf), "BPM:%d", lastBpm);
    display_centerText(buf, 80, 3);

    display_centerText("Waiting AI...", 140, 2);
    display_centerText("No Response", 170, 1);
}

// ================================================================
// HRV 7 项指标详情页 (当前 vs 静息基线 对比表)
// ================================================================
void ui_showHRVDetails(const HRVMetrics& cur, const HRVMetrics& base, int stressLevel) {
    ui_switchView(VIEW_MONITOR);

    char buf[16], bufB[16];

    // ★ 顶部压力等级 (y=20)
    if (stressLevel >= 0 && stressLevel <= 3) {
        uint16_t sc = stressColors[stressLevel];
        display_centerText(stressTexts[stressLevel], 20, 1, sc);
    }

    #define CH COLOR_GRAY
    #define CN COLOR_ACCENT
    #define CB 0x059F

    // 列标题 (y=40)
    display_leftText("Name", 35, 40, 1, CH);
    display_leftText("Now",  95, 40, 1, CN);
    display_leftText("Base",160, 40, 1, CB);

    // 分隔线
    display_centerText("-----------------", 47, 1, COLOR_GRAY);

    const int LX = 35, NX = 95, BX = 160;
    const int Y0 = 56, DY = 20;
    int y = Y0;

    #define ROW(name, cv, bv, fmt) \
        display_leftText(name, LX, y, 1, CH); \
        snprintf(buf,  16, cur.valid  ? fmt : "--", cv); \
        snprintf(bufB, 16, base.valid ? fmt : "--", bv); \
        display_leftText(buf,  NX, y, 1, cur.valid  ? CN : COLOR_GRAY); \
        display_leftText(bufB, BX, y, 1, base.valid ? CB : COLOR_GRAY); \
        y += DY;

    ROW("RMSSD",  cur.rmssd,     base.rmssd,     "%.1f");
    ROW("SDNN",   cur.sdnn,      base.sdnn,      "%.1f");
    ROW("MeanHR", cur.meanHR,    base.meanHR,    "%.1f");
    ROW("MedNN",  cur.medianNN,  base.medianNN,  "%.0f");
    ROW("pNN50",  cur.pnn50,     base.pnn50,     "%.1f");
    ROW("CVNN",   cur.cvnn,      base.cvnn,      "%.4f");
    ROW("IQR",    cur.iqrNN,     base.iqrNN,     "%.1f");

    #undef ROW
    #undef CH
    #undef CN
    #undef CB
}

// ================================================================
// 静息基线校准界面
//   - 手指未放: "Place Finger" + 剩余秒数
//   - 手指放上: 大号倒计时 + "Keep still..."
//   - 校准完成: "Calibration Complete"
// ================================================================
void ui_showCalibration(int remainingSec, bool fingerOn, bool complete) {
    ui_switchView(VIEW_MONITOR);

    if (complete) {
        display_centerText("Calibration", 80, 2, COLOR_ACCENT);
        display_centerText("Complete!", 130, 3, COLOR_ACCENT);
        return;
    }

    if (!fingerOn) {
        display_centerText("Place Finger", 80, 2);
        char buf[16];
        snprintf(buf, sizeof(buf), "Calibrating %ds", remainingSec);
        display_centerText(buf, 135, 1, COLOR_GRAY);
        return;
    }

    // 手指放上 → 大字倒计时
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", remainingSec);
    display_centerText(buf, 75, 7, COLOR_ACCENT);
    display_centerText("seconds", 145, 2);
    display_centerText("Keep still...", 180, 1, COLOR_GRAY);
}

// ================================================================
// 呼吸引导放松训练
//   Phase 0 (2s): "High Stress!" + "Start Breathing..."
//   动画: 连续三角波 — 3s 扩散 → 3s 收缩 → repeat
//   8 个同心圆环, 外层粗(5px)向内渐细(1px)
//   为避免 fillScreen 阻塞主循环导致传感器 FIFO 溢出,
//   首帧全屏清除后, 后续帧只擦旧环画新环 (SPI 时间 ~23ms → ~3ms)
// ================================================================
static uint8_t  breathPhase = 0;
static unsigned long breathPhaseStart = 0;
static int      breathCycles = 0;
static int      prevR = -1;
static bool     breathNeedClear = true;

#define BREATH_INTRO_MS   2000
#define BREATH_PHASE_MS   3000
#define BREATH_CYCLE_MS   (BREATH_PHASE_MS * 2)
#define BREATH_MAX_R      98
#define BREATH_CX         120
#define BREATH_CY         120
#define BREATH_NUM_RINGS  8
#define BREATH_RING_GAP   12

void ui_initBreathing() {
    breathPhase = 0;
    breathPhaseStart = millis();
    breathCycles = 0;
    prevR = -1;
    breathNeedClear = true;
}

// 擦除或画出全部同心环 (单色)
static void drawRingsMonochrome(int r, uint16_t color) {
    for (int i = 0; i < BREATH_NUM_RINGS; i++) {
        int ringR = r - i * BREATH_RING_GAP;
        if (ringR < 2) continue;
        int thick;
        if      (i == 0) thick = 5;
        else if (i == 1) thick = 4;
        else if (i == 2) thick = 3;
        else if (i == 3) thick = 2;
        else             thick = 1;
        for (int w = 0; w < thick; w++) {
            display_drawCircle(BREATH_CX, BREATH_CY, ringR - w, color);
        }
    }
}

// 画出同心环 (领头亮蓝 + 其余暗蓝)
static void drawRingsColored(int r) {
    for (int i = 0; i < BREATH_NUM_RINGS; i++) {
        int ringR = r - i * BREATH_RING_GAP;
        if (ringR < 2) continue;
        int thick;
        if      (i == 0) thick = 5;
        else if (i == 1) thick = 4;
        else if (i == 2) thick = 3;
        else if (i == 3) thick = 2;
        else             thick = 1;
        uint16_t color = (i == 0) ? COLOR_BREATH_BLUE : COLOR_BREATH_DIM;
        for (int w = 0; w < thick; w++) {
            display_drawCircle(BREATH_CX, BREATH_CY, ringR - w, color);
        }
    }
}

int ui_showBreathing() {
    ui_switchView(VIEW_BREATHING);
    unsigned long now = millis();
    unsigned long elapsed = now - breathPhaseStart;

    // ---- Phase 0: 提示文字 (2s) ----
    if (breathPhase == 0) {
        if (elapsed >= BREATH_INTRO_MS) {
            breathPhase = 1;
            breathPhaseStart = now;
            elapsed = 0;
            breathNeedClear = true;
        } else {
            display_centerText("High Stress!", 60, 3, COLOR_STRESS_RED);
            display_centerText("Start Breathing...", 115, 2, COLOR_BREATH_BLUE);
            return 0;
        }
    }

    // ---- 呼吸动画: 连续三角波 ----
    unsigned long cycleT = elapsed % BREATH_CYCLE_MS;
    bool isExhale = (cycleT < BREATH_PHASE_MS);
    float t = isExhale
        ? (float)cycleT / (float)BREATH_PHASE_MS
        : 1.0f - (float)(cycleT - BREATH_PHASE_MS) / (float)BREATH_PHASE_MS;

    int r = (int)(t * BREATH_MAX_R);

    // ★ 首帧全屏清除; 后续帧只擦旧环 (避免 fillScreen 阻塞 23ms)
    if (breathNeedClear) {
        display_clear();
        breathNeedClear = false;
    } else if (prevR >= 0 && prevR != r) {
        drawRingsMonochrome(prevR, COLOR_BG);
    }

    // 画新环
    if (r > 0) {
        drawRingsColored(r);
    }
    prevR = r;

    // 循环计数
    breathCycles = (int)(elapsed / BREATH_CYCLE_MS);

    // 中心文字 (TFT_eSPI 自带背景色填充, 无需额外擦除)
    display_centerText(isExhale ? "Breathe Out" : "Breathe In", 45, 2, COLOR_BREATH_BLUE);

    // 底部进度
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / 5", breathCycles + 1);
    display_centerText(buf, 180, 2, COLOR_BREATH_DIM);

    return breathCycles;
}

bool ui_isBreathingDone(int maxCycles) {
    return breathCycles >= maxCycles;
}

// ================================================================
// 临时提示屏
//   呼吸训练完成后 → "起来走走吧"
//   轻度压力提醒   → "压力有点大, 休息 15s 冥想"
// ================================================================
void ui_showPrompt(int type) {
    ui_switchView(VIEW_PROMPT);

    if (type == PROMPT_LV3_DONE) {
        display_centerText("Take a Walk!", 55, 3, COLOR_STRESS_YELLOW);
        // 手动居中: 两行 12 个字符 × size 2 × 6px = 144px, 左边距 (240-144)/2 = 48
        display_leftText("Stand up &",   48, 110, 2, COLOR_TEXT);
        display_leftText("move around",  48, 135, 2, COLOR_TEXT);
    } else {
        display_centerText("Stress a bit high", 55, 3, COLOR_STRESS_ORANGE);
        // 手动居中: 两行 15 个字符 × size 2 × 6px = 180px, 左边距 (240-180)/2 = 30
        display_leftText("Rest 15 seconds", 30, 110, 2, COLOR_TEXT);
        display_leftText("Meditate now",    30, 135, 2, COLOR_TEXT);
    }
}
