/**
 * Raspberry Pico WiFi class
 *
 * @package vzlogger
 * @copyright Copyright (c) 2011 - 2023, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Steffen Vogel <info@steffenvogel.de>
 */
/*
 * This file is part of volkzaehler.org
 *
 * volkzaehler.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * volkzaehler.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */

#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1
#include "pico/cyw43_arch.h"
#include "VzPicoWifi.h"
#include <Ntp.hpp>

/* Can be:
  * CYW43_LINK_DOWN         (0)     ///< link is down
  * CYW43_LINK_JOIN         (1)     ///< Connected to wifi
  * CYW43_LINK_NOIP         (2)     ///< Connected to wifi, but no IP address
  * CYW43_LINK_UP           (3)     ///< Connected to wifi with an IP address
  * CYW43_LINK_FAIL         (-1)    ///< Connection failed
  * CYW43_LINK_NONET        (-2)    ///< No matching SSID found (could be out of range, or down)
  * CYW43_LINK_BADAUTH      (-3)    ///< Authenticatation failure
  return code will be +=3 and yields index in statusTxt array below
  NOIP and UP will be return only by cyw43_tcpip_link_status(), all others also by cyw43_wifi_link_status()
*/
static const char * statusTxt[] =
{
  "Authentication failure",
  "No matching SSID found",
  "Connection failed",
  "WiFi link down",
  "WiFi connected",
  "WiFi connected - no IP address",
  "WiFi connected - has IP address"
};
static const char * wifiLogId = "wifi";

VzPicoWifi::VzPicoWifi(const char * hn, uint numRetries, uint timeout) : sysRefTS(0), firstTime(true), numUsed(0), accTimeConnecting(0), accTimeUp(0), accTimeDown(0), initialized(false), ntp(NULL)
{
  retries = numRetries;
  connTimeout = timeout;
  lastChange = time(NULL);
  if(hn != NULL)
  {
    hostname = hn;
  }
}
VzPicoWifi::~VzPicoWifi()
{
  this->disable();
}

/** ======================================================================
 * Enable and connect WiFi
 * To be done on demand
 * ====================================================================== */

bool VzPicoWifi::enable(uint enableRetries)
{
  int rc = 0;

  // --------------------------------------------------------------
  print(log_info, "Enabling WiFi ...", wifiLogId);
  // --------------------------------------------------------------

  if(initialized)
  {
    int linkStatus = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    print(log_debug, "Not initializing WiFi - already done. Link status: %d", wifiLogId, linkStatus);
  }
  else
  { 
    rc = cyw43_arch_init();
    if(rc != 0)
    {
      print(log_error, "WiFi init failed. Error %d.", wifiLogId, rc);
      return false;
    }

    cyw43_arch_enable_sta_mode();
    this->ledOn();
    cyw43_arch_lwip_begin();

    struct netif * n = &cyw43_state.netif[CYW43_ITF_STA];
    if(hostname.empty())
    {
      hostname = netif_get_hostname(n);
    }
    else
    {
      netif_set_hostname(n, hostname.c_str());
      netif_set_up(n);
    }
    cyw43_arch_lwip_end();
    initialized = true;
  }

  time_t now = time(NULL);
  accTimeDown += (now - lastChange);
  lastChange = now;

  for(uint i = 0; i < (enableRetries > 0 ? enableRetries : retries); i++)
  {
    // --------------------------------------------------------------
    print(log_info, "Connecting WiFi %s (%d) as '%s' ...", wifiLogId, wifiSSID, i, hostname.c_str());
    // --------------------------------------------------------------

    cyw43_arch_lwip_begin();
    if(! (rc = cyw43_arch_wifi_connect_timeout_ms(wifiSSID, wifiPW, CYW43_AUTH_WPA2_AES_PSK, connTimeout)))
    {
      int32_t rssi;
      uint8_t mac[6];
      
      cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
      cyw43_wifi_get_rssi(&cyw43_state, &rssi);
      cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

      print(log_info, "WiFi '%s' connected. Signal: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x, IP: %s", wifiLogId, wifiSSID, rssi,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ipaddr_ntoa(&cyw43_state.netif[CYW43_ITF_STA].ip_addr));
      firstTime = false;

      now = time(NULL);
      accTimeConnecting += (now - lastChange);
      lastChange = now;
      numUsed++;

      cyw43_arch_lwip_end();
      this->ledOn(10); // Turn off
      return true;
    }

    // Try again, blink LED
    cyw43_arch_lwip_end();
    this->ledOn(10); // Turn off
    sleep_ms(1000);
    this->ledOn(); // Turn on again
  }

  this->ledOn(10);
  print(log_error, "Failed to connect WiFi '%s'. Error: %d.", wifiLogId, wifiSSID, rc);
  this->disable();
  return false;
}

/** ======================================================================
 * Init WiFi - done only once at startup. Get NTP time.
 * No time yet, therefore no logger, just printf
 * ====================================================================== */

bool VzPicoWifi::init()
{
  if(this->enable())
  {
    // --------------------------------------------------------------
    if(! muteStdout) printf("** Getting NTP time ...\n");
    // --------------------------------------------------------------

    ntp = new Ntp();
    time_t utc = ntp->queryTime();
    if(utc)
    {
      if(! muteStdout) printf("** Got NTP UTC time %s", ctime(&utc));
      time(&sysRefTS); // Something like 1.1.1970 00:00:07, i.e. the sys is running 7secs
      sysRefTS = utc - sysRefTS;
      if(! muteStdout) printf("** Sys boot time UTC %s", ctime(&sysRefTS));

      lastUtc = utc;
      lastTimeSync = time(NULL);
    }
    return true;
  }
  return false;
}

/** ======================================================================
 * Disable WiFi to save power - to be done if not needed
 * ====================================================================== */

void VzPicoWifi::disable()
{
  if(! initialized) { return; }

  time_t now = time(NULL);
  accTimeUp += (now - lastChange);
  lastChange = now;

  print(log_info, "Disabling WiFi ...", wifiLogId);
  cyw43_arch_deinit();
  print(log_debug, "WiFi disabled.", wifiLogId);
  initialized = false;
}

time_t VzPicoWifi::getSysRefTime() { return sysRefTS; }

bool VzPicoWifi::isInitialized() { return initialized; }
bool VzPicoWifi::isConnected()
{
  if(! initialized) { return false; }

  cyw43_arch_lwip_begin();
  int linkStatus = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  cyw43_arch_lwip_end();

  if(linkStatus >= -3 && linkStatus <= 3)
  {
    print(log_finest, "WiFi link status: %s (%d).", wifiLogId, statusTxt[linkStatus + 3], linkStatus);
  }
  else
  {
    print(log_finest, "WiFi link status: %d.", wifiLogId, linkStatus);
  }

  return (linkStatus == CYW43_LINK_UP);
}

void VzPicoWifi::printStatistics(log_level_t logLevel)
{
  // Power consumption:
  //  Connecting ~60mA
  //  Up ~40mA
  //  Down ~20mA (with full CPU speed)
  print(logLevel, "WiFi statistics: NumUsed: %d, Time connecting: %ds, up: %ds, down: %ds", wifiLogId,
        numUsed, accTimeConnecting, accTimeUp, accTimeDown);
}

void VzPicoWifi::ledOn(uint msecs)
{
  cyw43_arch_lwip_begin();
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  if(msecs > 0)
  {
    // Otherwise leave it on
    sleep_ms(msecs);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
  }
  cyw43_arch_lwip_end();
}

time_t VzPicoWifi::syncTime()
{
  // Idea is: we know lastUtc and UTC as of now - which yields a deltaT
  // Then, the same deltaT should exist between the lastTimeSync and "now" - if not, probably
  // the system clock is wrong ... 

  print(log_debug, "Syncing NTP time ...", wifiLogId);
  time_t utc = ntp->queryTime();
  if(! utc)
  {
    // Something went wrong, try again later
    return sysRefTS;
  }

  time_t now = time(NULL);

  unsigned long deltaUtc = (utc - lastUtc);
  unsigned long deltaT = (now - lastTimeSync);

  long timeDiff = (deltaUtc - deltaT);
  print(log_debug, "Syncing NTP time. deltaUTC: %ld, deltaT: %ld -> %d", wifiLogId, deltaUtc, deltaT, timeDiff);

  if(timeDiff != 0)
  {
    // If deltaT is bigger, the sys clock is too fast
    print(log_info, "NTP time sync. Adjusting OS time by %ds ...", wifiLogId, timeDiff);
    sysRefTS += timeDiff;
  }

  lastUtc = utc;
  lastTimeSync = now;
  return sysRefTS;
}

