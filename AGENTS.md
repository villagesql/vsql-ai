# AGENTS.md

This file provides guidance to AI coding assistants when working with code in this repository.

**Note**: Also check `AGENTS.local.md` for additional local development instructions when present.

## Project Overview

This is the VillageSQL AI Extension (vsql-ai) that adds AI capabilities to VillageSQL Server. It provides functions to interact with AI models from Anthropic Claude and Google Gemini directly from SQL queries, including text generation (prompting) and text embeddings.

## Build System

- **Build**: `cd build && cmake .. && make`
- **Create VEB package**: Automatically created by `make` as `vsql_ai.veb`
- **Install extension**: `make install` (copies VEB to configured install directory)

The build process:
1. Uses CMake to configure build with VillageSQL SDK via `FindVillageSQL.cmake`
2. Compiles C++ source files into shared library `vsql_ai.so`
3. Packages library with `manifest.json` into `vsql_ai.veb` archive
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
- `src/ai_functions.cc` - VEF function implementations (`ai_prompt`, `create_embed`) and extension registration
- `src/ai_providers.h` - Abstract provider interface
- `src/ai_providers.cc` - Concrete provider implementations (Anthropic, Google)
- `src/http_client.h/cc` - HTTP/HTTPS client for API calls
- `manifest.json` - Extension metadata (name, version, description, author, license)
- `CMakeLists.txt` - CMake build configuration
- `test/t/` - Test files directory (`.test` files using MTR framework)
- `test/r/` - Expected test results directory (`.result` files)

**Available Functions:**
- `ai_prompt(provider, model, api_key, prompt)` - Send prompts to AI models and get text responses
- `create_embed(provider, model, api_key, text)` - Generate text embeddings (vector representations)

**Dependencies:**
- Requires VillageSQL Extension SDK
- Uses VillageSQL Extension Framework (VEF) API
- C++ compiler with C++17 support
- OpenSSL development libraries (for HTTPS)
- Third-party headers (included): cpp-httplib, nlohmann/json

**Code Organization:**
- File naming: lowercase with underscores (e.g., `ai_functions.cc`)
- Function naming: lowercase with underscores (e.g., `ai_prompt_impl`)
- Variable naming: lowercase with underscores (e.g., `provider_name`)
- Provider pattern: Abstract `AIProvider` base class with concrete implementations

## VillageSQL Extension Framework (VEF) Pattern

VEF functions use a different pattern than traditional MySQL UDFs. Each function requires:

1. **Implementation function** - The actual function logic
2. **Registration** - Using VEF builder API to register the function

Example pattern:
```cpp
// 1. Implementation function
void my_function_impl(vef_context_t* ctx,
                     vef_invalue_t* arg1,
                     vef_invalue_t* arg2,
                     vef_vdf_result_t* result) {
    // Validate NULL inputs
    if (arg1->is_null || arg2->is_null) {
        result->type = VEF_RESULT_NULL;
        return;
    }

    // Extract arguments
    std::string value1(arg1->str_value, arg1->str_len);
    std::string value2(arg2->str_value, arg2->str_len);

    // Function logic here
    std::string response = do_something(value1, value2);

    // Return result
    result->type = VEF_RESULT_VALUE;
    result->actual_len = response.length();
    size_t copy_len = std::min(response.length(), result->max_str_len - 1);
    memcpy(result->str_buf, response.c_str(), copy_len);
    result->str_buf[copy_len] = '\0';
}

// 2. Registration using VEF builder API
VEF_GENERATE_ENTRY_POINTS(
    make_extension("my_extension", "1.0.0")
        .func(make_func<&my_function_impl>("my_function")
                  .returns(STRING)
                  .param(STRING)  // arg1
                  .param(STRING)  // arg2
                  .buffer_size(65535)
                  .build())
)
```

**Important VEF Notes:**
- Use `result->max_str_len` not `sizeof(result->str_buf)` for buffer size checks
- `str_buf` is a pointer, not a fixed-size array
- Always null-terminate string results
- Set `result->actual_len` to the actual data length

## Testing

The extension includes test files using the MySQL Test Runner (MTR) framework:
- **Test Location**:
  - `test/t/` directory contains `.test` files with SQL test commands
  - `test/r/` directory contains `.result` files with expected output
- **Run Tests**:
  ```bash
  cd <BUILD_DIR>/mysql-test
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/test
  ```
  Where `<BUILD_DIR>` is your VillageSQL build directory
- **Create/Update Results**: Use `--record` flag to generate or update expected `.result` files:
  ```bash
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/test --record
  ```
- Tests should validate function output and behavior
- Each test should install extension, run tests, and clean up (uninstall extension)

## Extension Installation

After building and installing the VEB file with `make install`, load the extension in VillageSQL:

```sql
INSTALL EXTENSION vsql_ai;
```

Then test the functions:
```sql
-- Test AI prompting
SELECT vsql_ai.ai_prompt('google', 'gemini-2.5-flash', @api_key, 'What is 2+2?');

-- Test embeddings
SELECT vsql_ai.create_embed('google', 'text-embedding-004', @api_key, 'Hello world');
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

### Adding a New Provider
1. Create a new class that inherits from `AIProvider`
2. Implement `prompt()` and `embed()` methods
3. Add factory method in `create_provider()` function
4. Add tests for the new provider

## Supported AI Models

### Anthropic Claude
- claude-sonnet-4-5-20250929
- claude-haiku-4-5-20251001
- claude-opus-4-5-20251101

### Google Gemini
**Generative:**
- gemini-2.5-flash
- gemini-2.5-pro
- gemini-3-flash-preview
- gemini-3-pro-preview

**Embeddings:**
- text-embedding-004 (768 dimensions)

## Common Tasks for AI Agents

When asked to add functionality to this extension:

1. **Adding a new AI provider**: Create provider class, implement interface methods, add to factory, add tests
2. **Adding a new function**: Create implementation in `ai_functions.cc`, register in `VEF_GENERATE_ENTRY_POINTS`, create tests
3. **Modifying API requests**: Update provider implementation in `ai_providers.cc`
4. **Testing**: Create or update `.test` files in `test/t/` directory, generate expected results with `--record`
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
