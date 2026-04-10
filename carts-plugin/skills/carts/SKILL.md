---
name: carts-cli
description: General CARTS CLI reference. Use when the user asks about carts commands, how to use the CLI, compile files, run examples, format code, clean artifacts, update submodules, check the pipeline, or any general carts usage.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [command]
---

# CARTS CLI

Use `dekk carts` for project lifecycle and `carts` for compiler work. NEVER use
`make`, `ninja`, or full paths to `carts_cli.py`.

## Available commands

!`carts --help 2>&1`

## Quick Reference

| Command | Purpose |
|---------|---------|
| `dekk carts build` | Build the compiler |
| `dekk carts compile <file> -O3` | Compile C/C++ to ARTS executable |
| `dekk carts test` | Run all tests |
| `dekk carts lit <test.mlir>` | Run single lit test |
| `dekk carts format` | Format C/C++ code |
| `dekk carts doctor` | Check environment health |
| `dekk carts pipeline --json` | List pipeline stages |
| `dekk carts benchmarks list` | List available benchmarks |
| `dekk carts skills status` | Show agent skill status |

## Instructions

When the user asks about any carts command:
1. Run `carts <command> --help` to get up-to-date usage for that specific command
2. Show the relevant syntax and run the command if requested
3. For sub-commands (e.g., `carts examples`), run `carts <command> --help` to discover sub-commands
4. Always prefer `carts` over raw tools — it handles paths, environment, and dependencies
