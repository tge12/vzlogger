/***********************************************************************/
/** @file LocalGUI.cpp
 * Header file for LocalGUI API
 *
 * @copyright Copyright (c) 2014 - 2023, The volkszaehler.org project
 * @package vzlogger
 * @license http://opensource.org/licenses/gpl-license.php GNU Public License
 **/
/*---------------------------------------------------------------------*/

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

#include "Config_Options.hpp"
#include <api/LocalGUI.hpp>
#include <VzPicoSys.h>

// This assumes a ePaper display - nothing else supported yet
extern "C" {
#include "EPD_2in9b_V4.h"
#include "GUI_Paint.h"
}

extern Config_Options options;

vz::api::LocalGUI::LocalGUI(Channel::Ptr ch, std::list<Option> pOptions) : ApiIF(ch)
{
  OptionList optlist;
  print(log_debug, "Creating LocalGUI API instance", ch->name());

  // parse options
  const char * optName;
  try
  {
    optName = "format"; format = optlist.lookup_string(pOptions, optName);
  }
  catch (vz::VZException &e)
  {
    print(log_alert, "Missing '%s' or invalid type", "", optName);
    throw;
  }
}

vz::api::LocalGUI::~LocalGUI() { }

void vz::api::LocalGUI::send()
{
  print(log_debug, "Getting LocalGUI API impl instance", channel()->name());
  LocalGUIDisplay * display = vz::api::LocalGUIDisplay::getInstance();
  display->init();

  uint numTuples = channel()->size();
  print(log_debug, "Num tuples to display: %d", channel()->name(), numTuples);
  if(numTuples == 0)
  {
    // No data
    return;
  }

  double val;
  Buffer::Ptr buf = channel()->buffer();
  buf->lock();
  for (Buffer::iterator it = buf->begin(); it != buf->end(); it++)
  {
    const Reading &r = *it;
    val = r.value();
    it->mark_delete();
  }
  buf->unlock();
  buf->clean();

  print(log_debug, "Putting data to LocalGUI: Format: '%s', Val: '%.2f'", channel()->name(), format.c_str(), val);
  display->putLine(format.c_str(), val);
  print(log_debug, "Updating LocalGUI ...", channel()->name());
  display->showLines();
}

void vz::api::LocalGUI::register_device() {}

vz::api::LocalGUIDisplay * vz::api::LocalGUIDisplay::getInstance()
{
  static LocalGUIDisplay * theInstance = NULL;
  if(theInstance == NULL)
  {
    theInstance = new LocalGUIDisplay();
    print(log_debug, "Created LocalGUI instance at %p.", "", theInstance);
  }
  return theInstance;
}

vz::api::LocalGUIDisplay::LocalGUIDisplay() : initialized(false), refreshCount(0)
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
  if((contentCanvas = (UBYTE *)malloc(Imagesize)) == NULL)
  {
    throw vz::VZException("Failed to allocate for content canvas.");
  }
}

vz::api::LocalGUIDisplay::~LocalGUIDisplay()
{
  free(BlackImage);
  free(RYImage);
  free(contentCanvas);
}

void vz::api::LocalGUIDisplay::putLine(const char * format, double val)
{
  lines[format] = val;
}

void vz::api::LocalGUIDisplay::init()
{
  if(initialized)
  {
    return;
  }

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
  // All 3 canvases have the same size
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_Clear(WHITE);
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_SelectImage(contentCanvas);
  Paint_Clear(WHITE);

  initialized = true;
  print(log_debug, "Init local GUI complete.", "");
}

void vz::api::LocalGUIDisplay::showLines()
{
  print(log_info, "Updating GUI data (refresh cnt: %d) ...", "", refreshCount);

  VzPicoSys * vps = VzPicoSys::getInstance();
  const char * now = vps->getTimeString();

  if((refreshCount % 50) == 0)
  {
    Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
  }
  else
  {
    Paint_NewImage(contentCanvas, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);
  }

  Paint_DrawString_EN(5, 0, now, &Font12, WHITE, BLACK);
  Paint_DrawLine(5, 15, 290, 15, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

  print(log_debug, "Generating %d GUI data lines ...", "", lines.size());
  uint yPos = 20;
  std::map<std::string, double>::iterator it;
  for (it = lines.begin(); it != lines.end(); it++)
  {
    char str[64];
    sprintf(str, (it->first).c_str(), it->second);
    print(log_debug, "Generated GUI data line: '%s' (yPos: %d).", "", str, yPos);
    Paint_DrawString_EN(5, yPos, str, &Font12, WHITE, BLACK);
    yPos += 10;
  }

  Paint_DrawLine(5, 118, 290, 118, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawString_EN(5, 120, VzPicoSys::getVersion(), &Font8, WHITE, BLACK);

  if((refreshCount++ % 50) == 0)
  {
    print(log_debug, "Full GUI refresh ...", "");
    EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  }
  else
  {
    print(log_debug, "Partial GUI refresh ...", "");
    EPD_2IN9B_V4_Display_Partial(contentCanvas, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  }
  print(log_debug, "GUI refresh done.", "");
}
