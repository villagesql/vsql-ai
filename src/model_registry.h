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

#ifndef VSQL_AI_MODEL_REGISTRY_H
#define VSQL_AI_MODEL_REGISTRY_H

#include <string>

namespace vsql_ai {

// One entry of the vsql_ai.models system variable. The registry maps a model
// ID that SQL callers use to the provider, the provider's own model name, and
// where in the MySQL keyring the API key lives. It holds no secrets itself, so
// SELECT @@vsql_ai.models is safe to show.
struct ModelConfig {
  std::string provider;   // anthropic, google, openai or local
  std::string model;      // the provider's model name, passed through as is
  std::string key_id;     // keyring data ID of the API key
  std::string key_owner;  // keyring owner; empty reads an internal key
};

// Look up model_id in registry_json, the value of vsql_ai.models: a JSON
// object whose members are model IDs, e.g.
//
//   {"claude": {"provider": "anthropic", "model": "claude-opus-5-5",
//               "key_id": "anthropic_key", "key_owner": "root@localhost"}}
//
// Validates only the requested entry, so one malformed entry does not take
// the others down with it. Rejects unknown fields, which are far more likely
// to be a misspelt key_id than something meant to be ignored, and requires
// key_id for every provider that authenticates.
//
// Returns true and fills *config on success; returns false and sets *error to
// a message fit for a SQL warning otherwise.
bool find_model(const std::string& registry_json, const std::string& model_id,
                ModelConfig* config, std::string* error);

}  // namespace vsql_ai

#endif  // VSQL_AI_MODEL_REGISTRY_H
