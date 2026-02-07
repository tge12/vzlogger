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

#include <string.h>
#include <time.h>
#include <map>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#include <VZException.hpp>
#include <VzPicoLogger.h>
#include <VzPicoHttpd.h>
#include <api/LwipIF.hpp>

static void  vz_altcp_dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg);
static err_t vz_altcp_connected(void *arg, struct altcp_pcb *pcb, err_t err);
static err_t vz_altcp_poll(void *arg, struct altcp_pcb *pcb);
static void  vz_altcp_err(void *arg, err_t err);
static err_t vz_altcp_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
static err_t vz_altcp_sent(void *arg, struct altcp_pcb *pcb, u16_t len);
static err_t vz_altcp_accept(void * arg, struct altcp_pcb * client_pcb, err_t err);

static const char * logMsgId = "lwip";

vz::api::LwipIF::LwipIF(const char * hn, uint p, const char * apiId, uint t) :
  id(apiId), pcb(NULL), tls_config(NULL), timeout(t), state(VZ_SRV_INIT), connectInitiated(0), sendInitiated(0),
  hostname(hn), port(p), serverPcb(NULL)
{
  print(log_debug, "Creating LwipIF instance (timeout: %d)...", id.c_str(), timeout);
  this->initPCB();
  print(log_debug, "Created LwipIF instance (%x).", id.c_str(), pcb);
}

vz::api::LwipIF::LwipIF(uint p, const char * apiId) :
  id(apiId), pcb(NULL), tls_config(NULL), state(VZ_SRV_SERVER),
  port(p), serverPcb(NULL)
{
  print(log_info, "Creating LwipIF server instance at port %u ...", id.c_str(), p);
}

vz::api::LwipIF::~LwipIF()
{
  print(log_debug, "Destroying LwipIF instance (%x).", id.c_str(), pcb);
  this->deletePCB();

  if(serverPcb != NULL)
  {
    print(log_debug, "Deleting server PCB (%x).", id.c_str(), serverPcb);
    altcp_arg(serverPcb, NULL);
    altcp_close(serverPcb);
    serverPcb = NULL;
  }

}

void vz::api::LwipIF::initPCB(struct altcp_pcb * setPcb)
{
  if(pcb != NULL && setPcb != NULL)
  {
    // This may happen, if the HTTP server has accepted a new client connection.
    // TODO Actually that means, there can be only one client connection at a time - the "setPcb" will replace the
    // previous one below ... problem?
    this->deletePCB();
  }

  if(pcb == NULL)
  {
    if(setPcb != NULL)
    {
      print(log_debug, "Setting PCB %x ...", id.c_str(), setPcb);
      pcb = setPcb;
    }
    else
    {
      print(log_debug, "Initializing PCB ...", id.c_str());
#ifdef VZ_USE_HTTPS
      /* No CA certificate checking */
      tls_config = altcp_tls_create_config_client(NULL, 0);
      // tls_config = altcp_tls_create_config_client(cert, cert_len);
      assert(tls_config);

      //mbedtls_ssl_conf_authmode(&tls_config->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

      pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
#else // VZ_USE_HTTPS
      pcb = altcp_new_ip_type(NULL, IPADDR_TYPE_ANY);
#endif // VZ_USE_HTTPS
      if(!pcb)
      {
        throw vz::VZException("LwipIF: failed to create pcb");
      }
    }

    altcp_arg(pcb, this);
    altcp_poll(pcb, vz_altcp_poll, timeout * 2);
    altcp_recv(pcb, vz_altcp_recv);
    altcp_err(pcb, vz_altcp_err);
    altcp_sent(pcb, vz_altcp_sent);
    print(log_debug, "Initialized PCB.", id.c_str());
  }
}

int vz::api::LwipIF::deletePCB()
{
  err_t err = ERR_OK;
  if(pcb != NULL)
  {
    VzPicoLogger::getInstance()->print(log_debug, "Destroying PCB ...", id.c_str());
    altcp_arg(pcb, NULL);
    altcp_poll(pcb, NULL, 0);
    altcp_recv(pcb, NULL);
    altcp_err(pcb, NULL);
    altcp_sent(pcb, NULL);
    err = altcp_close(pcb);
    if (err != ERR_OK)
    {
      VzPicoLogger::getInstance()->print(log_error, "Close failed %d, calling abort\n", id.c_str(), err);
      altcp_abort(pcb);
      err = ERR_ABRT;
    }
    pcb = NULL;

#ifdef VZ_USE_HTTPS
    if(tls_config != NULL)
    {
      altcp_tls_free_config(tls_config);
    }
#endif // VZ_USE_HTTPS
  }

  return err;
}

void vz::api::LwipIF::addHeader(const std::string value) { headers.insert(value.c_str()); }
void vz::api::LwipIF::clearHeader() { headers.clear(); }
void vz::api::LwipIF::commitHeader() { /* no-op */ }
uint vz::api::LwipIF::getPort() { return port; }
const char * vz::api::LwipIF::getId() { return id.c_str(); }
const char * vz::api::LwipIF::getData() { return data.c_str(); }
void vz::api::LwipIF::setData(const char * buf) { data = buf; }

uint vz::api::LwipIF::getState() { return state; }
void vz::api::LwipIF::setState(uint state) { this->state = state; }
const char * vz::api::LwipIF::stateStr()
{
  static const char * stateStrings[] = { "initial", "DNS", "connecting", "ready", "sending", "error", "7", "8", "9", "server" };
  return stateStrings[state];
}
time_t vz::api::LwipIF::getConnectInit() { return connectInitiated; }
time_t vz::api::LwipIF::getSendInit()    { return sendInitiated; }

// ==============================

void vz::api::LwipIF::connect()
{
  print(log_info, "Connecting %s:%d ...", id.c_str(), hostname.c_str(), port);

  // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
  // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
  // these calls are a no-op and can be omitted, but it is a good practice to use them in
  // case you switch the cyw43_arch type later.
  cyw43_arch_lwip_begin();

  this->initPCB();

#ifdef VZ_USE_HTTPS
  /* Set SNI */
  mbedtls_ssl_set_hostname((mbedtls_ssl_context *) altcp_tls_context(pcb), hostname.c_str());
#endif // VZ_USE_HTTPS

  print(log_debug, "Resolving '%s'", id.c_str(), hostname.c_str());
  state = VZ_SRV_DNS;

  ip_addr_t server_ip;
  err_t err = dns_gethostbyname(hostname.c_str(), &server_ip, vz_altcp_dns_found, this);
  if (err == ERR_OK)
  {
    vz_altcp_dns_found(hostname.c_str(), &server_ip, this);
    connectInitiated = time(NULL);
    return;
  }
  else if(err != ERR_INPROGRESS)
  {
    cyw43_arch_lwip_end();
    print(log_error, "Error initiating DNS resolving: %d", id.c_str(), err);
    // Error - just try again ...
    state = VZ_SRV_INIT;
    return;
  }

  cyw43_arch_lwip_end();
  print(log_debug, "DNS resolution initiated.", id.c_str());

  while(state == VZ_SRV_DNS)
  {
    sleep_ms(1000);
  }
  connectInitiated = time(NULL);
  print(log_debug, "Connecting (state: %d).", id.c_str(), state);
}

struct altcp_pcb * vz::api::LwipIF::getPCB() { return pcb; }
void vz::api::LwipIF::resetPCB() { pcb = NULL; }

// ==============================

const char * vz::api::LwipIF::getChannel() const { return channel.c_str(); }
uint vz::api::LwipIF::postRequest(const char * ch, const char * data, const char * url)
{
  if(state != VZ_SRV_READY)
  {
    print(log_debug, "Not sending request - state: %d", id.c_str(), state);
    return state;
  }

  channel = ch;

  print(log_debug, "postRequest req buffer: %d (%p)", id.c_str(), request.capacity(), request.c_str());
  char buf[128];
  sprintf(buf, "POST %s HTTP/1.1\r\n", url); request = buf;
  sprintf(buf, "Host: %s:%d\r\n", hostname.c_str(), port); request += buf;
//  request += "Connection: close\r\n";
  std::set<std::string>::iterator it;
  for (it = headers.begin(); it != headers.end(); it++)
  {
    request += (*it).c_str();
    request += "\r\n";
  }
  sprintf(buf, "Content-Length: %d\r\n", strlen(data)); request += buf;
  request += "\r\n";
  request += data;

  print(log_debug, "Sending request (%d bytes): %s", id.c_str(), request.length(), request.c_str());
  err_t err = this->sendData(request);

  switch(err)
  {
    case ERR_MEM:
      // See: https://www.nongnu.org/lwip/2_1_x/group__tcp__raw.html#ga6b2aa0efbf10e254930332b7c89cd8c5
      // "The proper way to use this function is to call the function with at most tcp_sndbuf() bytes of data.
      // If the function returns ERR_MEM, the application should wait until some of the currently enqueued data
      // has been successfully received by the other host and try again."

      print(log_warning, "Error sending request: %d -> state RETRY (%d)", id.c_str(), err, altcp_sndbuf(pcb));
      state = VZ_SRV_RETRY;
      break;
    case ERR_OK:
      // State already set to SENDING above - must be done up there, because sometimes the response arrives so fast
      // that the recv already set the state before this point here ...
      print(log_debug, "Sending request complete", id.c_str());
      sendInitiated = time(NULL);
      break;
    default:
      // Error - just try again ...
      print(log_error, "Error sending request: %d", id.c_str(), err);
      state = VZ_SRV_INIT;
  }

  return state;
}

// ==============================

uint vz::api::LwipIF::sendData(const std::string & req)
{
  cyw43_arch_lwip_begin();
  err_t err = altcp_write(pcb, req.c_str(), strlen(req.c_str()), TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK)
  {
    // SERVER remains SERVER
    if(state != VZ_SRV_SERVER)
    {
      state = VZ_SRV_SENDING;
    }
    print(log_debug, "Flushing Lwip output ...", id.c_str());
    err = altcp_output(pcb);
  }
  cyw43_arch_lwip_end();
  return err;
}

// ==============================

static void vz_altcp_dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  if (ipaddr)
  {
    VzPicoLogger::getInstance()->print(log_debug, "DNS resolution complete: %s -> %s", ai->getId(), hostname, ipaddr_ntoa(ipaddr));
    VzPicoLogger::getInstance()->print(log_debug, "Connecting to %s:%d", ai->getId(), hostname, ai->getPort());
    ai->setState(VZ_SRV_CONNECTING);
    err_t err = altcp_connect(ai->getPCB(), ipaddr, ai->getPort(), vz_altcp_connected);
    if (err != ERR_OK)
    {
      // Error - just try again ...
      VzPicoLogger::getInstance()->print(log_error, "Error initiating connect: %d", ai->getId(), err);
      ai->setState(VZ_SRV_INIT);
    }
    else
    {
      VzPicoLogger::getInstance()->print(log_debug, "Connection initiated", ai->getId());
    }
    return;
  }

  // Error - just try again ...
  VzPicoLogger::getInstance()->print(log_error, "Error resolving hostname %s", ai->getId(), hostname);
  ai->setState(VZ_SRV_INIT);
}

// ==============================

static err_t vz_altcp_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  if (err != ERR_OK)
  {
    // Error - just try again ...
    VzPicoLogger::getInstance()->print(log_error, "Connect failed: %d", ai->getId(), err);
    ai->setState(VZ_SRV_INIT);
    return err;
  }
  VzPicoLogger::getInstance()->print(log_info, "Server connected", ai->getId());
  ai->setState(VZ_SRV_READY);
  return ERR_OK;
}

// ==============================

static err_t vz_altcp_poll(void *arg, struct altcp_pcb *pcb)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  if(ai->getState() == VZ_SRV_CONNECTING || ai->getState() == VZ_SRV_SENDING)
  {
    VzPicoLogger::getInstance()->print(log_debug, "Timed out", ai->getId());
    // Maybe just keep trying ... ?? Reconnect?
    ai->setState(VZ_SRV_INIT);
  }
  return ERR_OK;
}

// ==============================

static err_t vz_altcp_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  VzPicoLogger::getInstance()->print(log_debug, "Sent %d bytes", ai->getId(), len);
  return ERR_OK;
}

// ==============================

static void vz_altcp_err(void *arg, err_t err)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;

  // Keep trying ... reconnect
  ai->setState(VZ_SRV_INIT);
  VzPicoLogger::getInstance()->print(log_error, "LwipIF: vz_altcp_err: %d", ai->getId(), err);

  // Doc says: "The corresponding pcb is already freed when this callback is called!"
  // So we must not do this again, as it normally happens when reconnecting
  ai->resetPCB();
}

// ==============================

static err_t vz_altcp_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  if (!p)
  {
    VzPicoLogger::getInstance()->print(log_debug, "Connection closed by peer: %d", ai->getId(), err);
    err_t err = ai->deletePCB();
    ai->setState(VZ_SRV_INIT);
    return err;
  }

  VzPicoLogger::getInstance()->print(log_debug, "Receiving data (%d): %d", ai->getId(), p->tot_len, err);

  if (p->tot_len > 0)
  {
    /* For simplicity this examples creates a buffer on stack the size of the data pending here, 
       and copies all the data to it in one go.
       Do be aware that the amount of data can potentially be a bit large (TLS record size can be 16 KB),
       so you may want to use a smaller fixed size buffer and copy the data to it using a loop, if memory is a concern */

    char buf[p->tot_len + 1];
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    buf[p->tot_len] = 0;

    ai->setData(buf);
    VzPicoLogger::getInstance()->print(log_debug, "New data received from server (%d bytes):\n***\n%s\n***", ai->getId(), p->tot_len, buf);
    altcp_recved(pcb, p->tot_len);

    if(ai->getState() == VZ_SRV_SERVER)
    {
      // This is a HTTP server ... see what the client wants:
      ai->serverRequest(buf);
      pbuf_free(p);
      return ERR_OK;
    }
  }

  pbuf_free(p);
  ai->setState(VZ_SRV_REPLIED);
  return ERR_OK;
}

void vz::api::LwipIF::serverRequest(const char * buf)
{
  httpServer->processRequest(buf);
}

// ==============================

void vz::api::LwipIF::reconnect()
{
  print(log_debug, "Reconnecting %s:%d ...", id.c_str(), hostname.c_str(), port);

  cyw43_arch_lwip_begin();
  this->deletePCB();
  cyw43_arch_lwip_end();

  this->connect();
}

// ==============================

vz::api::LwipIF * vz::api::LwipIF::getInstance(const std::string & url, uint t)
{
  static std::map<std::string, vz::api::LwipIF *> connections;

  print(log_debug, "Getting Lwip instance for %s ...", logMsgId, url.c_str());

  uint numConn = connections.size();
  vz::api::LwipIF * ai = connections[url];
  if(ai)
  {
    print(log_debug, "Found Lwip instance for %s at %p (%d).", logMsgId, url.c_str(), ai, numConn);
  }
  else
  {
    char apiId[10];
    sprintf(apiId, "lwi%d", numConn);

    char prot[10], h[128];
    uint p;
    if(sscanf(url.c_str(), "%[^:]://%[^:]:%d", prot, h, &p) == 3)
    {
      ai = new vz::api::LwipIF(h, p, apiId, t);
    }
    else if(sscanf(url.c_str(), "%[^:]://%[^:]", prot, h) == 2)
    {
      throw vz::VZException("VZ-API: Cannot parse URL %s - need port.", url.c_str());
    }
    else
    {
      throw vz::VZException("VZ-API: Cannot parse URL %s.", url.c_str());
    }

    connections[url] = ai;
    print(log_debug, "Created new Lwip instance for %s at %p (%d).", logMsgId, url.c_str(), ai, numConn);
  }

  return ai;
}

// ==============================

void vz::api::LwipIF::initServer(VzPicoHttpd * httpd)
{
  cyw43_arch_lwip_begin();

  print(log_info, "Init HTTP Server at port %u ...", logMsgId, port);
  struct altcp_pcb * bindPcb = altcp_new_ip_type(NULL, IPADDR_TYPE_ANY); 
  err_t err = altcp_bind(bindPcb, IP_ANY_TYPE, port);
  if(err)
  {
    cyw43_arch_lwip_end();
    throw vz::VZException("LwipIF: failed to bind to server port");
  }

  print(log_debug, "HTTP Server listening.", logMsgId);
  serverPcb = altcp_listen_with_backlog(bindPcb, 1);
  if(! serverPcb)
  {
    if(bindPcb)
    {
      altcp_close(bindPcb);
    }
    cyw43_arch_lwip_end();
    throw vz::VZException("LwipIF: failed to listen on server port");
  }

  httpServer = httpd;

  altcp_arg(serverPcb, this);
  altcp_accept(serverPcb, vz_altcp_accept);
  print(log_info, "HTTP Server accepting connections ...", logMsgId);

  cyw43_arch_lwip_end();
}

static err_t vz_altcp_accept(void * arg, struct altcp_pcb * client_pcb, err_t err)
{
  vz::api::LwipIF * ai = (vz::api::LwipIF *) arg;
  if (err != ERR_OK || client_pcb == NULL)
  {
    VzPicoLogger::getInstance()->print(log_error, "Failure in accept: %d", ai->getId(), err);
    return ai->deletePCB();
  }
  VzPicoLogger::getInstance()->print(log_debug, "Client connected.", ai->getId());

  ai->initPCB(client_pcb);
  ai->setState(VZ_SRV_SERVER);
  return ERR_OK;
}
