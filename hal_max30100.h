/*
 * hal_max30100.h - MAX30100 心率血氧传感器驱动封装
 * 基于 MAX30100lib (OXullo Intersecans)
 */

#ifndef HAL_MAX30100_H
#define HAL_MAX30100_H

#include <Arduino.h>
#include "pins.h"

struct SensorData {
    uint16_t irRaw;
    uint16_t redRaw;
    float    irFiltered;
    float    redFiltered;
    bool     beatDetected;
    bool     sensorOk;
};

bool      max30100_begin();
SensorData max30100_read();
bool      max30100_isOk();
uint16_t  max30100_getRawIR();
uint16_t  max30100_getRawRed();

#endif
