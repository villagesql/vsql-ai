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

#ifndef VSQL_AI_PROVIDERS_H
#define VSQL_AI_PROVIDERS_H

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace vsql_ai {

// Shorten a UTF-8 string to at most max_bytes without splitting a multi-byte
// sequence. Upstream provider errors are UTF-8 and frequently non-ASCII, so a
// plain substr() can leave a truncated code point in a warning message.
//
// The view overload borrows from `text` and is the one to use on the result
// path, where copying a response that already fits would be pure waste; the
// caller must keep `text` alive for as long as the view is used.
std::string_view truncate_utf8_view(std::string_view text, size_t max_bytes);
std::string truncate_utf8(const std::string& text, size_t max_bytes);

// Abstract base class for AI providers
class AIProvider {
 public:
  virtual ~AIProvider() = default;

  // Send a prompt and get a response
  virtual std::string prompt(const std::string& model,
                             const std::string& api_key,
                             const std::string& prompt_text,
                             std::string* error) = 0;

  // Create embeddings for text (returns JSON array of floats)
  virtual std::string embed(const std::string& model,
                            const std::string& api_key,
                            const std::string& text,
                            std::string* error) = 0;

  // Whether a non-empty API key is required. Locally hosted providers do not
  // authenticate, so the caller asks the provider rather than testing its name.
  virtual bool requires_api_key() const { return true; }
};

// Anthropic Claude provider implementation
class AnthropicProvider : public AIProvider {
 public:
  std::string prompt(const std::string& model, const std::string& api_key,
                     const std::string& prompt_text,
                     std::string* error) override;

  std::string embed(const std::string& model, const std::string& api_key,
                    const std::string& text, std::string* error) override;

 private:
  std::string get_endpoint() const;
  std::map<std::string, std::string> get_headers(
      const std::string& api_key) const;
  std::string build_request_body(const std::string& model,
                                  const std::string& prompt) const;
  std::string parse_response(const std::string& response_json,
                             std::string* error) const;
};

// Google provider implementation (Gemini models)
class GoogleProvider : public AIProvider {
 public:
  std::string prompt(const std::string& model, const std::string& api_key,
                     const std::string& prompt_text,
                     std::string* error) override;

  std::string embed(const std::string& model, const std::string& api_key,
                    const std::string& text, std::string* error) override;

 private:
  std::string get_endpoint() const;
  std::map<std::string, std::string> get_headers(
      const std::string& api_key) const;
  std::string build_request_body(const std::string& prompt) const;
  std::string parse_response(const std::string& response_json,
                             std::string* error) const;
};

// OpenAI provider implementation (GPT models)
class OpenAIProvider : public AIProvider {
 public:
  std::string prompt(const std::string& model, const std::string& api_key,
                     const std::string& prompt_text,
                     std::string* error) override;

  std::string embed(const std::string& model, const std::string& api_key,
                    const std::string& text, std::string* error) override;

 private:
  std::string get_endpoint() const;
  std::map<std::string, std::string> get_headers(
      const std::string& api_key) const;
  std::string build_request_body(const std::string& model,
                                  const std::string& prompt) const;
  std::string parse_response(const std::string& response_json,
                             std::string* error) const;
};

// Local provider implementation (Ollama on 127.0.0.1:11434)
class LocalProvider : public AIProvider {
 public:
  std::string prompt(const std::string& model, const std::string& api_key,
                     const std::string& prompt_text,
                     std::string* error) override;

  std::string embed(const std::string& model, const std::string& api_key,
                    const std::string& text, std::string* error) override;

  bool requires_api_key() const override { return false; }

 private:
  std::string get_endpoint() const;
  std::map<std::string, std::string> get_headers() const;
  std::string build_request_body(const std::string& model,
                                  const std::string& prompt) const;
  std::string parse_response(const std::string& response_json,
                             std::string* error) const;
};

// Factory function to create provider by name
std::unique_ptr<AIProvider> create_provider(const std::string& provider_name);

}  // namespace vsql_ai

#endif  // VSQL_AI_PROVIDERS_H
