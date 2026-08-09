![VillageSQL Logo](https://villagesql.com/assets/logo-light.svg)

# VillageSQL AI Extension

A powerful AI extension for VillageSQL Server that adds AI prompt capabilities and text embeddings directly in SQL queries. Interact with AI models from Anthropic, OpenAI, Google, and local Ollama using familiar SQL syntax.

## Features

- **AI Prompting**: Send prompts to AI models directly from SQL queries
- **Multiple Providers**: Support for Anthropic Claude, Google Gemini, OpenAI GPT, and local Ollama
- **Embedding Generation**: Create text embeddings for vector search and similarity analysis using Google Gemini, OpenAI, or local Ollama
- **High Performance**: Efficient C++ implementation with minimal overhead
- **Secure**: HTTPS communication with SSL certificate verification

## Installation

If you installed VillageSQL with the install script, the Docker image, or a
release tarball, `vsql_ai.veb` is already in the server's `lib/veb/`
directory — this extension is bundled with the server. There is nothing to build
or download:

```sql
INSTALL EXTENSION vsql_ai;
```

Build from source only if you built the server from source without the bundled
extensions, or if you are working on this extension itself.

### Build from Source

#### Prerequisites
- VillageSQL build directory (specified via `VillageSQL_BUILD_DIR`)
- CMake 3.16 or higher
- C++17 compatible compiler
- OpenSSL development libraries (for HTTPS connections)

📚 **Full Documentation**: Visit [villagesql.com/docs](https://villagesql.com/docs) for comprehensive guides on building extensions, architecture details, and more.

#### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/villagesql/vsql-ai.git
   cd vsql-ai
   ```

2. Configure CMake with required paths:

   **Linux:**
   ```bash
   mkdir -p build
   cd build
   cmake .. -DVillageSQL_BUILD_DIR=$HOME/build/villagesql
   ```

   **macOS:**
   ```bash
   mkdir -p build
   cd build
   cmake .. -DVillageSQL_BUILD_DIR="$HOME/build/villagesql"
   ```

   **Note**: `VillageSQL_BUILD_DIR` should point to your VillageSQL build directory.

3. Build the extension:
   ```bash
   make -j $(getconf _NPROCESSORS_ONLN)
   ```

   This creates the `veb` package in the build directory.

4. Install the VEB (optional):
   ```bash
   make install
   ```

   This copies the VEB to the directory specified by `VillageSQL_VEB_INSTALL_DIR`. If not using `make install`, you can manually copy the VEB file to your desired location.

## Usage

After installation, load the extension in VillageSQL:

```sql
INSTALL EXTENSION vsql_ai;
```

### AI Prompting Examples

#### Anthropic Claude
```sql
-- Simple prompt with Claude
SELECT ai_prompt(
    'anthropic',
    'claude-opus-5',
    'your-api-key-here',
    'Explain quantum computing in one sentence'
) AS response;
```

#### Google Gemini
```sql
-- Simple prompt with Gemini
SELECT ai_prompt(
    'google',
    'gemini-2.5-flash',
    'your-api-key-here',
    'Explain quantum computing in one sentence'
) AS response;
```

#### OpenAI GPT
```sql
-- Simple prompt with GPT
SELECT ai_prompt(
    'openai',
    'gpt-4o-mini',
    'your-api-key-here',
    'Explain quantum computing in one sentence'
) AS response;
```

#### Local Ollama
```sql
-- Simple prompt with local Ollama (no API key needed)
SELECT ai_prompt(
    'local',
    'llama3.2',
    '',
    'Explain quantum computing in one sentence'
) AS response;
```

#### Using with Table Data
```sql
-- Use with table data
CREATE TABLE questions (id INT, question TEXT);
INSERT INTO questions VALUES
    (1, 'What is machine learning?'),
    (2, 'Explain neural networks'),
    (3, 'What is deep learning?');

-- Get AI responses for multiple questions using Claude
SET @api_key = 'your-anthropic-api-key';
SELECT id, question,
       ai_prompt('anthropic', 'claude-opus-5', @api_key, question) AS answer
FROM questions;

-- Or use Gemini
SET @api_key = 'your-google-api-key';
SELECT id, question,
       ai_prompt('google', 'gemini-2.5-flash', @api_key, question) AS answer
FROM questions;
```

### Text Embeddings Examples

#### Generate Embeddings
```sql
-- Generate embedding for text
SET @api_key = 'your-google-api-key';
SELECT ai_embedding(
    'google',
    'gemini-embedding-001',
    @api_key,
    'Machine learning is fascinating'
) AS embedding;

-- Returns: [0.02646778, 0.019067757, -0.05332306, ...]
```

#### Storing Embeddings in a JSON Column

`ai_embedding()` returns the vector as JSON text, but the result carries the
binary character set. MySQL's JSON functions and `JSON` columns reject it
directly:

```sql
-- This fails:
-- ERROR 3144 (22032): Cannot create a JSON value from a string with
-- CHARACTER SET 'binary'
INSERT INTO documents (id, content, embedding)
VALUES (1, 'Machine learning', ai_embedding('google', 'gemini-embedding-001', @api_key, 'Machine learning'));
```

Wrap the call in `CONVERT(... USING utf8mb4)` (or `CAST(... AS CHAR)`) whenever
the value flows into a `JSON` column or a JSON function:

```sql
-- Create a table to store documents and their embeddings
CREATE TABLE documents (
    id INT PRIMARY KEY,
    content TEXT,
    embedding JSON
);

-- Generate and store embeddings
SET @api_key = 'your-google-api-key';
INSERT INTO documents (id, content, embedding)
VALUES (1, 'Machine learning is a subset of artificial intelligence',
        CONVERT(ai_embedding('google', 'gemini-embedding-001', @api_key,
                             'Machine learning is a subset of artificial intelligence')
                USING utf8mb4));

-- Inspect what was stored
SELECT id, JSON_LENGTH(embedding) AS dimensions FROM documents;
```

Plain `TEXT` columns need no conversion — it is only JSON parsing that is
affected.

### Supported Providers

Currently supported:

#### Anthropic (provider: `anthropic`)
Claude models:
- **Claude Fable 5**: `claude-fable-5` (most capable model, for the most demanding reasoning and long-horizon work)
- **Claude Opus 5**: `claude-opus-5` (recommended - complex agentic coding and enterprise work)
- **Claude Sonnet 5**: `claude-sonnet-5` (best combination of speed and intelligence)
- **Claude Haiku 4.5**: `claude-haiku-4-5` (fastest and most cost-effective)

Also supported (previous generations): `claude-opus-4-8`, `claude-opus-4-7`,
`claude-opus-4-6`, `claude-opus-4-5-20251101`, `claude-sonnet-4-6`,
`claude-sonnet-4-5-20250929`.

#### Google (provider: `google`)
**Generative:**
- **Gemini 3 Flash**: `gemini-3-flash-preview` (balanced model for speed, scale, and frontier intelligence)
- **Gemini 3 Pro**: `gemini-3-pro-preview` (best model for multimodal understanding)
- **Gemini 2.5 Flash**: `gemini-2.5-flash` (stable - best price-performance ratio)
- **Gemini 2.5 Pro**: `gemini-2.5-pro` (stable - state-of-the-art reasoning over complex problems)

**Embeddings:**
- **Gemini Embedding 001**: `gemini-embedding-001` (3072 dimensions by default)
- **Gemini Embedding 2**: `gemini-embedding-2-preview` (improved embedding model)

#### OpenAI (provider: `openai`)
**Chat:**
- **GPT-4o**: `gpt-4o` (flagship multimodal model)
- **GPT-4o mini**: `gpt-4o-mini` (fast and cost-effective)

**Embeddings:**
- **text-embedding-3-small**: `text-embedding-3-small` (1536 dimensions, best value)
- **text-embedding-3-large**: `text-embedding-3-large` (3072 dimensions, highest performance)

#### Local Ollama (provider: `local`)
Connects to Ollama running on `127.0.0.1:11434`. No API key required. Use any model you have pulled in Ollama.

**Chat:** Any Ollama chat model (e.g., `llama3.2`, `mistral`, `gemma2`)

**Embeddings:** Any Ollama embedding model (e.g., `nomic-embed-text`, `mxbai-embed-large`)

### Function Reference

#### `ai_prompt(provider, model, api_key, prompt)`
Send a prompt to an AI provider and get a response.

**Parameters:**
- `provider` (STRING): AI provider name ("anthropic", "google", "openai", "local")
- `model` (STRING): Model identifier (e.g., "claude-opus-5", "gemini-2.5-flash", "gpt-4o-mini", "llama3.2")
- `api_key` (STRING): API key for authentication (use empty string `''` for local provider)
- `prompt` (STRING): The prompt text to send to the AI

**Returns:** STRING - The AI model's response

**Examples:**
```sql
-- Anthropic Claude
SELECT ai_prompt('anthropic', 'claude-opus-5', @api_key, 'Hello!');

-- Google Gemini
SELECT ai_prompt('google', 'gemini-2.5-flash', @api_key, 'Hello!');

-- OpenAI GPT
SELECT ai_prompt('openai', 'gpt-4o-mini', @api_key, 'Hello!');

-- Local Ollama (no API key needed)
SELECT ai_prompt('local', 'llama3.2', '', 'Hello!');
```

#### `ai_embedding(provider, model, api_key, text)`
Generate text embeddings for vector search and similarity analysis.

**Parameters:**
- `provider` (STRING): Embedding provider ("google", "openai", "local")
- `model` (STRING): Model identifier (e.g., "gemini-embedding-001", "text-embedding-3-small", "nomic-embed-text")
- `api_key` (STRING): API key for authentication (use empty string `''` for local provider)
- `text` (STRING): Text to create embedding from

**Returns:** STRING - JSON array of embedding vector (dimensions vary by model)

**Examples:**
```sql
-- Google Gemini text embeddings
SELECT ai_embedding('google', 'gemini-embedding-001', @api_key, 'Machine learning is fascinating');

-- Result: [0.02646778, 0.019067757, -0.05332306, ...]

-- OpenAI text embeddings
SELECT ai_embedding('openai', 'text-embedding-3-small', @api_key, 'Machine learning is fascinating');

-- Local Ollama text embeddings (no API key needed)
SELECT ai_embedding('local', 'nomic-embed-text', '', 'Machine learning is fascinating');
```

## Security Considerations

### API Key Safety

**Important:** API keys passed as function parameters may be visible in query logs, slow query logs, and process lists.

**Best Practices:**

1. **Use Session Variables** (Recommended):
   ```sql
   -- Store API key in session variable
   SET @api_key = 'sk-ant-your-api-key';

   -- Use variable in queries
   SELECT ai_prompt('anthropic', 'claude-opus-5', @api_key, 'prompt');
   ```
   Session variables keep API keys out of query text and reduce exposure in logs.

2. **Avoid Hardcoded Keys**:
   ```sql
   -- ❌ BAD: Key visible in logs
   SELECT ai_prompt('anthropic', 'model', 'sk-ant-12345...', 'prompt');

   -- ✅ GOOD: Use session variable
   SELECT ai_prompt('anthropic', 'model', @api_key, 'prompt');
   ```

3. **Shell Environment Variables** (Future Enhancement):
   Future versions may support reading API keys directly from shell environment variables (e.g., `$ANTHROPIC_API_KEY`) for additional security.

### Network Security

- All API requests use HTTPS with SSL certificate verification
- Cloud provider connections timeout after 30 seconds; the local (Ollama) provider allows 300 seconds, since local models can be slow to load
- Failed connections return clear error messages

## Performance Considerations

### Timeouts

AI API calls can take 5-30 seconds depending on prompt complexity and model speed. Consider:

1. **MySQL Query Timeout**: You may need to adjust `max_execution_time`:
   ```sql
   SET SESSION max_execution_time = 60000; -- 60 seconds
   ```

2. **Batch Processing**: For multiple prompts, process in batches to avoid long-running queries

### Rate Limiting

AI providers impose rate limits on API requests:
- **Anthropic**: Varies by plan (typically 50+ requests/minute)
- Error messages will indicate rate limit issues
- Consider spacing out bulk operations

## Known Limitations

**Each call blocks the connection.** Both functions perform a synchronous
outbound HTTP request from inside the SQL function, holding the server thread
until the provider responds — up to 30 seconds for cloud providers, 300 for
local Ollama. Cost scales linearly with row count: `SELECT ai_prompt(...) FROM t`
issues one request per row, serially. There is no batching or asynchronous
execution. Materialize results into a column rather than recomputing them per
query, and raise `max_execution_time` for multi-row statements.

**Embedding output needs an explicit conversion for JSON use.** The returned
string carries the binary character set, so `JSON_VALID()`, `JSON_LENGTH()`,
and inserts into a `JSON` column all reject it until you wrap the call in
`CONVERT(... USING utf8mb4)`. See
[Storing Embeddings in a JSON Column](#storing-embeddings-in-a-json-column).

**No embeddings from Anthropic.** `ai_embedding('anthropic', ...)` returns a
warning and NULL — Anthropic publishes no embeddings endpoint. Use `google`,
`openai`, or `local`.

**Warning text is truncated.** Provider errors are cut to 255 bytes (response
body excerpts to 100) before being surfaced as a warning. Truncation is
UTF-8-aware, so a cut never produces invalid text, but long upstream errors
are not shown in full.

**Errors surface as warnings, not errors.** Every failure path returns SQL
NULL plus `Warning 3200`. A statement that calls these functions will not
abort on a provider failure — check for NULL, and inspect `SHOW WARNINGS` to
see why.

## Testing

The extension includes comprehensive tests using the MySQL Test Runner (MTR) framework.

### Running Tests

**Option 1 (Default): Using installed VEB**

This method assumes you have successfully run `make install` to install the VEB to your veb_dir.

**Linux:**
```bash
cd $HOME/build/villagesql/mysql-test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test

# Run individual test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test error_handling
```

**macOS:**
```bash
cd ~/build/villagesql/mysql-test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test

# Run individual test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test error_handling
```

**Option 2: Using a specific VEB file**

Use this to test a specific VEB build without installing it first:

**Linux:**
```bash
cd $HOME/build/villagesql/mysql-test
VSQL_AI_VEB=/path/to/vsql-ai/build/vsql_ai.veb \
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test
```

**macOS:**
```bash
cd ~/build/villagesql/mysql-test
VSQL_AI_VEB=/path/to/vsql-ai/build/vsql_ai.veb \
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test
```

### Testing with Live API Calls

The extension includes live API tests for each provider. Each test will skip live API calls if the corresponding environment variable is not set.

#### Testing Anthropic Claude

1. **Export your API key:**
   ```bash
   export ANTHROPIC_API_KEY='your-api-key-here'
   ```

2. **Run the test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_anthropic
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_anthropic
   ```

#### Testing Google Gemini

1. **Export your API key:**
   ```bash
   export GEMINI_API_KEY='your-api-key-here'
   ```

2. **Run the prompt test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_google
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_google
   ```

3. **Run the embeddings test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_google
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_google
   ```

#### Testing OpenAI GPT

1. **Export your API key:**
   ```bash
   export OPENAI_API_KEY='your-api-key-here'
   ```

2. **Run the prompt test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_openai
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_openai
   ```

3. **Run the embeddings test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_openai
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_openai
   ```

#### Testing Local Ollama

1. **Ensure Ollama is running** on `127.0.0.1:11434` with a model pulled.

2. **Export the model name:**
   ```bash
   export OLLAMA_MODEL='llama3.2'
   export OLLAMA_EMBED_MODEL='nomic-embed-text'
   ```

3. **Run the prompt test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_local
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_local
   ```

4. **Run the embeddings test:**

   **Linux:**
   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_local
   ```

   **macOS:**
   ```bash
   cd ~/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_local
   ```

**Security Note:** All tests automatically:
- Skip live API calls if the environment variable is not set
- Hide API keys from test output using `--disable_query_log`
- Validate that responses contain expected content

**Note:** The `error_handling` test does not require an API key and only validates input validation and error handling.

## Development

### Project Structure
```
vsql-ai/
├── src/
│   ├── ai_functions.cc      # VEF function implementations and registration
│   ├── ai_providers.h/.cc   # AI provider implementations (Anthropic, OpenAI, Google, Local)
│   └── http_client.h/.cc    # HTTP client wrapper for API calls
├── include/
│   ├── httplib.h            # cpp-httplib single header
│   └── nlohmann/json.hpp    # nlohmann/json single header
├── cmake/
│   └── FindVillageSQL.cmake # CMake module to locate VillageSQL SDK
├── mysql-test/
│   ├── t/                   # MTR test files
│   └── r/                   # MTR expected results
├── manifest.json            # VEB package manifest
├── CMakeLists.txt           # Build configuration
└── AGENTS.md                # AI coding assistant instructions
```

### Architecture

The extension uses:
- **VillageSQL Extension Framework (VEF)**: Native extension API
- **cpp-httplib**: Header-only HTTP/HTTPS client library
- **nlohmann/json**: Header-only JSON parsing library
- **OpenSSL**: SSL/TLS for secure HTTPS connections

### Build Targets
- `make` - Build the extension and create the `veb` package

## Roadmap

- ✅ Anthropic Claude integration
- ✅ Google Gemini integration
- ✅ Embedding generation (Google Gemini)
- ✅ Session variable support for API keys
- ✅ OpenAI GPT integration
- ✅ OpenAI embeddings
- ✅ Local Ollama provider (no API key required)
- ⏳ Shell environment variable support for API keys
- ⏳ Configurable timeouts
- ⏳ Response streaming for long outputs
- ⏳ Token counting utilities

## Reporting Bugs and Requesting Features

If you encounter a bug or have a feature request, please open an [issue](./issues) using GitHub Issues. Please provide as much detail as possible, including:

*   A clear and descriptive title.
*   A detailed description of the issue or feature request.
*   Steps to reproduce the bug (if applicable).
*   Your environment details (OS, VillageSQL version, etc.).

## License

License information can be found in the [LICENSE](./LICENSE) file.

## Contributing

VillageSQL welcomes contributions from the community. For more information, please see the [VillageSQL Contributing Guide](https://github.com/villagesql/villagesql-server/blob/main/CONTRIBUTING.md).

## Contact

We are excited you want to be part of the Village that makes VillageSQL happen. You can interact with us and the community in several ways:

+ File a [bug or issue](./issues) and we will review
+ Start a discussion in the project [discussions](./discussions)
+ Join the [Discord channel](https://discord.gg/KSr6whd3Fr)
