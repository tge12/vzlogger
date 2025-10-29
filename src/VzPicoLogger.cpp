
#include "VzPicoSys.h"
#include "VzPicoFsys.h"
#include "VzPicoLogger.h"
#include <Config_Options.hpp>

#include "pico/cyw43_arch.h"

extern time_t sysRefTime;
extern Config_Options options;

// TODO for now just hardcoded here ...
static const char * logfilePath = "/vzlogger.log";

VzPicoLogger * VzPicoLogger::getInstance()
{
  static VzPicoLogger * theInstance = NULL;
  if(theInstance == NULL)
  {
    theInstance = new VzPicoLogger();
    theInstance->init();
  }
  return theInstance;
}

VzPicoLogger::VzPicoLogger() : fsInitialized(false), rotateSize(1000000), rotateMaxInst(10), noStdout(muteStdout)
{
}

void VzPicoLogger::init()
{
#ifdef VZ_FSYS_SD
  this->print(log_info, "Init FS ...", "");
  VzPicoDiskDeviceSD sdCard;
  VzPicoFsysLittle littleFS(sdCard);
  littleFS.init();
  littleFS.mount("/");
  this->print(log_info, "Init FS done.", "");

  struct stat st;
  if(stat(logfilePath, &st) < 0)
  {
    this->print(log_error, "stat error: %s", "", strerror(errno));
  }
  else
  {
    this->print(log_debug, "Log file %s size %ld", "", logfilePath, st.st_size);
  }

  FILE * fp = fopen(logfilePath, "r");
  if(fp == NULL)
  {
    this->print(log_error, "fopen error: %s", "", strerror(errno));
  }
  else
  {
    // TODO - This could go into a extra/dedicated program
    this->print(log_info, "Found existing vzlogger.log ... dumping ...", "");
    if(! noStdout)
    {
      char buf[1024];
      while(fgets(buf, sizeof(buf) - 1, fp))
      {
        printf(">>> %s", buf);
      }
    }

    // TODO Log rotation?
    ftruncate(fileno(fp), 0);

    int err = fclose(fp);
    if (err == -1)
    {
      this->print(log_error, "close error: %s", "", strerror(errno));
    }
  }
  fsInitialized = true;
#endif // VZ_FSYS_SD
}

void VzPicoLogger::print(log_level_t level, const char *format, const char *id, ...)
{
  // Skip message if its under the verbosity level
  if (level > options.verbosity()) { return; }

  va_list args;
  va_start(args, id);
  this->printv(false, level, format, id, args);
  va_end(args);
}

void VzPicoLogger::print(bool inIRQ, log_level_t level, const char *format, const char *id, ...)
{
  // Skip message if its under the verbosity level
  if (level > options.verbosity()) { return; }

  va_list args;
  va_start(args, id);
  this->printv(inIRQ, level, format, id, args);
  va_end(args);
}

void VzPicoLogger::printv(bool inIRQ, log_level_t level, const char *format, const char *id, va_list args)
{
  if (level < log_alert || level > log_finest) { level = log_info; }
  const char * levelStr = "AEWWIIDDDDDFFFFF"; // Match log_level_t

  // Mar 24 22:03:43][mtr1] D 
  char prefix[32];
  if(sysRefTime > 0)
  {
    strcpy(prefix, VzPicoSys::getInstance()->getTimeString());
    if (id)
    {
      snprintf(prefix + strlen(prefix), 10, "[%s] %c%s ", id, levelStr[level], (inIRQ ? "i" : ""));
    }
  }
  else
  {
    strcpy(prefix, "** ");
  }

  if(! noStdout)
  {
    printf((sysRefTime > 0 ? "%-24s" : "%s"), prefix);
    vprintf(format, args);
    printf("\n");
  }

  // SD-card logging does not work with reduced CPU clock speed :-(
  // At least I did not find a way yet ...
  bool clockSpeedIsDefault = VzPicoSys::getInstance()->isClockSpeedDefault();
  if(fsInitialized && clockSpeedIsDefault)
  {
    if(inIRQ)
    {
      char buf[512];
      sprintf(buf, (sysRefTime > 0 ? "%-24s" : "%s"), prefix);
      int bl = strlen(buf);
      vsnprintf(buf + bl, (sizeof(buf) - 1) - bl, format, args);
      strcat(buf, "\n");
      queuedMsgs.push_back(buf);
      return;
    }

    struct stat st;
    if(stat(logfilePath, &st) == 0 && st.st_size > rotateSize)
    {
      char fName[256];
      snprintf(fName, sizeof(fName), "%s.%d", logfilePath, rotateMaxInst);
      unlink(fName); // Ignore errors, may not exist yet
      for(uint i = (rotateMaxInst - 1); i > 0; i--)
      {
        char fNameBefore[256];
        snprintf(fNameBefore, sizeof(fNameBefore), "%s.%d", logfilePath, i);
        rename(fNameBefore, fName);
        strcpy(fName, fNameBefore);
      }
      rename(logfilePath, fName);
    }

    FILE * fp = fopen(logfilePath, "a+");
    if(fp == NULL)
    {
      printf("fopen error: %s\n", strerror(errno));
    }

    // We are not in IRQ, but possibly there are queued msgs - print them first:
    for(uint i = 0; i < queuedMsgs.size(); i++)
    {
      fprintf(fp, queuedMsgs[i].c_str());
    }
    queuedMsgs.clear();

    fprintf(fp, (sysRefTime > 0 ? "%-24s" : "%s"), prefix);
    vfprintf(fp, format, args);
    fprintf(fp, "\n");
    int err = fclose(fp);
    if (err == -1)
    {
      printf("close error: %s (%d)\n", strerror(errno), errno);
    }
  }
}

/** --------------------------------------------------------------
 * Print error/debug/info messages to stdout and/or logfile
 * Legacy function
 *
 * @param id could be NULL for general messages
 * @todo integrate into syslog
 * -------------------------------------------------------------- */

void print(log_level_t level, const char * format, const char * id, ...)
{
  // Skip message if its under the verbosity level
  if (level > options.verbosity()) { return; }

  va_list args;
  va_start(args, id);
  VzPicoLogger::getInstance()->printv(false, level, format, id, args);
  va_end(args);
}
extern "C" void tge_print_lwip(const char * format, ...)
{
  va_list args;
  va_start(args, format);
  VzPicoLogger::getInstance()->printv(true, log_debug, format, "LWIP", args);
  va_end(args);
}
extern "C" void tge_flush_lwip()
{
  print(log_debug, "Flush", "");
  fflush(NULL);
}

