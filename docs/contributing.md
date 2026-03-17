# Contributing to CARTS

## Project Structure

- `lib/arts/` - Core MLIR dialect implementation
- `include/arts/` - Public headers
- `tools/` - User-facing scripts and utilities
  - `carts_cli.py` - Main CLI entry point (run via `sniff` or `./tools/carts` wrapper)
  - `compile/` - C++ compilation driver (`carts-compile`)
  - `scripts/` - Python CLI subcommands/helpers
- `tests/` - Test suites
  - `contracts/` - Pipeline regression tests (MLIR + FileCheck)
  - `examples/` - End-to-end C/C++ tests
- `docker/` - Container workflows
- `external/` - Dependencies (ARTS, Polygeist, LLVM)

Compiler layering and ownership rules are documented in:
- `docs/compiler/pipeline.md`

## Build & Test Commands

```bash
# Setup and build
carts install                          # Install prerequisites
carts build                            # Build CARTS
carts build --clean                    # Clean build
carts build --arts --debug 0           # ARTS errors only
carts build --arts --debug 1           # ARTS warnings
carts build --arts --debug 2           # ARTS info
carts build --arts --debug 3           # Build ARTS with full debug logging

# Testing
carts compile simple.cpp -o simple     # Full compilation pipeline
carts test                             # Run all tests
carts lit tests/contracts/<file>.mlir  # Run focused lit regressions
carts lit --suite contracts            # Run the maintained contracts suite
make check-doc-flags                   # Validate docs flags against carts-compile options

# Formatting
carts format                           # Format tracked C/C++/TableGen files
carts format --check                   # Check formatting without edits

# Docker
carts docker build                     # Build Docker image + workspace volume
```

## Environment Management

CARTS uses [sniff](https://github.com/randreshg/sniff) for environment management. The environment configuration is defined in `.sniff.toml` at the repository root.

### Setup

```bash
pip install git+https://github.com/randreshg/sniff.git   # once, globally
sniff tools/carts_cli.py install                          # bootstrap + build
```

sniff auto-detects the project, creates a Python environment from `tools/pyproject.toml`, activates `.sniff.toml` paths/env vars, and runs the CLI. After installation, `carts` is in your PATH.

Alternative without sniff:

```bash
./tools/carts install
```

### Key Commands

```bash
carts doctor              # Validate that all required tools and dependencies are present
carts env                 # Show current environment variables and paths
```

### How It Works

- `.sniff.toml` declares the required toolchain versions, environment variables, and paths.
- When you run `sniff tools/carts_cli.py <command>`, sniff reads `.sniff.toml` and activates the environment (PATH, env vars) before executing.
- `carts doctor` runs a suite of checks (cmake version, compiler availability, LLVM install, etc.) and reports any issues.

Run `carts doctor` after cloning the repository and whenever you update dependencies to ensure your environment is correctly configured.

## Coding Style

Follow LLVM conventions:

- Two-space indentation
- Braces on same line
- `CamelCase` for types/ops (e.g., `DbDimOp`)
- `camelCase` for variables
- Run `carts format` (or `clang-format -i <file>`) before committing
- Keep `// RUN:` and `FileCheck` directives aligned

## Testing Guidelines

1. **MLIR regression tests** → `tests/contracts/<area>/`
   - Use `// RUN: %carts opt ...` with FileCheck
   - Cover new ops/passes with positive and negative cases

2. **Integration tests** → `tests/examples/<name>/`
   - Include source files, `arts.cfg`, and expected MLIR
   - Document in per-directory README

3. **Always run** `carts lit tests/contracts/<changed-test>.mlir` for focused compiler changes, and `carts test` before submitting
4. **For distributed changes**, run the 3-mode benchmark workflow in:
   - `docs/benchmarks/modes.md`

## Commit Guidelines

- Imperative, single-sentence subjects (e.g., `Update Dockerfile to install Node.js`)
- Split functional and tooling changes
- Reference issues/tasks when available
- Include build/test commands in PR description
- Attach logs/screenshots for UI or benchmark changes

## Environment Notes

- Build tree is out-of-source (`build/` and `.install/`)
- Never edit generated files
- Export `CARTS_VERBOSE=1` to debug wrapper invocations
- Docker scripts mount shared workspace volume for clean builds
- Environment configuration lives in `.sniff.toml` at the repo root
- Run `carts doctor` to diagnose environment issues
