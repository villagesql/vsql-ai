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

#include "model_registry.h"

#include <memory>

#include "ai_providers.h"
#include "nlohmann/json.hpp"

namespace vsql_ai {

using json = nlohmann::json;

namespace {

// Copy a string member into *out. A missing member leaves *out empty; any
// other type is an error, since a number or object there is a mistake.
bool read_string_field(const json& entry, const char* field,
                       const std::string& model_id, std::string* out,
                       std::string* error) {
  auto it = entry.find(field);
  if (it == entry.end()) return true;
  if (!it->is_string()) {
    *error = std::string("vsql_ai.models: '") + field + "' of model '" +
             model_id + "' must be a string";
    return false;
  }
  *out = it->get<std::string>();
  return true;
}

}  // namespace

bool find_model(const std::string& registry_json, const std::string& model_id,
                ModelConfig* config, std::string* error) {
  if (registry_json.empty()) {
    *error = "Model '" + model_id +
             "' is not registered: vsql_ai.models is empty";
    return false;
  }

  // Parse without exceptions: a malformed value is an ordinary user error.
  json registry = json::parse(registry_json, nullptr, false);
  if (registry.is_discarded()) {
    *error = "vsql_ai.models is not valid JSON";
    return false;
  }
  if (!registry.is_object()) {
    *error = "vsql_ai.models must be a JSON object keyed by model ID";
    return false;
  }

  const json& const_registry = registry;
  auto entry_it = const_registry.find(model_id);
  if (entry_it == const_registry.end()) {
    *error = "Model '" + model_id + "' is not registered in vsql_ai.models";
    return false;
  }
  const json& entry = *entry_it;
  if (!entry.is_object()) {
    *error = "vsql_ai.models: model '" + model_id + "' must be a JSON object";
    return false;
  }

  for (auto field = entry.begin(); field != entry.end(); ++field) {
    const std::string& name = field.key();
    if (name != "provider" && name != "model" && name != "key_id" &&
        name != "key_owner") {
      *error = "vsql_ai.models: unknown field '" + name + "' in model '" +
               model_id + "'";
      return false;
    }
  }

  ModelConfig result;
  if (!read_string_field(entry, "provider", model_id, &result.provider,
                         error) ||
      !read_string_field(entry, "model", model_id, &result.model, error) ||
      !read_string_field(entry, "key_id", model_id, &result.key_id, error) ||
      !read_string_field(entry, "key_owner", model_id, &result.key_owner,
                         error)) {
    return false;
  }

  if (result.provider.empty()) {
    *error = "vsql_ai.models: model '" + model_id + "' has no provider";
    return false;
  }
  if (result.model.empty()) {
    *error = "vsql_ai.models: model '" + model_id + "' has no model name";
    return false;
  }

  std::unique_ptr<AIProvider> provider = create_provider(result.provider);
  if (!provider) {
    *error = "vsql_ai.models: model '" + model_id + "' has unknown provider '" +
             result.provider + "'";
    return false;
  }
  if (result.key_id.empty() && provider->requires_api_key()) {
    *error = "vsql_ai.models: model '" + model_id +
             "' needs key_id, the keyring ID of its API key";
    return false;
  }

  *config = std::move(result);
  return true;
}

}  // namespace vsql_ai
