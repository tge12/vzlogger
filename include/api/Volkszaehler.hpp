/***********************************************************************/
/** @file Volkszaehler.hpp
 * Header file for volkszaehler.org API calls
 *
 *  @author Kai Krueger
 *  @date   2012-03-15
 *  @email  kai.krueger@itwm.fraunhofer.de
 * @copyright Copyright (c) 2011 - 2023, The volkszaehler.org project
 * @package vzlogger
 * @license http://opensource.org/licenses/gpl-license.php GNU Public License
 *
 * (C) Fraunhofer ITWM
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

#ifndef _Volkszaehler_hpp_
#define _Volkszaehler_hpp_

#ifdef VZ_PICO
# include "LwipIF.hpp"
#else // VZ_PICO
# include <curl/curl.h>
#endif // VZ_PICO

#include <json-c/json.h>
#include <stdint.h>

#include "Buffer.hpp"
#include <ApiIF.hpp>
#include <Options.hpp>

namespace vz {
namespace api {

typedef struct {
	char *data;
	size_t size;
} CURLresponse;

#ifndef VZ_PICO
typedef struct {
	CURL *curl;
	struct curl_slist *headers;
} api_handle_t;
#endif // VZ_PICO

class Volkszaehler : public ApiIF {
  public:
	typedef vz::shared_ptr<ApiIF> Ptr;

	Volkszaehler(Channel::Ptr ch, std::list<Option> options);
	~Volkszaehler();

	void send();
#ifdef VZ_PICO // Otherwise use base-class method
        bool isBusy() const;
#endif // VZ_PICO

	void register_device();

	const std::string middleware() const { return _middleware; }

  private:
	std::string _middleware;
	unsigned int _curlTimeout;
	std::string _url;

	/**
	 * Create JSON object of tuples
	 *
	 * @param buf	the buffer our readings are stored in (required for mutex)
	 * @return the json_object (has to be free'd)
	 */
	json_object *api_json_tuples(Buffer::Ptr buf);

        std::string  outputData;
	CURLresponse response;
        std::string  errMsg;

	/**
	 * Parses JSON encoded exception and stores describtion in err
	 */
	friend class Volkszaehler_Test;
	void api_parse_exception(CURLresponse response, char *err, size_t n);

  private:
#ifdef VZ_PICO
	vz::api::LwipIF * _api;
#else // VZ_PICO
	api_handle_t _api;
#endif // VZ_PICO

	// Volatil
	std::list<Reading> _values;
	int64_t _last_timestamp; /**< remember last timestamp */
	// duplicate support:
	Reading *_lastReadingSent;

}; // class Volkszaehler

#ifdef VZ_PICO
#else // VZ_PICO
/**
 * Reformat CURLs debugging output
 */
int curl_custom_debug_callback(CURL *curl, curl_infotype type, char *data, size_t size,
							   void *custom);

size_t curl_custom_write_callback(void *ptr, size_t size, size_t nmemb, void *data);
#endif // VZ_PICO

} // namespace api
} // namespace vz
#endif /* _Volkszaehler_hpp_ */
