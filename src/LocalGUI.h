/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LOCAL_GUI_H
#define LOCAL_GUI_H

#include <common.h>

class LocalGUI
{
  public:
    static LocalGUI * getInstance();
    ~LocalGUI();

    void init();
    void showData();

  private:
    LocalGUI();

    uint8_t * BlackImage;
    uint8_t * RYImage; // Red or Yellow
    uint8_t * clockCanvas;
    uint      refreshCount;
};

#endif // LOCAL_GUI_H

