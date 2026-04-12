# carts

Language: C++, MLIR

CARTS is an MLIR-based compiler that transforms C/C++ programs with OpenMP
pragmas into task-based parallel executables targeting the ARTS asynchronous
runtime.

## Commands

- **build**: `dekk carts build` -- Build the CARTS compiler
- **test**: `dekk carts test` -- Run pass tests (co-located with source)
- **compile**: `dekk carts compile <file> -O3` -- Compile C/C++ to ARTS executable
- **pipeline**: `dekk carts pipeline --json` -- List 18 pipeline stages
- **doctor**: `dekk carts doctor` -- Validate toolchain and environment
- **format**: `dekk carts format` -- Format C/C++/TableGen source files
- **lit**: `dekk carts lit <file>` -- Run single lit regression test
- **benchmarks**: `dekk carts benchmarks run --size large` -- Run benchmarks
- **triage-benchmark**: `dekk carts triage-benchmark` -- Benchmark triage workflow

## Build

```bash
dekk carts build
dekk carts build --arts       # Rebuild ARTS runtime
dekk carts build --clean      # Full clean rebuild
dekk carts build --polygeist  # Rebuild Polygeist frontend
```

## Test

```bash
dekk carts test                    # Pass tests (fast, default)
dekk carts test --suite e2e        # E2E compile+run tests
dekk carts test --suite all        # Both suites
dekk carts lit lib/arts/dialect/sde/test/my_test.mlir  # Single test
```

Pass tests are co-located with source code (IREE-style):
- `lib/arts/dialect/sde/test/` — SDE dialect (stages 1-5)
- `lib/arts/dialect/core/test/` — Core ARTS dialect (stages 6-16)
- `lib/arts/dialect/rt/test/` — Runtime dialect (stages 17-18)
- `tests/cli/` — CLI validation
- `tests/verify/` — Verification passes

E2E tests: `tests/e2e/` — compile and run `samples/` programs.
Demo programs: `samples/` (formerly `tests/examples/`).
