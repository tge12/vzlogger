/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VZ_PICO_SYS_H
#define VZ_PICO_SYS_H

#define VZ_PICO_SYS_OK                   0
#define VZ_PICO_SYS_VOLTAGE_INFO_UNAVAIL 0x01
#define VZ_PICO_SYS_BATTERY_INFO_UNAVAIL 0x02

#include <common.h>
#include "vz_pico_inline_config.h"

class VzPicoSys
{
  public:
    static VzPicoSys * getInstance();

    int   init();
    void  measureVoltage();
    time_t getVoltage(float & v, bool & b);

    bool  setCpuSpeedDefault();
    void  setCpuSpeedLow(int factor = 20);
    uint  getCurrentClockSpeed();
    bool  isClockSpeedDefault();

    long  getMemUsed();
    long  getMemFree();
    long  getMemTotal();

    static const char * getVersion();

    void  printStatistics(log_level_t logLevel);
    const char * getTimeString();

  private:
    VzPicoSys();

    uint defaultClockSpeed;

    // Statistics counters
    uint accTimeLowCpu;
    uint accTimeDefaultCpu;
    time_t lastChange;

    time_t lastVoltageTime;
    float  lastVoltageVal;
    bool   isOnBattery;

    char currentTime[24];
};

#endif // VZ_PICO_SYS_H

