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

#include <regex>

namespace vsql_ai {

HttpClient::HttpClient() {}

HttpClient::~HttpClient() {}

bool HttpClient::parse_url(const std::string& url, std::string* scheme,
                           std::string* host, int* port) {
  // Parse URL using regex: (https?)://([^:/]+)(?::(\d+))?
  std::regex url_regex(R"((https?)://([^:/]+)(?::(\d+))?)");
  std::smatch match;

  if (!std::regex_search(url, match, url_regex)) {
    return false;
  }

  *scheme = match[1].str();
  *host = match[2].str();

  if (match[3].matched) {
    *port = std::stoi(match[3].str());
  } else {
    *port = (*scheme == "https") ? 443 : 80;
  }

  return true;
}

HttpClient::Response HttpClient::post(
    const std::string& url, const std::string& path, const std::string& body,
    const std::map<std::string, std::string>& headers, int timeout_seconds) {
  Response response;
  response.status_code = 0;

  // Parse URL
  std::string scheme, host;
  int port;
  if (!parse_url(url, &scheme, &host, &port)) {
    response.error = "Invalid URL format";
    return response;
  }

  try {
    // Create HTTP client
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
      // Connection failed
      auto err = res.error();
      switch (err) {
        case httplib::Error::Connection:
          response.error = "Connection failed";
          break;
        case httplib::Error::BindIPAddress:
          response.error = "Failed to bind IP address";
          break;
        case httplib::Error::Read:
          response.error = "Read error";
          break;
        case httplib::Error::Write:
          response.error = "Write error";
          break;
        case httplib::Error::ExceedRedirectCount:
          response.error = "Too many redirects";
          break;
        case httplib::Error::Canceled:
          response.error = "Request canceled";
          break;
        case httplib::Error::SSLConnection:
          response.error = "SSL connection failed";
          break;
        case httplib::Error::SSLLoadingCerts:
          response.error = "Failed to load SSL certificates";
          break;
        case httplib::Error::SSLServerVerification:
          response.error = "SSL server verification failed";
          break;
        case httplib::Error::UnsupportedMultipartBoundaryChars:
          response.error = "Unsupported multipart boundary characters";
          break;
        case httplib::Error::Compression:
          response.error = "Compression error";
          break;
        default:
          response.error = "Unknown error";
          break;
      }
      return response;
    }

    // Success (HTTP response received, even if status >= 400)
    response.status_code = res->status;
    response.body = res->body;
    // Don't set error here - let the caller handle HTTP status codes
    // and parse the response body for detailed error messages

  } catch (const std::exception& e) {
    response.error = std::string("Exception: ") + e.what();
  }

  return response;
}

}  // namespace vsql_ai
