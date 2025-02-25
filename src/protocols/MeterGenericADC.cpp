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

#include "Options.hpp"
#include "protocols/MeterGenericADC.hpp"
#include <VZException.hpp>

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#define ADC_FIRST 26 // GPIO pin 26

MeterGenericADC::MeterGenericADC(std::list<Option> options) : Protocol("genericAdc")
{
  OptionList optlist;
  const char * optName;
  try
  {
    optName = "adcNum"; adcNum = optlist.lookup_int(options, optName);
    optName = "factor"; factor = optlist.lookup_double(options, optName);
  }
  catch (vz::VZException &e)
  {
    print(log_alert, "Missing '%s' or invalid type", name().c_str(), optName);
    throw;
  }

  ctrlPin = -1;
  try
  {
    // Optional
    optName = "ctrlPin"; ctrlPin = optlist.lookup_int(options, optName);
  }
  catch (vz::VZException &e) { }

  id = ReadingIdentifier::Ptr(new NilIdentifier());

  print(log_debug, "Created MeterGenericADC (GPIO %d, ADC %d)", "", ADC_FIRST + adcNum, adcNum);
}

MeterGenericADC::~MeterGenericADC() {}

int MeterGenericADC::open()
{
  uint dataPin = ADC_FIRST + adcNum;
  adc_gpio_init(dataPin);
  if(ctrlPin >= 0)
  {
    gpio_init(ctrlPin);
    gpio_set_dir(ctrlPin, GPIO_OUT);
  }
  return SUCCESS;
}

int MeterGenericADC::close() { return SUCCESS; }

ssize_t MeterGenericADC::read(std::vector<Reading> &rds, size_t n)
{
  uint dataPin = ADC_FIRST + adcNum;
  print(log_debug, "Reading MeterGenericADC at GPIO %d (ADC %d)", "", dataPin, adcNum);

  if(ctrlPin >= 0)
  {
    print(log_debug, "Enabling control PIN %d", "", ctrlPin);
    gpio_put(ctrlPin, 1);
    sleep_us(10);
  }

  // The ADC returns something between 0 .. 4096, which means 0 .. 3.3V
  // However, we have no idea what that means in terms of the real data source - apply the conversion factor
  adc_select_input(dataPin);

  uint adcVal = 0;
  for(int i = 0; i < PICO_ADC_SAMPLE_COUNT; i++)
  {
    adcVal += adc_read();
  }
  adcVal /= PICO_ADC_SAMPLE_COUNT;

  if(ctrlPin >= 0)
  {
    print(log_debug, "Disabling control PIN %d", "", ctrlPin);
    gpio_put(ctrlPin, 0);
  }

  float val = adcVal * factor * (3.3 / (ADC_COUNTS));
  print(log_debug, "MeterGenericADC::read: ADC %d -> %.2f", "", adcVal, val);

  rds[0].value(val);
  rds[0].identifier(id);
  rds[0].time(); // use current timestamp

  return 1;
}

