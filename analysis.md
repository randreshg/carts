# CARTS Analysis

## How CARTS Works

For a complete guide to the CARTS compilation pipeline, optimization passes, debug commands, and troubleshooting:

- **[docs/agents.md](docs/agents.md)** — Full pipeline reference (15 stages, debug channels, partition algorithm, example workflows)

## Running Benchmarks

For building, running, and analyzing CARTS benchmarks:

- **[external/carts-benchmarks/README.md](external/carts-benchmarks/README.md)** — Benchmark runner CLI reference (thread sweeps, counter profiling, JSON export)
- **Per-benchmark README** — Each benchmark directory has a `README.md` with algorithm description, problem sizes, and build targets

## Quick Start

```bash
# Full pipeline: compile and run a benchmark
carts compile gemm.c -O3
./gemm_arts

# Inspect a specific pipeline stage
carts compile gemm.mlir --pipeline concurrency-opt

# Debug partitioning decisions
carts compile gemm.mlir --pipeline concurrency-opt --debug-only=db_partitioning 2>&1

# Run benchmarks with verification
carts benchmarks run polybench/gemm --size small --threads 2 --trace
```

## Self-Contained Binaries

Compiled ARTS binaries embed their configuration at compile time. The binary is self-contained and does not need an external `arts.cfg` at runtime.

- `ARTS_CFG=<path>` or `--arts-config <path>` selects the config **at compile time**
- The resulting binary works anywhere without config files
- Fallback: if no embedded config (old binaries), the runtime checks `artsConfig` env var → `./arts.cfg`
