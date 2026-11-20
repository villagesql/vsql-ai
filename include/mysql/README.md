# Vendored MySQL component service headers

Verbatim copies from the VillageSQL server source tree
(`include/mysql/components/`, villagesql-server commit 53c1e76a4336), used by
the privilege check through the VEF `mysql_services` preview capability:

| File | Services |
|---|---|
| `service.h` | The `BEGIN_SERVICE_DEFINITION` / `DECLARE_*_METHOD` macros |
| `services/dynamic_privilege.h` | `dynamic_privilege_register`, `global_grants_check` |
| `services/mysql_current_thread_reader.h` | `mysql_current_thread_reader` |
| `services/security_context.h` | `mysql_thd_security_context` |

A service definition exists only in a server source tree, not in the
installed server or the extension SDK, so these four are vendored to keep the
extension buildable against the SDK alone. They are GPL-2.0 (with the
Universal FOSS Exception), like this extension. Do not edit them; refresh
them from the server tree if a service definition changes.
