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

#include "decisions.h"

#include <string>

#include "ai_providers.h"
#include "http_client.h"

namespace vsql_ai {
namespace decisions {

namespace {

// Decision models answer in one pass without generating text, so they are
// faster than the chat models, but a large batch of questions still takes a
// while. Same ceiling as the other cloud calls.
constexpr int kDecisionTimeoutSeconds = 120;

// Limits every supported provider accepts. Google allows up to 255 score
// levels and TypeSafe a single choice option, but OpenAI and TypeSafe cap
// score levels at 10 and Google and OpenAI need at least two choice options.
constexpr size_t kMaxNameLength = 64;
constexpr size_t kMinChoiceOptions = 2;
constexpr size_t kMaxChoiceOptions = 255;
constexpr size_t kMinScoreLevels = 2;
constexpr size_t kMaxScoreLevels = 10;

// Provider text can be anything, and dump() must not throw on the way out.
std::string safe_dump(const json& value) {
  return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

bool valid_name(const std::string& name) {
  if (name.empty() || name.size() > kMaxNameLength) return false;
  for (char c : name) {
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool check_question(const std::string& name, const json& question,
                    std::string* error) {
  const std::string where = "question '" + name + "'";
  if (!question.is_object()) {
    *error = where + " must be a JSON object";
    return false;
  }
  for (auto field = question.begin(); field != question.end(); ++field) {
    if (field.key() != "type" && field.key() != "instructions" &&
        field.key() != "criteria") {
      *error = where + " has unknown field '" + field.key() + "'";
      return false;
    }
  }

  auto type = question.find("type");
  if (type == question.end() || !type->is_string()) {
    *error = where + " needs a type: noul, choice or score";
    return false;
  }
  auto instructions = question.find("instructions");
  if (instructions == question.end() || !instructions->is_string() ||
      instructions->get_ref<const std::string&>().empty()) {
    *error = where + " needs instructions, a non-empty string";
    return false;
  }

  const std::string& kind = type->get_ref<const std::string&>();
  auto criteria = question.find("criteria");

  if (kind == "noul") {
    if (criteria == question.end()) return true;
    if (!criteria->is_object()) {
      *error = where + ": noul criteria must be an object with \"true\" "
                       "and \"false\" descriptions";
      return false;
    }
    for (auto item = criteria->begin(); item != criteria->end(); ++item) {
      if ((item.key() != "true" && item.key() != "false") ||
          !item->is_string()) {
        *error = where + ": noul criteria may only hold \"true\" and "
                         "\"false\" descriptions, as strings";
        return false;
      }
    }
    return true;
  }

  if (kind == "choice") {
    if (criteria == question.end() || !criteria->is_object() ||
        criteria->size() < kMinChoiceOptions ||
        criteria->size() > kMaxChoiceOptions) {
      *error = where + ": choice criteria must be an object of 2 to 255 "
                       "options, each mapped to a description";
      return false;
    }
    for (auto item = criteria->begin(); item != criteria->end(); ++item) {
      if (item.key().empty() || !item->is_string()) {
        *error = where + ": each choice option needs a non-empty name and "
                         "a string description (\"\" for none)";
        return false;
      }
    }
    return true;
  }

  if (kind == "score") {
    if (criteria == question.end() || !criteria->is_array() ||
        criteria->size() < kMinScoreLevels ||
        criteria->size() > kMaxScoreLevels) {
      *error = where + ": score criteria must be an array of 2 to 10 "
                       "levels, lowest first";
      return false;
    }
    for (const json& level : *criteria) {
      if (!level.is_string() || level.get_ref<const std::string&>().empty()) {
        *error = where + ": each score level must be a non-empty string";
        return false;
      }
    }
    return true;
  }

  *error = where + " has unknown type '" + kind +
           "'; use noul, choice or score";
  return false;
}

// Parse a response body that should be a JSON object.
bool parse_object(const std::string& body, json* out, std::string* error) {
  *out = json::parse(body, nullptr, false);
  if (out->is_discarded() || !out->is_object()) {
    *error = "Invalid response format: not a JSON object";
    return false;
  }
  return true;
}

// Google and TypeSafe already answer in the shared map shape, so their
// answers pass through unchanged: nothing is dropped and nothing is added.
bool map_answers(const json& response, json* answers, std::string* error) {
  auto found = response.find("answers");
  if (found == response.end() || !found->is_object()) {
    *error = "Invalid response format: missing answers";
    return false;
  }
  *answers = *found;
  return true;
}

// Look up `key` in an object, or nullptr.
const json* member(const json& object, const char* key) {
  auto it = object.find(key);
  return it == object.end() ? nullptr : &*it;
}

// OpenAI probabilities are an array of {value, probability[, label]}. Keys of
// the map are the values as text, so a score's level 0 becomes "0".
std::string value_key(const json& value) {
  if (value.is_string()) return value.get<std::string>();
  return safe_dump(value);
}

json translate_openai_answer(const json& answer) {
  const json* type = member(answer, "type");
  const std::string kind =
      type != nullptr && type->is_string() ? type->get<std::string>() : "";

  json out = json::object();
  if (kind == "predicate") {
    out["type"] = "noul";
    if (const json* p = member(answer, "probability")) out["noul"] = *p;
    return out;
  }
  if (kind == "choice" || kind == "score") {
    out["type"] = kind;
    if (const json* v = member(answer, kind.c_str())) out[kind] = *v;
    if (const json* list = member(answer, "probabilities");
        list != nullptr && list->is_array()) {
      json probabilities = json::object();
      json legend = json::object();
      for (const json& item : *list) {
        if (!item.is_object()) continue;
        const json* value = member(item, "value");
        if (value == nullptr) continue;
        const std::string key = value_key(*value);
        if (const json* p = member(item, "probability")) {
          probabilities[key] = *p;
        }
        if (const json* label = member(item, "label")) legend[key] = *label;
      }
      out["probabilities"] = probabilities;
      if (kind == "score" && !legend.empty()) out["legend"] = legend;
    }
    if (const json* c = member(answer, "confidence")) out["confidence"] = *c;
    return out;
  }
  if (kind == "refusal") {
    out["type"] = "refusal";
    return out;
  }
  // A type this version does not know: keep it as the provider sent it.
  out = answer;
  out.erase("name");
  return out;
}

}  // namespace

bool parse_questions(const std::string& questions_text, json* questions,
                     std::string* error) {
  *questions = json::parse(questions_text, nullptr, false);
  if (questions->is_discarded()) {
    *error = "Questions are not valid JSON";
    return false;
  }
  if (!questions->is_object() || questions->empty()) {
    *error = "Questions must be a JSON object of one or more named questions";
    return false;
  }
  for (auto item = questions->begin(); item != questions->end(); ++item) {
    if (!valid_name(item.key())) {
      *error = "Question name '" + truncate_utf8(item.key(), 64) +
               "' must be 1-64 characters of A-Z, a-z, 0-9, _ and -";
      return false;
    }
    if (!check_question(item.key(), *item, error)) return false;
  }
  return true;
}

std::string google_request(const std::string& model, const std::string& input,
                           const json& questions) {
  json translated = json::object();
  for (auto item = questions.begin(); item != questions.end(); ++item) {
    json question = *item;
    // Google names the noul criteria true_description / false_description.
    if (question["type"] == "noul" && question.contains("criteria")) {
      json criteria = json::object();
      const json& given = question["criteria"];
      if (given.contains("true")) criteria["true_description"] = given["true"];
      if (given.contains("false")) {
        criteria["false_description"] = given["false"];
      }
      question["criteria"] = criteria;
    }
    translated[item.key()] = question;
  }
  json body = {{"model", model}, {"input", input}, {"questions", translated}};
  return body.dump();
}

std::string typesafe_request(const std::string& model,
                             const std::string& input, const json& questions) {
  // TypeSafe takes the shared shape as is; the shared input is its "state".
  json body = {{"model", model}, {"state", input}, {"questions", questions}};
  return body.dump();
}

std::string openai_request(const std::string& model, const std::string& input,
                           const json& questions) {
  json translated = json::array();
  for (auto item = questions.begin(); item != questions.end(); ++item) {
    const json& question = *item;
    const std::string kind = question["type"].get<std::string>();
    std::string instructions = question["instructions"].get<std::string>();
    json out = {{"name", item.key()}};

    if (kind == "noul") {
      // OpenAI's predicate takes no criteria, so the descriptions become part
      // of the instructions rather than being dropped.
      out["type"] = "predicate";
      if (question.contains("criteria")) {
        const json& criteria = question["criteria"];
        if (criteria.contains("true")) {
          instructions += "\nAnswer true if: " +
                          criteria["true"].get<std::string>();
        }
        if (criteria.contains("false")) {
          instructions += "\nAnswer false if: " +
                          criteria["false"].get<std::string>();
        }
      }
    } else if (kind == "choice") {
      out["type"] = "choice";
      json choices = json::array();
      for (auto option = question["criteria"].begin();
           option != question["criteria"].end(); ++option) {
        json choice = {{"value", option.key()}};
        const std::string& description = option->get_ref<const std::string&>();
        if (!description.empty()) choice["description"] = description;
        choices.push_back(choice);
      }
      out["choices"] = choices;
    } else {  // score
      out["type"] = "score";
      json levels = json::array();
      for (const json& level : question["criteria"]) {
        levels.push_back({{"label", level}});
      }
      out["levels"] = levels;
    }
    out["instructions"] = instructions;
    translated.push_back(out);
  }
  json body = {{"model", model}, {"input", input}, {"questions", translated}};
  return body.dump();
}

bool google_answers(const std::string& body, json* answers,
                    std::string* error) {
  json response;
  if (!parse_object(body, &response, error)) return false;

  // A blocked input can come back as HTTP 200 with prompt_feedback set and no
  // answers, rather than as an error.
  if (const json* feedback = member(response, "prompt_feedback");
      feedback != nullptr && feedback->is_object()) {
    const json* reason = member(*feedback, "block_reason");
    if (reason != nullptr && !reason->is_null()) {
      *error = "Input blocked by the provider (" + value_key(*reason) + ")";
      if (const json* message = member(*feedback, "block_reason_message");
          message != nullptr && message->is_string()) {
        *error += ": " + message->get<std::string>();
      }
      return false;
    }
  }
  return map_answers(response, answers, error);
}

bool typesafe_answers(const std::string& body, json* answers,
                      std::string* error) {
  json response;
  if (!parse_object(body, &response, error)) return false;
  return map_answers(response, answers, error);
}

bool openai_answers(const std::string& body, json* answers,
                    std::string* error) {
  json response;
  if (!parse_object(body, &response, error)) return false;
  const json* list = member(response, "answers");
  if (list == nullptr || !list->is_array()) {
    *error = "Invalid response format: missing answers";
    return false;
  }

  // Every question is sent with its name, and OpenAI echoes it.
  *answers = json::object();
  for (const json& answer : *list) {
    if (!answer.is_object()) continue;
    const json* name = member(answer, "name");
    if (name == nullptr || !name->is_string()) {
      *error = "Invalid response format: answer without a question name";
      return false;
    }
    (*answers)[name->get<std::string>()] = translate_openai_answer(answer);
  }
  return true;
}

std::string http_error(int status_code, const std::string& body) {
  json response = json::parse(body, nullptr, false);
  if (!response.is_discarded() && response.is_object()) {
    // Google and OpenAI: {"error": {"message": ...}}.
    if (const json* node = member(response, "error")) {
      if (node->is_object()) {
        if (const json* message = member(*node, "message");
            message != nullptr && message->is_string()) {
          return message->get<std::string>();
        }
      }
      if (node->is_string()) return node->get<std::string>();
    }
    // TypeSafe: {"detail": "..."}, {"detail": {"message": ...}}, or for a
    // validation failure {"detail": [{"loc": [...], "msg": ...}, ...]}.
    if (const json* detail = member(response, "detail")) {
      if (detail->is_string()) return detail->get<std::string>();
      if (detail->is_object()) {
        if (const json* message = member(*detail, "message");
            message != nullptr && message->is_string()) {
          return message->get<std::string>();
        }
      }
      if (detail->is_array() && !detail->empty() && (*detail)[0].is_object()) {
        const json& first = (*detail)[0];
        std::string text;
        if (const json* msg = member(first, "msg"); msg && msg->is_string()) {
          text = msg->get<std::string>();
        }
        if (const json* loc = member(first, "loc"); loc && loc->is_array()) {
          std::string path;
          for (const json& part : *loc) {
            if (!path.empty()) path += ".";
            path += value_key(part);
          }
          text += " (" + path + ")";
        }
        if (!text.empty()) return text;
      }
    }
    return "HTTP " + std::to_string(status_code) + ": " +
           truncate_utf8(safe_dump(response), 100);
  }
  // Not JSON at all, as OpenAI's gateway sends for a bad key.
  return "HTTP " + std::to_string(status_code) + ": " +
         truncate_utf8(body, 100);
}

}  // namespace decisions

// =============================================================================
// decide() for the providers that offer a decisions API
// =============================================================================

namespace {

using decisions::json;

using AnswerParser = bool (*)(const std::string&, json*, std::string*);

// HttpClient sends the body as application/json itself; adding a
// Content-Type header here would send it twice, which OpenAI and TypeSafe
// reject as an invalid body.
std::string post_decision(const std::string& host, const std::string& path,
                          const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          AnswerParser parse, std::string* error) {
  HttpClient::Response response =
      HttpClient::post(host, path, body, headers, decisions::kDecisionTimeoutSeconds);
  if (!response.error.empty()) {
    *error = response.error;
    return "";
  }
  if (!response.is_success()) {
    *error = decisions::http_error(response.status_code, response.body);
    return "";
  }
  json answers;
  if (!parse(response.body, &answers, error)) return "";
  return answers.dump(-1, ' ', false, json::error_handler_t::replace);
}

}  // namespace

std::string AIProvider::decide(const std::string&, const std::string&,
                               const std::string&, const decisions::json&,
                               std::string* error) {
  *error =
      "This provider has no decisions API; use google, openai or typesafe";
  return "";
}

std::string GoogleProvider::decide(const std::string& model,
                                   const std::string& api_key,
                                   const std::string& input,
                                   const decisions::json& questions,
                                   std::string* error) {
  return post_decision("https://generativelanguage.googleapis.com",
                       "/v1beta/decisions",
                       decisions::google_request(model, input, questions),
                       {{"x-goog-api-key", api_key}},
                       decisions::google_answers, error);
}

std::string OpenAIProvider::decide(const std::string& model,
                                   const std::string& api_key,
                                   const std::string& input,
                                   const decisions::json& questions,
                                   std::string* error) {
  return post_decision("https://api.openai.com", "/v1/decisions",
                       decisions::openai_request(model, input, questions),
                       {{"Authorization", "Bearer " + api_key}},
                       decisions::openai_answers, error);
}

std::string TypeSafeProvider::decide(const std::string& model,
                                     const std::string& api_key,
                                     const std::string& input,
                                     const decisions::json& questions,
                                     std::string* error) {
  return post_decision("https://api.typesafe.ai", "/v1/systemone",
                       decisions::typesafe_request(model, input, questions),
                       {{"Authorization", "Bearer " + api_key}},
                       decisions::typesafe_answers, error);
}

std::string TypeSafeProvider::prompt(const std::string&, const std::string&,
                                     const std::string&, std::string* error) {
  *error = "TypeSafe offers only decisions; use ai_decision()";
  return "";
}

std::string TypeSafeProvider::embed(const std::string&, const std::string&,
                                    const std::string&, std::string* error) {
  *error = "TypeSafe offers only decisions; use ai_decision()";
  return "";
}

}  // namespace vsql_ai
