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

#### CARTS Compilation Pipeline

The CARTS compilation process follows this pipeline:

```
C++ Source (.cpp)
        ↓
carts cgeist → MLIR (.mlir)
        ↓
carts run → LLVM IR (.ll)
        ↓
carts compile → Executable (binary)
```

#### Step-by-Step Usage

```bash
# Step 1: Convert C++ to MLIR
carts cgeist simple.cpp -std=c++17 -fopenmp -O0 -S > simple.mlir

# Step 2: Apply ARTS transformations and convert to LLVM IR
carts run simple.mlir --O3 --arts-opt --emit-llvm > simple-arts.ll

# Step 3: Compile to executable with ARTS runtime
carts compile simple-arts.ll -o simple

# Alternative: Run the complete pipeline automatically
carts execute simple.cpp -o simple    # Automatically detects C++ and uses -std=c++17
carts execute simple.c -o simple      # Automatically detects C and uses -std=c17
```

#### Individual Commands

```bash
# Run individual optimization passes
carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg

# Run benchmarks
carts benchmark --target_examples matrixmul

# Generate interactive reports
carts report

# Clean generated files
carts clean
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
- `carts cgeist` - Convert C++ to MLIR (uses installed LLVM)
- `carts opt` - Run individual optimization passes (uses installed LLVM)
- `carts run` - Apply ARTS transformations and convert to LLVM IR (uses installed LLVM)
- `carts compile` - Compile LLVM IR to executable with ARTS runtime (uses installed LLVM)
- `carts execute` - Run complete pipeline: C++ → MLIR → LLVM IR → Executable
- `carts clean` - Clean generated files (.ll, .mlir, executables)
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
