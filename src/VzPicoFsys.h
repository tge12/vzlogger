
#ifndef VZ_PICO_FSYS_H
#define VZ_PICO_FSYS_H

#include <hardware/spi.h>
#include "filesystem/vfs.h"

class VzPicoDiskDevice;
class VzPicoFsys
{
  public:
    VzPicoFsys(VzPicoDiskDevice & device);
    virtual bool init() = 0;
    bool mount(const char * dir, bool create = true);

  protected:
    blockdevice_t * dev;
    filesystem_t * fs;
};

class VzPicoDiskDevice
{
  protected:
    blockdevice_t * dev;

  friend class VzPicoFsys;
};

#ifdef VZ_FSYS_FAT
class VzPicoFsysFAT : public VzPicoFsys
{
  public:
    VzPicoFsysFAT(VzPicoDiskDevice & device);
    virtual bool init();
};
#endif

#ifdef VZ_FSYS_LITTLE
class VzPicoFsysLittle : public VzPicoFsys
{
  public:
    VzPicoFsysLittle(VzPicoDiskDevice & device);
    virtual bool init();
};
#endif

#ifdef VZ_FSYS_FLASH
class VzPicoDiskDeviceFlash : public VzPicoDiskDevice
{
  public:
    VzPicoDiskDeviceFlash(uint size = PICO_FS_DEFAULT_SIZE, uint startOffset = 0);
};
#endif

#ifdef VZ_FSYS_SD
class VzPicoDiskDeviceSD : public VzPicoDiskDevice
{
  public:
    VzPicoDiskDeviceSD(uint spiNum = 0, uint txPin = PICO_DEFAULT_SPI_TX_PIN, uint rxPin = PICO_DEFAULT_SPI_RX_PIN,
                       uint sckPin = PICO_DEFAULT_SPI_SCK_PIN, uint csnPin = PICO_DEFAULT_SPI_CSN_PIN);
};
#endif

#endif // VZ_PICO_FSYS_H

