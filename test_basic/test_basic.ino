/*
 * test_basic.ino - PPG + BPM 最终调参版
 */
#include <Wire.h>
#include <MAX30105.h>

#define SDA 21
#define SCL 3

static MAX30105 sensor;

// ---- IIR 高速滤波 (DC去除, 截止 0.5Hz) ----
static float irPrevY = 0, irPrevX = 0;
static float redPrevY = 0, redPrevX = 0;
static bool  filterReady = false;

float dcRemove(float x, float &prevY, float &prevX) {
    if (!filterReady) {
        prevY = 0; prevX = x;
        return 0;
    }
    float y = 0.969 * (prevY + x - prevX);
    prevY = y; prevX = x;
    return y;
}

// ---- BPM ----
static byte rates[4];
static byte rateIdx = 0, rateCnt = 0;
static int avgBPM = 0;
static unsigned long lastBeatMs = 0;

// ---- 峰值检测 ----
static float peakVal = 0, troughVal = 0;
static bool  above = false;
static int   settle = 0;
static unsigned long lastBeatTime = 0;
static int   decayCnt = 0;

bool detectBeat(float irAC, unsigned long now) {
    // 稳定期 2 秒后重置波峰波谷 (跳过DC滤波瞬态)
    if (settle < 200) {
        settle++;
        if (settle == 200) {
            peakVal = 0; troughVal = 0;
        }
        return false;
    }

    // 不应期 300ms
    if (now - lastBeatTime < 300) return false;

    if (irAC > peakVal) peakVal = irAC;
    if (irAC < troughVal) troughVal = irAC;

    float range = peakVal - troughVal;
    if (range < 50) return false;

    float thresh = troughVal + range * 0.35f;

    bool beat = false;
    if (irAC > thresh && !above) { above = true; }
    if (irAC < thresh && above) {
        above = false;
        lastBeatTime = now;

        if (lastBeatMs > 0) {
            int rr = now - lastBeatMs;
            if (rr > 270 && rr < 2000) {
                float bpm = 60000.0f / rr;
                rates[rateIdx] = (byte)bpm;
                rateIdx = (rateIdx + 1) % 4;
                if (rateCnt < 4) rateCnt++;
                int sum = 0;
                for (int i = 0; i < rateCnt; i++) sum += rates[i];
                avgBPM = sum / rateCnt;
            }
        }
        lastBeatMs = now;
        beat = true;
    }

    // 周期性衰减
    if (++decayCnt > 300) {
        peakVal *= 0.8f; troughVal *= 1.2f; decayCnt = 0;
    }

    return beat;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== BPM Test ===");

    Wire.begin(SDA, SCL);
    Wire.setClock(400000);

    if (!sensor.begin(Wire, 400000)) {
        Serial.println("Sensor FAIL!");
        while (1) delay(1000);
    }

    sensor.setup(0x1F, 4, 2, 100, 411, 4096);
    Serial.println("Place finger...\n");
}

void loop() {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();

    sensor.check();
    bool gotSample = false;
    float irAC = 0;

    while (sensor.available()) {
        float ir  = (float)sensor.getFIFOIR();
        float red = (float)sensor.getFIFORed();
        sensor.nextSample();

        irAC = dcRemove(ir, irPrevY, irPrevX);
        float redAC = dcRemove(red, redPrevY, redPrevX);
        gotSample = true;
    }

    // 滤波准备好了
    if (!filterReady && millis() > 500) {
        filterReady = true;
        settle = 0;
        peakVal = 0; troughVal = 0;
        Serial.println("Filter ready");
    }

    // 3 秒无心跳则 BPM 清零
    if (avgBPM > 0 && now - lastBeatTime > 3000) {
        avgBPM = 0; rateCnt = 0; rateIdx = 0;
        settle = 0; peakVal = 0; troughVal = 0;
    }

    if (gotSample && filterReady) {
        bool beat = detectBeat(irAC, now);

        if (now - lastPrint > 40) {
            lastPrint = now;
            Serial.printf("%.0f,%d,%d\n", irAC, avgBPM, beat?200:0);
        }
    }

    delay(8);
}
