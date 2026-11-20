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

#include "embedding_format.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "nlohmann/json.hpp"

namespace vsql_ai {

bool pack_float32_le(const std::string& json_array, std::string* bytes,
                     std::string* error) {
  using json = nlohmann::json;
  json values = json::parse(json_array, nullptr, false);
  if (values.is_discarded() || !values.is_array() || values.empty()) {
    *error = "Embedding is not a non-empty JSON array";
    return false;
  }

  bytes->clear();
  bytes->reserve(values.size() * 4);
  for (const json& value : values) {
    if (!value.is_number()) {
      *error = "Embedding contains a value that is not a number";
      return false;
    }
    double d = value.get<double>();
    if (!std::isfinite(d) || std::fabs(d) > FLT_MAX) {
      *error = "Embedding contains a value that does not fit in a float";
      return false;
    }
    float f = static_cast<float>(d);
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    // Little-endian whatever the host's byte order.
    for (int shift = 0; shift < 32; shift += 8) {
      bytes->push_back(static_cast<char>((bits >> shift) & 0xFF));
    }
  }
  return true;
}

}  // namespace vsql_ai
