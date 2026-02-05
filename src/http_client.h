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
  struct Response {
    int status_code;
    std::string body;
    std::string error;

    bool is_success() const { return status_code >= 200 && status_code < 300; }
  };

  HttpClient();
  ~HttpClient();

  // Make a POST request
  Response post(const std::string& url, const std::string& path,
                const std::string& body,
                const std::map<std::string, std::string>& headers,
                int timeout_seconds = 30);

 private:
  // Helper to extract host from URL
  static bool parse_url(const std::string& url, std::string* scheme,
                        std::string* host, int* port);
};

}  // namespace vsql_ai

#endif  // VSQL_AI_HTTP_CLIENT_H
