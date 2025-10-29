
#ifndef VZ_PICO_LOGGER_H
#define VZ_PICO_LOGGER_H

#include <vector>
#include <common.h>

class VzPicoLogger
{
  public:
    static VzPicoLogger * getInstance();

    void print(log_level_t level, const char *format, const char *id, ...);
    void print(bool inIRQ, log_level_t level, const char *format, const char *id, ...);
    void printv(bool inIRQ, log_level_t level, const char *format, const char *id, va_list args);

  private:
    VzPicoLogger();
    void init();

    bool fsInitialized;
    std::vector<std::string> queuedMsgs;
    int  rotateSize;
    uint rotateMaxInst;
    bool noStdout;
};

#endif // VZ_PICO_LOGGER_H

