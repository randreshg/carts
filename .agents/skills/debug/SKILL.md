---
name: carts-debug
description: Debug CARTS compiler issues, analyze diagnostic output, inspect pipeline stages, troubleshoot runtime behavior, and profile with ARTS counters. Use when the user asks to debug, diagnose, inspect, troubleshoot, profile, or analyze compiler or runtime behavior.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<input-file>]
---

# CARTS Debug

Two layers: **compile-time** (compiler diagnostics) and **runtime** (ARTS debug builds + counters).

## Compile-Time Diagnostics

```bash
# Stop at a pipeline stage (C or MLIR input)
carts compile <file> --pipeline <stage> -O3

# Dump ALL stages (MLIR input only)
carts compile <file.mlir> --all-pipelines -o /tmp/stages/

# Diagnostic JSON (partitioning decisions, EDT/DB info)
carts compile <file> --diagnose --diagnose-output diag.json -O3

# Debug channels (requires debug build)
carts compile <file.mlir> --arts-debug db_partitioning,loop_fusion
```

**C input** compiles through Polygeist first, saving `<name>.mlir`. For full pipeline inspection, take that `.mlir` and use `--all-pipelines`.

**Debug channel convention**: `snake_case(PassFilename)` — e.g., `DbPartitioning.cpp` -> `db_partitioning`.

Use `carts pipeline --json` for the canonical stage list.

## Runtime Debugging (ARTS)

```bash
carts build --arts --debug 0    # Release, errors only
carts build --arts --debug 1    # + warnings
carts build --arts --debug 2    # + info messages
carts build --arts --debug 3    # Debug build (-O0, sanitizers), full logs
```

Levels 0-2 stay Release. Level 3 is a Debug build — never benchmark with it. Logs go to stderr. Rebuild in release with `carts build --arts` when done.

## Counter Profiles

```bash
carts build --arts --counters 0  # all OFF (baseline)
carts build --arts --counters 1  # timing only (default)
carts build --arts --counters 2  # workload characterization
carts build --arts --counters 3  # full overhead analysis
```

Counters are compile-time configured — rebuild ARTS to change. Output: JSON in `./counters/`.

## Quick Reference

| Symptom | Action |
|---------|--------|
| Program slow | `--diagnose` for partitioning, `--counters 2` for runtime |
| Pass crashes | `--all-pipelines` to find breaking stage |
| Wrong codegen | `--pipeline <stage>` to inspect IR |
| Runtime hang | `carts build --arts --debug 3`, run, check stderr |
| Benchmark regression | `--counters 1`, compare timing JSON |

## Instructions

1. Identify the layer: compile-time or runtime
2. For compile-time: use `--diagnose`, `--pipeline`, `--arts-debug`, or `--all-pipelines`
3. For C source pipeline inspection: compile C first to get `.mlir`, then `--all-pipelines`
4. For runtime: rebuild ARTS with `--debug 3` or `--counters N`, run the program
5. Remind user to rebuild ARTS in release mode when done debugging
