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

#ifndef VSQL_AI_DECISIONS_H
#define VSQL_AI_DECISIONS_H

#include <string>

#include "nlohmann/json.hpp"

// ai_decision() takes questions in one shape and returns answers in one
// shape, whatever the provider. Both follow the map form that Google's Gemini
// Decisions API and TypeSafe's System One API use natively; OpenAI's array
// form is translated to and from it.
//
// Questions: a JSON object keyed by question name.
//   {"is_spam": {"type": "noul",   "instructions": "...",
//                "criteria": {"true": "...", "false": "..."}},   // optional
//    "tone":    {"type": "choice", "instructions": "...",
//                "criteria": {"angry": "...", "calm": "..."}},
//    "urgency": {"type": "score",  "instructions": "...",
//                "criteria": ["Can wait", "This week", "Today"]}}
//
// Answers: a JSON object keyed by the same names.
//   noul:    {"type": "noul", "noul": P(true), "confidence"?}
//   choice:  {"type": "choice", "choice", "probabilities": {option: p},
//             "confidence"}
//   score:   {"type": "score", "score", "probabilities": {"0": p, ...},
//             "legend": {"0": text, ...}, "confidence"}
//   refusal: {"type": "refusal"}   (OpenAI only: the model declined)
// A field a provider does not return is left out, never made up: TypeSafe and
// OpenAI give no confidence for a noul answer.

namespace vsql_ai {
namespace decisions {

using json = nlohmann::json;

// Parse and check questions_text against the shape above, with limits every
// supported provider accepts: names of 1-64 characters from [A-Za-z0-9_-],
// 2-255 choice options, and 2-10 score levels. Returns false and sets *error
// on the first problem found.
bool parse_questions(const std::string& questions_text, json* questions,
                     std::string* error);

// Request bodies for each provider. `questions` must have passed
// parse_questions().
std::string google_request(const std::string& model, const std::string& input,
                           const json& questions);
std::string typesafe_request(const std::string& model,
                             const std::string& input, const json& questions);
std::string openai_request(const std::string& model, const std::string& input,
                           const json& questions);

// Turn a successful (2xx) response body into the answers object above.
// Return false and set *error if the body is not the expected shape, or if
// the provider blocked the input.
bool google_answers(const std::string& body, json* answers,
                    std::string* error);
bool typesafe_answers(const std::string& body, json* answers,
                      std::string* error);
bool openai_answers(const std::string& body, json* answers,
                    std::string* error);

// Describe a non-2xx response. The three providers report errors in
// different shapes ({"error": {...}}, {"detail": ...}, or plain text).
std::string http_error(int status_code, const std::string& body);

}  // namespace decisions
}  // namespace vsql_ai

#endif  // VSQL_AI_DECISIONS_H
