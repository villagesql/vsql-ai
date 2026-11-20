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

#ifndef VSQL_AI_PRIVILEGE_H
#define VSQL_AI_PRIVILEGE_H

#include <cstddef>
#include <string>

#include <villagesql/preview/mysql_services.h>

namespace vsql_ai {

// The dynamic privilege a caller needs to use any vsql_ai function:
//   GRANT VSQL_AI_USER ON *.* TO app@'%';
// MySQL does not grant a newly registered dynamic privilege to existing
// accounts, root included, so it has to be granted explicitly.
constexpr char kPrivilegeName[] = "VSQL_AI_USER";

// The MySQL component services the check uses, acquired at load. Pass to
// .with() when registering the extension.
extern vsql::preview_mysql_services::MysqlServices g_mysql_services;

// Storage for vsql_ai.require_privilege (default ON). A bool is written in
// place by the server, so it may be read directly.
extern bool g_require_privilege;

// Register / unregister VSQL_AI_USER. Called from the extension's on_init
// and on_deinit hooks. Grants already given survive an unregister and apply
// again once the privilege is registered.
void register_privilege();
void unregister_privilege();

// Whether the account running the current statement may call vsql_ai
// functions: always when vsql_ai.require_privilege is OFF, otherwise only if
// it holds VSQL_AI_USER. The check uses the statement's active security
// context, so a SQL SECURITY DEFINER routine is checked as its definer.
// Returns false and sets *error when the call must be refused.
bool caller_may_use(std::string* error);

}  // namespace vsql_ai

#endif  // VSQL_AI_PRIVILEGE_H
