# QuasarDB
Team course project of DBMS on the course "System Programming" of Moscow Aviation Institute (MAI).

## Dev Container

1. Open the project in VS Code.
2. Run `Dev Containers: Reopen in Container`.
3. Wait for `postCreateCommand` to run `make build`.

## Local Build (Makefile + Conan + CMake)

```bash
cd /home/eldar/QuasarDB
make setup
make build
make test
```

Useful targets:

```bash
cd /home/eldar/QuasarDB
make format
make check
make ci
```
