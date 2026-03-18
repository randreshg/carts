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

Run `carts compile --help` for all flags. View pipeline stages with `carts pipeline --json`.

### Input types and what works with each

The `carts compile` command routes based on file extension. **Available flags differ by input type.**

#### C/C++ input (`.c` / `.cpp`) — full 5-step pipeline

```
carts compile <file.c> [options]
```

The C pipeline runs 5 steps internally:
1. Sequential Polygeist compilation → `<name>_seq.mlir`
2. Metadata extraction → `<name>_arts_metadata.mlir` + `.carts-metadata.json`
3. Parallel Polygeist compilation → **`<name>.mlir`** (Polygeist output, INPUT to CARTS pipeline)
4. ARTS transformations (runs `carts-compile` on the `.mlir`) → LLVM IR or stopped-at MLIR
5. Link with ARTS runtime → `<name>_arts` executable

**Available flags for C input:**
| Flag | Effect |
|------|--------|
| `--pipeline <stage>` | Stops at step 4 (ARTS transformations) at the given stage. Output: `<name>.<stage>.mlir`. Skips linking (step 5). |
| `--diagnose` | Generates diagnostic JSON during step 4. Output: `<name>-diagnose.json` |
| `--diagnose-output <path>` | Custom path for diagnostic JSON |
| `-O3` | Enable O3 optimizations in the ARTS pipeline |
| `--arts-debug <channels>` | Enable debug channels (requires debug build) |

**NOT available for C input:** `--all-pipelines`, `--start-from`, `--emit-llvm`, `--collect-metadata` (these are .mlir-only flags).

**Key insight**: The `<name>.mlir` file saved in step 3 is the **Polygeist parallel output** — the INPUT to the CARTS pipeline, NOT the CARTS output. To inspect all CARTS pipeline stages, take that `.mlir` file and use it as MLIR input (see below).

#### MLIR input (`.mlir`) — CARTS transformation pipeline only

```
carts compile <file.mlir> [options]
```

Runs the `carts-compile` binary directly on the MLIR. This is where fine-grained pipeline inspection lives.

**Available flags for MLIR input:**
| Flag | Effect |
|------|--------|
| `--pipeline <stage>` | Stop after a specific pipeline stage. Output to stdout or `-o <file>`. |
| `--all-pipelines -o <dir>/` | Dump ALL pipeline stages as separate files: `<name>_<stage>.mlir` per stage. **MLIR input only.** |
| `--start-from <stage>` | Start pipeline from a specific stage (input must already be at that stage). |
| `--emit-llvm` | Emit LLVM IR instead of MLIR output. |
| `--collect-metadata` | Run metadata collection only. |
| `--diagnose` | Generate diagnostic JSON. |
| `--diagnose-output <path>` | Custom path for diagnostic JSON. |
| `-O3` | Enable O3 optimizations. |
| `--arts-debug <channels>` | Enable debug channels (requires debug build). |

#### LLVM IR input (`.ll`) — link only

```
carts compile <file.ll> [-o <output>]
```

Links LLVM IR with the ARTS runtime to produce an executable. No pipeline flags apply.

### Two-step workflow for full pipeline inspection from C source

To inspect every pipeline stage for a C source file:

```bash
# Step 1: Compile from C — this saves <name>.mlir (Polygeist output)
carts compile program.c -O3

# Step 2: Run all pipeline stages on the saved MLIR
carts compile program.mlir --all-pipelines -o /tmp/stages/

# Or stop at a specific stage:
carts compile program.mlir --pipeline concurrency-opt -o /tmp/concurrency.mlir
```

Alternatively, use `--pipeline` directly on C input for a single stage:

```bash
# This runs the full C pipeline but stops ARTS transforms at the given stage
carts compile program.c --pipeline concurrency-opt -O3
# Output: program.concurrency-opt.mlir
```

### Pipeline stages (in order)

```
raise-memref-dimensionality → collect-metadata → initial-cleanup →
openmp-to-arts → pattern-pipeline → edt-transforms → create-dbs →
db-opt → edt-opt → concurrency → edt-distribution → concurrency-opt →
epochs → pre-lowering → arts-to-llvm → complete
```

Use `carts pipeline --json` for the canonical list.

### Diagnostic JSON

```bash
# From C source
carts compile program.c --diagnose --diagnose-output diag.json -O3

# From MLIR
carts compile program.mlir --diagnose --diagnose-output diag.json -O3
```

The diagnostic JSON contains partitioning decisions, EDT info, DB info, and machine config. **Note**: diagnostics are collected at the `collect-metadata` stage (early in the pipeline), so they reflect early state, not final optimized partitioning. To see final partitioning, use `--pipeline concurrency-opt` and read the MLIR directly.

### Debug Channels

Channels are passed via `--arts-debug channel1,channel2`. Requires a debug build of `carts-compile`.

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
- **"Compilation fails at a pass"** → `--all-pipelines` to find where it breaks (use `.mlir` input)
- **"What does the IR look like after optimization?"** → `--pipeline concurrency-opt` for final partitioned IR
- **"Wrong code generation"** → `--emit-llvm` or `--pipeline <stage>` to inspect IR
- **"Runtime hangs or wrong behavior"** → `carts build --arts --debug 3`, run, inspect stderr
- **"Benchmark regression"** → `carts build --arts --counters 1`, run benchmark, compare timing JSON

## Instructions

When the user asks to debug:
1. Identify the layer: compile-time (compiler passes) or runtime (ARTS behavior)
2. For compile-time: use `--diagnose`, `--pipeline`, `--arts-debug`, or `--all-pipelines`
3. For pipeline inspection from C source: compile C first to get `.mlir`, then use `--all-pipelines` on it
4. For runtime: rebuild ARTS with `--debug 3` or `--counters N`, then run the program
5. Analyze output and explain findings
6. Remind user to rebuild ARTS in release mode (`carts build --arts`) when done debugging
