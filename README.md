![VillageSQL Logo](https://villagesql.com/assets/logo-light.svg)

# VillageSQL AI Extension

A powerful AI extension for VillageSQL Server that adds AI prompt capabilities and text embeddings directly in SQL queries. Interact with AI models from Anthropic, OpenAI, and Google using familiar SQL syntax.

## Features

- **AI Prompting**: Send prompts to AI models directly from SQL queries
- **Multiple Providers**: Support for Anthropic Claude and Google Gemini
- **Embedding Generation**: Create text embeddings for vector search and similarity analysis using Google Gemini
- **High Performance**: Efficient C++ implementation with minimal overhead
- **Secure**: HTTPS communication with SSL certificate verification

## Installation

### Build from Source

#### Prerequisites
- VillageSQL Extension SDK (installed at `~/.villagesql` or specified via `VillageSQL_SDK_DIR`)
- CMake 3.16 or higher
- C++17 compatible compiler
- OpenSSL development libraries (for HTTPS connections)

#### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/villagesql/vsql-ai.git
   cd vsql-ai
   ```

2. Configure CMake with required paths:
   ```bash
   mkdir -p build
   cd build
   cmake .. -DVillageSQL_SDK_DIR=/path/to/villagesql/sdk -DVEB_INSTALL_DIR=/path/to/veb/directory
   ```

   **Note**:
   - `VillageSQL_SDK_DIR`: Path to VillageSQL Extension SDK
   - `VEB_INSTALL_DIR`: Directory where `make install` will copy the VEB file (optional)

3. Build the extension:
   ```bash
   make
   ```

   This creates the `vsql_ai.veb` package in the build directory.

4. Install the VEB (optional):
   ```bash
   make install
   ```

   This copies the VEB to the directory specified by `VEB_INSTALL_DIR`. If not using `make install`, you can manually copy the VEB file to your desired location.

## Usage

After installation, load the extension in VillageSQL:

```sql
INSTALL EXTENSION vsql_ai;
```

### AI Prompting Examples

#### Anthropic Claude
```sql
-- Simple prompt with Claude
SELECT vsql_ai.ai_prompt(
    'anthropic',
    'claude-sonnet-4-5-20250929',
    'your-api-key-here',
    'Explain quantum computing in one sentence'
) AS response;
```

#### Google Gemini
```sql
-- Simple prompt with Gemini
SELECT vsql_ai.ai_prompt(
    'google',
    'gemini-2.5-flash',
    'your-api-key-here',
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
       vsql_ai.ai_prompt('anthropic', 'claude-sonnet-4-5-20250929', @api_key, question) AS answer
FROM questions;

-- Or use Gemini
SET @api_key = 'your-google-api-key';
SELECT id, question,
       vsql_ai.ai_prompt('google', 'gemini-2.5-flash', @api_key, question) AS answer
FROM questions;
```

### Text Embeddings Examples

#### Generate Embeddings
```sql
-- Generate embedding for text
SET @api_key = 'your-google-api-key';
SELECT vsql_ai.create_embed(
    'google',
    'text-embedding-004',
    @api_key,
    'Machine learning is fascinating'
) AS embedding;

-- Returns: [0.02646778, 0.019067757, -0.05332306, ...]
```

#### Using Embeddings with Table Data
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
        vsql_ai.create_embed('google', 'text-embedding-004', @api_key,
                            'Machine learning is a subset of artificial intelligence'));

-- Query to generate embeddings for multiple documents
SELECT id, content,
       vsql_ai.create_embed('google', 'text-embedding-004', @api_key, content) AS embedding
FROM documents;
```

### Supported Providers

Currently supported:

#### Anthropic (provider: `anthropic`)
Claude 4.5 models:
- **Claude Sonnet 4.5**: `claude-sonnet-4-5-20250929` (recommended - best for complex agents and coding)
- **Claude Haiku 4.5**: `claude-haiku-4-5-20251001` (fastest with near-frontier intelligence)
- **Claude Opus 4.5**: `claude-opus-4-5-20251101` (maximum capability and intelligence)

#### Google (provider: `google`)
Gemini models:
- **Gemini 3 Flash**: `gemini-3-flash-preview` (balanced model for speed, scale, and frontier intelligence)
- **Gemini 3 Pro**: `gemini-3-pro-preview` (best model for multimodal understanding)
- **Gemini 2.5 Flash**: `gemini-2.5-flash` (stable - best price-performance ratio)
- **Gemini 2.5 Pro**: `gemini-2.5-pro` (stable - state-of-the-art reasoning over complex problems)

Coming soon:
- **OpenAI**: GPT models (gpt-4, gpt-4-turbo, etc.)

### Function Reference

#### `ai_prompt(provider, model, api_key, prompt)`
Send a prompt to an AI provider and get a response.

**Parameters:**
- `provider` (STRING): AI provider name ("anthropic", "google")
- `model` (STRING): Model identifier (e.g., "claude-sonnet-4-5-20250929", "gemini-2.5-flash")
- `api_key` (STRING): API key for authentication
- `prompt` (STRING): The prompt text to send to the AI

**Returns:** STRING - The AI model's response

**Examples:**
```sql
-- Anthropic Claude
SELECT vsql_ai.ai_prompt('anthropic', 'claude-sonnet-4-5-20250929', @api_key, 'Hello!');

-- Google Gemini
SELECT vsql_ai.ai_prompt('google', 'gemini-2.5-flash', @api_key, 'Hello!');
```

#### `create_embed(provider, model, api_key, text)`
Generate text embeddings for vector search and similarity analysis.

**Parameters:**
- `provider` (STRING): Embedding provider ("google")
- `model` (STRING): Model identifier (e.g., "text-embedding-004")
- `api_key` (STRING): API key for authentication
- `text` (STRING): Text to create embedding from

**Returns:** STRING - JSON array of embedding vector (768 dimensions for text-embedding-004)

**Examples:**
```sql
-- Google Gemini text embeddings
SELECT vsql_ai.create_embed('google', 'text-embedding-004', @api_key, 'Machine learning is fascinating');

-- Result: [0.02646778, 0.019067757, -0.05332306, ...]
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
   SELECT vsql_ai.ai_prompt('anthropic', 'claude-sonnet-4-5-20250929', @api_key, 'prompt');
   ```
   Session variables keep API keys out of query text and reduce exposure in logs.

2. **Avoid Hardcoded Keys**:
   ```sql
   -- ❌ BAD: Key visible in logs
   SELECT vsql_ai.ai_prompt('anthropic', 'model', 'sk-ant-12345...', 'prompt');

   -- ✅ GOOD: Use session variable
   SELECT vsql_ai.ai_prompt('anthropic', 'model', @api_key, 'prompt');
   ```

3. **Shell Environment Variables** (Future Enhancement):
   Future versions may support reading API keys directly from shell environment variables (e.g., `$ANTHROPIC_API_KEY`) for additional security.

### Network Security

- All API requests use HTTPS with SSL certificate verification
- Connections timeout after 30 seconds by default
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

## Testing

The extension includes comprehensive tests using the MySQL Test Runner (MTR) framework.

### Running Tests

**Option 1 (Default): Using installed VEB**

This method assumes you have successfully run `make install` to install the VEB to your veb_dir.

```bash
cd ~/build/mysql-test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/test

# Run individual test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/test error_handling
```

**Option 2: Using a specific VEB file**

Use this to test a specific VEB build without installing it first:

```bash
cd ~/build/mysql-test
VSQL_AI_VEB=/path/to/vsql-ai/build/vsql_ai.veb \
  perl mysql-test-run.pl --suite=/path/to/vsql-ai/test
```

### Testing with Live API Calls

The extension includes live API tests for each provider. Each test will skip live API calls if the corresponding environment variable is not set.

#### Testing Anthropic Claude

1. **Export your API key:**
   ```bash
   export ANTHROPIC_API_KEY='your-api-key-here'
   ```

2. **Run the test:**
   ```bash
   cd ~/build/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/test ai_prompt_anthropic
   ```

#### Testing Google Gemini

1. **Export your API key:**
   ```bash
   export GEMINI_API_KEY='your-api-key-here'
   ```

2. **Run the prompt test:**
   ```bash
   cd ~/build/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/test ai_prompt_google
   ```

3. **Run the embeddings test:**
   ```bash
   cd ~/build/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/test create_embed_google
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
│   ├── ai_providers.h/.cc   # AI provider implementations (Anthropic, OpenAI, Google)
│   └── http_client.h/.cc    # HTTP client wrapper for API calls
├── include/
│   ├── httplib.h            # cpp-httplib single header
│   └── nlohmann/json.hpp    # nlohmann/json single header
├── cmake/
│   └── FindVillageSQL.cmake # CMake module to locate VillageSQL SDK
├── test/
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
- `make` - Build the extension and create the `vsql_ai.veb` package

## Roadmap

- ✅ Anthropic Claude integration
- ✅ Google Gemini integration
- ✅ Embedding generation (Google Gemini)
- ✅ Session variable support for API keys
- ⏳ OpenAI GPT integration
- ⏳ OpenAI embeddings
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

VillageSQL welcomes contributions from the community. For more information, please see the [VillageSQL Contributing Guide](https://github.com/villagesql/villagesql/blob/main/CONTRIBUTING.md).

## Contact

We are excited you want to be part of the Village that makes VillageSQL happen. You can interact with us and the community in several ways:

+ File a [bug or issue](./issues) and we will review
+ Start a discussion in the project [discussions](./discussions)
+ Join the [Discord channel](https://discord.gg/KSr6whd3Fr)
