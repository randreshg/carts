# Contributing to CARTS

## Project Structure

- `lib/arts/` - Core MLIR dialect implementation
- `include/arts/` - Public headers
- `tools/` - User-facing scripts and utilities
  - `carts` - Main wrapper script
  - `run/` - C++ compilation driver
  - `benchmark/` - Performance utilities
  - `setup/` - Environment provisioning
- `tests/` - Test suites
  - `contracts/` - Pipeline regression tests (MLIR + FileCheck)
  - `examples/` - End-to-end C/C++ tests
- `docker/` - Container workflows
- `external/` - Dependencies (ARTS, Polygeist, LLVM)

## Build & Test Commands

```bash
# Setup and build
python3 tools/setup/carts-setup.py    # Install prerequisites
carts build                            # Build CARTS
carts build --clean                    # Clean build
carts build --arts --debug             # Build ARTS with debug logging

# Testing
carts compile simple.cpp -o simple     # Full compilation pipeline
carts test                             # Run all tests
carts check                            # Alias for carts test

# Formatting
carts format                           # Format tracked C/C++/TableGen files
carts format --check                   # Check formatting without edits

# Docker
docker/docker-build.sh --smart         # Build with cached layers
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

3. **Always run** `carts test` locally before submitting

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
