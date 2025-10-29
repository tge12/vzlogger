
#include <Config_Options.hpp>
#include "VzPicoHttpd.h"
#include "VzPicoLogger.h"
#include "MeterMap.hpp"

extern Config_Options options;

VzPicoHttpd * VzPicoHttpd::getInstance(MapContainer * mc)
{
  static VzPicoHttpd * theInstance = NULL;
  if(theInstance == NULL)
  {
    theInstance = new VzPicoHttpd(mc);
    theInstance->init();
  }
  return theInstance;
}

VzPicoHttpd::VzPicoHttpd(MapContainer * mc) : mappings(mc)
{
  lwip = new vz::api::LwipIF(options.port(), "http");
}

VzPicoHttpd::~VzPicoHttpd()
{
  delete lwip;
}

void VzPicoHttpd::init()
{
  lwip->initServer(this);
}

void VzPicoHttpd::processRequest(const char * req)
{
  VzPicoLogger::getInstance()->print(log_debug, "HTTP server received:\n***\n%s\n***", "http", req);
  // At the beginning, there must be something like this:
  //  GET /815e8820-bd87-11ef-992f-017e1a2d8ec8 ...
  // Extract the channel UUID:
  char uuid[37];
  bool showAll = false;
  if(sscanf(req, "GET /%36[0-9a-f-]", uuid) == 1)
  {
    VzPicoLogger::getInstance()->print(log_debug, "HTTP server channel UUID: %s", "http", uuid);
  }
  else if(options.channel_index() && (sscanf(req, "GET %1[/] ", uuid) == 1))
  {
    VzPicoLogger::getInstance()->print(log_debug, "HTTP server all channels", "http");
    showAll = true;
  }
  else
  {
    VzPicoLogger::getInstance()->print(log_error, "HTTP server invalid request data: %s", "http", req);
    return;
  }

  char respCode[32] = "404 Not Found";
  char respData[1024];
  sprintf(respData, "{ \"version\": \"%s\", \"generator\": \"%s\", \"data\": [", VERSION, PACKAGE);

  for (MapContainer::iterator mapping = mappings->begin(); mapping != mappings->end(); mapping++)
  {
    for (MeterMap::iterator ch = mapping->begin(); ch != mapping->end(); ch++)
    {
      if (! strcmp((*ch)->uuid(), uuid) || showAll)
      {
        strcpy(respCode, "200 OK");

        uint respLen = strlen(respData);
        int64_t ts = (*ch)->time_ms();
        double val = (*ch)->lastVal();
        snprintf(respData + respLen,  (1024 - respLen),
                 "{ \"uuid\": \"%s\", \"last\": %lld, \"interval\": %u, \"protocol\": \"%s\", \"tuples\": [ [ %lld, %.2f ] ] },",
                 (*ch)->uuid(), ts, mapping->meter()->interval(), meter_get_details(mapping->meter()->protocolId())->name, ts, val);
      }
    }
  }

  uint respLen = strlen(respData);
  snprintf(respData + respLen - 1,  (1024 - respLen), "]}");

  char buf[128];
  sprintf(buf, "HTTP/1.1 %s\r\n", respCode); data = buf;
  sprintf(buf, "Content-type: application/json\r\n"); data += buf;
  sprintf(buf, "Content-Length: %d\r\n", strlen(respData)); data += buf;
  data += "\r\n";
  data += respData;

  lwip->sendData(data);
}

