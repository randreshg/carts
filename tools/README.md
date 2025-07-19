# CARTS Tools

This directory contains all the tools for the CARTS project.

## Directory Structure

```
tools/
├── carts               # Main wrapper script
├── run/                # CARTS C++ executable
├── opt/                # CARTS optimization passes
├── benchmark/          # Benchmarking tools
├── report/             # Reporting tools
└── setup/              # Setup tools
    └── carts-setup.py  # Automated setup script
```

## Quick Start

1. **Setup everything automatically:**
   ```bash
   python3 tools/setup/carts-setup.py
   source setup_env.sh
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
   # Compile C++ to MLIR (uses installed LLVM)
   carts cgeist simple.cpp -std=c++17 -fopenmp -O0 -S > simple.mlir
   
   # Run optimization passes (uses installed LLVM)
   carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg
   
   # Run the CARTS C++ executable
   carts run --help
   
   # Compile with ARTS runtime
   carts compile simple-arts.ll -o simple
   
   # Run benchmarks
   carts benchmark --target_examples matrixmul
   
   # Generate reports
   carts report --help
   ```

> **Important**: Project build uses **system clang**, while ARTS operations use **installed LLVM**

## Individual Tools

### Setup (`tools/setup/carts-setup.py`)
Automated setup script that installs all dependencies and builds the project.

```bash
python3 tools/setup/carts-setup.py --help
```

### Benchmark (`tools/benchmark/carts-benchmark`)
Performance testing and benchmarking tool.

```bash
python3 tools/benchmark/carts-benchmark --help
```

### Report (`tools/report/carts-report`)
Results visualization and analysis tool.

```bash
python3 tools/report/carts-report --help
```

## Environment Setup

### Adding carts to PATH
```bash
python3 tools/setup/carts-setup.py --add-to-path
```

### Environment Variables

- `CARTS_VERBOSE=1` - Enable verbose output
- `CARTS_INSTALL_DIR` - Override installation directory 