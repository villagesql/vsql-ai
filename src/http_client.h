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

#ifndef VSQL_AI_HTTP_CLIENT_H
#define VSQL_AI_HTTP_CLIENT_H

#include <map>
#include <string>

namespace vsql_ai {

class HttpClient {
 public:
  // Exactly one of two outcomes holds:
  //   `error` non-empty - no HTTP exchange happened; `error` says why and
  //                       `status_code` is 0.
  //   `error` empty     - a response was received; `status_code` and `body`
  //                       are valid, and `error` stays empty even for 4xx/5xx
  //                       so the caller can parse the body for a provider
  //                       error message.
  struct Response {
    int status_code = 0;
    std::string body;
    std::string error;

    bool is_success() const { return status_code >= 200 && status_code < 300; }
  };

  // Make a POST request. Stateless — no instance required.
  static Response post(const std::string& url, const std::string& path,
                       const std::string& body,
                       const std::map<std::string, std::string>& headers,
                       int timeout_seconds);
};

}  // namespace vsql_ai

#endif  // VSQL_AI_HTTP_CLIENT_H
