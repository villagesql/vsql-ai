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
#include <villagesql/preview/keyring.h>
#include <villagesql/preview/sys_var.h>

#include <memory>
#include <string>

#include "ai_providers.h"
#include "model_registry.h"

namespace vsql_ai {

// Longest error text passed to out.warning(). Provider messages can be far
// longer than a useful warning, and the server clamps warnings anyway.
constexpr size_t kMaxWarningBytes = 255;

// Storage for vsql_ai.models. The server owns the buffer and frees it on SET,
// so it is never read directly: read_registry() asks the server for a copy.
char* g_models = nullptr;

namespace sv = vsql::preview_sys_var;

auto g_sys_vars = sv::make_capability({
    sv::make_str("models",
                 "JSON object mapping each model ID to its provider, the "
                 "provider's model name, and the keyring ID (key_id) and "
                 "owner (key_owner) of its API key",
                 &g_models, ""),
});

vsql::preview_keyring::KeyringCapability g_keyring;

namespace {

// A model resolved from vsql_ai.models, with its API key read from the
// keyring. The key lives only in this struct for the length of one call.
struct ResolvedModel {
  ModelConfig config;
  std::string api_key;
  std::unique_ptr<AIProvider> provider;
};

// Read the API key named by config from the keyring. Every failure names the
// data ID and owner it looked for, but never any part of a secret.
bool read_api_key(const ModelConfig& config, std::string* api_key,
                  std::string* error) {
  using Status = vsql::preview_keyring::KeyringCapability::Status;
  auto result = g_keyring.read(config.key_id, config.key_owner);
  switch (result.status) {
    case Status::OK:
      if (result.value.empty()) {
        *error = "Keyring secret '" + config.key_id + "' is empty";
        return false;
      }
      *api_key = std::move(result.value);
      return true;
    case Status::UNAVAILABLE:
      *error =
          "No keyring component is installed; vsql_ai reads API keys from the "
          "MySQL keyring";
      return false;
    case Status::NOT_FOUND:
    case Status::ERROR:
    default:
      if (config.key_owner.empty()) {
        *error = "Keyring has no internal key '" + config.key_id +
                 "'. A key stored with keyring_key_store() belongs to the "
                 "account that stored it; set key_owner to that account";
      } else {
        *error = "Keyring has no secret '" + config.key_id + "' owned by '" +
                 config.key_owner + "'";
      }
      return false;
  }
}

// Turn a model ID into a provider, model name and API key.
bool resolve_model(const std::string& model_id, ResolvedModel* resolved,
                   std::string* error) {
  std::string registry;
  if (g_sys_vars.get("vsql_ai", "models", registry)) {
    *error = "Could not read vsql_ai.models";
    return false;
  }
  if (!find_model(registry, model_id, &resolved->config, error)) return false;

  resolved->provider = create_provider(resolved->config.provider);
  if (!resolved->provider) {  // find_model already checked; defensive
    *error = "Unknown provider: " + resolved->config.provider;
    return false;
  }
  if (resolved->config.key_id.empty()) return true;  // local provider
  return read_api_key(resolved->config, &resolved->api_key, error);
}

// Body of ai_prompt. Wrapped by prompt_impl so no exception escapes into the
// server: JSON serialization throws on input that isn't valid UTF-8, and an
// exception crossing the extension ABI boundary would terminate the process.
void prompt_body(vsql::StringArg model_id_arg, vsql::StringArg prompt_arg,
                 vsql::StringResult out) {
  if (model_id_arg.is_null() || prompt_arg.is_null()) {
    out.set_null();
    return;
  }

  std::string model_id(model_id_arg.value());
  std::string prompt_text(prompt_arg.value());

  if (model_id.empty()) {
    out.warning("Model ID cannot be empty");
    return;
  }
  if (prompt_text.empty()) {
    out.warning("Prompt text cannot be empty");
    return;
  }

  std::string error;
  ResolvedModel resolved;
  if (!resolve_model(model_id, &resolved, &error)) {
    out.warning(truncate_utf8(error, kMaxWarningBytes));
    return;
  }

  std::string response = resolved.provider->prompt(
      resolved.config.model, resolved.api_key, prompt_text, &error);
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
void embedding_body(vsql::StringArg model_id_arg, vsql::StringArg text_arg,
                    vsql::StringResult out) {
  if (model_id_arg.is_null() || text_arg.is_null()) {
    out.set_null();
    return;
  }

  std::string model_id(model_id_arg.value());
  std::string text(text_arg.value());

  if (model_id.empty()) {
    out.warning("Model ID cannot be empty");
    return;
  }
  if (text.empty()) {
    out.warning("Text cannot be empty");
    return;
  }

  std::string error;
  ResolvedModel resolved;
  if (!resolve_model(model_id, &resolved, &error)) {
    out.warning(truncate_utf8(error, kMaxWarningBytes));
    return;
  }

  std::string embedding_json = resolved.provider->embed(
      resolved.config.model, resolved.api_key, text, &error);
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
void prompt_impl(vsql::StringArg model_id_arg, vsql::StringArg prompt_arg,
                 vsql::StringResult out) {
  try {
    prompt_body(model_id_arg, prompt_arg, out);
  } catch (const std::exception& e) {
    report_exception(e, out);
  } catch (...) {
    out.warning("Internal error");
  }
}

void embedding_impl(vsql::StringArg model_id_arg, vsql::StringArg text_arg,
                    vsql::StringResult out) {
  try {
    embedding_body(model_id_arg, text_arg, out);
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

// Both capabilities are preview capabilities, so the server installs this
// extension only with vsql_allow_preview_extensions=ON.
VEF_GENERATE_ENTRY_POINTS(
    vsql::make_extension()
        .with(vsql_ai::g_sys_vars)
        .with(vsql_ai::g_keyring)
        .func(vsql::make_func<&vsql_ai::prompt_impl>("ai_prompt")
                  .returns(vsql::STRING)
                  .param(vsql::STRING)  // model ID from vsql_ai.models
                  .param(vsql::STRING)  // prompt
                  .buffer_size(65535)   // Large buffer for AI responses
                  .build())

        .func(vsql::make_func<&vsql_ai::embedding_impl>("ai_embedding")
                  .returns(vsql::STRING)
                  .param(vsql::STRING)  // model ID from vsql_ai.models
                  .param(vsql::STRING)  // text
                  .buffer_size(65535)
                  .build()))
