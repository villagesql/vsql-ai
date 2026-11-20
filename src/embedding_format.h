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

#ifndef VSQL_AI_EMBEDDING_FORMAT_H
#define VSQL_AI_EMBEDDING_FORMAT_H

#include <string>

namespace vsql_ai {

// Convert an embedding from the JSON array every provider returns
// ("[0.1, -0.2, ...]") to packed little-endian IEEE 754 float32: four bytes
// per dimension, in order, with no header. That is the layout MySQL's VECTOR
// type stores, so the result can go into a VECTOR column, or a VARBINARY or
// BLOB column, and be decoded by any client.
//
// Fails, setting *error, on anything that is not a non-empty array of finite
// numbers that fit in a float. A value too small for a normal float is
// rounded as the C++ conversion does (towards zero), not rejected.
bool pack_float32_le(const std::string& json_array, std::string* bytes,
                     std::string* error);

}  // namespace vsql_ai

#endif  // VSQL_AI_EMBEDDING_FORMAT_H
