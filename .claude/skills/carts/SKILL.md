---
name: carts
description: General CARTS CLI reference. Use when the user asks about carts commands, how to use the CLI, compile files, run examples, format code, clean artifacts, update submodules, check the pipeline, or any general carts usage.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [command]
---

# CARTS CLI

The `carts` command is the unified CLI. NEVER use `make`, `ninja`, or full paths to `carts_cli.py`.

## Available commands

!`carts --help 2>&1`

## Instructions

When the user asks about any carts command:
1. Run `carts <command> --help` to get up-to-date usage for that specific command
2. Show the relevant syntax and run the command if requested
3. For sub-commands (e.g., `carts examples`), run `carts <command> --help` to discover sub-commands
