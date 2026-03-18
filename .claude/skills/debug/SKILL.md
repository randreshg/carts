---
name: debug
description: Debug CARTS compiler issues, analyze diagnostic output, inspect pipeline stages, and troubleshoot compilation failures. Use when the user asks to debug, diagnose, inspect, troubleshoot, or analyze compiler behavior.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<input-file>]
---

# CARTS Debug

Diagnose and debug CARTS compiler behavior. Run `carts compile --help` for all diagnostic flags.

## Key Diagnostic Commands

```bash
# Get diagnostic JSON with partitioning decisions
carts compile <input> --diagnose --diagnose-output diag.json -O3

# Dump MLIR at a specific pipeline stage
carts compile <input.mlir> --pipeline <stage>

# Dump ALL pipeline stages
carts compile <input.mlir> --all-pipelines -o output_dir/

# Start from a specific stage (MLIR input only)
carts compile <input.mlir> --start-from <stage>

# Enable ARTS runtime debug channels
carts compile <input> --arts-debug info,debug

# View available pipeline stages
carts pipeline --json
```

## Debug Channels

Channels are passed via `--arts-debug channel1,channel2`.

**Convention**: channel name = `snake_case(filename)`, no prefix. For example:
- `DbPartitioning.cpp` → `--arts-debug db_partitioning`
- `LoopFusion.cpp` → `--arts-debug loop_fusion`
- `DeadCodeElimination.cpp` → `--arts-debug dead_code_elimination`

Some subsystem files share a channel (e.g., all DB transform files use `db_transforms`).

## Common Workflows

- **"Why is my program slow?"** — use `--diagnose` to inspect partitioning decisions
- **"Compilation fails at a pass"** — use `--all-pipelines` to find where it breaks
- **"Wrong code generation"** — use `--emit-llvm` or `--pipeline <stage>` to inspect IR
- **"DB partitioning is wrong"** — check `diag.json` for partition mode entries

## Instructions

When the user asks to debug:
1. Identify what they're debugging (crash, wrong output, performance, partitioning)
2. Run the appropriate diagnostic command
3. Analyze output and explain findings
4. Suggest fixes or workarounds
