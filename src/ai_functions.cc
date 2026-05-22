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

#include <villagesql/vsql.h>

#include <memory>
#include <string>

#include "ai_providers.h"
#include "http_client.h"
#include "nlohmann/json.hpp"

using namespace vsql;

using json = nlohmann::json;

namespace vsql_ai {

// =============================================================================
// PROMPT Implementation
// =============================================================================

void prompt_impl(StringArg provider_arg, StringArg model_arg,
                 StringArg api_key_arg, StringArg prompt_arg,
                 StringResult out) {
  // Validate NULL inputs
  if (provider_arg.is_null() || model_arg.is_null() || api_key_arg.is_null() ||
      prompt_arg.is_null()) {
    out.set_null();
    return;
  }

  // Extract arguments
  std::string provider_name(provider_arg.value());
  std::string model(model_arg.value());
  std::string api_key(api_key_arg.value());
  std::string prompt_text(prompt_arg.value());

  // Validate empty strings
  if (provider_name.empty()) {
    out.warning("Provider name cannot be empty");
    return;
  }

  if (model.empty()) {
    out.warning("Model name cannot be empty");
    return;
  }

  if (api_key.empty() && provider_name != "local") {
    out.warning("API key cannot be empty");
    return;
  }

  if (prompt_text.empty()) {
    out.warning("Prompt text cannot be empty");
    return;
  }

  // Create provider
  auto provider = create_provider(provider_name);
  if (!provider) {
    std::string error_msg = "Unknown provider: " + provider_name;
    out.warning(error_msg);
    return;
  }

  // Call provider
  std::string error;
  std::string response = provider->prompt(model, api_key, prompt_text, &error);

  // Handle errors
  if (!error.empty()) {
    if (error.length() > 255) {
      error = error.substr(0, 255);
    }
    out.warning(error);
    return;
  }

  // Return result (truncates to buffer size automatically)
  out.set(response);
}

// =============================================================================
// EMBEDDING Implementation
// =============================================================================

void embedding_impl(StringArg provider_arg, StringArg model_arg,
                    StringArg api_key_arg, StringArg text_arg,
                    StringResult out) {
  // Validate NULL inputs
  if (provider_arg.is_null() || model_arg.is_null() || api_key_arg.is_null() ||
      text_arg.is_null()) {
    out.set_null();
    return;
  }

  // Extract arguments
  std::string provider_name(provider_arg.value());
  std::string model(model_arg.value());
  std::string api_key(api_key_arg.value());
  std::string text(text_arg.value());

  // Validate empty strings
  if (provider_name.empty()) {
    out.warning("Provider name cannot be empty");
    return;
  }

  if (model.empty()) {
    out.warning("Model name cannot be empty");
    return;
  }

  if (api_key.empty() && provider_name != "local") {
    out.warning("API key cannot be empty");
    return;
  }

  if (text.empty()) {
    out.warning("Text cannot be empty");
    return;
  }

  // Create provider
  auto provider = create_provider(provider_name);
  if (!provider) {
    std::string error_msg = "Unknown provider: " + provider_name;
    out.warning(error_msg);
    return;
  }

  // Call provider's embed method
  std::string error;
  std::string embedding_json = provider->embed(model, api_key, text, &error);

  // Handle errors
  if (!error.empty()) {
    if (error.length() > 255) {
      error = error.substr(0, 255);
    }
    out.warning(error);
    return;
  }

  // Return result (JSON array of floats; truncates to buffer size automatically)
  out.set(embedding_json);
}

}  // namespace vsql_ai

// =============================================================================
// Extension Registration
// =============================================================================

VEF_GENERATE_ENTRY_POINTS(
    make_extension()
        .func(make_func<&vsql_ai::prompt_impl>("ai_prompt")
                  .returns(STRING)
                  .param(STRING)  // provider
                  .param(STRING)  // model
                  .param(STRING)  // api_key
                  .param(STRING)  // prompt
                  .buffer_size(65535)  // Large buffer for AI responses
                  .build())

        .func(make_func<&vsql_ai::embedding_impl>("ai_embedding")
                  .returns(STRING)
                  .param(STRING)  // provider
                  .param(STRING)  // model
                  .param(STRING)  // api_key
                  .param(STRING)  // text
                  .buffer_size(65535)
                  .build()))
