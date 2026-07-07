/*
 * comm_uart.cpp - UART 通信实现
 */

#include "comm_uart.h"
#include "pins.h"

static StressResult lastResult  = {0, 0, false, 0};
static StressResult pendingResult = {0, 0, false, 0};

// 接收缓冲区
#define RX_BUF_SIZE 64
static char rxBuf[RX_BUF_SIZE];
static int  rxIdx = 0;

void comm_uart_begin() {
    Serial1.begin(COMM_BAUDRATE, SERIAL_8N1, COMM_RX, COMM_TX);
    rxIdx = 0;
    Serial.printf("[COMM] UART: RX=GPIO%d TX=GPIO%d @%d bps\n",
                  COMM_RX, COMM_TX, COMM_BAUDRATE);
}

// ================================================================
// 发送函数
// ================================================================

void comm_uart_sendBPM(int bpm) {
    Serial1.printf("BPM:%d\n", bpm);
}

void comm_uart_sendHRVDeltas(const HRVDeltaFeatures& d) {
    if (!d.valid) return;

    // 协议: HRV:d1,d2,d3,d4,d5,d6,d7
    // d1=deltaRMSSD  d2=deltaSDNN   d3=deltaMeanHR
    // d4=deltaMedNN  d5=deltaPNN50  d6=deltaCVNN  d7=deltaIQR
    Serial1.printf("HRV:%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.1f\n",
                   d.deltaRMSSD,
                   d.deltaSDNN,
                   d.deltaMeanHR,
                   d.deltaMedianNN,
                   d.deltaPNN50,
                   d.deltaCVNN,
                   d.deltaIQRNN);
}

void comm_uart_sendHeartbeat() {
    Serial1.println("HB:1");
}

// ================================================================
// 接收解析
// ================================================================

/**
 * @brief 解析队友返回的压力/放松指令
 * 格式: STR:<level>,REL:<method>
 */
static bool parseLine(const char* line, StressResult& out) {
    int level = -1, method = -1;
    int n = sscanf(line, "STR:%d,REL:%d", &level, &method);

    if (n >= 1) {
        out.stressLevel = constrain(level, STRESS_RELAXED, STRESS_HIGH);
        out.relaxMethod = (n >= 2) ? constrain(method, RELAX_NONE, RELAX_MOVE) : 0;
        out.isNew = true;
        out.timestamp = millis();
        return true;
    }
    return false;
}

StressResult comm_uart_checkRx() {
    // 返回待消费数据
    if (pendingResult.isNew) {
        StressResult r = pendingResult;
        pendingResult.isNew = false;
        return r;
    }

    while (Serial1.available()) {
        char c = Serial1.read();

        if (c == '\n' || c == '\r') {
            if (rxIdx > 0) {
                rxBuf[rxIdx] = '\0';
                StressResult parsed;
                if (parseLine(rxBuf, parsed)) {
                    lastResult = parsed;
                    rxIdx = 0;
                    return parsed;
                } else {
                    Serial.printf("[COMM] 未知指令: %s\n", rxBuf);
                }
            }
            rxIdx = 0;
        }
        else if (c >= 0x20 && rxIdx < RX_BUF_SIZE - 1) {
            rxBuf[rxIdx++] = c;
        }
    }

    StressResult empty = {0, 0, false, 0};
    return empty;
}

StressResult comm_uart_getLastResult() {
    return lastResult;
}
