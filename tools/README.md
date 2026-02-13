# CARTS Tools

This directory contains all the tools for the CARTS project.

## Directory Structure

```
tools/
├── carts               # Main wrapper script
├── run/                # CARTS compilation pipeline binary (carts-compile)
├── opt/                # CARTS optimization passes
├── benchmark/          # Benchmarking tools
└── setup/              # Setup tools
    └── carts-setup.py  # Automated setup script
```

## Quick Start

1. **Setup everything automatically:**
   ```bash
   python3 tools/setup/carts-setup.py
   # The 'carts' command is now available in your PATH
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

   # Stop at a pipeline stage (C input)
   carts compile simple.c --pipeline concurrency --arts-config arts.cfg

   # MLIR transformations
   carts compile simple.mlir --O3 --arts-config arts.cfg

   # MLIR transformations, stop at stage
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
| `carts compile <file.c> --pipeline <stage>` | Full pipeline, stop after MLIR stage |
| `carts compile <file.mlir>` | MLIR transformations via carts-compile binary |
| `carts compile <file.mlir> --pipeline <stage>` | MLIR transformations, stop at stage |
| `carts compile <file.ll> -o out` | Link LLVM IR with ARTS runtime |
| `carts build` | Build CARTS project |
| `carts check` | Run test suite |
| `carts check --examples` | Run example compilation+execution tests |
| `carts benchmarks` | Build and manage benchmarks |
| `carts cgeist` | Run cgeist C-to-MLIR frontend |
| `carts clang` | Compile with LLVM clang and OpenMP |
| `carts clean` | Clean generated files |
| `carts format` | Format source files |
| `carts report` | Analyze benchmark results |
| `carts setup` | Set up CARTS environment |

## Individual Tools

### Setup (`tools/setup/carts-setup.py`)
Automated setup script that installs all dependencies, builds the project, and automatically adds `carts` to your PATH.

```bash
python3 tools/setup/carts-setup.py --help
```

### Benchmarks (`carts benchmarks`)
Performance testing and benchmarking commands.

```bash
carts benchmarks --help
carts benchmarks run --help
```

### Examples (`carts check --examples`)
Run and test CARTS examples.

```bash
# Run all examples
carts check --examples
```

**ARTS Configuration Priority:**
1. **Custom config** (`--arts-config /path/to/config.cfg`)
2. **Local config** (`example_dir/arts.cfg`)
3. **Global default** (`tests/examples/arts.cfg`)

## Environment Setup

### Adding carts to PATH
The setup script automatically adds `carts` to your PATH. If you restart your terminal, you can re-add it with:

```bash
python3 tools/setup/carts-setup.py --add-to-path
```

### Environment Variables

- `CARTS_VERBOSE=1` - Enable verbose output
- `CARTS_INSTALL_DIR` - Override installation directory
