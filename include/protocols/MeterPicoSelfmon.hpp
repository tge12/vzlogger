/**
 * Raspberry Pico self monitoring
 **/

#ifndef _meterpicoselfmon_hpp_
#define _meterpicoselfmon_hpp_

#include <protocols/Protocol.hpp>
#include <vector>

class MeterPicoSelfmon : public vz::protocol::Protocol {
  public:
    MeterPicoSelfmon(const std::list<Option> &options);
    virtual ~MeterPicoSelfmon();

    virtual int open();
    virtual int close();
    virtual ssize_t read(std::vector<Reading> &rds, size_t n);

  private:
    ReadingIdentifier::Ptr ids[4];
};

#endif
