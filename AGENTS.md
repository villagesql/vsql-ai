# AGENTS.md

This file provides guidance to AI coding assistants when working with code in this repository.

**Note**: Also check `AGENTS.local.md` for additional local development instructions when present.

## Project Overview

This is the VillageSQL AI Extension (vsql-ai) that adds AI capabilities to VillageSQL Server. It provides functions to interact with AI models from Anthropic Claude, Google Gemini, OpenAI GPT, and local Ollama directly from SQL queries, including text generation (prompting) and text embeddings.

## Build System

- **Build**: `cd build && cmake .. && make`
- **Create VEB package**: Automatically created by `make` as `veb`
- **Install extension**: `make install` (copies VEB to configured install directory)

The build process:
1. Uses CMake to configure build with VillageSQL SDK via `FindVillageSQL.cmake`
2. Compiles C++ source files into shared library `so`
3. Packages library with `manifest.json` into `veb` archive
4. VEB can be installed to VillageSQL's VEB directory for use

For development with VillageSQL SDK:
```bash
cd build
cmake .. -DVillageSQL_SDK_DIR=/path/to/sdk -DVEB_INSTALL_DIR=/path/to/veb/directory
make
make install
```

## Architecture

**Core Components:**
- `src/ai_functions.cc` - VEF function implementations (`ai_prompt`, `ai_embedding`), the `vsql_ai.models` system variable, keyring reads, and extension registration
- `src/model_registry.h/cc` - Parses and validates `vsql_ai.models` entries
- `src/ai_providers.h` - Abstract provider interface
- `src/ai_providers.cc` - Concrete provider implementations (Anthropic, Google, OpenAI, Local/Ollama)
- `src/http_client.h/cc` - HTTP/HTTPS client for API calls
- `manifest.json` - Extension metadata (name, version, description, author, license)
- `CMakeLists.txt` - CMake build configuration
- `mysql-test/t/` - Test files directory (`.test` files using MTR framework)
- `mysql-test/r/` - Expected test results directory (`.result` files)
- `mysql-test/include/` - `keyring_setup.inc` / `keyring_teardown.inc` for tests that need a keyring

**Available Functions:**
- `ai_prompt(model_id, prompt)` - Send a prompt to a registered model and get a text response
- `ai_embedding(model_id, text)` - Generate a text embedding with a registered model

**Model registry and keys:** `model_id` is looked up in the `vsql_ai.models`
system variable, a JSON object mapping each ID to `provider`, `model`,
`key_id` and optional `key_owner`. The API key is read from the MySQL keyring
(`key_id`, `key_owner`) on every call; it never appears in SQL text. Both the
system variable and the keyring are VEF preview capabilities, so the server
needs `vsql_allow_preview_extensions=ON`.

**Dependencies:**
- Requires VillageSQL Extension SDK
- Uses VillageSQL Extension Framework (VEF) API
- C++ compiler with C++17 support
- OpenSSL development libraries (for HTTPS)
- Third-party headers (included): cpp-httplib, nlohmann/json

**Code Organization:**
- File naming: lowercase with underscores (e.g., `ai_functions.cc`)
- Function naming: lowercase with underscores (e.g., `prompt_impl`)
- Variable naming: lowercase with underscores (e.g., `provider_name`)
- Provider pattern: Abstract `AIProvider` base class with concrete implementations

## VillageSQL Extension Framework (VEF) Pattern — Protocol V3

This extension uses the Protocol V3 stable SDK (`<villagesql/vsql.h>`).

```cpp
#include <villagesql/vsql.h>

void my_function_impl(vsql::StringArg arg1, vsql::StringArg arg2,
                      vsql::StringResult out) {
    if (arg1.is_null() || arg2.is_null()) { out.set_null(); return; }
    std::string value1(arg1.value());
    // ...
    out.set(response);
}

VEF_GENERATE_ENTRY_POINTS(
    vsql::make_extension()
        .func(vsql::make_func<&my_function_impl>("my_function")
                  .returns(vsql::STRING).param(vsql::STRING).param(vsql::STRING)
                  .buffer_size(65535).build())
)
```

Qualify SDK names with `vsql::` — do not put a `using namespace vsql;` directive at file scope.

Use `out.set_null()` for NULL, `out.warning("msg")` for soft errors (Warning 3200 + NULL), `out.error("msg")` for hard errors. Extension name/version come from `manifest.json`; `make_extension()` takes no args in V3.

**Exception safety is the extension's responsibility.** The SDK does not wrap
VDF implementations in a try/catch, so an exception escaping an entry point
crosses the C ABI and terminates the server. This is reachable from ordinary
SQL — `nlohmann::json::dump()` throws on input that is not valid UTF-8. Every
entry point must wrap its body in `try` / `catch (const std::exception&)` /
`catch (...)` and convert the exception into `out.warning(...)`.

## Testing

The extension includes test files using the MySQL Test Runner (MTR) framework:
- **Test Location**:
  - `mysql-test/t/` directory contains `.test` files with SQL test commands
  - `mysql-test/r/` directory contains `.result` files with expected output
- **Run Tests**:
  ```bash
  cd <BUILD_DIR>/mysql-test
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test
  ```
  Where `<BUILD_DIR>` is your VillageSQL build directory
- **Create/Update Results**: Use `--record` flag to generate or update expected `.result` files:
  ```bash
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test --record
  ```
- Tests should validate function output and behavior
- Each test should install extension, run tests, and clean up (uninstall extension)
- **IMPORTANT**: Never manually edit `.result` files in `mysql-test/r/`. They must be generated by running tests with the `--record` flag.
- Every test needs a `<test>-master.opt` with `--vsql_allow_preview_extensions=ON`.
- Tests that restart the server (e.g. via `keyring_setup.inc`) must
  `UNINSTALL EXTENSION` before the restart and `INSTALL EXTENSION` after it;
  never assume the extension survives a restart.
- Storing a real key: wrap `keyring_key_store()` in `--disable_query_log` and
  `SET SESSION sql_log_off = ON/OFF` so the key reaches neither the result file
  nor the general query log.
- On a packaged server MTR prefers the bundled `lib/veb/vsql_ai.veb` over
  `--veb-source-dir`; pass `--mysqld=--veb-dir=<dir containing your build>` to
  test your own build.

## Extension Installation

After building and installing the VEB file with `make install`, load the extension in VillageSQL.
It uses preview capabilities (keyring, sys var), so preview extensions must be allowed first:

```sql
SET PERSIST vsql_allow_preview_extensions = ON;
INSTALL EXTENSION vsql_ai;
```

Then register models and test the functions (keys must already be in the
keyring, e.g. via `keyring_key_store()`):
```sql
SET GLOBAL vsql_ai.models = '{
  "gemini": {"provider": "google", "model": "gemini-3.5-flash-lite",
             "key_id": "gemini_key", "key_owner": "root@localhost"},
  "gemini-embed": {"provider": "google", "model": "gemini-embedding-001",
                   "key_id": "gemini_key", "key_owner": "root@localhost"},
  "llama": {"provider": "local", "model": "llama3.2"}
}';

SELECT ai_prompt('gemini', 'What is 2+2?');
SELECT ai_embedding('gemini-embed', 'Hello world');
SELECT ai_prompt('llama', 'What is 2+2?');  -- local Ollama, no key
```

## Provider Architecture

The extension uses a provider pattern to support multiple AI services:

### Base Provider Interface
```cpp
class AIProvider {
public:
  virtual std::string prompt(const std::string& model,
                            const std::string& api_key,
                            const std::string& prompt_text,
                            std::string* error) = 0;

  virtual std::string embed(const std::string& model,
                           const std::string& api_key,
                           const std::string& text,
                           std::string* error) = 0;
};
```

### Concrete Providers
- **AnthropicProvider**: Implements Claude API (messages endpoint)
- **GoogleProvider**: Implements Gemini API (generateContent, embedContent endpoints)
- **OpenAIProvider**: Implements OpenAI API (chat/completions, embeddings endpoints)
- **LocalProvider**: Connects to Ollama on 127.0.0.1:11434 — `/api/chat` (native, with `"think": false`) for prompting and the OpenAI-compatible `/v1/embeddings` for embeddings. No API key required; 300-second timeout.

### Adding a New Provider
1. Create a new class that inherits from `AIProvider`
2. Implement `prompt()` and `embed()` methods
3. Add factory method in `create_provider()` function
4. Add tests for the new provider

## Supported AI Models

Model names are passed through unchanged; only the provider is validated.

### Anthropic Claude
- claude-opus-5-5 (recommended default)
- claude-fable-5-2 (most capable)
- claude-sonnet-5-5
- claude-haiku-5-5
- Previous generations: claude-fable-5-1, claude-fable-5, claude-opus-5,
  claude-opus-4-8, claude-opus-4-7, claude-opus-4-6, claude-sonnet-5,
  claude-sonnet-4-6, claude-haiku-4-5

Opus 5.5 and Fable 5.2 always think; thinking counts against `max_tokens`,
which is why the request ceiling is 16000 rather than a small value.

### Google Gemini
**Generative:**
- gemini-3.8-flash
- gemini-3.6-flash
- gemini-3.5-flash-lite (used by the tests)
- gemini-4-hydrogen (and gemini-4-carbon, gemini-4-neon, gemini-4-argon)
- gemini-3.1-pro-preview
- Deprecated: gemini-2.5-flash, gemini-2.5-pro (rerouted Dec 1, 2026)

**Embeddings:**
- gemini-embedding-2
- gemini-embedding-001 (3072 dimensions by default)

### OpenAI
**Chat:**
- gpt-6.2-astra
- gpt-6.2-sol
- gpt-6-luna (used by the tests)
- Deprecated: gpt-4o, gpt-4o-mini (shut down Jan 14, 2027)

**Embeddings:**
- text-embedding-3-small (1536 dimensions)
- text-embedding-3-large (3072 dimensions)

### Local (Ollama on 127.0.0.1:11434)
Any model pulled in Ollama. No API key required. Common models:
- llama3.2, mistral, gemma2 (chat)
- nomic-embed-text, mxbai-embed-large (embeddings)

## Common Tasks for AI Agents

When asked to add functionality to this extension:

1. **Adding a new AI provider**: Create provider class, implement interface methods, add to factory, add tests
2. **Adding a new function**: Create implementation in `ai_functions.cc`, register in `VEF_GENERATE_ENTRY_POINTS`, create tests
3. **Modifying API requests**: Update provider implementation in `ai_providers.cc`
4. **Testing**: Create or update `.test` files in `mysql-test/t/` directory, generate expected results with `--record`
5. **Documentation**: Update README.md to reflect new functionality

Always maintain consistency with existing code style and include proper copyright headers.

## Licensing and Copyright

All source code files (`.cc`, `.h`, `.cpp`, `.hpp`) and CMake files (`CMakeLists.txt`) must include the following copyright header at the top of the file:

```
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
```

When creating new source files, always include this copyright block before any code or includes.
