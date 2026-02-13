# CARTS - Compiler for Asynchronous Runtime System

CARTS is an LLVM/MLIR-based compiler framework that transforms OpenMP C/C++ programs into ARTS (Asynchronous Runtime Task System) executables for distributed and parallel computing.

**Pipeline**: `C/C++ → Polygeist MLIR → CARTS Passes → LLVM IR → Executable`

---

## Quick Start

```bash
# Install dependencies, build, and setup PATH
python3 tools/setup/carts-setup.py

# Build CARTS
carts build

# Complete pipeline: C++ to executable
carts compile simple.cpp -o simple

# Run tests
carts test

# Format source files
carts format
```

## Running Tests

- Use `carts test` (or `carts check`) to run the lit test suites under `tests/`.
- The lit configuration (`tests/lit.cfg.py`) resolves tools from `.install/`, so re-run `carts build` whenever you modify the compiler to keep the installed toolchain fresh.
- `carts test` validates `llvm-lit`, `FileCheck`, and `carts-compile` from `.install/`.

---
## Common Commands

```bash
# Build commands
carts build                  # Build CARTS
carts build --clean          # Clean build
carts build --arts --debug   # Build ARTS with debug logging

# Compilation pipeline
carts cgeist file.c -fopenmp -S > file.mlir     # C to MLIR
carts compile file.mlir --arts-config arts.cfg      # Apply CARTS passes
carts compile file.mlir --pipeline edt-distribution # Stop after strategy selection + ForLowering
carts compile file.ll -o file                       # LLVM IR to executable
carts compile file.c -o file                        # All-in-one

# Testing
carts test                   # Run all tests
carts check                  # Alias for carts test

# Formatting
carts format                 # Format tracked C/C++/TableGen sources
carts format --check         # Verify formatting only

# Benchmarking
cd external/carts-benchmarks/kastors/<benchmark>
../../../tools/run/dump-carts-stages.sh \
  file.c --outdir ./stages --arts-config ./arts.cfg
```

---

## Project Structure

```
carts/
├── lib/arts/              # ARTS MLIR dialect implementation
│   ├── Conversion/        # Dialect conversions (OpenMP→ARTS, ARTS→LLVM)
│   ├── Passes/            # Compiler passes
│   └── Utils/             # Utility functions
├── include/arts/          # Public headers
├── tools/                 # Command-line tools
│   ├── carts              # Main wrapper script
│   ├── run/               # Compilation driver
│   └── setup/             # Environment setup
├── tests/                 # Test suites
│   ├── contracts/         # Contract-focused compiler tests
│   └── examples/          # End-to-end tests
├── docs/                  # Documentation
├── external/
│   ├── arts/              # ARTS runtime (submodule)
│   ├── Polygeist/         # C→MLIR frontend (submodule)
│   └── carts-benchmarks/  # Benchmark suite
└── docker/                # Container workflows
```

---

## Environment Variables

### Compilation

- `CARTS_VERBOSE=1` - Enable verbose debugging output
- `CARTS_INSTALL_DIR` - Override installation directory

### Runtime (arts.cfg)

```ini
[ARTS]
threads=8                # Number of worker threads
launcher=ssh             # Launcher type (ssh, mpi, local)
masterNode=localhost     # Master node
nodeCount=1              # Number of nodes
protocol=tcp             # Communication protocol
```

See `docs/agents.md` for pipeline usage and
`docs/heuristics/distribution/distribution.md` for distribution strategy details.

---

## Debugging

### Dump all compilation stages

```bash
tools/run/dump-carts-stages.sh \
  example.c --outdir ./stages --arts-config ./arts.cfg
```

Output includes one file per pipeline stage, e.g.
`stages/example_concurrency.mlir`, `stages/example_edt-distribution.mlir`,
`stages/example_concurrency-opt.mlir`, and `stages/example_arts-to-llvm.mlir`.

### Enable verbose output

```bash
export CARTS_VERBOSE=1
carts compile example.c -o example
```

### Runtime debugging

```bash
# Build ARTS with debug logging
carts build --arts --debug

# Run with ARTS tracing
./example
```

## Notes

- Always use the `carts` wrapper for all operations
- Project builds with system clang; ARTS uses installed LLVM toolchain
- Setup script automatically adds `carts` to PATH
- Generated files (`*.mlir`, `*.ll`, `stages/`) are gitignored

---

**Author**: Rafael Andres Herrera Guaitero
**Email**: <rafaelhg@udel.edu>
**License**: See LICENSE file
