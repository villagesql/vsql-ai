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

// Bytes of an unrecognised response body quoted back in an error message.
constexpr size_t kMaxErrorBodyBytes = 100;

std::string_view truncate_utf8_view(std::string_view text, size_t max_bytes) {
  if (text.size() <= max_bytes) {
    return text;
  }

  // Walk back off any continuation bytes (10xxxxxx) so the cut lands on a
  // code point boundary.
  size_t cut = max_bytes;
  while (cut > 0 &&
         (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
    --cut;
  }

  return text.substr(0, cut);
}

std::string truncate_utf8(const std::string& text, size_t max_bytes) {
  return std::string(truncate_utf8_view(text, max_bytes));
}

namespace {

// Turn an upstream `error` member into text. The shape is not guaranteed:
// Anthropic, Google and OpenAI nest {"error": {"message": "..."}}, Ollama also
// sends {"error": "..."}, and any of them can send a "message" that is not a
// string (an object, an array, a number) — .get<std::string>() would throw
// type_error.302 on those, which the caller reports as a bogus "JSON parse
// error". Only a string is read as text; anything else is dumped verbatim so
// the message still carries what the provider said.
//
// dump() uses the replacing error handler because the error path must not
// throw: a body with invalid UTF-8 would otherwise mask the real failure.
std::string extract_error_message(const json& error_node) {
  if (error_node.is_string()) {
    return error_node.get<std::string>();
  }

  if (error_node.is_object()) {
    auto message = error_node.find("message");
    if (message != error_node.end() && message->is_string()) {
      return message->get<std::string>();
    }
  }

  return error_node.dump(-1, ' ', false, json::error_handler_t::replace);
}

}  // namespace

// =============================================================================
// AnthropicProvider Implementation
// =============================================================================


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
      *error = extract_error_message(response["error"]);
      return "";
    }

    // Extract response text. Models with extended thinking (e.g.
    // claude-fable-5) return thinking blocks before the text block, so scan
    // the content array for "text" blocks rather than only checking the
    // first element. Concatenate in case the response contains multiple
    // text blocks.
    if (response.contains("content") && response["content"].is_array()) {
      std::string result;
      bool found_text = false;
      for (const auto& block : response["content"]) {
        if (block.contains("text") && block["text"].is_string()) {
          result += block["text"].get<std::string>();
          found_text = true;
        }
      }
      if (found_text) {
        return result;
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
  auto response = HttpClient::post(get_endpoint(), "/v1/messages", request_body,
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
               " - " + truncate_utf8(response.body, kMaxErrorBodyBytes);
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


std::string GoogleProvider::get_endpoint() const {
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
      *error = extract_error_message(response["error"]);
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
          if (first_part.contains("text") && first_part["text"].is_string()) {
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
  auto response =
      HttpClient::post(get_endpoint(), path, request_body, headers, 30);

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
               truncate_utf8(response.body, kMaxErrorBodyBytes);
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
  auto response = HttpClient::post(get_endpoint(), path, request_body, headers, 30);

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
        *error = extract_error_message(error_response["error"]);
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 truncate_utf8(response.body, kMaxErrorBodyBytes);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               truncate_utf8(response.body, kMaxErrorBodyBytes);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      *error = extract_error_message(response_json["error"]);
      return "";
    }

    // Try new format first (gemini-embedding-2-preview). Bind through a const
    // reference: on a non-const json, operator[] inserts a null member for a
    // missing key, which would dump as the string "null" and be returned as a
    // successful embedding.
    const json& const_response = response_json;
    if (const_response.contains("embeddings") &&
        const_response["embeddings"].is_array() &&
        !const_response["embeddings"].empty() &&
        const_response["embeddings"][0].contains("values")) {
      return const_response["embeddings"][0]["values"].dump();
    }

    // Fall back to old format (gemini-embedding-001)
    if (const_response.contains("embedding") &&
        const_response["embedding"].contains("values")) {
      return const_response["embedding"]["values"].dump();
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
      *error = extract_error_message(response["error"]);
      return "";
    }

    // Extract response text from choices[0].message.content
    if (response.contains("choices") && response["choices"].is_array() &&
        !response["choices"].empty()) {
      const auto& first_choice = response["choices"][0];
      // content is null on refusals and tool-call responses, so check the
      // type rather than only its presence.
      if (first_choice.contains("message") &&
          first_choice["message"].contains("content") &&
          first_choice["message"]["content"].is_string()) {
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
  auto response =
      HttpClient::post(get_endpoint(), "/v1/chat/completions", request_body,
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
               truncate_utf8(response.body, kMaxErrorBodyBytes);
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
  auto response =
      HttpClient::post(get_endpoint(), "/v1/embeddings", request_body, headers, 30);

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
        *error = extract_error_message(error_response["error"]);
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 truncate_utf8(response.body, kMaxErrorBodyBytes);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               truncate_utf8(response.body, kMaxErrorBodyBytes);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      *error = extract_error_message(response_json["error"]);
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

// Local inference is not rate limited but can be slow: large models (e.g.
// 12B+ parameters) may need minutes to load and generate a full
// non-streaming response, so allow far longer than the cloud providers.
namespace {
constexpr int kLocalTimeoutSeconds = 300;
}


std::string LocalProvider::get_endpoint() const {
  return "http://127.0.0.1:11434";
}

std::map<std::string, std::string> LocalProvider::get_headers() const {
  return {};
}

std::string LocalProvider::build_request_body(
    const std::string& model, const std::string& prompt) const {
  // Disable thinking: reasoning traces are discarded by parse_response, so
  // generating them only wastes inference time. Ollama ignores "think" for
  // models without thinking support.
  json request = {{"model", model},
                  {"messages", json::array({json::object(
                                   {{"role", "user"}, {"content", prompt}})})},
                  {"stream", false},
                  {"think", false}};

  return request.dump();
}

std::string LocalProvider::parse_response(const std::string& response_json,
                                           std::string* error) const {
  try {
    auto response = json::parse(response_json);

    // Check for API error
    if (response.contains("error")) {
      *error = extract_error_message(response["error"]);
      return "";
    }

    // Extract response text from choices[0].message.content (OpenAI-compatible)
    if (response.contains("choices") && response["choices"].is_array() &&
        !response["choices"].empty()) {
      const auto& first_choice = response["choices"][0];
      // content is null on refusals and tool-call responses, so check the
      // type rather than only its presence.
      if (first_choice.contains("message") &&
          first_choice["message"].contains("content") &&
          first_choice["message"]["content"].is_string()) {
        return first_choice["message"]["content"].get<std::string>();
      }
    }

    // Fall back to Ollama native format: message.content
    if (response.contains("message") &&
        response["message"].contains("content") &&
        response["message"]["content"].is_string()) {
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

  // Use Ollama's native chat endpoint rather than the OpenAI-compatible
  // one so thinking can be disabled via the "think" request option.
  auto response = HttpClient::post(get_endpoint(), "/api/chat", request_body,
                              headers, kLocalTimeoutSeconds);

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
               truncate_utf8(response.body, kMaxErrorBodyBytes);
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
  auto response = HttpClient::post(get_endpoint(), "/v1/embeddings", request_body,
                              headers, kLocalTimeoutSeconds);

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
        *error = extract_error_message(error_response["error"]);
      } else {
        *error = "HTTP " + std::to_string(response.status_code) + " - " +
                 truncate_utf8(response.body, kMaxErrorBodyBytes);
      }
    } catch (const json::exception& e) {
      *error = "HTTP " + std::to_string(response.status_code) + " - " +
               truncate_utf8(response.body, kMaxErrorBodyBytes);
    }
    return "";
  }

  // Parse successful response to extract embedding values
  try {
    auto response_json = json::parse(response.body);

    // Check for API error
    if (response_json.contains("error")) {
      *error = extract_error_message(response_json["error"]);
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
