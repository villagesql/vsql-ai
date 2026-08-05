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

namespace vsql_ai {

// Longest error text passed to out.warning(). Provider messages can be far
// longer than a useful warning, and the server clamps warnings anyway.
constexpr size_t kMaxWarningBytes = 255;

namespace {

// Body of ai_prompt. Wrapped by prompt_impl so no exception escapes into the
// server: JSON serialization throws on input that isn't valid UTF-8, and an
// exception crossing the extension ABI boundary would terminate the process.
void prompt_body(vsql::StringArg provider_arg, vsql::StringArg model_arg,
                 vsql::StringArg api_key_arg, vsql::StringArg prompt_arg,
                 vsql::StringResult out) {
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

  if (prompt_text.empty()) {
    out.warning("Prompt text cannot be empty");
    return;
  }

  // Resolve the provider before checking the key: an unrecognised name should
  // be reported as such rather than as a missing key.
  auto provider = create_provider(provider_name);
  if (!provider) {
    out.warning(
        truncate_utf8("Unknown provider: " + provider_name, kMaxWarningBytes));
    return;
  }

  if (api_key.empty() && provider->requires_api_key()) {
    out.warning("API key cannot be empty");
    return;
  }

  // Call provider
  std::string error;
  std::string response = provider->prompt(model, api_key, prompt_text, &error);

  // Handle errors
  if (!error.empty()) {
    out.warning(truncate_utf8(error, kMaxWarningBytes));
    return;
  }

  // out.set() clamps with memcpy and would cut a multi-byte sequence, so trim
  // on a code point boundary first. The view overload borrows from `response`,
  // which outlives the call, so a response that already fits is not copied.
  out.set(truncate_utf8_view(response, out.buffer().size()));
}

// Body of ai_embedding. Wrapped by embedding_impl for the same reason as
// prompt_body above.
void embedding_body(vsql::StringArg provider_arg, vsql::StringArg model_arg,
                    vsql::StringArg api_key_arg, vsql::StringArg text_arg,
                    vsql::StringResult out) {
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

  if (text.empty()) {
    out.warning("Text cannot be empty");
    return;
  }

  // Resolve the provider before checking the key: an unrecognised name should
  // be reported as such rather than as a missing key.
  auto provider = create_provider(provider_name);
  if (!provider) {
    out.warning(
        truncate_utf8("Unknown provider: " + provider_name, kMaxWarningBytes));
    return;
  }

  if (api_key.empty() && provider->requires_api_key()) {
    out.warning("API key cannot be empty");
    return;
  }

  // Call provider's embed method
  std::string error;
  std::string embedding_json = provider->embed(model, api_key, text, &error);

  // Handle errors
  if (!error.empty()) {
    out.warning(truncate_utf8(error, kMaxWarningBytes));
    return;
  }

  // A clipped JSON array is not a smaller embedding, it is corrupt data.
  // Refuse rather than return something that parses as a shorter vector.
  if (embedding_json.size() > out.buffer().size()) {
    out.warning("Embedding too large for the result buffer");
    return;
  }
  out.set(embedding_json);
}

}  // namespace

namespace {

// Report an exception as a warning. Building the message allocates, so a
// second failure (bad_alloc, or anything thrown while unwinding) falls back to
// a fixed string — out.warning() on a literal cannot throw.
void report_exception(const std::exception& e, vsql::StringResult out) {
  try {
    out.warning(truncate_utf8(std::string("Internal error: ") + e.what(),
                              kMaxWarningBytes));
  } catch (...) {
    out.warning("Internal error");
  }
}

}  // namespace

// These are the ABI boundary: the SDK does not wrap VDF calls, so an escaping
// exception terminates the server. They would be marked noexcept to have the
// compiler enforce that, but make_func's FuncParamTypes has no specialization
// for noexcept function types, so registration fails to compile. The catch-all
// below plus report_exception's own fallback are what hold the guarantee.
void prompt_impl(vsql::StringArg provider_arg, vsql::StringArg model_arg,
                 vsql::StringArg api_key_arg, vsql::StringArg prompt_arg,
                 vsql::StringResult out) {
  try {
    prompt_body(provider_arg, model_arg, api_key_arg, prompt_arg, out);
  } catch (const std::exception& e) {
    report_exception(e, out);
  } catch (...) {
    out.warning("Internal error");
  }
}

void embedding_impl(vsql::StringArg provider_arg, vsql::StringArg model_arg,
                    vsql::StringArg api_key_arg, vsql::StringArg text_arg,
                    vsql::StringResult out) {
  try {
    embedding_body(provider_arg, model_arg, api_key_arg, text_arg, out);
  } catch (const std::exception& e) {
    report_exception(e, out);
  } catch (...) {
    out.warning("Internal error");
  }
}

}  // namespace vsql_ai

// =============================================================================
// Extension Registration
// =============================================================================

VEF_GENERATE_ENTRY_POINTS(
    vsql::make_extension()
        .func(vsql::make_func<&vsql_ai::prompt_impl>("ai_prompt")
                  .returns(vsql::STRING)
                  .param(vsql::STRING) // provider
                  .param(vsql::STRING) // model
                  .param(vsql::STRING) // api_key
                  .param(vsql::STRING) // prompt
                  .buffer_size(65535)  // Large buffer for AI responses
                  .build())

        .func(vsql::make_func<&vsql_ai::embedding_impl>("ai_embedding")
                  .returns(vsql::STRING)
                  .param(vsql::STRING) // provider
                  .param(vsql::STRING) // model
                  .param(vsql::STRING) // api_key
                  .param(vsql::STRING) // text
                  .buffer_size(65535)
                  .build()))
