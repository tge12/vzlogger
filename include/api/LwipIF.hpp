/**
 * Header file for volkszaehler.org API calls
 *
 * @author Kai Krueger <kai.krueger@itwm.fraunhofer.de>
 * @copyright Copyright (c) 2011, The volkszaehler.org project
 * @package vzlogger
 * @license http://opensource.org/licenses/gpl-license.php GNU Public License
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

#ifndef _LwipIF_hpp_
#define _LwipIF_hpp_

#include <string>
#include <set>

namespace vz {
namespace api {

#define VZ_SRV_INIT 0
#define VZ_SRV_CONNECTING 1
#define VZ_SRV_READY 2
#define VZ_SRV_SENDING 3
#define VZ_SRV_ERROR 4
#define VZ_SRV_RETRY 5

class LwipIF {
  public:
    LwipIF(const char * hn, uint port, const char * apiId, uint timeout = 30);
    ~LwipIF();

    void addHeader(const std::string value);
    void clearHeader();
    void commitHeader();

    // Null values for connect can be used as reconnect, if already set
    void   connect();
    void   reconnect();
    uint   postRequest(const char * data, const char * url);
    uint   getPort();

    const char * getResponse();
    void   setResponse(const char * resp);

    uint   getState();
    void   setState(uint state);

    void               resetPCB();
    struct altcp_pcb * getPCB();
    void               initPCB();
    void               deletePCB();
    const char       * stateStr();
    time_t             getConnectInit();
    const char       * getId();

    static vz::api::LwipIF * getInstance(const std::string & url, uint timeout);

  private:
    std::string        id;
    std::set<std::string> headers;

    uint               state;
    int                error;
    uint               timeout;

    std::string        hostname;
    int                port;
    std::string        request;
    std::string        response;

    struct altcp_tls_config * tls_config;
    struct altcp_pcb        * pcb;

    time_t             connectInitiated; // May time out, so retry after a while

}; // class LwipIF

} // namespace api
} // namespace vz
#endif /* _LwipIF_hpp_ */
