# tools/ — CLI and Compilation Driver

## Directory Layout

```
carts_cli.py           Main CLI entry point (runs via dekk)
compile/               C++ compilation driver
  Compile.cpp          Pipeline definition (stage ordering, pass registration)
scripts/               Python modules for CLI subcommands
  build.py             `carts build` implementation
  compile.py           `carts compile` implementation
  test.py              `carts test` and `carts lit` implementation
  triage.py            `carts triage-benchmark` implementation
```
