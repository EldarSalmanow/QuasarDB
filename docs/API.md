# QuasarDB JSON API

QuasarDB exchanges UTF-8 JSON messages over plain TCP sockets. Every frame is length-prefixed:

```text
[4-byte uint32 payload size, network byte order][JSON payload]
```

The public client connection is stateful: Entrypoint keeps a session per TCP connection, including the active database selected by `USE <database>;`. Internal Entrypoint-to-Storage messages are stateless.

## Envelope

Request:

```json
{
  "action": "query",
  "token": "JWT_TOKEN",
  "data": {}
}
```

Response:

```json
{
  "status": "success",
  "message": "Query executed",
  "data": {}
}
```

`status` is one of `success`, `error`, or `pending`. `token` is omitted for unauthenticated actions such as `handshake` and `login`.

## Public API

### handshake

The client sends `handshake` immediately after connecting. Entrypoint returns whether authentication is enabled and whether first-run superuser creation is required.

```json
{
  "action": "handshake",
  "data": {}
}
```

```json
{
  "status": "success",
  "message": "Handshake complete",
  "data": {
    "auth_required": true,
    "setup_required": true
  }
}
```

### login

Normal login:

```json
{
  "action": "login",
  "data": {
    "username": "admin",
    "password": "my_secure_password"
  }
}
```

First-run superuser creation:

```json
{
  "action": "login",
  "data": {
    "username": "admin",
    "password": "my_secure_password",
    "create": true
  }
}
```

Successful response:

```json
{
  "status": "success",
  "message": "Login successful",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5c..."
  }
}
```

The first created account is a superuser: Entrypoint grants it all permissions on all databases and tables.

### query

The client sends raw SQL. Entrypoint parses it, validates semantics and RBAC, resolves the active database, builds an AST, and routes `execute_ast` to Storage.

```json
{
  "action": "query",
  "token": "eyJhbGciOiJIUzI1NiIsInR5c...",
  "data": {
    "query": "SELECT SUM(salary) AS total_salary FROM users WHERE age >= 18 AND name LIKE \"^A.*\";"
  }
}
```

Synchronous success:

```json
{
  "status": "success",
  "message": "Query executed",
  "data": {
    "result": [
      {
        "total_salary": 150000
      }
    ]
  }
}
```

Long-running operation:

```json
{
  "status": "pending",
  "message": "Operation is running in background",
  "data": {
    "task_id": "550e8400-e29b-41d4-a716-446655440000"
  }
}
```

### check_task

```json
{
  "action": "check_task",
  "token": "eyJhbGciOiJIUzI1NiIsInR5c...",
  "data": {
    "task_id": "550e8400-e29b-41d4-a716-446655440000"
  }
}
```

Pending:

```json
{
  "status": "pending",
  "message": "Operation is still running",
  "data": {
    "task_id": "550e8400-e29b-41d4-a716-446655440000",
    "status": "running"
  }
}
```

Completed tasks return the same shape as a synchronous `query` response. Failed tasks return `status: "error"` with the stored error message.

### telemetry

Returns Entrypoint metrics. Authentication is required when auth is enabled.

```json
{
  "action": "telemetry",
  "token": "eyJhbGciOiJIUzI1NiIsInR5c...",
  "data": {}
}
```

```json
{
  "status": "success",
  "message": "Telemetry",
  "data": {
    "total_requests": 42,
    "total_errors": 1,
    "error_rate": 0.02,
    "current_rps": 3.0,
    "avg_rps_10min": 0.5,
    "max_rps_10min": 7,
    "avg_duration_ms": 12.4
  }
}
```

`current_rps` is the last full-second request count, `avg_rps_10min` and `max_rps_10min` use a 10-minute rolling window, `avg_duration_ms` uses a 10-second rolling window, and `error_rate` uses a 60-second rolling window.

## Client Commands

These are local CLI commands and are not SQL statements:

- `exit` closes the client.
- `logout` clears the current token and starts the login flow again.
- `telemetry` sends the public `telemetry` API request.

## Internal API

Storage does not parse SQL strings. Entrypoint sends already serialized ASTs to storage nodes.

### ping

Used by health monitoring.

```json
{
  "action": "ping",
  "data": {}
}
```

```json
{
  "status": "success",
  "message": "pong",
  "data": {}
}
```

### execute_ast

```json
{
  "action": "execute_ast",
  "data": {
    "ast_root": {
      "node_type": "SelectStatement",
      "table": "users",
      "projections": [
        {
          "node_type": "AggregateFunction",
          "function": "SUM",
          "argument": {
            "node_type": "Identifier",
            "value": "salary"
          },
          "alias": "total_salary"
        }
      ]
    }
  }
}
```

Storage response:

```json
{
  "status": "success",
  "message": "",
  "data": {
    "rows": [],
    "rows_affected": 0
  }
}
```

## Security

JWT is required for every public action except `handshake` and `login` when authentication is enabled. RBAC checks happen in Entrypoint before `execute_ast` is sent to Storage. Errors returned to clients must avoid internal paths and stack traces.
