/***********************************************************************/
/** @file LocalGUI.hpp
 * Header file for local GUI API
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

#ifndef _LocalGUI_hpp_
#define _LocalGUI_hpp_

#include <ApiIF.hpp>
#include <Options.hpp>
#include <map>

namespace vz {
namespace api {

class LocalGUI : public ApiIF
{
  public:
    typedef vz::shared_ptr<ApiIF> Ptr;
    LocalGUI(Channel::Ptr ch, std::list<Option> options);
    ~LocalGUI();
    void send();
    void register_device();

  private:
    std::string format;
};

class LocalGUIDisplay
{
  public:
    static LocalGUIDisplay * getInstance();
    ~LocalGUIDisplay();
    void init();
    void putLine(const char * format, double val);
    void showLines();

  private:
    LocalGUIDisplay();

    std::map<std::string, double> lines;

    uint8_t * BlackImage;
    uint8_t * RYImage; // Red or Yellow
    uint8_t * contentCanvas;
    uint      refreshCount;
    bool      initialized;
    time_t    lastUpdate;
    uint      prevNumLines;
    bool      sleeping;
};

} // namespace api
} // namespace vz
#endif // _LocalGUI_hpp_
