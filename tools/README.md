# CARTS Tools

This directory contains the CARTS compiler CLI and supporting implementation
modules.

## Command Model

Use one command model only:

- `dekk carts ...` for project lifecycle operations such as setup, install,
  worktrees, and agent generation
- `carts ...` for daily compiler, testing, benchmark, and Docker usage after
  install

Do not use checked-in wrappers or direct `python tools/carts_cli.py` invocations
as the public interface. The repo-local `tools/carts` file exists only as a
compatibility shim for internal benchmark/tooling paths that still expect that
entrypoint.

## Directory Layout

```text
tools/
  carts_cli.py     Main CARTS CLI entry point
  compile/         C++ compilation driver (`carts-compile`)
  scripts/         Python modules backing CLI subcommands
```

## Common Commands

```bash
dekk carts install
dekk carts agents generate

carts build
carts compile simple.c -O3 -o simple_arts
carts test
carts lit tests/contracts/example.mlir
carts benchmarks list
```

## Notes

- dekk creates the project-local conda environment from `environment.yml`
- the installed `carts` wrapper lives under `.install/bin/`
- `carts-compile` lives under `.install/carts/bin/`
