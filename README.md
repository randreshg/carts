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
carts execute simple.cpp -o simple

# Run tests
carts check
```

---

## Documentation

All documentation is in the [`docs/`](docs/) directory:

### Essential Guides

- **[docs/COMPILER.md](docs/COMPILER.md)** - Complete compiler pipeline, passes, stages, and limitations
- **[docs/BENCHMARKS.md](docs/BENCHMARKS.md)** - Available benchmarks, performance comparisons, and tuning
- **[docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)** - Development guidelines, coding style, and testing

### Advanced Topics

- **[docs/COMPILER_PATTERNS.md](docs/COMPILER_PATTERNS.md)** - Critical OpenMP patterns requiring special handling
- **[NEXT_TASKS_OMP_SINGLE.md](NEXT_TASKS_OMP_SINGLE.md)** - Implementation plan for OMP single region support

### External Documentation

- [CARTS Documentation (Notion)](https://www.notion.so/248fde4bf3a2816d8c11f3c1cf5c4617)
- [Architecture & Dependencies](https://www.notion.so/249fde4bf3a281b5ab16e087ee4056b1)
- [Next Steps Dashboard (Kanban)](https://www.notion.so/a5b6038e1d06457ba2c97032d0da8e79)

---

## Key Features

### Fully Supported

- Direct `#pragma omp parallel for` patterns
- Multidimensional arrays (automatic linearization)
- Task-based parallelism with dependencies
- `#pragma omp task depend` clauses

### Partial Support

- Nested `parallel` + multiple `for` (requires proper barrier insertion)
- See [docs/COMPILER_PATTERNS.md](docs/COMPILER_PATTERNS.md) for details

### Not Yet Supported

- `#pragma omp single` regions
- Implementation plan in [NEXT_TASKS_OMP_SINGLE.md](NEXT_TASKS_OMP_SINGLE.md)

---

## Performance

CARTS provides **2-3x speedup** over OpenMP for:

- Pipeline parallelism (overlapping execution stages)
- Async recursion (no taskwait overhead)
- Irregular workloads (adaptive task graphs)

See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for details and examples.

---

## Common Commands

```bash
# Build commands
carts build                  # Build CARTS
carts build --clean          # Clean build
carts build --arts --debug   # Build ARTS with debug logging

# Compilation pipeline
carts cgeist file.c -fopenmp -S > file.mlir     # C to MLIR
carts run file.mlir --arts-config arts.cfg      # Apply CARTS passes
carts compile file.ll -o file                   # LLVM IR to executable
carts execute file.c -o file                    # All-in-one

# Testing
carts check                  # Run all tests
carts check --suite arts     # Run specific suite

# Benchmarking
cd external/carts-benchmarks/kastors/<benchmark>
/Users/randreshg/Documents/carts/tools/run/dump-carts-stages.sh \
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
│   ├── benchmark/         # Benchmarking utilities
│   └── setup/             # Environment setup
├── tests/                 # Test suites
│   ├── arts/              # Pipeline regression tests
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

See [docs/COMPILER.md](docs/COMPILER.md) for complete configuration options.

---

## Debugging

### Dump all compilation stages

```bash
/Users/randreshg/Documents/carts/tools/run/dump-carts-stages.sh \
  example.c --outdir ./stages --arts-config ./arts.cfg
```

Output: `stages/example_{simplify,openmp-to-arts,edt,epochs,...}.mlir`

### Enable verbose output

```bash
export CARTS_VERBOSE=1
carts execute example.c -o example
```

### Runtime debugging

```bash
# Build ARTS with debug logging
carts build --arts --debug

# Run with ARTS tracing
ARTS_DEBUG=1 ARTS_TRACE=1 ./example
```

See [docs/COMPILER.md](docs/COMPILER.md) for more debugging techniques.

---

## Contributing

1. Read [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines
2. Follow LLVM coding style (2-space indent, CamelCase types)
3. Run `carts check` before submitting
4. Include test commands in PR description
5. Track work in [Next Steps Dashboard](https://www.notion.so/a5b6038e1d06457ba2c97032d0da8e79)

---

## Notes

- Always use the `carts` wrapper for all operations
- Project builds with system clang; ARTS uses installed LLVM toolchain
- Setup script automatically adds `carts` to PATH
- Generated files (`*.mlir`, `*.ll`, `stages/`) are gitignored

---

**Author**: Rafael Andres Herrera Guaitero

**Email**: <rafaelhg@udel.edu>

**License**: See LICENSE file
