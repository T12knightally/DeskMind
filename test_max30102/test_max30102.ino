/*
 * test_max30102.ino — MAX30102 传感器独立诊断脚本
 *
 * 使用方法:
 *   1. Arduino IDE → 新建项目 → 粘贴此文件全部内容
 *   2. 选择开发板: ESP32S3 Dev Module
 *   3. 编译烧录, 打开串口监视器 (115200 bps)
 *
 * 诊断项目:
 *   ① I2C 总线扫描 — 确认传感器物理连通
 *   ② Part ID 读取  — 确认传感器型号正确 (应为 0x15)
 *   ③ 传感器初始化  — 测试 begin() 是否成功
 *   ④ FIFO 数据读取 — 确认传感器在产出数据
 *   ⑤ LED 电流测试 — 验证 IR LED 是否正常发光
 */

#include <Wire.h>
#include <MAX30105.h>

// ===== 引脚 (与主工程一致的 ESP32-S3 引脚) =====
#define SDA_PIN   21
#define SCL_PIN   3

// ===== 测试模式选择 =====
// 设为 true   → 仅扫描 I2C, 不初始化 MAX30102 (传感器坏了也能跑)
// 设为 false  → 完整诊断 (需要传感器正常)
#define SCAN_ONLY false

// ================================================================
// ① I2C 总线扫描
// ================================================================
void i2cScan() {
    Serial.println(F("\n========== [1] I2C 总线扫描 =========="));
    Serial.println(F("扫描地址 0x01 ~ 0x7F ..."));

    int found = 0;
    for (uint8_t addr = 1; addr < 0x7F; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("  ✓ 发现设备: 0x%02X", addr);
            if (addr == 0x57) {
                Serial.print(F("  ← MAX30102/MAX30105 默认地址"));
            }
            Serial.println();
            found++;
        }
    }

    if (found == 0) {
        Serial.println(F("  ✗ 未发现任何 I2C 设备!"));
        Serial.println(F("\n  ▶ 请检查:"));
        Serial.println(F("    1. SDA(21) / SCL(3) 接线是否正确"));
        Serial.println(F("    2. 模块是否 3.3V 供电"));
        Serial.println(F("    3. 杜邦线是否插紧 (用万用表测通断)"));
        Serial.println(F("    4. 模块是否自带 4.7kΩ 上拉电阻"));
        Serial.println(F("\n  ▶ 尝试换引脚 (GPIO3 是 strapping pin, 有时不可靠):"));
        Serial.println(F("    编辑本文件 SDA_PIN/SCL_PIN 宏定义, 改到 17/18 试试"));
    } else if (found == 1) {
        Serial.println(F("  ✓ 发现 1 个设备, 总线正常"));
    } else {
        Serial.printf ("  ✓ 发现 %d 个设备, 总线正常\n", found);
    }
}

// ================================================================
// ② 读取 Part ID (直接 I2C, 不经过库的 begin)
// ================================================================
void readPartID() {
    Serial.println(F("\n========== [2] Part ID 寄存器读取 =========="));

    Wire.beginTransmission(0x57);
    Wire.write(0xFF);   // Part ID 寄存器地址
    if (Wire.endTransmission(false) != 0) {
        Serial.println(F("  ✗ I2C 通信失败! 无法访问 0x57"));
        Serial.println(F("    设备可能处于掉电/复位状态"));
        return;
    }

    Wire.requestFrom(0x57, (uint8_t)1);
    if (Wire.available()) {
        uint8_t id = Wire.read();
        Serial.printf("  Part ID: 0x%02X (%d)\n", id, id);
        if (id == 0x15) {
            Serial.println(F("  ✓ 正确! 这是 MAX30102 或 MAX30105"));
        } else if (id == 0x11) {
            Serial.println(F("  ⚠ MAX30100 (旧版), 兼容性问题可能"));
        } else {
            Serial.println(F("  ✗ 未知型号! 可能不是 MAX3010x 系列"));
        }
    } else {
        Serial.println(F("  ✗ 无响应! 传感器可能未上电或已损坏"));
    }
}

// ================================================================
// ③ 传感器初始化
// ================================================================
bool initSensor(MAX30105 &sensor) {
    Serial.println(F("\n========== [3] 传感器初始化 =========="));

    bool ok = sensor.begin(Wire, 400000);
    if (!ok) {
        Serial.println(F("  ✗ sensor.begin() 返回 false!"));
        Serial.println(F("\n  ▶ 可能原因:"));
        Serial.println(F("    1. Part ID 读不到 (已在上一步验证)"));
        Serial.println(F("    2. 库内部复位超时 (I2C 时序问题, 降低速率试试)"));
        Serial.println(F("    3. 模块供电不足 (检查 3.3V)"));
        Serial.println(F("\n  ▶ 尝试: 把 Wire.setClock(400000) 改成 100000"));
        return false;
    }

    Serial.printf("  ✓ begin() 成功, Part ID: 0x%02X\n", sensor.readPartID());
    return true;
}

// ================================================================
// ④ 配置传感器并验证 FIFO
// ================================================================
void configAndTestFIFO(MAX30105 &sensor) {
    Serial.println(F("\n========== [4] FIFO 数据读取测试 =========="));

    // 温和配置: LED电流=0x1F(≈50mA), ADC=18bit, 采样率=100Hz, pulse=411us, slot=0
    sensor.setup(0x1F, 4, 2, 100, 411, 4096);
    Serial.println(F("  配置: LED=0x1F, ADC=18bit, SR=100Hz, PW=411us"));
    Serial.println(F("  等待 1 秒让传感器稳定..."));

    unsigned long start = millis();
    int sampleCount = 0, fifoEmpty = 0;
    uint32_t minIR = 0xFFFFFFFF, maxIR = 0;

    while (millis() - start < 2000) {
        sensor.check();
        while (sensor.available()) {
            uint32_t ir  = sensor.getFIFOIR();
            uint32_t red = sensor.getFIFORed();
            sensor.nextSample();

            if (ir < minIR) minIR = ir;
            if (ir > maxIR) maxIR = ir;

            if (sampleCount < 3) {
                Serial.printf("  样本#%d: IR=%5lu  Red=%5lu\n", sampleCount + 1, ir, red);
            }
            sampleCount++;
        }
        if (sensor.available() == false && sampleCount == 0) {
            fifoEmpty++;
        }
    }

    Serial.printf("\n  2秒内读取 %d 个样本 (期望 ~200 @100Hz)\n", sampleCount);
    if (sampleCount == 0) {
        Serial.println(F("  ✗ FIFO 无数据!"));
        Serial.println(F("\n  ▶ 可能原因:"));
        Serial.println(F("    1. IR LED 烧了或根本没亮"));
        Serial.println(F("    2. 传感器被遮挡 (拿近看LED有没有红光)"));
        Serial.println(F("    3. FIFO 配置错误, 数据被丢弃"));
    } else {
        Serial.printf("  ✓ FIFO 正常输出数据\n");
        Serial.printf("  IR 范围: %lu ~ %lu\n", minIR, maxIR);

        // 诊断数据范围
        if (maxIR < 5000) {
            Serial.println(F("  ⚠ IR 值偏低 (< 5000)"));
            Serial.println(F("    → LED 电流可能太小 或 传感器表面脏了/被挡住"));
        } else if (maxIR > 200000) {
            Serial.println(F("  ⚠ IR 值偏高 (> 200000)"));
            Serial.println(F("    → 可能是环境光漏进去了 或 LED 电流过大饱和"));
        } else {
            Serial.println(F("  ✓ IR 值在正常范围"));
        }

        int dataRate = sampleCount / 2;  // Hz
        if (dataRate < 80) {
            Serial.printf("  ⚠ 实际采样率 ~%dHz, 低于设定的100Hz\n", dataRate);
            Serial.println(F("    → loop() 中是否有 delay()? 或者 I2C 速率瓶颈?"));
        } else {
            Serial.printf("  ✓ 实际采样率 ~%dHz, 接近设定值\n", dataRate);
        }
    }
}

// ================================================================
// ⑤ 手指检测 & 信号质量
// ================================================================
void fingerTest(MAX30105 &sensor) {
    Serial.println(F("\n========== [5] 手指检测 & 信号质量 =========="));
    Serial.println(F("  请放下手指... 5秒后"));
    delay(5000);

    sensor.check();
    long irNoFinger = 0;
    int cnt = 0;
    while (sensor.available()) {
        irNoFinger += sensor.getFIFOIR();
        sensor.nextSample();
        cnt++;
    }
    if (cnt > 0) irNoFinger /= cnt;
    Serial.printf("  无手指时 IR 平均值: %ld\n", irNoFinger);

    Serial.println(F("  现在请把手指放在传感器上... 等待10秒采样"));
    unsigned long start = millis();
    long irFinger = 0, irMin = 999999, irMax = 0;
    int cntF = 0;

    while (millis() - start < 10000) {
        sensor.check();
        while (sensor.available()) {
            long ir = sensor.getFIFOIR();
            sensor.nextSample();
            irFinger += ir;
            if (ir < irMin) irMin = ir;
            if (ir > irMax) irMax = ir;
            cntF++;
        }
        delay(1);
    }

    if (cntF == 0) {
        Serial.println(F("  ✗ 手指模式下仍无数据!"));
        return;
    }

    irFinger /= cntF;
    long swing = irMax - irMin;

    Serial.printf("  放置手指后 IR 平均值: %ld\n", irFinger);
    Serial.printf("  IR 范围: %ld ~ %ld (摆幅: %ld)\n", irMin, irMax, swing);

    if (irFinger < irNoFinger * 1.5 && irNoFinger > 0) {
        Serial.println(F("  ⚠ 放置手指后 IR 变化不大"));
        Serial.println(F("    → LED 可能没亮, 或者手指放偏了"));
    } else if (irFinger > irNoFinger * 1.5) {
        Serial.println(F("  ✓ IR 值因手指放置而明显变化 (反射光增强)"));
    }

    if (swing < 50) {
        Serial.println(F("  ⚠ PPG 信号摆幅 < 50, 心跳信号很弱"));
        Serial.println(F("    → 手指按压力度调整一下 (太轻或太重都不好)"));
        Serial.println(F("    → 或者环境光太强"));
    } else if (swing > 5000) {
        Serial.println(F("  ⚠ PPG 信号摆幅 > 5000, 可能手指在移动"));
    } else {
        Serial.printf("  ✓ PPG 信号摆幅 %ld, 正常范围\n", swing);
    }
}

// ================================================================
// ⑥ 持续输出模式 (可选: 串口绘图器可视化波形)
// ================================================================
void plotMode(MAX30105 &sensor) {
    Serial.println(F("\n========== [6] 波形输出模式 =========="));
    Serial.println(F("  持续输出 IR 原始值 + 滤波值"));
    Serial.println(F("  可在 Arduino IDE 的 工具→串口绘图器 中查看实时波形"));
    Serial.println(F("  格式: IR_Raw,IR_Filtered"));
    Serial.println(F("  按任意键停止..."));

    float prevY = 0, prevX = 0;
    bool filterReady = false;
    unsigned long filterStart = millis();
    int settle = 0;

    while (!Serial.available()) {
        sensor.check();
        while (sensor.available()) {
            float ir = (float)sensor.getFIFOIR();
            sensor.nextSample();

            unsigned long now = millis();
            if (!filterReady && now - filterStart > 500) {
                filterReady = true;
                prevX = ir; prevY = 0;
                settle = 0;
            }

            float irFilt = 0;
            if (filterReady && settle < 200) {
                settle++;
                if (settle == 200) { prevY = 0; prevX = ir; }
                irFilt = 0;
            } else if (filterReady) {
                irFilt = 0.969f * (prevY + ir - prevX);
                prevY = irFilt; prevX = ir;
            }

            Serial.print(ir);
            Serial.print(",");
            Serial.println(irFilt);
        }
        delay(8);  // ~100Hz
    }
    while (Serial.available()) Serial.read();  // 清空输入缓冲
}

// ================================================================
// main
// ================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n\n"));
    Serial.println(F("╔══════════════════════════════════════════╗"));
    Serial.println(F("║   MAX30102 传感器诊断工具 v1.0          ║"));
    Serial.println(F("║   引脚: SDA=21  SCL=3                    ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));

    // ---- 第一步: 初始化 I2C ----
    Serial.println(F("\n[初始化 I2C]"));
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);
    delay(50);
    Serial.printf("  Wire.begin(SDA=%d, SCL=%d) @ 400kHz\n", SDA_PIN, SCL_PIN);
    Serial.println(F("  ✓ I2C 初始化完成"));

    // ---- 第二步: I2C 扫描 ----
    i2cScan();

    if (SCAN_ONLY) {
        Serial.println(F("\nSCAN_ONLY=true, 诊断结束。"));
        Serial.println(F("如果扫到 0x57, 硬件连接正常; 没扫到就排查接线/供电。"));
        return;
    }

    // ---- 第三步: Part ID ----
    readPartID();

    // ---- 第四步: 初始化传感器 ----
    MAX30105 sensor;
    if (!initSensor(sensor)) {
        Serial.println(F("\n✗ 初始化失败, 诊断中断。"));
        Serial.println(F("  请根据上面的提示排查问题。"));
        return;
    }

    // ---- 第五步: FIFO 测试 ----
    configAndTestFIFO(sensor);

    // ---- 第六步: 手指检测 ----
    fingerTest(sensor);

    // ---- 第七步: 汇总 ----
    Serial.println(F("\n╔══════════════════════════════════════════╗"));
    Serial.println(F("║   ✓ 基础诊断全部通过!                   ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));
    Serial.println(F("\n如果以上测试全部通过但主程序仍初始化失败, 检查:"));
    Serial.println(F("  1. 主程序是否在 setup() 中调用了两次 Wire.begin()"));
    Serial.println(F("     (display_begin() 里可能也调了, 导致冲突)"));
    Serial.println(F("  2. 是否有其他设备占用了 GPIO21/3"));
    Serial.println(F("  3. TFT_eSPI 库是否也配置了这些引脚"));

    // ---- 第八步: 波形输出 (可选) ----
    Serial.println(F("\n══════════════════════════════════════════"));
    Serial.println(F("按 'y' + 回车 → 进入持续波形输出模式 (串口绘图器)"));
    Serial.println(F("其他任意键     → 结束诊断"));
    Serial.println(F("══════════════════════════════════════════"));

    unsigned long waitStart = millis();
    while (millis() - waitStart < 10000 && !Serial.available()) {
        delay(10);
    }

    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'y' || c == 'Y') {
            plotMode(sensor);
        }
    }

    Serial.println(F("\n诊断结束。"));
}

void loop() {
    // 诊断脚本不需要 loop, 全部在 setup() 中完成
    delay(100);
}
