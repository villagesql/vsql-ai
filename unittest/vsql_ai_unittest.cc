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

// Unit tests for code that SQL cannot reach, or cannot reach on demand:
//
//  - The exception guard around each SQL function (issue #41). The server
//    converts every STRING argument to valid utf8mb4 before the extension
//    sees it, so no SQL input makes json::dump() throw any more. Here the
//    entry points are called directly with invalid UTF-8.
//  - Embedding packing, request preparation for ai_request(), and decision
//    answer translation, including provider replies (an OpenAI refusal, a
//    Google input block) that a live test cannot provoke reliably.
//
// No network access is needed: every path here stops before a request is
// sent. Run with ctest, or run the binary directly.

#include <villagesql/vsql.h>
#include <villagesql/preview/sys_var.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "decisions.h"
#include "embedding_format.h"
#include "raw_request.h"

namespace vsql_ai {
// Defined in ai_functions.cc.
void prompt_impl(vsql::StringArg, vsql::StringArg, vsql::StringResult);
void embedding_impl(vsql::StringArg, vsql::StringArg, vsql::StringResult);
void request_impl(vsql::StringArg, vsql::StringArg, vsql::StringResult);
void on_models_change(vsql::preview_sys_var::SysVarChange);
}  // namespace vsql_ai

namespace {

int g_failures = 0;

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,     \
                   __LINE__, #cond);                                  \
      ++g_failures;                                                   \
    }                                                                 \
  } while (0)

using vsql_ai::decisions::json;

// A STRING argument as the server passes it.
struct Arg {
  vef_invalue_t value{};
  explicit Arg(const std::string& text) {
    value.is_null = false;
    value.str_value = text.data();
    value.str_len = text.size();
  }
  vsql::StringArg get() const { return vsql::StringArg(&value); }
};

// A STRING result buffer as the server provides it.
struct Result {
  char buf[65535];
  char error[VEF_MAX_ERROR_LEN];
  char* alt = nullptr;
  vef_vdf_result_t raw{};
  Result() {
    error[0] = '\0';
    raw.error_msg = error;
    raw.str_buf = buf;
    raw.max_str_len = sizeof(buf);
    raw.alt_str_buf = &alt;
    raw.type = VEF_RESULT_VALUE;
  }
  vsql::StringResult get() { return vsql::StringResult(&raw); }
};

void set_registry(const char* value) {
  vef_sys_var_change_t change{};
  change.var_name = "models";
  change.type = VEF_VAR_STR;
  change.str_val = value;
  vsql_ai::on_models_change(vsql::preview_sys_var::SysVarChange(&change));
}

// Issue #41: invalid UTF-8 reaching an entry point is reported as a warning,
// never thrown across the extension boundary. A local model needs no key, and
// the request body is serialized before any connection is attempted.
void test_exception_guard() {
  set_registry(R"({"local": {"provider": "local", "model": "m"}})");
  const std::string invalid = "abc\xFF\xFE" "def";  // split: \xFE would eat "def"

  {
    Arg model("local"), prompt(invalid);
    Result out;
    vsql_ai::prompt_impl(model.get(), prompt.get(), out.get());
    CHECK(out.raw.type == VEF_RESULT_WARNING);
    CHECK(std::strstr(out.error, "Internal error") != nullptr);
  }
  {
    Arg model("local"), text(invalid);
    Result out;
    vsql_ai::embedding_impl(model.get(), text.get(), out.get());
    CHECK(out.raw.type == VEF_RESULT_WARNING);
    CHECK(std::strstr(out.error, "Internal error") != nullptr);
  }
  {
    // ai_request parses the body without exceptions, so invalid UTF-8 is an
    // ordinary "not a JSON object" warning rather than an internal error.
    Arg model("local"), body("{\"messages\": \"" + invalid + "\"}");
    Result out;
    vsql_ai::request_impl(model.get(), body.get(), out.get());
    CHECK(out.raw.type == VEF_RESULT_WARNING);
  }
  set_registry("");
}

std::string hex(const std::string& bytes) {
  static const char digits[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : bytes) {
    out += digits[c >> 4];
    out += digits[c & 0xF];
  }
  return out;
}

void test_pack_float32_le() {
  std::string bytes, error;
  CHECK(vsql_ai::pack_float32_le("[1.0, -2.5, 0, 1.5e-05]", &bytes, &error));
  // 1.0f = 3F800000, -2.5f = C0200000, 0, 1.5e-05f = 377BA882, little-endian
  CHECK(hex(bytes) == "0000803F000020C00000000082A87B37");

  CHECK(!vsql_ai::pack_float32_le("[]", &bytes, &error));
  CHECK(!vsql_ai::pack_float32_le("{\"a\": 1}", &bytes, &error));
  CHECK(!vsql_ai::pack_float32_le("[1, \"2\"]", &bytes, &error));
  CHECK(!vsql_ai::pack_float32_le("[1e39]", &bytes, &error));
  CHECK(!vsql_ai::pack_float32_le("not json", &bytes, &error));
}

void test_raw_request_prepare() {
  using vsql_ai::raw_request::ModelPlacement;
  using vsql_ai::raw_request::prepare;
  json body;
  std::string error;

  CHECK(prepare(R"({"messages": []})", "m", ModelPlacement::kInBody, false,
                &body, &error));
  CHECK(body["model"] == "m");
  CHECK(!body.contains("stream"));

  CHECK(prepare(R"({"model": "m"})", "m", ModelPlacement::kInPath, false,
                &body, &error));
  CHECK(!body.contains("model"));

  CHECK(prepare("{}", "m", ModelPlacement::kInBody, true, &body, &error));
  CHECK(body["stream"] == false);

  CHECK(!prepare(R"({"model": "other"})", "m", ModelPlacement::kInBody, false,
                 &body, &error));
  CHECK(!prepare(R"({"stream": true})", "m", ModelPlacement::kInBody, false,
                 &body, &error));
  CHECK(!prepare("[]", "m", ModelPlacement::kInBody, false, &body, &error));
}

void test_openai_answers() {
  namespace d = vsql_ai::decisions;
  json answers;
  std::string error;
  CHECK(d::openai_answers(R"({"answers": [
      {"type": "predicate", "name": "a", "probability": 0.25},
      {"type": "choice", "name": "b", "choice": "x", "confidence": 0.9,
       "probabilities": [{"value": "x", "probability": 0.9},
                         {"value": "y", "probability": 0.1}]},
      {"type": "score", "name": "c", "score": 1.5, "confidence": 0.5,
       "probabilities": [{"value": 0, "label": "low", "probability": 0.0},
                         {"value": 1, "label": "mid", "probability": 0.5},
                         {"value": 2, "label": "high", "probability": 0.5}]},
      {"type": "refusal", "name": "d"}]})",
                          &answers, &error));
  CHECK(answers["a"] == json({{"type", "noul"}, {"noul", 0.25}}));
  CHECK(answers["b"]["probabilities"]["y"] == 0.1);
  CHECK(!answers["a"].contains("confidence"));
  CHECK(answers["c"]["legend"]["2"] == "high");
  CHECK(answers["c"]["probabilities"]["1"] == 0.5);
  CHECK(answers["d"] == json({{"type", "refusal"}}));

  CHECK(!d::openai_answers(R"({"answers": [{"type": "predicate"}]})",
                           &answers, &error));
  CHECK(!d::openai_answers("[]", &answers, &error));
}

void test_google_block_and_errors() {
  namespace d = vsql_ai::decisions;
  json answers;
  std::string error;
  CHECK(!d::google_answers(R"({"answers": {}, "prompt_feedback":
      {"block_reason": "safety", "block_reason_message": "blocked"}})",
                           &answers, &error));
  CHECK(error.find("safety") != std::string::npos);

  CHECK(d::http_error(401, "Identity Edge route authentication") ==
        "HTTP 401: Identity Edge route authentication");
  CHECK(d::http_error(422, R"({"detail": [{"loc": ["body", "state"],
      "msg": "Field required", "type": "missing"}]})") ==
        "Field required (body.state)");
  CHECK(d::http_error(401, R"({"detail": {"message": "bad key"}})") ==
        "bad key");
  CHECK(d::http_error(400, R"({"error": {"message": "nope"}})") == "nope");
}

}  // namespace

int main() {
  test_exception_guard();
  test_pack_float32_le();
  test_raw_request_prepare();
  test_openai_answers();
  test_google_block_and_errors();
  if (g_failures == 0) std::printf("All unit tests passed\n");
  return g_failures == 0 ? 0 : 1;
}
