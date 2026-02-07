
class Ntp
{
  public:
    Ntp(const char * srv = NULL);  // If NULL, use DNS server
    ~Ntp();

    time_t queryTime();
    void request();
    void result(int status, time_t *result);
    void setServerAddress(const ip_addr_t * ipaddr);
    const ip_addr_t * getServerAddress();

  private:
    std::string      server;
    ip_addr_t        ntp_server_address;
    bool             request_sent;
    struct udp_pcb * ntp_pcb;
    alarm_id_t       ntp_resend_alarm;
    time_t           theTime;
};

