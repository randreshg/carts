# Contributing to CARTS

## Project Structure

- `lib/arts/` - Core MLIR dialect implementation
- `include/arts/` - Public headers
- `tools/` - User-facing scripts and utilities
  - `carts` - Main wrapper script
  - `run/` - C++ compilation driver
  - `setup/` - Environment provisioning
- `tests/` - Test suites
  - `contracts/` - Pipeline regression tests (MLIR + FileCheck)
  - `examples/` - End-to-end C/C++ tests
- `docker/` - Container workflows
- `external/` - Dependencies (ARTS, Polygeist, LLVM)

Compiler layering and ownership rules are documented in:
- `docs/contributing/compiler-structure.md`

## Build & Test Commands

```bash
# Setup and build
python3 tools/setup/carts-setup.py    # Install prerequisites
carts build                            # Build CARTS
carts build --clean                    # Clean build
carts build --arts --debug 0           # ARTS errors only
carts build --arts --debug 1           # ARTS warnings
carts build --arts --debug 2           # ARTS info
carts build --arts --debug 3           # Build ARTS with full debug logging

# Testing
carts compile simple.cpp -o simple     # Full compilation pipeline
carts test                             # Run all tests
carts check                            # Alias for carts test
carts lit tests/contracts/<file>.mlir  # Run focused lit regressions
carts lit --suite contracts            # Run the maintained contracts suite
make check-doc-flags                   # Validate docs flags against carts-compile options

# Formatting
carts format                           # Format tracked C/C++/TableGen files
carts format --check                   # Check formatting without edits

# Docker
carts docker build                     # Build Docker image + workspace volume
```

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
   - `docs/testing/benchmark-modes.md`

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
