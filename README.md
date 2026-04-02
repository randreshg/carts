# CARTS - Compiler for Asynchronous Runtime Systems

CARTS is an MLIR-based compiler that transforms C and C++ programs annotated
with OpenMP pragmas into task-based parallel executables targeting the ARTS
asynchronous runtime. It lowers OpenMP constructs through a multi-stage pipeline
of analysis, optimization, and code-generation passes, producing executables
that exploit fine-grained dataflow parallelism on shared-memory and distributed
systems.

## Quick Start
```bash
git clone <repo-url> && cd carts
pip install dekk
dekk carts install
```

After installation, the `carts` command is installed into your PATH:

```bash
carts doctor                            # Verify the toolchain
carts compile example.c -O3 -o example  # Compile a program
carts test                              # Run the test suite
```

See **[Getting Started](docs/getting-started.md)** for the full walkthrough.

## Project Structure

| Directory | Purpose |
|---|---|
| `include/arts/` | Public C++ headers (dialect, passes, utilities) |
| `lib/arts/` | Core MLIR dialect, analysis, and pass implementations |
| `tools/` | CLI entry point (`carts_cli.py`) and compilation driver |
| `tools/compile/` | C++ compilation driver (`carts-compile`) |
| `tools/scripts/` | Python modules backing each CLI subcommand |
| `tests/contracts/` | Pipeline regression tests (MLIR + FileCheck) |
| `tests/examples/` | End-to-end C/C++ integration tests |
| `external/` | Vendored dependencies (ARTS, Polygeist, LLVM) |
| `docs/` | Architecture, pipeline, and developer documentation |
| `docker/` | Container workflows for multi-node execution |

## CLI Commands

| Command | Description |
|---|---|
| `dekk carts install` | Create the project environment, fetch submodules, build, and install the `carts` wrapper |
| `carts doctor` | Validate the toolchain and environment |
| `carts build` | Build CARTS (`--clean` for fresh, `--arts` for runtime) |
| `carts compile <file> [flags]` | Run the full compilation pipeline |
| `carts pipeline` | Display or query compiler pipeline stages |
| `carts test` | Run the complete test suite |
| `carts lit <test.mlir>` | Run a single lit regression test |
| `carts format` | Format tracked C/C++/TableGen files (`--check` to verify) |
| `carts benchmarks <cmd>` | Build and run benchmarks (list, run, build, clean) |
| `carts clean` | Remove generated files (`--all` for full clean) |
| `carts update` | Update git submodules |
| `carts docker <cmd>` | Docker operations (build, start, stop, exec) |
| `carts examples <cmd>` | List or run bundled examples |
| `dekk carts agents <cmd>` | Generate/install agent resources from `.agents/` |
| `dekk carts worktree <cmd>` | Manage dekk-aware CARTS worktrees |

## Documentation

- [Getting Started](docs/getting-started.md) -- setup, first compilation, examples
- [Developer Guide](docs/developer-guide.md) -- supported constructs and memory layouts
- [Compiler Pipeline](docs/compiler/pipeline.md) -- pass ordering and stage ownership
- [Contributing](docs/contributing.md) -- coding style, testing, and commit guidelines
