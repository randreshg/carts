# Getting Started

This guide takes you from a fresh clone to your first compiled CARTS program
in about 15 minutes (plus build time).

## Prerequisites

Install the following system tools before you begin:

| Tool | Version | Check |
|------|---------|-------|
| conda | any | `conda --version` |
| git | any | `git --version` |
| cmake | >= 3.20 | `cmake --version` |
| ninja | any | `ninja --version` |
| make | any | `make --version` |
| clang | >= 14 | `clang --version` |
| clang++ | >= 14 | `clang++ --version` |
| python | >= 3.11 | `python --version` |

Optional:

| Tool | Purpose |
|------|---------|
| clang-format | Source formatting (`carts format`) |
| lit | Test runner (`carts test`) -- built automatically if missing |

## 1. Clone

```bash
git clone <repo-url>
cd carts
```

You do **not** need `--recurse-submodules` -- the install step handles
submodules automatically with shallow clones to save bandwidth.

## 2. Install

The canonical install path is dekk-first. dekk creates the project-local conda
environment, activates the `.dekk.toml` configuration, initializes submodules,
builds the toolchain, and installs the `carts` wrapper.

```bash
pip install dekk
dekk carts install
```

### What install does

1. **Creates the project environment** -- creates `.dekk/env` from `environment.yml`
2. **Initializes git submodules** -- ARTS, Polygeist, LLVM, carts-benchmarks
3. **Builds the full toolchain** in order:
   - LLVM + MLIR + OpenMP (longest step, ~30-60 min)
   - Polygeist (C-to-MLIR frontend)
   - ARTS runtime
   - CARTS compiler
4. **Installs the `carts` wrapper** into `.install/bin/` and updates shell PATH

The installed binaries go into `.install/{llvm,polygeist,arts,carts}/`.

## 3. Use `carts`

After installation, the `carts` command is available in your PATH. All
subsequent operations go through it:

```bash
carts doctor    # System and toolchain diagnostics
carts test      # Run the full test suite
```

### If something fails

```bash
# Check what's missing
carts doctor

# Rebuild a specific component
carts build --llvm      # Rebuild LLVM
carts build --arts      # Rebuild ARTS runtime
carts build --polygeist # Rebuild Polygeist
carts build             # Rebuild CARTS compiler only
carts build --clean     # Clean rebuild of CARTS
```

## 4. Compile your first program

CARTS compiles OpenMP-annotated C/C++ into ARTS task-based executables:

```bash
carts compile tests/examples/dotproduct/dotproduct.c -O3 -o dotproduct
./dotproduct
```

The `compile` command automatically routes through the full pipeline:
C source -> Polygeist (cgeist) -> MLIR passes -> LLVM IR -> executable.

### Compile flags

```bash
carts compile file.c -O3 -o output    # Optimized build
carts compile file.c -g -o output     # Debug build
carts compile file.c --emit-llvm      # Stop at LLVM IR
carts compile file.c --diagnose       # Emit compiler diagnostics
carts compile file.c --pipeline=stage # Run up to a specific pipeline stage
```

Run `carts compile --help` for the full list.

## 5. Explore examples

CARTS ships with 17 example programs covering common parallel patterns:

| Example | Pattern |
|---------|---------|
| `dotproduct/` | Parallel reduction |
| `matrixmul/` | Block task dependencies |
| `stencil/` | Neighbor (point) dependencies |
| `jacobi/` | Iterative stencil with convergence |
| `cholesky/` | Complex multi-block factorization |
| `parallel_for/` | Parallel loop variants |
| `smith-waterman/` | Dynamic programming wavefront |
| `convolution/` | Sliding window computation |

Run all examples:

```bash
carts examples run --all
```

List available examples:

```bash
carts examples list
```

All examples live in `tests/examples/` and serve as reference patterns for
writing your own CARTS programs.

## Project structure

```
carts/
  include/arts/        C++ headers (dialect, passes, utilities)
  lib/arts/            Core MLIR dialect, analysis, and passes
  tools/               CLI entry point (carts_cli.py) and scripts
  tools/compile/       C++ compilation driver (carts-compile)
  tests/contracts/     MLIR regression tests (FileCheck)
  tests/examples/      End-to-end C/C++ example programs
  external/            Vendored deps (ARTS, Polygeist, LLVM)
  docs/                Architecture and pipeline documentation
  docker/              Multi-node Docker cluster setup
  .dekk.toml           Environment and dependency configuration
```

## Docker setup (multi-node)

For multi-node benchmarking with SLURM:

```bash
carts docker build     # Build the Docker image
carts docker start     # Start a 6-node simulated cluster
carts docker exec      # Open a shell in the cluster
carts docker stop      # Tear down
```

See [docker/README.md](../docker/README.md) for details.

## All CLI commands

Run `carts --help` for the full command list. Key commands:

| Command | Description |
|---------|-------------|
| `dekk carts install` | Full project setup: environment, submodules, build, wrapper install |
| `carts doctor` | System and toolchain diagnostics |
| `carts build` | Build components (`--arts`, `--llvm`, `--polygeist`) |
| `carts compile` | Compile C/C++/MLIR through the pipeline |
| `carts test` | Run the test suite |
| `carts examples` | List and run example programs |
| `carts benchmarks` | Performance benchmarking tools |
| `carts pipeline` | Inspect compiler pipeline stages |
| `carts clean` | Clean generated files |
| `carts update` | Update git submodules |
| `carts docker` | Multi-node Docker operations |
| `carts format` | Format source files |
| `dekk carts agents` | Generate and install agent resources |
| `dekk carts worktree` | Create and manage project worktrees |

## Next steps

- [Developer Guide](developer-guide.md) -- Supported OpenMP constructs and
  memory layout requirements
- [Compiler Pipeline](compiler/pipeline.md) -- 16-stage compilation pipeline,
  pass ordering, and stage ownership
- [Contributing](contributing.md) -- Coding style, testing, and commit
  guidelines
