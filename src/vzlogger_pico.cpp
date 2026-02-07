
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"

#include <Config_Options.hpp>
#include <Meter.hpp>

#include "VzPicoWifi.h"
#include "VzPicoSys.h"
#include "VzPicoLogger.h"
#include "VzPicoHttpd.h"

// No major benefit from being user-configurable
static const uint mainLoopSleep = 1000;    // in ms

// --------------------------------------------------------------
// Global vars referenced elsewhere
// --------------------------------------------------------------

time_t         sysRefTime = 0;
Config_Options options;    // global application options

// --------------------------------------------------------------
// Will be called in Out-of-memory situation. Do a HW reset (not 100% reliable)
// --------------------------------------------------------------

extern "C" void vzlogger_panic(const char * fmt, ...)
{
  extern char __StackLimit, __bss_end__;
  printf("PANIC: %s stacklimit: %p, BSS end: %p, heap: %d, stack: %d\n", fmt, &__StackLimit, &__bss_end__, PICO_HEAP_SIZE, PICO_STACK_SIZE);
  print(log_error, "PANIC: %s stacklimit: %p, BSS end: %p, heap: %d, stack: %d", "", fmt, &__StackLimit, &__bss_end__, PICO_HEAP_SIZE, PICO_STACK_SIZE);
  watchdog_enable(1, 1);
}

// --------------------------------------------------------------
// Main function 
// --------------------------------------------------------------

int main()
{
  if(muteStdout)
  {
    fclose(stdout);
    fclose(stderr);
  }

  try
  {
    // -----------------------------------------
    // Must do this at very beginning - in any case before "stdio_init_all", otherwise USB stdio stuck
    // -----------------------------------------

    VzPicoSys * vzPicoSys = VzPicoSys::getInstance();
    int rc = vzPicoSys->init();
    if(rc < 0)
    {
      fprintf(stderr, "*** ERROR: Init Vsys failed: %d\n", rc);
    }

    if(! muteStdout)
    {
      // Must not do this for https://github.com/majbthrd/pico-debug
      stdio_init_all();
    }

    // Wait a bit at beginning to see stdout on USB. Say:
    //  sudo screen -L /dev/ttyACM0
    sleep_ms(5000);

    options.verbosity(5); // INFO at startup, will be overwritten when parsing config

    print(log_info, "--------------------------------------------------------------", "");
    print(log_info, "%s starting up ...", "", VzPicoSys::getVersion());
    print(log_info, "Connecting WiFi ...", "");
    print(log_info, "--------------------------------------------------------------", "");

    VzPicoWifi wifi(myHostname);
    if(! wifi.init())
    {
      fprintf(stderr, "*** ERROR: Wi-Fi connect failed.\n");
      throw vz::VZException("ERROR: Wi-Fi connect failed.");
      // TODO - we should retry infinitely
    }
    sysRefTime = wifi.getSysRefTime();
    if(! sysRefTime)
    {
      fprintf(stderr, "*** ERROR: Failed to get NTP time.\n");
      throw vz::VZException("ERROR: Failed to get NTP time.");
      // TODO - we should retry infinitely
// Exception will trigger panic function, which in turn should reboot ...

    }

    // --------------------------------------------------------------
    print(log_debug, "Parsing config and creating meters ...", "");
    // --------------------------------------------------------------

    MapContainer mappings;     // mapping between meters and channels
    try
    {
      options.config_parse(mappings, inlineConfig);
    }
    catch (std::exception &e)
    {
      print(log_alert, "Failed to parse configuration due to: %s", NULL, e.what());
      return EXIT_FAILURE;
    }
    if (mappings.size() <= 0)
    {
      print(log_alert, "No meters found - quitting!", NULL);
      return EXIT_FAILURE;
    }

    // If configured and WiFi will not go down, start HTTP server
    if(options.local() && (wifiStopTime == 0))
    {
      // --------------------------------------------------------------
      print(log_debug, "Creating HTTP Server ...", "");
      // --------------------------------------------------------------

      VzPicoHttpd * httpd = VzPicoHttpd::getInstance(&mappings);
    }

    // --------------------------------------------------------------
    print(log_debug, "===> Start meters", "");
    // --------------------------------------------------------------

    try
    {
      // open connection meters
      for (MapContainer::iterator it = mappings.begin(); it != mappings.end(); it++)
      {
        it->start();
      }
    }
    catch (std::exception &e)
    {
      print(log_alert, "Startup failed: %s", "", e.what());
      exit(1);
    }
    print(log_debug, "Startup done.", "");

    // --------------------------------------------------------------
    // Main loop
    // --------------------------------------------------------------

    print(log_info, "Default clock speed: %dMHz.", "", vzPicoSys->getCurrentClockSpeed());

    time_t sendDataComplete = time(NULL);
    uint cycle = 0;
    bool syncTime = false;
    bool isSendingData  = false;
    bool wasSendingData = false;
    while(true)
    {
      // --------------------------------------------------------------
      // Check the meters ...
      // --------------------------------------------------------------

      // Must be able to detect situation "on -> off"
      wasSendingData = isSendingData;
      isSendingData  = false;

      int  nextSend = ((sendDataComplete + sendDataInterval) - time(NULL));
      int  nextDue  = 0;
      bool clockSpeedIsDefault = vzPicoSys->isClockSpeedDefault();

      try
      {
        // --------------------------------------------------------------
        // Read meters if due ...
        // --------------------------------------------------------------

        for (MapContainer::iterator it = mappings.begin(); it != mappings.end(); it++)
        {
          int due = it->isDueIn();
          if(due <= 0)
          {
            // Some meters depend on default clock speed - so enable
            if(! clockSpeedIsDefault)
            {
              clockSpeedIsDefault = vzPicoSys->setCpuSpeedDefault();
            }

            // Make sure, the reading is not interrupted by some network activity
            // However, only if WiFi is on (freezes otherwise)
            bool wifiInitialized = wifi.isInitialized();
            if(wifiInitialized) { cyw43_arch_lwip_begin(); }
            it->read();
            if(wifiInitialized) { cyw43_arch_lwip_end(); }
          }
          else if(nextDue == 0 || due < nextDue)
          {
            nextDue = due;
          }
        }

        for (MapContainer::iterator it = mappings.begin(); it != mappings.end(); it++)
        {
          // --------------------------------------------------------------
          // If there is data to be sent + last sending is longer ago than "sendDataInterval"
          // --------------------------------------------------------------

          if(it->readyToSend() && (nextSend <= 0))
          {
            // Also, WiFi does not work right on lower speeds
            if(! clockSpeedIsDefault)
            {
              clockSpeedIsDefault = vzPicoSys->setCpuSpeedDefault();
            }

            bool wifiConnected = wifi.isConnected();
            if(! wifiConnected)
            {
              wifiConnected = wifi.enable();
            }

            if(wifiConnected)
            {
              it->sendData();
              isSendingData = true;
            }
          }
          else if(it->isBusy())
          {
            // Data sent, but no response yet - make sure, WiFi does not go down
            it->checkResponse();
            isSendingData = it->isBusy();
          }
        }
      }
      catch (std::exception &e)
      {
        // We don't exit, just go on, try to recover whatever possibe
        print(log_alert, "Reading meter or sending data failed: %s", "", e.what());
      }

      if((nextDue > 0) && ((cycle % 10) == 0))
      {
        print(log_info, "MEM: Used: %ld, Free: %ld", "", vzPicoSys->getMemUsed(), vzPicoSys->getMemFree());
        if(nextSend > 0)
        {
          print(log_info, "Cycle %d: All meters and pending I/O processed. Next due: %ds, next send: %ds. Napping ...", "", cycle, nextDue, nextSend);
        }
        else
        {
          print(log_info, "Cycle %d: All meters and pending I/O processed. Next due: %ds. Napping ...", "", cycle, nextDue);
        }
      }

      // --------------------------------------------------------------
      // Blink the LED to see something is happening
      // Cannot use onboard LED as this is preventing from disabling WiFi if not needed (for saving energy)
      // So, do that only if currently WiFi is on
      // --------------------------------------------------------------

      // In low-power mode, do not even query WiFi
      if(clockSpeedIsDefault && wifi.isInitialized())
      {
        // Blink LED for 10ms (depends on WiFi, so only if WiFi is on) saying: "WiFi is on"
        wifi.ledOn(10);

        // Measure and remember voltage, also depends on WiFi (can be queried by meter afterwards)
        vzPicoSys->measureVoltage();
        if((cycle % 10) == 0)
        {
          bool isOnBattery;
          float voltage;
          vzPicoSys->getVoltage(voltage, isOnBattery);
          print(log_info, "Power Source: %s Voltage: %.2f", "", (isOnBattery ? "Battery" : "USB"), voltage);
        }

        // At least one channel was busy sending, but now not anymore:
        if(wasSendingData && ! isSendingData)
        {
          sendDataComplete = time(NULL);
        }

        // TODO: Make these metrics available as a meter (?)
        wifi.printStatistics(log_finest);

        // Must do this, while WiFi is on
        if(syncTime && wifi.isConnected())
        {
          sysRefTime = wifi.syncTime();
          syncTime = false;
        }

        // Shutdown Wifi if either it's worth (nextDue > wifiStopTime) or data sending 
        // is scheduled separately (and not happening immediately)
        // The var "isSendingData" indicates that either data has just been sent or there is some
        // response still pending.
        if((! isSendingData) && (wifiStopTime > 0) && ((nextDue > wifiStopTime) || (sendDataInterval > 0)))
        {
          wifi.disable();
        }
      }

      // Reduce clock speed after everything WiFi or peripheral is done
      if(clockSpeedIsDefault && ! wifi.isInitialized())
      {
        if(lowCPUfactor > 0)
        {
          vzPicoSys->setCpuSpeedLow(lowCPUfactor);
        }

        vzPicoSys->printStatistics(log_finest);
        for (MapContainer::iterator it = mappings.begin(); it != mappings.end(); it++)
        {
          it->printStatistics(log_finest);
        }
      }

      // Sleep a while ...
      sleep_ms(mainLoopSleep);

      cycle++;

      if((ntpSyncCycles > 0) && ((cycle % ntpSyncCycles) == 0))
      {
        // Just set a flag ... done when WiFi connected next time
        syncTime = true;
      }
    } // while
  }
  catch (std::exception &e)
  {
    print(log_alert, "Exception: %s", "main", e.what());
    vzlogger_panic("Exception: %s", e.what());
  }

  // NOTREACHED
  return EXIT_SUCCESS;
}

