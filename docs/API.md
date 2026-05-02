# QuasarDB API

## Overview
This document defines the JSON-based client-server communication protocol for QuasarDB. All requests and responses are exchanged as JSON objects over TCP connections.

## Message Format

### Request Structure
```json
{
  "method": "string",
  "sql": "string (optional)",
  "token": "string (optional)",
  "request_id": "string (optional)"
}
```

### Response Structure
```json
{
  "status": "ok | error",
  "code": "string",
  "message": "string",
  "data": "string (JSON-encoded)"
}
```

## Methods

### ExecuteQuery (Synchronous)
Executes a SQL query and waits for the result.

**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "SELECT * FROM users WHERE id = 1",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response (Success):**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Query executed successfully",
  "data": "[{\"id\": 1, \"name\": \"Alice\", \"age\": 30}]"
}
```

**Response (Error):**
```json
{
  "status": "error",
  "code": "SYNTAX_ERROR",
  "message": "Unexpected token at line 1, column 15",
  "data": ""
}
```

### SubmitQuery (Asynchronous)
Submits a long-running query for background execution. Returns immediately with a request ID.

**Request:**
```json
{
  "method": "SubmitQuery",
  "sql": "SELECT COUNT(*) FROM large_table WHERE condition = true",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "ACCEPTED",
  "message": "Query submitted for execution",
  "data": "{\"request_id\": \"550e8400-e29b-41d4-a716-446655440000\"}"
}
```

### GetStatus (Asynchronous Status Check)
Checks the status of an asynchronous query.

**Request:**
```json
{
  "method": "GetStatus",
  "request_id": "550e8400-e29b-41d4-a716-446655440000",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response (Pending):**
```json
{
  "status": "ok",
  "code": "PENDING",
  "message": "Query is pending execution",
  "data": "{\"request_id\": \"550e8400-e29b-41d4-a716-446655440000\", \"state\": \"Pending\"}"
}
```

**Response (Running):**
```json
{
  "status": "ok",
  "code": "RUNNING",
  "message": "Query is currently executing",
  "data": "{\"request_id\": \"550e8400-e29b-41d4-a716-446655440000\", \"state\": \"Running\"}"
}
```

**Response (Completed):**
```json
{
  "status": "ok",
  "code": "COMPLETED",
  "message": "Query execution completed",
  "data": "{\"request_id\": \"550e8400-e29b-41d4-a716-446655440000\", \"state\": \"Completed\", \"result\": \"[{\\\"count\\\": 42}]\"}"
}
```

**Response (Failed):**
```json
{
  "status": "error",
  "code": "FAILED",
  "message": "Query execution failed: table not found",
  "data": "{\"request_id\": \"550e8400-e29b-41d4-a716-446655440000\", \"state\": \"Failed\"}"
}
```

### Login (Authentication)
Authenticates a user and returns a JWT token.

**Request:**
```json
{
  "method": "Login",
  "username": "alice",
  "password": "secure_password_123"
}
```

**Response (Success):**
```json
{
  "status": "ok",
  "code": "AUTHENTICATED",
  "message": "Login successful",
  "data": "{\"token\": \"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...\", \"expires_at\": \"2026-05-03T12:00:00Z\"}"
}
```

**Response (Error):**
```json
{
  "status": "error",
  "code": "UNAUTHORIZED",
  "message": "Invalid username or password",
  "data": ""
}
```

## Error Codes

| Code | Description |
|------|-------------|
| `SUCCESS` | Operation completed successfully |
| `ACCEPTED` | Async query accepted for execution |
| `PENDING` | Async query is pending |
| `RUNNING` | Async query is running |
| `COMPLETED` | Async query completed |
| `FAILED` | Async query failed |
| `SYNTAX_ERROR` | SQL syntax error |
| `SEMANTIC_ERROR` | Semantic error (type mismatch, NOT_NULL violation, etc.) |
| `AUTH_REQUIRED` | Authentication required |
| `UNAUTHORIZED` | Invalid credentials or token |
| `FORBIDDEN` | Insufficient permissions (RBAC) |
| `NOT_FOUND` | Database, table, or record not found |
| `CONFLICT` | Constraint violation (e.g., duplicate key) |
| `INTERNAL_ERROR` | Internal server error |
| `TIMEOUT` | Query execution timeout |
| `UNAVAILABLE` | Storage node unavailable |

## Data Types

### SELECT Result Format
Results are returned as a JSON array of objects:

```json
[
  {"id": 1, "name": "Alice", "age": 30},
  {"id": 2, "name": "Bob", "age": null},
  {"id": 3, "name": "Charlie", "age": 25}
]
```

**Type Mapping:**
- `int` → JSON number
- `string` → JSON string
- `NULL` → JSON null

### INSERT/UPDATE/DELETE Result Format
```json
{
  "affected_rows": 5
}
```

### CREATE/DROP Result Format
```json
{
  "message": "Table 'users' created successfully"
}
```

## SQL Grammar Support

### Supported Statements
- `CREATE DATABASE <name>`
- `DROP DATABASE <name>`
- `CREATE TABLE <name> (<column_definitions>)`
- `DROP TABLE <name>`
- `INSERT INTO <table> (<columns>) VALUES (<values>)`
- `SELECT <columns> FROM <table> [WHERE <condition>]`
- `UPDATE <table> SET <assignments> [WHERE <condition>]`
- `DELETE FROM <table> [WHERE <condition>]`
- `REVERT <table> <timestamp>`

### Column Modifiers
- `NOT_NULL` - Column cannot contain NULL values
- `INDEXED` - Column is indexed with B*+ tree
- `DEFAULT <value>` - Default value for column

### Data Types
- `int` - 32-bit signed integer
- `string` - Variable-length string

### Operators
- Comparison: `=`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `AND`, `OR`, `NOT`
- Pattern matching: `LIKE` (with regex support)
- Parentheses for grouping: `(`, `)`

### Aggregate Functions
- `COUNT(<column>)` - Count non-NULL values
- `SUM(<column>)` - Sum of numeric values
- `AVG(<column>)` - Average of numeric values

## Examples

### Example 1: Create Database and Table
**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "CREATE DATABASE mydb",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Database created",
  "data": "{\"message\": \"Database 'mydb' created successfully\"}"
}
```

**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "CREATE TABLE users (id int NOT_NULL INDEXED, name string NOT_NULL, age int DEFAULT 0)",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Table created",
  "data": "{\"message\": \"Table 'users' created successfully\"}"
}
```

### Example 2: Insert and Query Data
**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30)",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Insert successful",
  "data": "{\"affected_rows\": 1}"
}
```

**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "SELECT * FROM users WHERE age > 25 AND name LIKE 'A%'",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Query executed",
  "data": "[{\"id\": 1, \"name\": \"Alice\", \"age\": 30}]"
}
```

### Example 3: Aggregation
**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "SELECT COUNT(id), AVG(age) FROM users WHERE age > 18",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Query executed",
  "data": "[{\"COUNT(id)\": 42, \"AVG(age)\": 28.5}]"
}
```

### Example 4: Temporal Revert
**Request:**
```json
{
  "method": "ExecuteQuery",
  "sql": "REVERT users 2026.05.01-14:30:00.000",
  "token": "..."
}
```

**Response:**
```json
{
  "status": "ok",
  "code": "SUCCESS",
  "message": "Revert successful",
  "data": "{\"message\": \"Table 'users' reverted to 2026-05-01 14:30:00.000\"}"
}
```

## Protocol Details

### Connection Flow
1. Client establishes TCP connection to Entrypoint server
2. Client sends Login request (if authentication is enabled)
3. Server responds with JWT token
4. Client sends ExecuteQuery or SubmitQuery requests with token
5. Server processes and responds
6. Connection remains open for multiple requests (keep-alive)

### Message Framing
- Each JSON message is terminated with a newline character (`\n`)
- Messages are UTF-8 encoded
- Maximum message size: 16 MB

### Timeout Behavior
- Synchronous queries: 30 seconds default timeout
- Asynchronous queries: No timeout (runs until completion or failure)
- Connection idle timeout: 5 minutes

### Async Query Lifecycle
1. Client submits query with `SubmitQuery`
2. Server returns GUID v4 request_id immediately
3. Query enters queue (state: `Pending`)
4. Worker picks up query (state: `Running`)
5. Query completes (state: `Completed` or `Failed`)
6. Client polls with `GetStatus` to retrieve result
7. Result is cached for 1 hour after completion

## Security

### Authentication
- JWT tokens are required for all operations (except Login)
- Tokens expire after 24 hours
- Token format: `eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...`

### Authorization (RBAC)
- Permissions checked at database and table level
- Operations: `READ`, `WRITE`, `CREATE`, `DELETE`
- User can have direct permissions or inherit from groups

### Error Handling
- Never expose internal paths or stack traces in error messages
- Log detailed errors server-side for debugging
- Return generic `INTERNAL_ERROR` for unexpected failures
