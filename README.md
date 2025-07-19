# CARTS - Compiler for Asynchronous Runtime System

CARTS is an LLVM/MLIR-based compiler framework that implements the ARTS (Asynchronous Runtime System) dialect for distributed programming.

## Quick Start

### 1. Automated Setup (Recommended)

```bash
# Install all dependencies and build the project
python3 tools/setup/carts-setup.py

# Set up the environment
source setup_env.sh
```

For detailed setup instructions, see [tools/README.md](tools/README.md).

### 2. Build CARTS Project

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

### 3. Use CARTS

```bash
# Compile C++ with OpenMP to MLIR
carts cgeist simple.cpp -std=c++17 -fopenmp -O0 -S > simple.mlir

# Run CARTS optimization passes
carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg

# Compile with ARTS runtime
carts compile simple-arts.ll -o simple

# Run benchmarks
carts benchmark --target_examples matrixmul

# Generate interactive reports
carts report
```

> **Important**: Project build uses **system clang**, while ARTS operations use **installed LLVM**

## Project Structure

```
carts/
├── tools/              # All CARTS tools
│   ├── setup.py       # Automated setup
│   ├── carts/         # Main compiler tools
│   ├── benchmark/     # Performance testing
│   └── report/        # Results visualization
├── examples/          # Example programs
├── test/             # Test cases
├── include/          # Header files
├── lib/              # Library source
└── external/         # External dependencies
```

## Manual Setup

If you prefer manual setup, see [tools/README.md](tools/README.md) for detailed instructions.

## Tools

### Unified Wrapper (`carts`)

The main interface for all CARTS operations:

- `carts build` - Build CARTS project (uses system clang)
- `carts cgeist` - Compile C++ to MLIR (uses installed LLVM)
- `carts opt` - Run optimization passes (uses installed LLVM)
- `carts run` - Run the CARTS C++ executable
- `carts compile` - Compile with ARTS runtime
- `carts benchmark` - Run performance tests
- `carts report` - Generate reports
- `carts setup` - Automated setup

### Individual Tools

- `tools/setup/carts-setup.py` - Automated dependency installation
- `tools/benchmark/carts-benchmark` - Performance testing
- `tools/report/carts-report` - Results visualization

## Examples

See `examples/` directory for sample programs and `test/` for test cases.

## Documentation

- [Tools Documentation](tools/README.md)
- [Configuration Guide](docs/configuration.md)

---

**Author**: Rafael Andres Herrera Guaitero  
**Email**: <rafaelhg@udel.edu>
