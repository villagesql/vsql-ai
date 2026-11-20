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

#include "raw_request.h"

#include <map>
#include <string>

#include "ai_providers.h"
#include "decisions.h"
#include "http_client.h"

namespace vsql_ai {
namespace raw_request {

bool prepare(const std::string& body_text, const std::string& model,
             ModelPlacement placement, bool stream_default_false, json* body,
             std::string* error) {
  *body = json::parse(body_text, nullptr, false);
  if (body->is_discarded() || !body->is_object()) {
    *error = "Request body must be a JSON object";
    return false;
  }

  auto given = body->find("model");
  if (given != body->end()) {
    if (!given->is_string() || given->get<std::string>() != model) {
      *error = "Request body names a different model; ai_request() uses the "
               "registered model '" + model + "'";
      return false;
    }
    if (placement == ModelPlacement::kInPath) body->erase(given);
  }
  if (placement == ModelPlacement::kInBody) (*body)["model"] = model;

  auto stream = body->find("stream");
  if (stream != body->end()) {
    if (!stream->is_boolean() || stream->get<bool>()) {
      *error = "Streaming is not supported; ai_request() returns the whole "
               "response";
      return false;
    }
  } else if (stream_default_false) {
    (*body)["stream"] = false;
  }
  return true;
}

}  // namespace raw_request

namespace {

using raw_request::json;
using raw_request::ModelPlacement;

// Timeouts match the provider's other calls.
constexpr int kCloudRequestTimeoutSeconds = 120;
constexpr int kLocalRequestTimeoutSeconds = 300;

// Send a prepared body and return the provider's response body unchanged.
std::string send(const std::string& host, const std::string& path,
                 const json& body,
                 const std::map<std::string, std::string>& headers,
                 int timeout_seconds, std::string* error) {
  HttpClient::Response response = HttpClient::post(
      host, path, body.dump(), headers, timeout_seconds);
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }
  if (!response.is_success()) {
    *error = decisions::http_error(response.status_code, response.body);
    return "";
  }
  return response.body;
}

}  // namespace

std::string AnthropicProvider::request(const std::string& model,
                                       const std::string& api_key,
                                       const std::string& body_text,
                                       std::string* error) {
  json body;
  if (!raw_request::prepare(body_text, model, ModelPlacement::kInBody, false,
                            &body, error)) {
    return "";
  }
  return send(get_endpoint(), "/v1/messages", body, get_headers(api_key),
              kCloudRequestTimeoutSeconds, error);
}

std::string GoogleProvider::request(const std::string& model,
                                    const std::string& api_key,
                                    const std::string& body_text,
                                    std::string* error) {
  json body;
  if (!raw_request::prepare(body_text, model, ModelPlacement::kInPath, false,
                            &body, error)) {
    return "";
  }
  return send(get_endpoint(), "/v1beta/models/" + model + ":generateContent",
              body, get_headers(api_key), kCloudRequestTimeoutSeconds, error);
}

std::string OpenAIProvider::request(const std::string& model,
                                    const std::string& api_key,
                                    const std::string& body_text,
                                    std::string* error) {
  json body;
  if (!raw_request::prepare(body_text, model, ModelPlacement::kInBody, false,
                            &body, error)) {
    return "";
  }
  return send(get_endpoint(), "/v1/chat/completions", body,
              get_headers(api_key), kCloudRequestTimeoutSeconds, error);
}

std::string LocalProvider::request(const std::string& model,
                                   const std::string& /*api_key*/,
                                   const std::string& body_text,
                                   std::string* error) {
  json body;
  if (!raw_request::prepare(body_text, model, ModelPlacement::kInBody, true,
                            &body, error)) {
    return "";
  }
  std::string endpoint = get_endpoint(error);
  if (endpoint.empty()) return "";
  return send(endpoint, "/api/chat", body, get_headers(),
              kLocalRequestTimeoutSeconds, error);
}

std::string TypeSafeProvider::request(const std::string& model,
                                      const std::string& api_key,
                                      const std::string& body_text,
                                      std::string* error) {
  json body;
  if (!raw_request::prepare(body_text, model, ModelPlacement::kInBody, false,
                            &body, error)) {
    return "";
  }
  return send("https://api.typesafe.ai", "/v1/systemone", body,
              {{"Authorization", "Bearer " + api_key}},
              kCloudRequestTimeoutSeconds, error);
}

}  // namespace vsql_ai
