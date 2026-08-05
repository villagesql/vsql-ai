/* Copyright (c) 2025 VillageSQL Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "http_client.h"

namespace vsql_ai {

HttpClient::Response HttpClient::post(
    const std::string& url, const std::string& path, const std::string& body,
    const std::map<std::string, std::string>& headers, int timeout_seconds) {
  Response response;

  try {
    // Create HTTP client. httplib parses the URL itself; there is no need to
    // split out scheme/host/port here.
    httplib::Client cli(url);

    // Set timeouts
    cli.set_connection_timeout(timeout_seconds);
    cli.set_read_timeout(timeout_seconds);
    cli.set_write_timeout(timeout_seconds);

    // Build headers
    httplib::Headers http_headers;
    for (const auto& header : headers) {
      http_headers.insert({header.first, header.second});
    }

    // Make POST request
    auto res = cli.Post(path, http_headers, body, "application/json");

    if (!res) {
      // No HTTP exchange took place. Use httplib's own mapping so newer error
      // kinds (notably Timeout and ConnectionTimeout, the likely outcome on
      // the 300-second local path) are reported instead of "Unknown error".
      response.error = httplib::to_string(res.error());
      return response;
    }

    // Success (HTTP response received, even if status >= 400)
    response.status_code = res->status;
    response.body = std::move(res->body);
    // Don't set error here - let the caller handle HTTP status codes
    // and parse the response body for detailed error messages

  } catch (const std::exception& e) {
    response.error = std::string("Exception: ") + e.what();
  }

  return response;
}

}  // namespace vsql_ai
