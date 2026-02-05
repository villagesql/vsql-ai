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

#include <villagesql/extension.h>

#include <cstring>
#include <memory>
#include <string>

#include "ai_providers.h"
#include "http_client.h"
#include "nlohmann/json.hpp"

using namespace villagesql::extension_builder;
using namespace villagesql::func_builder;
using namespace villagesql::type_builder;

using json = nlohmann::json;

namespace vsql_ai {

// =============================================================================
// AI_PROMPT Implementation
// =============================================================================

void ai_prompt_impl(vef_context_t* ctx, vef_invalue_t* provider_arg,
                    vef_invalue_t* model_arg, vef_invalue_t* api_key_arg,
                    vef_invalue_t* prompt_arg, vef_vdf_result_t* result) {
  // Validate NULL inputs
  if (provider_arg->is_null || model_arg->is_null || api_key_arg->is_null ||
      prompt_arg->is_null) {
    result->type = VEF_RESULT_NULL;
    return;
  }

  // Extract arguments
  std::string provider_name(provider_arg->str_value, provider_arg->str_len);
  std::string model(model_arg->str_value, model_arg->str_len);
  std::string api_key(api_key_arg->str_value, api_key_arg->str_len);
  std::string prompt_text(prompt_arg->str_value, prompt_arg->str_len);

  // Validate empty strings
  if (provider_name.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Provider name cannot be empty");
    return;
  }

  if (model.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Model name cannot be empty");
    return;
  }

  if (api_key.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "API key cannot be empty");
    return;
  }

  if (prompt_text.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Prompt text cannot be empty");
    return;
  }

  // Create provider
  auto provider = create_provider(provider_name);
  if (!provider) {
    result->type = VEF_RESULT_ERROR;
    std::string error_msg = "Unknown provider: " + provider_name;
    strncpy(result->error_msg, error_msg.c_str(),
            sizeof(result->error_msg) - 1);
    result->error_msg[sizeof(result->error_msg) - 1] = '\0';
    return;
  }

  // Call provider
  std::string error;
  std::string response = provider->prompt(model, api_key, prompt_text, &error);

  // Handle errors
  if (!error.empty()) {
    result->type = VEF_RESULT_ERROR;
    // Limit error message length to avoid buffer overflow
    if (error.length() > 255) {
      error = error.substr(0, 255);
    }
    strcpy(result->error_msg, error.c_str());
    return;
  }

  // Return result
  result->type = VEF_RESULT_VALUE;
  result->actual_len = response.length();

  // Copy response to buffer (truncate if necessary)
  size_t copy_len = std::min(response.length(), result->max_str_len - 1);
  memcpy(result->str_buf, response.c_str(), copy_len);
  result->str_buf[copy_len] = '\0';

  // If response was truncated, note it in error but still return value
  if (response.length() > result->max_str_len - 1) {
    result->actual_len = copy_len;
  }
}

// =============================================================================
// CREATE_EMBED Implementation
// =============================================================================

void create_embed_impl(vef_context_t* ctx, vef_invalue_t* provider_arg,
                       vef_invalue_t* model_arg, vef_invalue_t* api_key_arg,
                       vef_invalue_t* text_arg, vef_vdf_result_t* result) {
  // Validate NULL inputs
  if (provider_arg->is_null || model_arg->is_null || api_key_arg->is_null ||
      text_arg->is_null) {
    result->type = VEF_RESULT_NULL;
    return;
  }

  // Extract arguments
  std::string provider_name(provider_arg->str_value, provider_arg->str_len);
  std::string model(model_arg->str_value, model_arg->str_len);
  std::string api_key(api_key_arg->str_value, api_key_arg->str_len);
  std::string text(text_arg->str_value, text_arg->str_len);

  // Validate empty strings
  if (provider_name.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Provider name cannot be empty");
    return;
  }

  if (model.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Model name cannot be empty");
    return;
  }

  if (api_key.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "API key cannot be empty");
    return;
  }

  if (text.empty()) {
    result->type = VEF_RESULT_ERROR;
    strcpy(result->error_msg, "Text cannot be empty");
    return;
  }

  // Create provider
  auto provider = create_provider(provider_name);
  if (!provider) {
    result->type = VEF_RESULT_ERROR;
    std::string error_msg = "Unknown provider: " + provider_name;
    strncpy(result->error_msg, error_msg.c_str(),
            sizeof(result->error_msg) - 1);
    result->error_msg[sizeof(result->error_msg) - 1] = '\0';
    return;
  }

  // Call provider's embed method
  std::string error;
  std::string embedding_json = provider->embed(model, api_key, text, &error);

  // Handle errors
  if (!error.empty()) {
    result->type = VEF_RESULT_ERROR;
    // Limit error message length to avoid buffer overflow
    if (error.length() > 255) {
      error = error.substr(0, 255);
    }
    strcpy(result->error_msg, error.c_str());
    return;
  }

  // Return result (JSON array of floats)
  result->type = VEF_RESULT_VALUE;
  result->actual_len = embedding_json.length();

  // Copy embedding JSON to buffer (truncate if necessary)
  size_t copy_len =
      std::min(embedding_json.length(), result->max_str_len - 1);
  memcpy(result->str_buf, embedding_json.c_str(), copy_len);
  result->str_buf[copy_len] = '\0';

  // If response was truncated, note it
  if (embedding_json.length() > result->max_str_len - 1) {
    result->actual_len = copy_len;
  }
}

}  // namespace vsql_ai

// =============================================================================
// Extension Registration
// =============================================================================

VEF_GENERATE_ENTRY_POINTS(
    make_extension("vsql_ai", "0.0.1")
        .func(make_func<&vsql_ai::ai_prompt_impl>("ai_prompt")
                  .returns(STRING)
                  .param(STRING)  // provider
                  .param(STRING)  // model
                  .param(STRING)  // api_key
                  .param(STRING)  // prompt
                  .buffer_size(65535)  // Large buffer for AI responses
                  .build())

        .func(make_func<&vsql_ai::create_embed_impl>("create_embed")
                  .returns(STRING)
                  .param(STRING)  // provider
                  .param(STRING)  // model
                  .param(STRING)  // api_key
                  .param(STRING)  // text
                  .buffer_size(65535)
                  .build()))
