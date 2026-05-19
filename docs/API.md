# QuasarDB JSON API

QuasarDB exchanges UTF-8 JSON messages over plain TCP sockets. Every frame is length-prefixed:

```text
[4-byte uint32 payload size, network byte order][JSON payload]
```

The public client connection is stateful: Entrypoint keeps a session per TCP connection, including the active database selected by `USE <database>;`. Internal Entrypoint-to-Storage messages are stateless and include the database in every command.

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
  "message": "Query routed",
  "data": {}
}
```

`status` is one of `success`, `error`, or `pending`. `token` is omitted only for unauthenticated actions such as `login`.

## Public API

### login

```json
{
  "action": "login",
  "data": {
    "username": "admin",
    "password": "my_secure_password"
  }
}
```

```json
{
  "status": "success",
  "message": "Login successful",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5c..."
  }
}
```

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

Error:

```json
{
  "status": "error",
  "message": "Syntax error near 'WHERE': unknown column 'age'.",
  "data": {}
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
  "data": {}
}
```

Completed tasks return the same shape as a synchronous `query` response.

## Internal API

Internal requests are sent between Entrypoint and Storage nodes. Storage does not parse SQL strings.

### heartbeat

```json
{
  "action": "heartbeat",
  "data": {}
}
```

```json
{
  "status": "success",
  "message": "Node is alive",
  "data": {
    "node_id": "storage-node-01",
    "uptime_seconds": 3600,
    "cpu_load_percent": 15
  }
}
```

### execute_ast

```json
{
  "action": "execute_ast",
  "data": {
    "database": "my_db",
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
      ],
      "where_clause": {
        "node_type": "BinaryExpression",
        "operator": "AND",
        "left": {
          "node_type": "BinaryExpression",
          "operator": ">=",
          "left": {
            "node_type": "Identifier",
            "value": "age"
          },
          "right": {
            "node_type": "Literal",
            "data_type": "int",
            "value": 18
          }
        },
        "right": {
          "node_type": "BinaryExpression",
          "operator": "LIKE",
          "left": {
            "node_type": "Identifier",
            "value": "name"
          },
          "right": {
            "node_type": "Literal",
            "data_type": "string",
            "value": "^A.*"
          }
        }
      }
    }
  }
}
```

Create table:

```json
{
  "action": "execute_ast",
  "data": {
    "database": "my_db",
    "ast_root": {
      "node_type": "CreateTableStatement",
      "table": "users",
      "columns": [
        {
          "name": "id",
          "data_type": "int",
          "modifiers": ["INDEXED", "NOT_NULL"]
        },
        {
          "name": "role",
          "data_type": "string",
          "modifiers": [],
          "default_value": {
            "node_type": "Literal",
            "data_type": "string",
            "value": "user"
          }
        }
      ]
    }
  }
}
```

Temporal revert:

```json
{
  "action": "execute_ast",
  "data": {
    "database": "my_db",
    "ast_root": {
      "node_type": "RevertStatement",
      "table": "users",
      "target_timestamp": "2025.10.15-12:30:00.000000"
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

Entrypoint merges shard responses, combines aggregate partials, and returns the final public `query` result to the client.

## Security

JWT is required for every public action except `login` when authentication is enabled. RBAC checks happen in Entrypoint before `execute_ast` is sent to Storage. Errors returned to clients must avoid internal paths and stack traces.
