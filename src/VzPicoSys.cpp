/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdbool.h"
#include <stdio.h>
#include <pico/stdlib.h>
#include "hardware/adc.h"

// TGE for get_baudrate only
// #include <hardware/spi.h>

#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1
#if CYW43_USES_VSYS_PIN
# include "pico/cyw43_arch.h"
#endif

#include "hardware/clocks.h"
#include "VzPicoSys.h"
#include <malloc.h>
#include "lwip/init.h"

#ifndef PICO_POWER_SAMPLE_COUNT
# define PICO_POWER_SAMPLE_COUNT 3
#endif

// Pin used for ADC 0
#define PICO_FIRST_ADC_PIN 26
#define PLL_SYS_KHZ (133 * 1000)

extern char __StackLimit, __bss_end__;

VzPicoSys * VzPicoSys::getInstance()
{
  static VzPicoSys * theInstance = NULL;
  if(theInstance == NULL)
  {
    theInstance = new VzPicoSys();
  }
  return theInstance;
}

VzPicoSys::VzPicoSys() : accTimeLowCpu(0), accTimeDefaultCpu(0), curYear(0), dstOn(0), dstOff(0), lastVoltageVal(0),
  lastVoltageTime(0), isOnBattery(false)
{
  // See SDK Examples clocks/detached_clk_peri/detached_clk_peri.c:
  set_sys_clock_khz(PLL_SYS_KHZ, true);
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, PLL_SYS_KHZ * 1000, PLL_SYS_KHZ * 1000);
  defaultClockSpeed = (clock_get_hz(clk_sys) / 1000000);

  lastChange = time(NULL);

  adc_init();
  currentTime[0] = 0;
}

/** ============================================================
 * init - return state flags
 *  ============================================================ */

int VzPicoSys::init()
{
  int state = VZ_PICO_SYS_OK;
#ifdef PICO_VSYS_PIN
  // setup adc
  adc_gpio_init(PICO_VSYS_PIN);
#else
  state |= VZ_PICO_SYS_VOLTAGE_INFO_UNAVAIL;
#endif

#if ! (defined CYW43_WL_GPIO_VBUS_PIN || defined PICO_VBUS_PIN)
  state |= VZ_PICO_SYS_BATTERY_INFO_UNAVAIL;
#endif
  return state;
}

/** ============================================================
 * measureVoltage() and getVoltage() 
 * This function can only be used when WiFi is initialized. So remember the last sample, so if the MeterPicoSelfmon
 * asks for this value, we can return this
 *  ============================================================ */

void VzPicoSys::measureVoltage()
{
#ifndef PICO_VSYS_PIN
  return 0;
#else
# if CYW43_USES_VSYS_PIN
  cyw43_thread_enter();
  // Make sure cyw43 is awake
  cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN);
# endif

  adc_gpio_init(PICO_VSYS_PIN);
  adc_select_input(PICO_VSYS_PIN - PICO_FIRST_ADC_PIN);
  adc_fifo_setup(true, false, 0, false, false);
  adc_run(true);

  // We seem to read low values initially - this seems to fix it
  int ignore_count = PICO_POWER_SAMPLE_COUNT;
  while (!adc_fifo_is_empty() || ignore_count-- > 0)
  {
    (void)adc_fifo_get_blocking();
  }

  // read vsys
  uint32_t vsys = 0;
  for(int i = 0; i < PICO_POWER_SAMPLE_COUNT; i++)
  {
    uint16_t val = adc_fifo_get_blocking();
    vsys += val;
  }
  vsys /= PICO_POWER_SAMPLE_COUNT;

  adc_run(false);
  adc_fifo_drain();

# if defined CYW43_WL_GPIO_VBUS_PIN
  isOnBattery = (! cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN));
# elif defined PICO_VBUS_PIN
  gpio_set_function(PICO_VBUS_PIN, GPIO_FUNC_SIO);
  isOnBattery = (! gpio_get(PICO_VBUS_PIN));
# endif

# if CYW43_USES_VSYS_PIN
  cyw43_thread_exit();
# endif

  // Calc voltage
  const float conversion_factor = 3.3f / (1 << 12);

  // Store for when queried by meter - then WiFi may be off
  lastVoltageVal = (vsys *3 * conversion_factor);
  struct timeval now;
  gettimeofday(&now, NULL);
  lastVoltageTime = now.tv_sec;
#endif
}

time_t VzPicoSys::getVoltage(float & v, bool & b)
{
  v = lastVoltageVal;
  b = isOnBattery;
  return lastVoltageTime;
}

/** ============================================================
 * getMemUsed
 * getMemTotal
 * getMemFree
 *  ============================================================ */

long VzPicoSys::getMemUsed()  { struct mallinfo m = mallinfo(); return m.uordblks; }
long VzPicoSys::getMemTotal() { return (&__StackLimit - &__bss_end__); }
long VzPicoSys::getMemFree()  { return (getMemTotal() - getMemUsed()); }

/** ============================================================
 * getStatistics
 *  ============================================================ */

void VzPicoSys::printStatistics(log_level_t logLevel)
{
  print(logLevel, "Default CPU: %ds, LowPower CPU %ds", "", accTimeDefaultCpu, accTimeLowCpu);
}

/** ============================================================
 * Deal with CPU clock speed
 *  ============================================================ */

bool VzPicoSys::setCpuSpeedDefault()
{
  time_t now = time(NULL);
  accTimeLowCpu += (now - lastChange);
  lastChange = now;

  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                  PLL_SYS_KHZ, PLL_SYS_KHZ);
  print(log_info, "Set normal CPU speed: %dMHz.", "", defaultClockSpeed);
  return true;
}
void VzPicoSys::setCpuSpeedLow(int factor)
{
  time_t now = time(NULL);
  accTimeDefaultCpu += (now - lastChange);
  lastChange = now;

/*
  printf("DEFAULT SPEED\n");
  {
    uint f_pll_sys   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc      = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    printf("pll_usb  = %dkHz\n", f_pll_usb);
    printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\n", f_clk_sys);
    printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb  = %dkHz\n", f_clk_usb);
    printf("clk_adc  = %dkHz\n", f_clk_adc);
    printf("clk_rtc  = %dkHz\n", f_clk_rtc);

    printf("SPI: %d %d\n", spi_get_baudrate(spi0), spi_get_baudrate(spi1));
  }
*/

  print(log_info, "Setting low-power CPU speed ...", "");
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                  PLL_SYS_KHZ, PLL_SYS_KHZ / factor);
  print(log_debug, "Set low-power CPU speed %dMhz ...", "", getCurrentClockSpeed());

/*
  printf("LOW SPEED\n");
  {
    uint f_pll_sys   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc      = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri  = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc   = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    printf("pll_usb  = %dkHz\n", f_pll_usb);
    printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\n", f_clk_sys);
    printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb  = %dkHz\n", f_clk_usb);
    printf("clk_adc  = %dkHz\n", f_clk_adc);
    printf("clk_rtc  = %dkHz\n", f_clk_rtc);

    printf("SPI: %d %d\n", spi_get_baudrate(spi0), spi_get_baudrate(spi1));
  }
*/
}

uint VzPicoSys::getCurrentClockSpeed() { return (frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 1000); }
bool VzPicoSys::isClockSpeedDefault() { return (getCurrentClockSpeed() == defaultClockSpeed); }
const char * VzPicoSys::getVersion() { return PACKAGE "/" VERSION " (LwIP " LWIP_VERSION_STRING ")"; }
const char * VzPicoSys::getTimeString()
{
  struct timeval now;
  gettimeofday(&now, NULL); // Initially seconds since boot
  extern time_t sysRefTime;
  now.tv_sec += sysRefTime + tzOffset; // Add offset queried from NTP to make it real UTC. Add TZ offset to make it MET

  if(isDST(now.tv_sec)) { now.tv_sec += 3600; }; // Add 1h to make it MEST if needed

  struct tm tm;
  struct tm * timeinfo = localtime_r(&now.tv_sec, &tm);

  /* format timestamp */
  strftime(currentTime, 18, "[%b %d %H:%M:%S]", timeinfo);

  return currentTime;
}

bool VzPicoSys::isDST(const time_t now)
{
  extern time_t sysRefTime;
  if(curYear == 0 || sysRefTime >= endOfYear)
  {
    struct tm tm;
    localtime_r(&now, &tm);
    curYear = tm.tm_year;
 
    tm.tm_mon  = 2;        // March
    tm.tm_mday = (31 - 7); // Safely within last week of month
    tm.tm_hour = 2 - (tzOffset / 3600); // DST begins at 02:00 MET (not UTC), so sub tzOffset
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    dstOn = mktime(&tm);
    localtime_r(&dstOn, &tm);
    tm.tm_mday += (7 - tm.tm_wday); // Make it Sunday and calculate again
    dstOn = mktime(&tm);

    tm.tm_mon  = 9; // Oct
    tm.tm_mday = (31 - 7); // Safely within last week of month
    dstOff = mktime(&tm);
    localtime_r(&dstOff, &tm);
    tm.tm_mday += (7 - tm.tm_wday); // Make it Sunday and calculate again
    dstOff = mktime(&tm);

    tm.tm_mon  = 0;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_year = curYear + 1;
    endOfYear = mktime(&tm); // Actually this 1.1. of next year

    print(log_info, "Calculated DST limits: %lld .. %lld (EndOfYear: %lld)", "", dstOn, dstOff, endOfYear);
  }

  return ((sysRefTime > dstOn) && (sysRefTime < dstOff));
}
