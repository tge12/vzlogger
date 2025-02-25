/**
 * Raspberry Pico Self monitoring
 **/

#include "Options.hpp"
#include "protocols/MeterPicoSelfmon.hpp"
#include "VzPicoSys.h"

MeterPicoSelfmon::MeterPicoSelfmon(const std::list<Option> &options) : Protocol("selfmon")
{
  ids[0] = ReadingIdentifier::Ptr(new StringIdentifier("MemFree"));
  ids[1] = ReadingIdentifier::Ptr(new StringIdentifier("MemTotal"));
  ids[2] = ReadingIdentifier::Ptr(new StringIdentifier("Voltage"));
  ids[3] = ReadingIdentifier::Ptr(new StringIdentifier("OnBattery"));

  print(log_debug, "Created MeterPicoSelfmon", "");
}

MeterPicoSelfmon::~MeterPicoSelfmon() { }

int MeterPicoSelfmon::open()
{
  return SUCCESS;
}

int MeterPicoSelfmon::close() { return SUCCESS; }

ssize_t MeterPicoSelfmon::read(std::vector<Reading> &rds, size_t n)
{
  print(log_debug, "Reading SelfMon info from system object ...", name().c_str());
  VzPicoSys * vpz = VzPicoSys::getInstance();

  float values[4];
  values[0] = vpz->getMemFree();
  values[1] = vpz->getMemTotal();

  bool isOnBattery;
  float voltage;
  time_t vTime = vpz->getVoltage(voltage, isOnBattery);

  if(vTime > 0)
  {
    values[2] = voltage;
    values[3] = (isOnBattery ? 1 : 0);
  }

  uint i = 0;
  for(; i < 4; i++)
  {
    if(i > 1 && vTime == 0)
    {
      break; // Don't have samples yet for this
    }

    print(log_debug, "Reading SelfMon metric[%d] %s: %f ...", name().c_str(), i, ids[i]->toString().c_str(), values[i]);

    rds[i].identifier(ids[i]);
    rds[i].time(); // use current timestamp
    rds[i].value(values[i]);
  }

  if(vTime > 0)
  {
    struct timeval vt;
    extern time_t sysRefTime;
    vt.tv_sec = vTime + sysRefTime;
    vt.tv_usec = 0;

    rds[2].time(vt);
    rds[3].time(vt);
  }

  return i;
}
