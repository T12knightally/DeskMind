/*
 * hal_max30100.cpp - MAX30102 驱动 (SparkFun 库 + 已调参峰值检测)
 */
#include "hal_max30100.h"
#include "app_config.h"
#include <Wire.h>
#include <MAX30105.h>

static MAX30105 *g_sensor = nullptr;
static bool       g_initialized = false;

// ---- IIR DC 滤波 ----
static float irPrevY = 0, irPrevX = 0;
static float redPrevY = 0, redPrevX = 0;
static bool  filterReady = false;
static unsigned long filterStart = 0;

// ---- 峰值检测 ----
static float peakVal = 0, troughVal = 0;
static bool  above = false;
static int   settle = 0;
static unsigned long lastBeatTime2 = 0;
static int   decayCnt = 0;

// ---- 缓存 ----
static SensorData g_cache;

bool max30100_begin() {
    Wire.begin(MAX30100_SDA, MAX30100_SCL);
    Wire.setClock(400000);
    delay(50);

    g_sensor = new MAX30105();
    if (!g_sensor) return false;

    g_initialized = g_sensor->begin(Wire, 400000);

    if (g_initialized) {
        Serial.printf("[SENSOR] Part ID: 0x%02X\n", g_sensor->readPartID());
        g_sensor->setup(0x1F, 4, 2, 100, 411, 4096);
        Serial.println("[SENSOR] OK");

        filterReady = false;
        filterStart = millis();
        settle = 0; peakVal = 0; troughVal = 0;
        memset(&g_cache, 0, sizeof(g_cache));
    } else {
        Serial.println("[SENSOR] FAIL!");
        delete g_sensor; g_sensor = nullptr;
    }
    return g_initialized;
}

bool max30100_isOk() { return g_initialized && g_sensor != nullptr; }

static float dcRemove(float x, float &prevY, float &prevX) {
    if (!filterReady) { prevY = 0; prevX = x; return 0; }
    float y = FILTER_ALPHA * (prevY + x - prevX);
    prevY = y; prevX = x;
    return y;
}

static bool detectBeat(float irAC, unsigned long now) {
    if (settle < 200) {
        settle++;
        if (settle == 200) { peakVal = 0; troughVal = 0; }
        return false;
    }
    if (now - lastBeatTime2 < 300) return false;

    if (irAC > peakVal) peakVal = irAC;
    if (irAC < troughVal) troughVal = irAC;
    float range = peakVal - troughVal;
    if (range < 50) return false;
    float thresh = troughVal + range * 0.30f;

    bool beat = false;
    if (irAC > thresh && !above) { above = true; }
    if (irAC < thresh && above) {
        above = false; lastBeatTime2 = now; beat = true;
    }
    if (++decayCnt > 500) { peakVal *= 0.95f; troughVal *= 1.05f; decayCnt = 0; }
    return beat;
}

SensorData max30100_read() {
    unsigned long now = millis();
    g_cache.beatDetected = false;
    g_cache.sensorOk = g_initialized;

    if (!g_initialized || !g_sensor) return g_cache;

    if (!filterReady && now - filterStart > 500) {
        filterReady = true;
        settle = 0; peakVal = 0; troughVal = 0;
    }

    // ★ 手指状态变化检测: 从"无手指"变成"有手指"时，重置峰值检测器
    //    避免 raw 值跳变（800→60000）产生假尖峰锁死 range
    static bool wasFingerOn = false;
    bool fingerOn = (g_cache.irRaw > 10000);
    if (fingerOn && !wasFingerOn) {
        // 手指刚放上，重置 DC 滤波和峰值检测状态
        irPrevY = 0; irPrevX = 0;
        redPrevY = 0; redPrevX = 0;
        peakVal = 0; troughVal = 0;
        settle = 0;
        above = false;
        decayCnt = 0;
        Serial.println("[SENSOR] 手指放上 → 重置滤波器");
    }
    wasFingerOn = fingerOn;

    g_sensor->check();

    while (g_sensor->available()) {
        float ir  = (float)g_sensor->getFIFOIR();
        float red = (float)g_sensor->getFIFORed();
        g_sensor->nextSample();

        g_cache.irRaw  = (uint16_t)ir;
        g_cache.redRaw = (uint16_t)red;
        g_cache.irFiltered  = dcRemove(ir,  irPrevY,  irPrevX);
        g_cache.redFiltered = dcRemove(red, redPrevY, redPrevX);

        if (filterReady && g_cache.irRaw > 10000) {
            if (detectBeat(g_cache.irFiltered, now))
                g_cache.beatDetected = true;
        }
    }

    return g_cache;
}

uint16_t max30100_getRawIR()  { return g_cache.irRaw; }
uint16_t max30100_getRawRed() { return g_cache.redRaw; }
