/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdbool.h"
#include <stdio.h>
#include <pico/stdlib.h>
#include "LocalGUI.h"
#include "VzPicoSys.h"

extern "C" {
#include "EPD_2in9b_V4.h"
#include "GUI_Paint.h"
}

LocalGUI * LocalGUI::getInstance()
{
  static LocalGUI * theInstance = NULL;
  if(theInstance == NULL)
  {
    theInstance = new LocalGUI();
  }
  return theInstance;
}

LocalGUI::LocalGUI() : BlackImage(NULL), RYImage(NULL), clockCanvas(NULL), refreshCount(0)
{
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0)? (EPD_2IN9B_V4_WIDTH / 8 ): (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  print(log_info, "Creating localGUI - imageSize: %d", "", Imagesize);
  if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL)
  {
    throw vz::VZException("Failed to allocate for black memory.");
  }
  if((RYImage = (UBYTE *)malloc(Imagesize)) == NULL)
  {
    throw vz::VZException("Failed to allocate for red memory.");
  }
  if((clockCanvas = (UBYTE *)malloc(16 * 125)) == NULL)
  {
    throw vz::VZException("Failed to allocate for clock canvas.");
  }
}

LocalGUI::~LocalGUI()
{
  free(BlackImage);
  free(RYImage);
  free(clockCanvas);
}

void LocalGUI::init()
{
  print(log_info, "Init local GUI device ...", "");
  if(DEV_Module_Init() != 0)
  {
    throw vz::VZException("Failed to init GUI device.");
  }
  EPD_2IN9B_V4_Init();
  print(log_debug, "Init GUI device complete. Clearing ...", "");
  EPD_2IN9B_V4_Clear();

  // 128 x 296
  print(log_debug, "Init GUI canvases (%d x %d)", "", EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);

  // Rotate canvas by 270 - then 0,0 is left top corner
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_Clear(WHITE);
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_NewImage(clockCanvas, 16, 125, 270, WHITE);
  Paint_Clear(WHITE);

  print(log_debug, "Init local GUI complete.", "");
}

void LocalGUI::showData()
{
//  print(log_info, "Updating GUI data - init.", "");
//  EPD_2IN9B_V4_Init();
  print(log_info, "Updating GUI data ...", "");

  VzPicoSys * vps = VzPicoSys::getInstance();
  const char * now = vps->getTimeString();

  if((refreshCount++ % 50) == 0)
  {
    Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
  
    Paint_DrawString_EN(5, 0, now, &Font12, WHITE, BLACK);
    Paint_DrawLine(5, 15, 290, 15, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    char str[128];
    float v; bool b;
    vps->getVoltage(v, b);
    sprintf(str, "Voltage: %.2fV (%s)", v, (b ? "Battery" : "USB"));
    Paint_DrawString_EN(5, 20, str, &Font12, WHITE, BLACK);

    long mu = vps->getMemUsed();
    long mt = vps->getMemTotal(); 
    long mup = mu * 100 / mt;
    sprintf(str, "Memory: %ldkB Used (%ld%% %ld Total)", mu, mup, mt);
    Paint_DrawString_EN(5, 30, str, &Font12, WHITE, BLACK);

    Paint_DrawLine(5, 118, 290, 118, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawString_EN(5, 120, VzPicoSys::getVersion(), &Font8, WHITE, BLACK);

    print(log_debug, "Full GUI refresh ...", "");
    EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  }
  else
  {
    Paint_NewImage(clockCanvas, 16, 125, 270, WHITE);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(2, 2, now, &Font12, WHITE, BLACK);
    Paint_DrawRectangle(0, 0, 125, 16, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    print(log_debug, "Partial GUI refresh ...", "");
    // OK: EPD_2IN9B_V4_Display_Partial(clockCanvas, 0, 0, 16, 125);
    // Weiter unten: EPD_2IN9B_V4_Display_Partial(clockCanvas, 24, 0, 40, 125);
    // Weiter links: EPD_2IN9B_V4_Display_Partial(clockCanvas, 0, 100, 16, 225);
    // Ganz links - offenbar ist hier 0,0 rechts oben:
    // params: xStart, yStart, xEnd, yEnd
    // dabei MUSS xEnd-xStart (dito y) exakt gleich der newImage Grösse sein
    EPD_2IN9B_V4_Display_Partial(clockCanvas, 0, 171, 16, 296);
  }
  print(log_debug, "GUI refresh done.", "");
//  EPD_2IN9B_V4_Sleep();
//  print(log_debug, "GUI sleeping ...", "");
}

