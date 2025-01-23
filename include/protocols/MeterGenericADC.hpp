/**
 * Generic Raspberry Pico ADC meter
 * External wiring not considered here - we just expect an analog value 0 .. 3.3V 
 *
 * @package vzlogger
 * @copyright Copyright (c) 2011, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Steffen Vogel <info@steffenvogel.de>
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

#ifndef _METER_GENERIC_ADC_H_
#define _METER_GENERIC_ADC_H_

#define ADC_BITS    12
#define ADC_COUNTS  (1<<ADC_BITS)

#ifndef PICO_ADC_SAMPLE_COUNT
# define PICO_ADC_SAMPLE_COUNT 5
#endif

#include <protocols/Protocol.hpp>

class MeterGenericADC : public vz::protocol::Protocol
{
  public:
    MeterGenericADC(std::list<Option> options);
    virtual ~MeterGenericADC();

    int open();
    int close();
    ssize_t read(std::vector<Reading> &rds, size_t n);

  private:
    uint  dataPin;  // Which ADC pin, starting with 0 from 26
    int   ctrlPin;  // Control PIN, to support things like:
                    // https://electronics.stackexchange.com/questions/39412/measure-lithium-ion-battery-voltage-thus-remaining-capacity
    float factor;   // Conversion factor

    NilIdentifier * id;
};

#endif /* _METER_GENERIC_ADC_H_ */
