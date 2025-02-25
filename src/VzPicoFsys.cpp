
#ifdef VZ_FSYS_FLASH
# include <hardware/flash.h>
# include "blockdevice/flash.h"
#endif
#ifdef VZ_FSYS_SD
# include "blockdevice/sd.h"
#endif
#ifdef VZ_FSYS_FAT
# include "filesystem/fat.h"
#endif
#ifdef VZ_FSYS_LITTLE
# include "filesystem/littlefs.h"
#endif

#include <hardware/clocks.h>
#include "VzPicoFsys.h"
#include "common.h"

VzPicoFsys::VzPicoFsys(VzPicoDiskDevice & device)
{
  dev = device.dev;
}
bool VzPicoFsys::mount(const char * dir, bool create)
{
  print(log_info, "Mounting FS on %s", "", dir);
  int err = fs_mount("/", fs, dev);
  if (err == -1 && create)
  {
    print(log_info, "Formatting FS ...", "");
    err = fs_format(fs, dev);
    if (err == -1)
    {
      print(log_error, "FS format error: %s", "", strerror(errno));
      return false;
    }
    err = fs_mount("/", fs, dev);
    if (err == -1)
    {
      print(log_error, "FS mount error: %s", "", strerror(errno));
      return false;
    }
  }
  return true;
}

#ifdef VZ_FSYS_FAT
VzPicoFsysFAT::VzPicoFsysFAT(VzPicoDiskDevice & device) : VzPicoFsys(device) { }
bool VzPicoFsysFAT::init()
{
  fs = filesystem_fat_create();
  if(! fs) { print(log_error, "Cound not create FAT filesystem: %s", "", strerror(errno)); }
  return (dev != NULL && fs != NULL);
}
#endif

#ifdef VZ_FSYS_LITTLE
VzPicoFsysLittle::VzPicoFsysLittle(VzPicoDiskDevice & device) : VzPicoFsys(device) { }
bool VzPicoFsysLittle::init()
{
  fs = filesystem_littlefs_create(500, 16);
  if(! fs) { print(log_error, "Cound not create LittleFS filesystem: %s", "", strerror(errno)); }
  return (dev != NULL && fs != NULL);
}
#endif

#ifdef VZ_FSYS_FLASH
VzPicoDiskDeviceFlash::VzPicoDiskDeviceFlash(uint s, uint so)
{
  uint size = (s == 0 ? PICO_FS_DEFAULT_SIZE : s);
  uint startOffset = (so == 0 ? (PICO_FLASH_SIZE_BYTES - size) : so);
  dev = blockdevice_flash_create(startOffset, size);
  if(! dev) { print(log_error, "Cound not create Flash disk device: %s", "", strerror(errno)); }
}
#endif

#ifdef VZ_FSYS_SD
VzPicoDiskDeviceSD::VzPicoDiskDeviceSD(uint spiNum, uint txPin, uint rxPin, uint sckPin, uint csnPin)
{
  dev = blockdevice_sd_create(spiNum == 0 ? spi0 : spi1, txPin, rxPin, sckPin, csnPin, 24 * MHZ, false);
  if(! dev) { print(log_error, "Cound not create SD card disk device: %s", "", strerror(errno)); }
}
#endif

