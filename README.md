![VillageSQL Logo](https://villagesql.com/assets/logo-light.svg)

# VillageSQL AI Extension

A powerful AI extension for VillageSQL Server that adds AI prompt capabilities and text embeddings directly in SQL queries. Interact with AI models from Anthropic, OpenAI, Google, and local Ollama using familiar SQL syntax.

## Features

- **AI Prompting**: Send prompts to AI models directly from SQL queries
- **Multiple Providers**: Support for Anthropic Claude, Google Gemini, OpenAI GPT, and local Ollama
- **Embedding Generation**: Create text embeddings for vector search and similarity analysis using Google Gemini, OpenAI, or local Ollama; store them as JSON or in a [vsql-vector](https://github.com/villagesql/vsql-vector) `SVECTOR` column
- **Binary Embeddings**: `ai_embedding_binary()` returns packed float32 vectors, the layout of MySQL's `VECTOR` type
- **Raw Requests**: `ai_request()` sends your own JSON body to a registered model's native API and returns the raw response
- **Decisions**: Ask decision models (Gemini Decision, TypeSafe Jev, OpenAI) yes/no, multiple-choice and scored questions and get calibrated probabilities back as JSON
- **High Performance**: Efficient C++ implementation with minimal overhead
- **Secure**: API keys stay in the MySQL keyring, never in SQL text; HTTPS with certificate verification
- **Model Registry**: Administrators register models once; queries name a model ID

## Installation

If you installed VillageSQL with the install script, the Docker image, or a
release tarball, `vsql_ai.veb` is already in the server's `lib/veb/`
directory — this extension is bundled with the server. There is nothing to build
or download. It uses VillageSQL preview capabilities (the MySQL keyring and its
own system variable), so the server must allow preview extensions first:

```sql
SET PERSIST vsql_allow_preview_extensions = ON;
INSTALL EXTENSION vsql_ai;
```

Without that setting, `INSTALL EXTENSION` fails. Cloud models also need a
keyring component; see [Setup](#setup).

Build from source only if you built the server from source without the bundled
extensions, or if you are working on this extension itself.

### Build from Source

#### Prerequisites
- VillageSQL 0.0.8 or later build directory (specified via `VillageSQL_BUILD_DIR`)
- CMake 3.16 or higher
- C++17 compatible compiler
- OpenSSL development libraries (for HTTPS connections)
  - macOS: `brew install openssl@3` (detected automatically; override with `-DOPENSSL_ROOT_DIR=...`)

📚 **Full Documentation**: Visit [villagesql.com/docs](https://villagesql.com/docs) for comprehensive guides on building extensions, architecture details, and more.

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
   cmake .. -DVillageSQL_BUILD_DIR=$HOME/build/villagesql
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

Calls name a **model ID** that an administrator registers once, and API keys
live in the **MySQL keyring** instead of in SQL text:

```sql
SELECT ai_prompt('claude', 'Explain quantum computing in one sentence');
SELECT ai_embedding('gemini-embed', 'Machine learning is fascinating');
```

> **Upgrading from 0.0.8 or earlier:** two changes break existing calls.
>
> - `ai_prompt(provider, model, api_key, prompt)` and
>   `ai_embedding(provider, model, api_key, text)` are gone. Calls with four
>   arguments now fail with `wrong number of arguments`. Register each model
>   in `vsql_ai.models`, store its key in the keyring, and pass the model ID
>   instead.
> - Calling any vsql_ai function needs the `VSQL_AI_USER` privilege, and no
>   account has it until it is granted, root included. Until then every call
>   fails with "Access denied; you need the VSQL_AI_USER privilege". See
>   [Grant access](#5-grant-access).

### Setup

#### 1. Allow preview extensions and install

`vsql_ai` reads the keyring and its own system variable through VillageSQL
preview capabilities, so the server must allow preview extensions:

```sql
SET PERSIST vsql_allow_preview_extensions = ON;
INSTALL EXTENSION vsql_ai;
```

#### 2. Enable a keyring component

API keys are read from whichever MySQL keyring component the server loads,
for example `component_keyring_file` or `component_keyring_encrypted_file`.

A keyring component is not installed with `INSTALL COMPONENT`. The server
loads it at startup from a manifest file, so that the keyring is available
before anything needs it. With `component_keyring_file`, that takes two JSON
files and a restart:

1. A manifest named `mysqld.my`, in the same directory as the `mysqld` binary:
   ```json
   { "components": "file://component_keyring_file" }
   ```
2. A configuration file named `component_keyring_file.cnf`, in the plugin
   directory (`SELECT @@plugin_dir`). `path` is where the component keeps the
   keys; the server's account must be able to write to it:
   ```json
   { "path": "/var/lib/mysql-keyring/component_keyring_file", "read_only": false }
   ```
3. Restart the server and check that the component is active:
   ```sql
   SELECT * FROM performance_schema.keyring_component_status;
   ```
   `Component_status` should be `Active`.

Either file can instead live in the data directory: a global file containing
`{ "read_local_manifest": true }` or `{ "read_local_config": true }` sends the
server there. See
[Keyring Components](https://dev.mysql.com/doc/refman/8.4/en/keyring-component-installation.html)
in the MySQL manual for the other components and their options.

Models served by local Ollama need no key, so this step is optional if you
only use `local`.

#### 3. Store API keys

`vsql_ai` reads the keyring directly and needs nothing beyond the keyring
component. Storing a key from SQL, though, needs `keyring_udf`, a separate
helper that ships with the server as a **plugin**, not a component, which is
why it is installed with `INSTALL PLUGIN`. It provides the
`keyring_key_store()` and `keyring_key_remove()` functions and works only once
the keyring component is active. Install it once per server; the plugin and
its functions persist across restarts:

```sql
INSTALL PLUGIN keyring_udf SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME 'keyring_udf.so';
```

Then store each key. The statement contains the key, so keep it out of the
general query log:

```sql
SET SESSION sql_log_off = ON;
SELECT keyring_key_store('anthropic_key', 'SECRET', 'sk-ant-...');
SET SESSION sql_log_off = OFF;
```

The key type (`SECRET` here) is not checked; vsql-duckdb's examples use
`AES`, and either works.

**A key stored that way belongs to the account that stored it**, so name that
account as `key_owner` in the registry. The owner is the full `user@host`
form, as `CURRENT_USER()` shows it: `root` and `root@%` do not match a key
stored by `root@localhost`. To rotate a key, remove it and store the new one:

```sql
SELECT keyring_key_remove('anthropic_key');
SELECT keyring_key_store('anthropic_key', 'SECRET', 'sk-ant-new...');
```

Keys provisioned outside SQL, by tooling that writes the keyring directly,
are internal keys with no owner: leave `key_owner` out for those.

#### 4. Register models

`vsql_ai.models` is a JSON object that maps each model ID to a provider, the
provider's model name, and where its key is in the keyring. Use `SET PERSIST`
so the registry survives a restart:

```sql
SET PERSIST vsql_ai.models = '{
  "claude":       {"provider": "anthropic", "model": "claude-opus-5-5",
                   "key_id": "anthropic_key", "key_owner": "root@localhost"},
  "gemini":       {"provider": "google", "model": "gemini-3.5-flash-lite",
                   "key_id": "gemini_key", "key_owner": "root@localhost"},
  "gemini-embed": {"provider": "google", "model": "gemini-embedding-001",
                   "key_id": "gemini_key", "key_owner": "root@localhost"},
  "gpt":          {"provider": "openai", "model": "gpt-6-luna",
                   "key_id": "openai_key", "key_owner": "root@localhost"},
  "jev":          {"provider": "typesafe", "model": "jev-latest",
                   "key_id": "typesafe_key", "key_owner": "root@localhost"},
  "llama":        {"provider": "local", "model": "llama3.2"}
}';
```

| Field | Required | Meaning |
|---|---|---|
| `provider` | yes | `anthropic`, `google`, `openai`, `typesafe` or `local` |
| `model` | yes | The provider's model name, passed through unchanged |
| `key_id` | except `local` | Keyring data ID of the API key |
| `key_owner` | no | Keyring owner of the key; omit for an internal key |

Model IDs are case-sensitive. Unknown fields are rejected, so a misspelt
`key_id` is reported rather than ignored. The registry is checked when a model
is used: a mistake in one entry does not affect the others.

The registry holds no secrets, so any user may read it with
`SELECT @@vsql_ai.models`. Changing it requires `SYSTEM_VARIABLES_ADMIN`, which
is what controls who can add a model or point one at a key.

**A registered model's key is shared by everyone who may call the
functions.** The key is read as `key_owner` no matter which account makes the
call, so every caller spends that owner's provider credits, the way an
application's service credentials are shared by all its users. Who may call
is controlled by the `VSQL_AI_USER` privilege (next step). To limit spending
further, register only the models you are willing to share, and use a key
with a spending limit at the provider.

Two things to know about where the registry lives:

- `UNINSTALL EXTENSION vsql_ai` discards it, including a value saved with
  `SET PERSIST`. Set it again after reinstalling.
- Set it with `SET GLOBAL` or `SET PERSIST`. A value given in an option file
  or on the server command line is read through a 1 KB buffer, so one longer
  than 1023 bytes (roughly eight models) makes every call fail with a warning
  that says so.

#### 5. Grant access

Calling any vsql_ai function requires the `VSQL_AI_USER` dynamic privilege,
which the extension registers when it is installed. MySQL does not give a
newly registered privilege to existing accounts, so grant it, to root as well
if root will call the functions:

```sql
GRANT VSQL_AI_USER ON *.* TO app@'%';
GRANT VSQL_AI_USER ON *.* TO root@localhost;
```

A call without it fails with an error ("Access denied; you need the
VSQL_AI_USER privilege to use vsql_ai functions") rather than returning NULL,
so a statement over many rows stops at the first one.

- The check uses the statement's active account, so a `SQL SECURITY DEFINER`
  routine runs with its definer's privilege. That is a way to let users call
  one vetted routine without granting them the functions in general.
- Grants survive `UNINSTALL EXTENSION` and apply again after reinstalling.
- To grant it, an account needs `VSQL_AI_USER` itself `WITH GRANT OPTION`,
  or `UPDATE` on the `mysql` schema (as root has). That is MySQL's rule for
  every dynamic privilege; `CREATE USER` or `GRANT OPTION` on other privileges
  is not enough. To let an account manage access without holding broad
  rights, give it the privilege with the grant option:

  ```sql
  GRANT VSQL_AI_USER ON *.* TO ai_admin@localhost WITH GRANT OPTION;
  ```
- To let every account call the functions, for example on a development
  server, turn the check off. Changing `vsql_ai.require_privilege` needs
  `SYSTEM_VARIABLES_ADMIN` (or the deprecated `SUPER`), like any global
  system variable; it can also be set in the server's option file or on its
  command line by whoever controls the server's configuration:

  ```sql
  SET PERSIST vsql_ai.require_privilege = OFF;
  ```

  So an account with `SYSTEM_VARIABLES_ADMIN` can always reach the functions,
  by turning the check off. That is the same privilege that edits
  `vsql_ai.models`, so it already decides which models and keys exist.
  Turning the check off is global: it opens the functions to every account,
  not just the one that changed it.

#### Privileges at a glance

| Task | Privilege needed |
|---|---|
| Allow preview extensions (`SET PERSIST vsql_allow_preview_extensions`; `SET GLOBAL` is refused) | `SYSTEM_VARIABLES_ADMIN` (or `SUPER`) |
| `INSTALL EXTENSION` / `UNINSTALL EXTENSION` | `EXTENSION_ADMIN` (or `SUPER`) |
| Enable a keyring component | Write access to the server's files (manifest and config) and a restart; not done from SQL |
| Install `keyring_udf` (`INSTALL PLUGIN`, `CREATE FUNCTION`) | `INSERT` on the `mysql` schema, as for any plugin and loadable function |
| Store or remove a key (`keyring_key_store()`, `keyring_key_remove()`) | `EXECUTE`; the key then belongs to the account that stored it |
| Turn off the general query log for the session (`SET SESSION sql_log_off`) | `SESSION_VARIABLES_ADMIN` (or `SYSTEM_VARIABLES_ADMIN` or `SUPER`) |
| Register or change models (`vsql_ai.models`) | `SYSTEM_VARIABLES_ADMIN` (or `SUPER`) |
| Read the registry (`SELECT @@vsql_ai.models`) | None |
| Grant `VSQL_AI_USER` to others | `VSQL_AI_USER WITH GRANT OPTION`, or `UPDATE` on the `mysql` schema |
| Call `ai_prompt()`, `ai_embedding()`, `ai_embedding_binary()`, `ai_request()`, `ai_decision()` | `VSQL_AI_USER`, unless `vsql_ai.require_privilege` is `OFF` |
| Turn the privilege check off (`vsql_ai.require_privilege`) | `SYSTEM_VARIABLES_ADMIN` (or `SUPER`), or control of the server's configuration |

### AI Prompting Examples

```sql
SELECT ai_prompt('claude', 'Explain quantum computing in one sentence') AS response;
SELECT ai_prompt('gemini', 'Explain quantum computing in one sentence') AS response;
SELECT ai_prompt('gpt',    'Explain quantum computing in one sentence') AS response;
SELECT ai_prompt('llama',  'Explain quantum computing in one sentence') AS response;
```

#### Using with Table Data
```sql
CREATE TABLE questions (id INT, question TEXT);
INSERT INTO questions VALUES
    (1, 'What is machine learning?'),
    (2, 'Explain neural networks'),
    (3, 'What is deep learning?');

SELECT id, question, ai_prompt('claude', question) AS answer
FROM questions;
```

### Text Embeddings Examples

#### Generate Embeddings
```sql
SELECT ai_embedding('gemini-embed', 'Machine learning is fascinating') AS embedding;

-- Returns: [0.02646778, 0.019067757, -0.05332306, ...]
```

#### Storing Embeddings in a JSON Column

```sql
CREATE TABLE documents (
    id INT PRIMARY KEY,
    content TEXT,
    embedding JSON
);

INSERT INTO documents (id, content, embedding)
VALUES (1, 'Machine learning is a subset of artificial intelligence',
        CONVERT(ai_embedding('gemini-embed',
                             'Machine learning is a subset of artificial intelligence')
                USING utf8mb4));

SELECT id, JSON_LENGTH(embedding) AS dimensions FROM documents;
```

On VillageSQL 0.0.6 and earlier the result carries the binary character set,
which `JSON` columns and JSON functions reject, so wrap the call in
`CONVERT(... USING utf8mb4)` there. VillageSQL 0.0.7 and later return the
right character set; the conversion is harmless to keep.

#### Storing Embeddings in a Vector Column

For similarity search, store embeddings in an `SVECTOR` column from the
[vsql-vector](https://github.com/villagesql/vsql-vector) extension instead of
JSON. `SVECTOR(n)` holds `n` float32 values in InnoDB-managed columnar
storage, and vsql-vector provides `COSINE_DISTANCE`, `L2_DISTANCE`,
`L1_DISTANCE` and `INNER_PRODUCT`. It is not bundled with the server: build
and install it from its repository, and note that its README describes it as
under active development and not yet recommended for production.

Declare the column with the model's dimension (1536 for
`text-embedding-3-small`, 3072 for `gemini-embedding-001`; vsql-vector allows
up to 3072). `SVECTOR` accepts the JSON array `ai_embedding()` returns, but
only as a value, not as an expression: vsql-vector cannot yet convert a
function result into a typed vector, so `INSERT ... VALUES (..., ai_embedding(...))`
fails with "cannot implicitly cast string expression". Assign the embedding to
a user variable first:

```sql
INSTALL EXTENSION vsql_vector;

-- 'embed' is a registered model ID for OpenAI's text-embedding-3-small:
--   "embed": {"provider": "openai", "model": "text-embedding-3-small",
--             "key_id": "openai_key", "key_owner": "root@localhost"}
CREATE TABLE docs (
    id        INT PRIMARY KEY,
    body      TEXT,
    embedding SVECTOR(1536)
);

SET @e = ai_embedding('embed', 'How do I reset my password?');
INSERT INTO docs VALUES (1, 'How do I reset my password?', @e);

SET @e = ai_embedding('embed', 'Best pizza toppings for a party');
INSERT INTO docs VALUES (2, 'Best pizza toppings for a party', @e);
```

The distance functions take a stored vector, not a variable, so keep the
query's embedding in a table too and join to it:

```sql
CREATE TABLE queries (id INT PRIMARY KEY, embedding SVECTOR(1536));
SET @q = ai_embedding('embed', 'I cannot log in to my account');
INSERT INTO queries VALUES (1, @q);

SELECT d.id, d.body, COSINE_DISTANCE(d.embedding, q.embedding) AS dist
FROM docs d JOIN queries q ON q.id = 1
ORDER BY dist
LIMIT 5;
```

Both restrictions are vsql-vector limitations (see its Known Limitations);
the search is a full scan until it adds an index.

#### Binary Embeddings

`ai_embedding_binary(model_id, text)` returns the same embedding as
`ai_embedding()`, as packed little-endian IEEE 754 float32 values: four bytes
per dimension, in order, with no header (6,144 bytes for a 1536-dimension
embedding). That is the byte layout MySQL's `VECTOR` type uses (MySQL 9, and
Cloud SQL for MySQL), so systems built around that type can read it as is. On
any server it fits a `VARBINARY` or `BLOB` column at a fifth to a third of the
JSON size, and clients decode it directly, for example with NumPy's
`np.frombuffer(value, dtype="<f4")`.

```sql
CREATE TABLE doc_vectors (id INT PRIMARY KEY, embedding VARBINARY(6144));
INSERT INTO doc_vectors
VALUES (1, ai_embedding_binary('embed', 'How do I reset my password?'));
SELECT id, LENGTH(embedding) DIV 4 AS dimensions FROM doc_vectors;
```

The result is labelled with the binary character set.

### Raw Requests

`ai_request(model_id, request_json)` sends a JSON body you write to the
registered model's native API and returns the provider's response unchanged,
in the spirit of Cloud SQL's `ml_predict_row()`. Use it for anything the other
functions do not expose: system prompts, multi-turn messages, temperature,
structured output, tool definitions, token usage.

```sql
SELECT JSON_UNQUOTE(JSON_EXTRACT(
  ai_request('gpt', '{
    "messages": [{"role": "system", "content": "Answer in one word."},
                 {"role": "user", "content": "Capital of France?"}],
    "temperature": 0}'),
  '$.choices[0].message.content')) AS answer;
```

| Provider | Endpoint | Body |
|---|---|---|
| `anthropic` | `POST /v1/messages` | Messages API; include `max_tokens` |
| `google` | `POST /v1beta/models/{model}:generateContent` | `generateContent` body; the model is in the URL |
| `openai` | `POST /v1/chat/completions` | Chat Completions body |
| `local` | `POST /api/chat` (Ollama) | Ollama chat body |
| `typesafe` | `POST /v1/systemone` | System One body (`state`, `questions`) |

Before sending, the body is checked against the registry:

- The registry decides the model. A body without `model` gets the registered
  one; a body that names a different model is refused, so a caller cannot
  switch to a costlier model than the one an administrator registered.
- Streaming is refused (`"stream": true`); the response must arrive whole. For
  Ollama, which streams by default, `"stream": false` is added.

A non-2xx response returns NULL with the provider's error message as a
warning. A response larger than the 64 KB result returns NULL with a warning
rather than truncated JSON.

### Decisions

`ai_decision(model_id, input, questions)` asks a decision model one or more
questions about `input` and returns calibrated probabilities instead of
generated text. Decision models suit classification, routing, moderation and
triage, where you want a probability you can threshold rather than prose to
parse. Register a decision model like any other:

```sql
-- in vsql_ai.models:
--   "decide": {"provider": "google", "model": "gemini-decision-1",
--              "key_id": "gemini_key", "key_owner": "root@localhost"}

SET @questions = '{
  "is_billing": {"type": "noul", "instructions": "Is this message about billing?",
                 "criteria": {"true": "About charges, invoices or refunds",
                              "false": "About anything else"}},
  "tone":       {"type": "choice", "instructions": "What is the tone of this message?",
                 "criteria": {"angry": "Upset or hostile", "calm": "Neutral or polite",
                              "excited": "Enthusiastic"}},
  "urgency":    {"type": "score", "instructions": "How urgent is this message?",
                 "criteria": ["Can wait", "Needs attention this week",
                              "Needs attention today"]}
}';

SELECT id,
       JSON_EXTRACT(d, '$.is_billing.noul') AS p_billing,
       JSON_UNQUOTE(JSON_EXTRACT(d, '$.tone.choice')) AS tone,
       JSON_EXTRACT(d, '$.urgency.score') AS urgency
FROM (SELECT id, ai_decision('decide', body, @questions) AS d FROM tickets) t;
```

**Questions** are a JSON object keyed by name (1–64 characters of `A-Z`,
`a-z`, `0-9`, `_`, `-`). Each has a `type`, `instructions` (a non-empty
string), and `criteria`:

| Type | Asks | `criteria` |
|---|---|---|
| `noul` | Yes or no | Optional: `{"true": "what counts as yes", "false": "what counts as no"}` |
| `choice` | Pick one option | Required: `{"option": "description", ...}`, 2–255 options; `""` for no description |
| `score` | Rate on a scale | Required: `["lowest", ..., "highest"]`, 2–10 levels |

**Answers** come back as a JSON object keyed by the same names. Every
provider's answers are translated to this shape:

| Type | Fields |
|---|---|
| `noul` | `noul`: probability the answer is yes; `confidence` (Google only) |
| `choice` | `choice`: most likely option; `probabilities`: `{"option": p}`; `confidence` |
| `score` | `score`: probability-weighted level, 0 to levels−1, can be fractional; `probabilities` and `legend`, keyed `"0"`, `"1"`, ...; `confidence` |
| `refusal` | OpenAI only: the model declined that question; others are still answered |

A field a provider does not return is left out rather than made up: TypeSafe
and OpenAI return no `confidence` for a `noul` answer. OpenAI's decisions API
has no criteria for yes/no questions, so for an `openai` model the `true` and
`false` descriptions are appended to the instructions.

The limits above are the ones every supported provider accepts. Each provider
also limits the number of questions per call (OpenAI 400, Google 1,000,
TypeSafe by context length), and the answers must fit the 64 KB result.

### Supported Providers

The `provider` field of a registry entry is one of the names below. Model
names are passed through to the provider unchanged — the extension
restricts only the provider, not the model. The lists below are the models
we test and recommend; any other model the provider's API accepts on the same
endpoint works too, including models released after this extension.

#### Anthropic (provider: `anthropic`)
Claude models:
- **Claude Opus 5.5**: `claude-opus-5-5` (recommended default - complex agentic and enterprise work)
- **Claude Fable 5.2**: `claude-fable-5-2` (most capable model, for the most demanding reasoning and long-horizon work)
- **Claude Sonnet 5.5**: `claude-sonnet-5-5` (best combination of speed and intelligence)
- **Claude Haiku 5.5**: `claude-haiku-5-5` (fastest and most cost-effective)

Also supported (previous generations): `claude-fable-5-1`, `claude-fable-5`,
`claude-opus-5`, `claude-opus-4-8`, `claude-opus-4-7`, `claude-opus-4-6`,
`claude-sonnet-5`, `claude-sonnet-4-6`, `claude-haiku-4-5`.

Current Claude models think before answering, and on Opus 5.5 and Fable 5.2
thinking cannot be turned off. Requests are sent with a 16,000-token output
ceiling so thinking does not crowd out the answer. A refusal, or a response
that reaches that ceiling before it produces any text, returns NULL with a
warning saying so. A response that reaches the ceiling after its answer has
begun returns the text produced so far, with no warning.

#### Google (provider: `google`)
**Generative:**
- **Gemini 3.8 Flash**: `gemini-3.8-flash` (stable - Google's migration target for Gemini 2.5 Pro)
- **Gemini 3.6 Flash**: `gemini-3.6-flash` (stable)
- **Gemini 3.5 Flash-Lite**: `gemini-3.5-flash-lite` (stable - fast and low cost)
- **Gemini 4**: `gemini-4-hydrogen` and the rest of the Gemini 4 family (`gemini-4-carbon`, `gemini-4-neon`, `gemini-4-argon`)
- **Gemini 3.1 Pro (preview)**: `gemini-3.1-pro-preview`

`gemini-2.5-flash` and `gemini-2.5-pro` are deprecated by Google and are
rerouted to `gemini-3.5-flash-lite` and `gemini-3.8-flash` on December 1, 2026.
`gemini-3-pro-preview` has been shut down.

**Embeddings:**
- **Gemini Embedding 2**: `gemini-embedding-2` (multimodal embedding model)
- **Gemini Embedding 001**: `gemini-embedding-001` (3072 dimensions by default)

**Decisions** (`ai_decision()` only):
- **Gemini Decision 1**: `gemini-decision-1` (beta; `/v1beta/decisions`)

#### OpenAI (provider: `openai`)
**Chat:**
- **GPT-6.2 Astra**: `gpt-6.2-astra` (flagship for complex reasoning)
- **GPT-6.2 Sol**: `gpt-6.2-sol` (balance of intelligence and cost)
- **GPT-6 Luna**: `gpt-6-luna` (fast and cost-effective for high-volume work)

`gpt-4o` and `gpt-4o-mini` are deprecated by OpenAI and shut down on
January 14, 2027; OpenAI recommends `gpt-6-luna` as the replacement.

**Embeddings:**
- **text-embedding-3-small**: `text-embedding-3-small` (1536 dimensions, best value)
- **text-embedding-3-large**: `text-embedding-3-large` (3072 dimensions, highest performance)

**Decisions** (`ai_decision()` only):
- **GPT-6 Luna**: `gpt-6-luna`, the only model OpenAI's decisions API accepts

#### TypeSafe (provider: `typesafe`)
TypeSafe's Jev models through the System One API. Decisions only:
`ai_prompt()` and `ai_embedding()` return a warning for a `typesafe` model.
- **Jev**: `jev-latest` (currently `jev-1.14.0`), or a versioned ID such as `jev-1.13.0`

#### Local Ollama (provider: `local`)
Connects to Ollama running on `127.0.0.1:11434`. No API key required. Use any model you have pulled in Ollama.

If Ollama listens on another port, set `VSQL_AI_LOCAL_PORT` in the environment that starts the server, for example `VSQL_AI_LOCAL_PORT=12345`. A new value takes effect when the server restarts. SQL users cannot change the port.

**Chat:** Any Ollama chat model (e.g., `llama3.2`, `mistral`, `gemma2`)

**Embeddings:** Any Ollama embedding model (e.g., `nomic-embed-text`, `mxbai-embed-large`)

### Function Reference

#### `ai_prompt(model_id, prompt)`
Send a prompt to a registered model and get its response.

**Parameters:**
- `model_id` (STRING): A model ID registered in `vsql_ai.models`
- `prompt` (STRING): The prompt text to send

**Returns:** STRING - The model's response, or NULL with a warning on failure

#### `ai_embedding(model_id, text)`
Generate a text embedding with a registered embedding model.

**Parameters:**
- `model_id` (STRING): A model ID registered in `vsql_ai.models` whose
  provider is `google`, `openai` or `local`
- `text` (STRING): Text to embed

**Returns:** STRING - JSON array of the embedding vector (dimensions vary by model)

#### `ai_embedding_binary(model_id, text)`
Generate a text embedding as packed little-endian float32.

**Parameters:** as for `ai_embedding()`.

**Returns:** BINARY - four bytes per dimension; see [Binary Embeddings](#binary-embeddings)

#### `ai_request(model_id, request_json)`
Send a native request body to a registered model's API.

**Parameters:**
- `model_id` (STRING): A model ID registered in `vsql_ai.models`
- `request_json` (STRING): The request body, as JSON; see [Raw Requests](#raw-requests)

**Returns:** STRING - the provider's JSON response, unchanged

#### `ai_decision(model_id, input, questions)`
Ask a decision model questions about a piece of input.

**Parameters:**
- `model_id` (STRING): A model ID registered in `vsql_ai.models` whose
  provider is `google`, `openai` or `typesafe`
- `input` (STRING): The text the questions are about
- `questions` (STRING): JSON object of named questions; see [Decisions](#decisions)

**Returns:** STRING - JSON object of answers keyed by question name, or NULL
with a warning on failure

#### `vsql_ai.models` (system variable)
Global, string, default empty. The model registry described in
[Register models](#4-register-models).

#### `vsql_ai.require_privilege` (system variable)
Global, boolean, default `ON`. Whether calling the functions requires the
`VSQL_AI_USER` privilege; see [Grant access](#5-grant-access). Changing it
needs `SYSTEM_VARIABLES_ADMIN` (or `SUPER`).

#### `VSQL_AI_USER` (dynamic privilege)
Required to call any vsql_ai function while `vsql_ai.require_privilege` is
`ON`. Grant it with `GRANT VSQL_AI_USER ON *.* TO ...`; granting it needs
`VSQL_AI_USER WITH GRANT OPTION` or `UPDATE` on the `mysql` schema. See
[Privileges at a glance](#privileges-at-a-glance).

## Security Considerations

### API Key Safety

API keys never appear in `ai_prompt()` or `ai_embedding()` calls. Each call
reads the key from the MySQL keyring for the length of that request only, so
keys stay out of query text, the general and slow query logs, the binary log,
`SHOW PROCESSLIST` and `performance_schema` statement history.

Storing a key is the one moment it passes through SQL. `keyring_key_store()`
is an ordinary function call, so its text, key included, is recorded wherever
statements are: run `SET SESSION sql_log_off = ON` first to keep it out of the
general log, and remember that `performance_schema` statement history keeps
the session's recent statements in memory. Keys written to the keyring by
other tooling never pass through SQL at all.

Error messages name the keyring data ID and owner that were looked up, never
any part of a key.

### Network Security

- All API requests use HTTPS with SSL certificate verification
- Cloud provider connections time out after 120 seconds, since current models reason before answering; the local (Ollama) provider allows 300 seconds, since local models can be slow to load
- Failed connections return clear error messages

## Performance Considerations

### Timeouts

AI API calls can take 5-120 seconds depending on prompt complexity, model speed, and how long the model reasons. Consider:

1. **MySQL Query Timeout**: You may need to adjust `max_execution_time`:
   ```sql
   SET SESSION max_execution_time = 120000; -- 120 seconds
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
until the provider responds — up to 120 seconds for cloud providers, 300 for
local Ollama. Cost scales linearly with row count: `SELECT ai_prompt(...) FROM t`
issues one request per row, serially. There is no batching or asynchronous
execution. Materialize results into a column rather than recomputing them per
query, and raise `max_execution_time` for multi-row statements.

**Needs preview extensions and a keyring.** The keyring, system variable and
MySQL services capabilities `vsql_ai` uses are VillageSQL preview capabilities, so
`vsql_allow_preview_extensions` must be ON, and they may change in a future
server release. Cloud models also need a keyring component.

**No embeddings from Anthropic.** `ai_embedding()` on an `anthropic` model returns a
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

The tests run `INSTALL EXTENSION vsql_ai`, which loads `vsql_ai.veb` from the
server's VEB directory. Run `make install` after every build so the tests
exercise your latest changes; otherwise they fail with `VEB file not found` or
silently test an older build.

On a packaged server, `lib/veb/` already contains a bundled `vsql_ai.veb`, and
MTR copies the bundled file in preference to one passed with
`--veb-source-dir`. To test your own build there, give the server a VEB
directory that holds it:

```bash
mkdir -p /tmp/vebdir
cp <server>/lib/veb/*.veb /tmp/vebdir/
cp build/vsql_ai.veb /tmp/vebdir/
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test \
    --mysqld=--veb-dir=/tmp/vebdir
```

The cloud provider tests and `ai_registry` restart the server with
`component_keyring_file` (configured in the test's data directory), store the
API key with `keyring_key_store()`, and remove it before finishing.

Each test grants `VSQL_AI_USER` to root after installing the extension and
revokes it before uninstalling, so no grant is left in `mysql.global_grants`.
`ai_privilege` covers the check itself.

```bash
cd $HOME/build/villagesql/mysql-test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test

# Run individual test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test error_handling
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
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_anthropic
   ```

#### Testing Google Gemini

1. **Export your API key:**
   ```bash
   export GEMINI_API_KEY='your-api-key-here'
   ```

2. **Run the prompt test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_google
   ```

3. **Run the embeddings test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_google
   ```

#### Testing OpenAI GPT

1. **Export your API key:**
   ```bash
   export OPENAI_API_KEY='your-api-key-here'
   ```

2. **Run the prompt test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_openai
   ```

3. **Run the embeddings test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_openai
   ```

#### Unit Tests

`unittest/vsql_ai_unittest.cc` covers code that SQL cannot reach on demand:
the exception guard around each function (which no SQL input reaches now that
the server converts arguments to valid utf8mb4), float32 packing, request
checks for `ai_request()`, and translation of decision answers, including
replies a live API will not produce on request. It needs no server or
network. It is built with the extension (turn it off with
`-DVSQL_AI_BUILD_TESTS=OFF`):

```bash
cd build
make
ctest --output-on-failure
```

#### Testing Decisions

The offline `ai_decision` test needs nothing. The live tests use the same keys
as the prompt tests, plus a TypeSafe key:

```bash
export GEMINI_API_KEY='...'      # ai_decision_google
export OPENAI_API_KEY='...'      # ai_decision_openai
export TYPESAFE_API_KEY='...'    # ai_decision_typesafe
cd $HOME/build/villagesql/mysql-test
perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test \
    ai_decision ai_decision_google ai_decision_openai ai_decision_typesafe
```

#### Testing Local Ollama

1. **Ensure Ollama is running** on `127.0.0.1:11434` with a model pulled. If Ollama uses another port, export `VSQL_AI_LOCAL_PORT` before you run the tests; MTR passes it to the server.

2. **Export the model name:**
   ```bash
   export OLLAMA_MODEL='llama3.2'
   export OLLAMA_EMBED_MODEL='nomic-embed-text'
   ```

3. **Run the prompt test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_prompt_local
   ```

4. **Run the embeddings test:**

   ```bash
   cd $HOME/build/villagesql/mysql-test
   perl mysql-test-run.pl --suite=/path/to/vsql-ai/mysql-test ai_embedding_local
   ```

**Security Note:** All tests automatically:
- Skip live API calls if the environment variable is not set
- Keep API keys out of the result file (`--disable_query_log`) and out of the
  server's general query log (`sql_log_off`)
- Validate that responses contain expected content

**Note:** The `ai_registry`, `ai_privilege`, `ai_decision`, `ai_request` and
`error_handling` tests need no API key and no network access; they cover the registry, the
keyring lookups, question and request validation, and argument validation.
The live prompt and embedding tests also exercise `ai_request()` and
`ai_embedding_binary()`.

## Development

### Project Structure
```
vsql-ai/
├── src/
│   ├── ai_functions.cc      # VEF function implementations and registration
│   ├── ai_providers.h/.cc   # AI provider implementations (Anthropic, OpenAI, Google, TypeSafe, Local)
│   ├── decisions.h/.cc      # ai_decision: question checks, per-provider translation
│   ├── embedding_format.h/.cc # Packing embeddings as float32 for ai_embedding_binary
│   ├── raw_request.h/.cc    # ai_request: body checks and native endpoints
│   ├── privilege.h/.cc      # VSQL_AI_USER: registration and the per-call check
│   ├── model_registry.h/.cc # Parsing and validation of vsql_ai.models
│   └── http_client.h/.cc    # HTTP client wrapper for API calls
├── unittest/
│   └── vsql_ai_unittest.cc  # Unit tests (ctest); no server needed
├── include/
│   ├── mysql/components/    # Vendored MySQL service headers (see include/mysql/README.md)
│   ├── httplib.h            # cpp-httplib single header
│   └── nlohmann/json.hpp    # nlohmann/json single header
├── cmake/
│   └── FindVillageSQL.cmake # CMake module to locate VillageSQL SDK
├── mysql-test/
│   ├── include/             # Keyring setup/teardown for tests
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
- ✅ API keys in the MySQL keyring
- ✅ Model registry (`vsql_ai.models`)
- ✅ OpenAI GPT integration
- ✅ OpenAI embeddings
- ✅ Local Ollama provider (no API key required)
- ✅ Binary (float32) embeddings and raw provider requests
- ✅ `VSQL_AI_USER` privilege for calling the functions
- ✅ Decision models: Gemini Decision, TypeSafe Jev, OpenAI decisions (`ai_decision()`)
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
