# CARTS Tools

This directory contains all the tools for the CARTS project.

## Directory Structure

```
tools/
├── carts               # Main wrapper script
├── run/                # CARTS compilation pipeline binary (carts-compile)
├── opt/                # CARTS optimization passes
└── setup/              # Setup tools
    └── carts-setup.py  # Automated setup script
```

## Quick Start

1. **Install everything automatically:**
   ```bash
   carts install
   # Checks deps, inits submodules, builds LLVM → Polygeist → ARTS → CARTS
   ```

2. **Build the project:**
   ```bash
   # Build CARTS project (default)
   carts build

   # Clean build
   carts build --clean

   # Build only ARTS components
   carts build --arts

   # Build only Polygeist
   carts build --polygeist

   # Build only LLVM
   carts build --llvm
   ```

3. **Use the unified wrapper:**
   ```bash
   # Full pipeline: C → MLIR → LLVM IR → executable
   carts compile simple.c -o simple_arts -O3 --arts-config arts.cfg

   # Stop at a pipeline step (C input)
   carts compile simple.c --pipeline concurrency --arts-config arts.cfg

   # MLIR transformations
   carts compile simple.mlir --O3 --arts-config arts.cfg

   # MLIR transformations, stop at a pipeline step
   carts compile simple.mlir --O3 --arts-config arts.cfg --pipeline edt-distribution

   # Link LLVM IR with ARTS runtime
   carts compile simple-arts.ll -o simple_arts

   # Run benchmarks
   carts benchmarks run polybench/gemm --size small --threads 2
   ```

> **Important**: Project build uses **system clang**, while ARTS operations use **installed LLVM**

## Commands

| Command | Description |
|---------|-------------|
| `carts compile <file.c>` | Full pipeline: C → MLIR → LLVM IR → executable |
| `carts compile <file.c> --pipeline <step>` | Full pipeline, stop after a pipeline step |
| `carts compile <file.mlir>` | MLIR transformations via carts-compile binary |
| `carts compile <file.mlir> --pipeline <step>` | MLIR transformations, stop at a pipeline step |
| `carts compile <file.ll> -o out` | Link LLVM IR with ARTS runtime |
| `carts build` | Build CARTS project |
| `carts test` | Run test suite |
| `carts lit [path...]` | Run bundled `llvm-lit` directly |
| `carts examples run --all` | Run example compilation+execution tests |
| `make check-doc-flags` | Check documented `carts-compile` flags vs CLI options |
| `carts benchmarks` | Build and manage benchmarks |
| `carts cgeist` | Run cgeist C-to-MLIR frontend |
| `carts clang` | Compile with LLVM clang and OpenMP |
| `carts clean` | Clean generated files |
| `carts format` | Format source files |
| `carts install` | Install CARTS: check deps, init submodules, build everything |

## Individual Tools

### Install (`carts install`)
Automated install that checks system dependencies, initializes submodules, and builds the full toolchain (LLVM → Polygeist → ARTS → CARTS).

```bash
carts install --help
carts install --check       # Only check dependencies
carts install --skip-deps   # Skip dependency checking
carts install --skip-build  # Only check/install deps + init submodules
carts install --auto        # Auto-install missing deps without prompting
```

### Benchmarks (`carts benchmarks`)
Performance testing and benchmarking commands.

```bash
carts benchmarks --help
carts benchmarks run --help
```

### Examples (`carts examples run --all`)
Run and test CARTS examples.

```bash
# Run all examples
carts examples run --all
```

**ARTS Configuration Priority:**
1. **Custom config** (`--arts-config /path/to/config.cfg`)
2. **Local config** (`example_dir/arts.cfg`)
3. **Global default** (`tests/examples/arts.cfg`)

## Environment Setup

### Adding carts to PATH
The install command automatically adds `carts` to your PATH.

### Environment Variables

- `CARTS_VERBOSE=1` - Enable verbose output
- `CARTS_INSTALL_DIR` - Override installation directory
