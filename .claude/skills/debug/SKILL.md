---
name: debug
description: Debug CARTS compiler issues, analyze diagnostic output, inspect pipeline stages, troubleshoot runtime behavior, and profile with ARTS counters. Use when the user asks to debug, diagnose, inspect, troubleshoot, profile, or analyze compiler or runtime behavior.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<input-file>]
---

# CARTS Debug

Two layers of debugging: **compile-time** (compiler diagnostics) and **runtime** (ARTS debug builds and counters).

## 1. Compile-Time Diagnostics

Run `carts compile --help` for all flags.

```bash
# Diagnostic JSON with partitioning decisions
carts compile <input> --diagnose --diagnose-output diag.json -O3

# Dump MLIR at a specific pipeline stage
carts compile <input.mlir> --pipeline <stage>

# Dump ALL pipeline stages
carts compile <input.mlir> --all-pipelines -o output_dir/

# Start from a specific stage (MLIR input only)
carts compile <input.mlir> --start-from <stage>

# Enable compiler debug channels (requires debug build of carts-compile)
carts compile <input> --arts-debug db_partitioning,concurrency

# View available pipeline stages
carts pipeline --json
```

### Debug Channels

Channels are passed via `--arts-debug channel1,channel2`.

**Convention**: channel name = `snake_case(filename)`, no prefix. For example:
- `DbPartitioning.cpp` → `--arts-debug db_partitioning`
- `LoopFusion.cpp` → `--arts-debug loop_fusion`
- `DeadCodeElimination.cpp` → `--arts-debug dead_code_elimination`

Some subsystem files share a channel (e.g., all DB transform files use `db_transforms`).

## 2. Runtime Debugging (ARTS)

### Debug Build

Build ARTS with debug logging to see runtime behavior (scheduler, DBs, EDTs):

```bash
carts build --arts --debug 0    # Release, errors only
carts build --arts --debug 1    # Release, + warnings
carts build --arts --debug 2    # Release, + info messages
carts build --arts --debug 3    # DEBUG BUILD (-O0, sanitizers, symbols), + full debug logs
```

Levels 0-2 stay **Release** (optimized) — only the log verbosity changes. Level 3 switches to a **Debug build** (no optimization, ASan/UBSan enabled) — never benchmark with this.

ARTS logs go to **stderr**. Compile and run your program normally after rebuilding. To go back to release:

```bash
carts build --arts               # release build, no debug output
```

Run `carts build --help` for all build flags.

### Counter Profiles

Build ARTS with counter instrumentation for performance profiling:

```bash
carts build --arts --counters 0  # all OFF (baseline, no overhead)
carts build --arts --counters 1  # timing only (minimal overhead, DEFAULT)
carts build --arts --counters 2  # workload characterization (EDT/DB/memory/remote)
carts build --arts --counters 3  # full overhead analysis (all counters)
carts build --arts --profile path/to/custom.cfg  # custom profile
```

Counters are **compile-time configured** — you must rebuild ARTS to change profiles. Counter output is written as JSON to the `counter_folder` directory (default: `./counters/`). Profile configs live in `external/carts-benchmarks/configs/profiles/`.

## Common Workflows

- **"Why is my program slow?"** → `--diagnose` for partitioning, then `--counters 2` for runtime workload
- **"Compilation fails at a pass"** → `--all-pipelines` to find where it breaks
- **"Wrong code generation"** → `--emit-llvm` or `--pipeline <stage>` to inspect IR
- **"Runtime hangs or wrong behavior"** → `carts build --arts --debug 3`, run, inspect stderr
- **"Benchmark regression"** → `carts build --arts --counters 1`, run benchmark, compare timing JSON

## Instructions

When the user asks to debug:
1. Identify the layer: compile-time (compiler passes) or runtime (ARTS behavior)
2. For compile-time: use `--diagnose`, `--pipeline`, `--arts-debug`, or `--all-pipelines`
3. For runtime: rebuild ARTS with `--debug 3` or `--counters N`, then run the program
4. Analyze output and explain findings
5. Remind user to rebuild ARTS in release mode (`carts build --arts`) when done debugging
