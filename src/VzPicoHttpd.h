
#ifndef VZ_PICO_HTTPD_H
#define VZ_PICO_HTTPD_H

#include <common.h>
#include <api/LwipIF.hpp>
#include <MeterMap.hpp>

class VzPicoHttpd
{
  public:
    static VzPicoHttpd * getInstance(MapContainer * mc);
    ~VzPicoHttpd();

    void processRequest(const char * buf);

  private:
    VzPicoHttpd(MapContainer * mc);
    void              init();

    vz::api::LwipIF * lwip;
    std::string       data;
    MapContainer    * mappings;
};

#endif // VZ_PICO_HTTPD_H

