
#include "VzPicoSys.h"
#include "VzPicoFsys.h"
#include "VzPicoLogger.h"
#include <Config_Options.hpp>

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

VzPicoLogger::VzPicoLogger() : fsInitialized(false), rotateSize(100000), rotateMaxInst(10)
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
    printf("stat error: %s\n", strerror(errno));
  }
  else
  {
    printf("Log file %s size %ld\n", logfilePath, st.st_size);
  }

  FILE * fp = fopen(logfilePath, "r");
  if(fp == NULL)
  {
    printf("fopen error: %s\n", strerror(errno));
  }
  else
  {
/* TODO - This should go into a dedicated program */
    printf("Found existing vzlogger.log ... dumping ...\n");
    char buf[1024];
    while(fgets(buf, sizeof(buf) - 1, fp))
    {
      printf(">>> %s", buf);
    }
/* */

    // TODO Log rotation?
    ftruncate(fileno(fp), 0);

    int err = fclose(fp);
    if (err == -1)
    {
      printf("close error: %s\n", strerror(errno));
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

  char prefix[27];
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

  printf((sysRefTime > 0 ? "%-24s" : "%s"), prefix);
  vprintf(format, args);
  printf("\n");

  if(fsInitialized)
  {
    if(inIRQ)
    {
      char buf[256];
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
      printf("close error: %s\n", strerror(errno));
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

void print(log_level_t level, const char *format, const char *id, ...)
{
  // Skip message if its under the verbosity level
  if (level > options.verbosity()) { return; }

  va_list args;
  va_start(args, id);
  VzPicoLogger::getInstance()->printv(false, level, format, id, args);
  va_end(args);
}

