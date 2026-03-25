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

#include "ai_providers.h"

#include "http_client.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace vsql_ai {

// =============================================================================
// AnthropicProvider Implementation
// =============================================================================

AnthropicProvider::AnthropicProvider() {}

AnthropicProvider::~AnthropicProvider() {}

std::string AnthropicProvider::get_endpoint() const {
  return "https://api.anthropic.com";
}

std::map<std::string, std::string> AnthropicProvider::get_headers(
    const std::string& api_key) const {
  return {{"x-api-key", api_key},
          {"anthropic-version", "2023-06-01"}};
}

std::string AnthropicProvider::build_request_body(
    const std::string& model, const std::string& prompt) const {
  json request = {{"model", model},
                  {"max_tokens", 1024},
                  {"messages", json::array({json::object({{"role", "user"},
                                                          {"content", prompt}})})}};

  return request.dump();
}

std::string AnthropicProvider::parse_response(const std::string& response_json,
                                               std::string* error) const {
  try {
    auto response = json::parse(response_json);

    // Check for API error
    if (response.contains("error")) {
      if (response["error"].contains("message")) {
        *error = response["error"]["message"].get<std::string>();
      } else {
        *error = response["error"].dump();
      }
      return "";
    }

    // Extract response text
    if (response.contains("content") && response["content"].is_array() &&
        !response["content"].empty()) {
      const auto& first_content = response["content"][0];
      if (first_content.contains("text")) {
        return first_content["text"].get<std::string>();
      }
    }

    *error = "Invalid response format: missing content";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

std::string AnthropicProvider::prompt(const std::string& model,
                                      const std::string& api_key,
                                      const std::string& prompt_text,
                                      std::string* error) {
  // Build request
  std::string request_body = build_request_body(model, prompt_text);
  auto headers = get_headers(api_key);

  // Make HTTP request
  HttpClient client;
  auto response = client.post(get_endpoint(), "/v1/messages", request_body,
                              headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    // Try to extract error message from response body
    std::string api_error;
    std::string parsed_response =
        parse_response(response.body, &api_error);

    if (!api_error.empty()) {
      *error = api_error;
    } else {
      *error = "HTTP " + std::to_string(response.status_code) +
               " - " + response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response
  return parse_response(response.body, error);
}

std::string AnthropicProvider::embed(const std::string& model,
                                     const std::string& api_key,
                                     const std::string& text,
                                     std::string* error) {
  // Anthropic doesn't have a native embeddings API yet
  *error = "Embeddings not supported for Anthropic provider";
  return "";
}

// =============================================================================
// GoogleProvider Implementation
// =============================================================================

GoogleProvider::GoogleProvider() {}

GoogleProvider::~GoogleProvider() {}

std::string GoogleProvider::get_endpoint(const std::string& model) const {
  return "https://generativelanguage.googleapis.com";
}

std::map<std::string, std::string> GoogleProvider::get_headers(
    const std::string& api_key) const {
  return {{"x-goog-api-key", api_key}};
}

std::string GoogleProvider::build_request_body(
    const std::string& prompt) const {
  json request = {
      {"contents",
       json::array({json::object({{"parts", json::array({json::object(
                                                {{"text", prompt}})})}})})}};

  return request.dump();
}

std::string GoogleProvider::parse_response(const std::string& response_json,
                                            std::string* error) const {
  try {
    auto response = json::parse(response_json);

    // Check for API error
    if (response.contains("error")) {
      if (response["error"].contains("message")) {
        *error = response["error"]["message"].get<std::string>();
      } else {
        *error = response["error"].dump();
      }
      return "";
    }

    // Extract response text from candidates[0].content.parts[0].text
    if (response.contains("candidates") && response["candidates"].is_array() &&
        !response["candidates"].empty()) {
      const auto& first_candidate = response["candidates"][0];
      if (first_candidate.contains("content")) {
        const auto& content = first_candidate["content"];
        if (content.contains("parts") && content["parts"].is_array() &&
            !content["parts"].empty()) {
          const auto& first_part = content["parts"][0];
          if (first_part.contains("text")) {
            return first_part["text"].get<std::string>();
          }
        }
      }
    }

    *error = "Invalid response format: missing candidates or content";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

std::string GoogleProvider::prompt(const std::string& model,
                                    const std::string& api_key,
                                    const std::string& prompt_text,
                                    std::string* error) {
  // Build request
  std::string request_body = build_request_body(prompt_text);
  auto headers = get_headers(api_key);

  // Build the full path with model name
  std::string path = "/v1beta/models/" + model + ":generateContent";

  // Make HTTP request
  HttpClient client;
  auto response =
      client.post(get_endpoint(model), path, request_body, headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    // Try to extract error message from response body
    std::string api_error;
    std::string parsed_response = parse_response(response.body, &api_error);

    if (!api_error.empty()) {
      *error = api_error;
    } else {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response
  return parse_response(response.body, error);
}

std::string GoogleProvider::embed(const std::string& model,
                                   const std::string& api_key,
                                   const std::string& text,
                                   std::string* error) {
  // Build request body for embedContent API
  json request = {{"content", json::object({{"parts", json::array({json::object({{"text", text}})})}})}};

  std::string request_body = request.dump();
  auto headers = get_headers(api_key);

  // Build the full path with model name for embedContent
  std::string path = "/v1beta/models/" + model + ":embedContent";

  // Make HTTP request
  HttpClient client;
  auto response = client.post(get_endpoint(model), path, request_body, headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    // Try to extract error message from response body
    try {
      auto error_response = json::parse(response.body);
      if (error_response.contains("error")) {
        if (error_response["error"].contains("message")) {
          *error = error_response["error"]["message"].get<std::string>();
        } else {
          *error = error_response["error"].dump();
        }
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 response.body.substr(0, 100);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      if (response_json["error"].contains("message")) {
        *error = response_json["error"]["message"].get<std::string>();
      } else {
        *error = response_json["error"].dump();
      }
      return "";
    }

    // Try new format first (gemini-embedding-2-preview)
    if (response_json.contains("embeddings") &&
        response_json["embeddings"].is_array() &&
        !response_json["embeddings"].empty()) {
      return response_json["embeddings"][0]["values"].dump();
    }

    // Fall back to old format (gemini-embedding-001)
    if (response_json.contains("embedding") &&
        response_json["embedding"].contains("values")) {
      return response_json["embedding"]["values"].dump();
    }

    *error = "Invalid response format: missing embedding values";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

// =============================================================================
// OpenAIProvider Implementation
// =============================================================================

OpenAIProvider::OpenAIProvider() {}

OpenAIProvider::~OpenAIProvider() {}

std::string OpenAIProvider::get_endpoint() const {
  return "https://api.openai.com";
}

std::map<std::string, std::string> OpenAIProvider::get_headers(
    const std::string& api_key) const {
  return {{"Authorization", "Bearer " + api_key}};
}

std::string OpenAIProvider::build_request_body(
    const std::string& model, const std::string& prompt) const {
  json request = {{"model", model},
                  {"messages", json::array({json::object(
                                   {{"role", "user"}, {"content", prompt}})})}};

  return request.dump();
}

std::string OpenAIProvider::parse_response(const std::string& response_json,
                                            std::string* error) const {
  try {
    auto response = json::parse(response_json);

    // Check for API error
    if (response.contains("error")) {
      if (response["error"].contains("message")) {
        *error = response["error"]["message"].get<std::string>();
      } else {
        *error = response["error"].dump();
      }
      return "";
    }

    // Extract response text from choices[0].message.content
    if (response.contains("choices") && response["choices"].is_array() &&
        !response["choices"].empty()) {
      const auto& first_choice = response["choices"][0];
      if (first_choice.contains("message") &&
          first_choice["message"].contains("content")) {
        return first_choice["message"]["content"].get<std::string>();
      }
    }

    *error = "Invalid response format: missing choices or content";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

std::string OpenAIProvider::prompt(const std::string& model,
                                    const std::string& api_key,
                                    const std::string& prompt_text,
                                    std::string* error) {
  // Build request
  std::string request_body = build_request_body(model, prompt_text);
  auto headers = get_headers(api_key);

  // Make HTTP request
  HttpClient client;
  auto response =
      client.post(get_endpoint(), "/v1/chat/completions", request_body,
                  headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    // Try to extract error message from response body
    std::string api_error;
    std::string parsed_response = parse_response(response.body, &api_error);

    if (!api_error.empty()) {
      *error = api_error;
    } else {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response
  return parse_response(response.body, error);
}

std::string OpenAIProvider::embed(const std::string& model,
                                   const std::string& api_key,
                                   const std::string& text,
                                   std::string* error) {
  // Build request body for embeddings API
  json request = {{"model", model}, {"input", text}};

  std::string request_body = request.dump();
  auto headers = get_headers(api_key);

  // Make HTTP request
  HttpClient client;
  auto response =
      client.post(get_endpoint(), "/v1/embeddings", request_body, headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    // Try to extract error message from response body
    try {
      auto error_response = json::parse(response.body);
      if (error_response.contains("error")) {
        if (error_response["error"].contains("message")) {
          *error = error_response["error"]["message"].get<std::string>();
        } else {
          *error = error_response["error"].dump();
        }
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 response.body.substr(0, 100);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      if (response_json["error"].contains("message")) {
        *error = response_json["error"]["message"].get<std::string>();
      } else {
        *error = response_json["error"].dump();
      }
      return "";
    }

    // Extract embedding values from response.data[0].embedding
    if (response_json.contains("data") && response_json["data"].is_array() &&
        !response_json["data"].empty()) {
      const auto& first_data = response_json["data"][0];
      if (first_data.contains("embedding")) {
        return first_data["embedding"].dump();
      }
    }

    *error = "Invalid response format: missing data or embedding";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

// =============================================================================
// LocalProvider Implementation (Ollama on 127.0.0.1:11434)
// =============================================================================

LocalProvider::LocalProvider() {}

LocalProvider::~LocalProvider() {}

std::string LocalProvider::get_endpoint() const {
  return "http://127.0.0.1:11434";
}

std::map<std::string, std::string> LocalProvider::get_headers() const {
  return {};
}

std::string LocalProvider::build_request_body(
    const std::string& model, const std::string& prompt) const {
  json request = {{"model", model},
                  {"messages", json::array({json::object(
                                   {{"role", "user"}, {"content", prompt}})})}};

  return request.dump();
}

std::string LocalProvider::parse_response(const std::string& response_json,
                                           std::string* error) const {
  try {
    auto response = json::parse(response_json);

    // Check for API error
    if (response.contains("error")) {
      if (response["error"].is_string()) {
        *error = response["error"].get<std::string>();
      } else if (response["error"].contains("message")) {
        *error = response["error"]["message"].get<std::string>();
      } else {
        *error = response["error"].dump();
      }
      return "";
    }

    // Extract response text from choices[0].message.content (OpenAI-compatible)
    if (response.contains("choices") && response["choices"].is_array() &&
        !response["choices"].empty()) {
      const auto& first_choice = response["choices"][0];
      if (first_choice.contains("message") &&
          first_choice["message"].contains("content")) {
        return first_choice["message"]["content"].get<std::string>();
      }
    }

    // Fall back to Ollama native format: message.content
    if (response.contains("message") &&
        response["message"].contains("content")) {
      return response["message"]["content"].get<std::string>();
    }

    *error = "Invalid response format: missing choices or content";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

std::string LocalProvider::prompt(const std::string& model,
                                   const std::string& api_key,
                                   const std::string& prompt_text,
                                   std::string* error) {
  // Build request
  std::string request_body = build_request_body(model, prompt_text);
  auto headers = get_headers();

  // Make HTTP request using OpenAI-compatible endpoint
  HttpClient client;
  auto response =
      client.post(get_endpoint(), "/v1/chat/completions", request_body,
                  headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    std::string api_error;
    std::string parsed_response = parse_response(response.body, &api_error);

    if (!api_error.empty()) {
      *error = api_error;
    } else {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response
  return parse_response(response.body, error);
}

std::string LocalProvider::embed(const std::string& model,
                                  const std::string& api_key,
                                  const std::string& text,
                                  std::string* error) {
  // Build request body for OpenAI-compatible embeddings API
  json request = {{"model", model}, {"input", text}};

  std::string request_body = request.dump();
  auto headers = get_headers();

  // Make HTTP request
  HttpClient client;
  auto response =
      client.post(get_endpoint(), "/v1/embeddings", request_body, headers, 30);

  // Check for network errors
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }

  // Check HTTP status
  if (!response.is_success()) {
    try {
      auto error_response = json::parse(response.body);
      if (error_response.contains("error")) {
        if (error_response["error"].is_string()) {
          *error = error_response["error"].get<std::string>();
        } else if (error_response["error"].contains("message")) {
          *error = error_response["error"]["message"].get<std::string>();
        } else {
          *error = error_response["error"].dump();
        }
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 response.body.substr(0, 100);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               response.body.substr(0, 100);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      if (response_json["error"].is_string()) {
        *error = response_json["error"].get<std::string>();
      } else if (response_json["error"].contains("message")) {
        *error = response_json["error"]["message"].get<std::string>();
      } else {
        *error = response_json["error"].dump();
      }
      return "";
    }

    // Extract embedding values from response.data[0].embedding
    if (response_json.contains("data") && response_json["data"].is_array() &&
        !response_json["data"].empty()) {
      const auto& first_data = response_json["data"][0];
      if (first_data.contains("embedding")) {
        return first_data["embedding"].dump();
      }
    }

    *error = "Invalid response format: missing data or embedding";
    return "";

  } catch (const json::exception& e) {
    *error = std::string("JSON parse error: ") + e.what();
    return "";
  }
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<AIProvider> create_provider(const std::string& provider_name) {
  if (provider_name == "anthropic") {
    return std::make_unique<AnthropicProvider>();
  }

  if (provider_name == "google") {
    return std::make_unique<GoogleProvider>();
  }

  if (provider_name == "openai") {
    return std::make_unique<OpenAIProvider>();
  }

  if (provider_name == "local") {
    return std::make_unique<LocalProvider>();
  }

  // Unknown provider
  return nullptr;
}

}  // namespace vsql_ai
