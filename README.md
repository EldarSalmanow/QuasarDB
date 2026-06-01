# QuasarDB

![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/EldarSalmanow/QuasarDB/.github%2Fworkflows%2Fci.yml)

Team course project of DBMS on the course "System Programming" of Moscow Aviation Institute (MAI).

To see more detailed information about project, please refer to [docs/](./docs/).

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

### Install

```bash
make install
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

- `apps/client/` - CLI client terminal
- `apps/server/` - Entrypoint server (routing, security, observability)
- `apps/storage/` - Storage node (data, indexes, journal)
- `docs/` - Documentation, tasks and report
- `examples/` - Example usage of client
- `external/` - Dependency manifests
- `libs/core/` - Shared code and network primitives
- `scripts/` - Utility scripts for development and testing
- `tests/` - Integration tests'
