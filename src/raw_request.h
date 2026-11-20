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

#ifndef VSQL_AI_RAW_REQUEST_H
#define VSQL_AI_RAW_REQUEST_H

#include <string>

#include "nlohmann/json.hpp"

namespace vsql_ai {
namespace raw_request {

using json = nlohmann::json;

// How a provider's endpoint names the model.
enum class ModelPlacement {
  kInBody,  // a "model" member of the body (Anthropic, OpenAI, Ollama, TypeSafe)
  kInPath,  // the URL path (Google); the body must not carry it
};

// Parse body_text for ai_request() and make it safe to send for the
// registered model:
//  - it must be a JSON object;
//  - a "model" member naming any other model is refused, because the registry
//    decides which model (and so which price) a caller may use. A matching
//    one is kept for kInBody and removed for kInPath; a missing one is added
//    for kInBody;
//  - "stream": true is refused, because the response must come back whole.
//    stream_default_false adds "stream": false when absent, for Ollama, which
//    streams unless told not to.
// Returns false and sets *error on the first problem.
bool prepare(const std::string& body_text, const std::string& model,
             ModelPlacement placement, bool stream_default_false, json* body,
             std::string* error);

}  // namespace raw_request
}  // namespace vsql_ai

#endif  // VSQL_AI_RAW_REQUEST_H
