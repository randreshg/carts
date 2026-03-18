---
name: build
description: Build the CARTS project (compiler, runtime, LLVM, Polygeist). Use when the user asks to build, compile the project, rebuild, or fix build errors.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [--clean | --arts | --polygeist | --llvm]
---

# CARTS Build

Build the CARTS project. NEVER use `make` or `ninja` directly.

## Usage

Run `carts build --help` for the latest options.

Common workflows:
- `carts build` — rebuild CARTS compiler only (fastest)
- `carts build --clean` — full clean rebuild
- `carts build --arts --debug 3` — rebuild ARTS runtime with debug logging

## Troubleshooting

If the build fails:
1. Run `carts install --check` to verify dependencies
2. Try `--clean` flag for a fresh build
3. Check submodules: `git submodule update --init --recursive`

## Instructions

When the user asks to build:
1. Run `carts build $ARGUMENTS`
2. Report success/failure with relevant output
3. If build fails, analyze the error and suggest fixes
