# QuasarDB

Team course project of DBMS on the course "System Programming" of Moscow Aviation Institute (MAI).

## Dev Container

1. Open the project in VS Code.
2. Run `Dev Containers: Reopen in Container`.
3. Wait for `postCreateCommand` to run `make build`.

## Actions

### Build

```bash
make build
```

### Test

```bash
make test
```

### Check

```bash
make check
```

### Format

```bash
make format
```

## Project Structure

- `apps/client` - CLI client terminal
- `apps/server` - Entrypoint server (routing, security, observability)
- `apps/storage` - Storage node (data, indexes, journal)
- `libs/core` - Shared code and network primitives
- `docs/` - Documentation, tasks and report
- `external/` - Dependency manifests
