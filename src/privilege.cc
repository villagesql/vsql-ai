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

#include "privilege.h"

#include <cstring>

#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/security_context.h>

namespace vsql_ai {

vsql::preview_mysql_services::MysqlServices g_mysql_services;
bool g_require_privilege = true;

// Must follow g_mysql_services in this file: each line registers itself with
// it during static initialization.
VSQL_REQUIRE_SERVICE(g_mysql_services, dynamic_privilege_register,
                     g_privilege_register);
VSQL_REQUIRE_SERVICE(g_mysql_services, global_grants_check, g_grants_check);
VSQL_REQUIRE_SERVICE(g_mysql_services, mysql_current_thread_reader,
                     g_thread_reader);
VSQL_REQUIRE_SERVICE(g_mysql_services, mysql_thd_security_context,
                     g_security_context);

void register_privilege() {
  // MySQL service methods return false on success. A failure leaves the
  // privilege unregistered, and caller_may_use() then refuses everyone while
  // the check is on, which is the safe outcome.
  if (g_privilege_register.valid()) {
    (void)g_privilege_register->register_privilege(
        kPrivilegeName, std::strlen(kPrivilegeName));
  }
}

void unregister_privilege() {
  if (g_privilege_register.valid()) {
    (void)g_privilege_register->unregister_privilege(
        kPrivilegeName, std::strlen(kPrivilegeName));
  }
}

bool caller_may_use(std::string* error) {
  if (!g_require_privilege) return true;

  if (!g_grants_check.valid() || !g_thread_reader.valid() ||
      !g_security_context.valid()) {
    *error = "Cannot check the VSQL_AI_USER privilege: MySQL privilege "
             "services are unavailable";
    return false;
  }

  MYSQL_THD thd = nullptr;
  Security_context_handle context = nullptr;
  if (g_thread_reader->get(&thd) || thd == nullptr ||
      g_security_context->get(thd, &context) || context == nullptr) {
    *error = "Cannot check the VSQL_AI_USER privilege: no security context";
    return false;
  }

  // Unlike most service methods, has_global_grant returns true for "yes".
  if (g_grants_check->has_global_grant(context, kPrivilegeName,
                                        std::strlen(kPrivilegeName))) {
    return true;
  }
  *error = "Access denied; you need the VSQL_AI_USER privilege to use "
           "vsql_ai functions";
  return false;
}

}  // namespace vsql_ai
