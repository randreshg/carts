---
name: carts-build
description: Use when the user asks to build, compile the project, rebuild CARTS/ARTS/LLVM/Polygeist, or fix build errors.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [--clean | --arts | --polygeist | --llvm]
---

# CARTS Build

Build the CARTS project. NEVER use `make` or `ninja` directly.

## Usage

Run `dekk carts build --help` for the latest options.

Common workflows:
- `dekk carts build` — rebuild CARTS compiler only (fastest, incremental)
- `dekk carts build --clean` — full clean rebuild
- `dekk carts build --arts` — rebuild ARTS runtime; the production multinode
  transport is GASNet-EX. With no `ARTS_GASNET_PREFIX`, the build downloads the
  GASNet release, auto-detects the conduit, installs its deps, and builds it
  (override with `ARTS_GASNET_CONDUIT` / `ARTS_GASNET_VERSION`; point
  `ARTS_GASNET_PREFIX` at a prebuilt GASNet to skip the download)
- `dekk carts build --arts --gasnet-no-bootstrap` — require a prebuilt
  `ARTS_GASNET_PREFIX` instead of downloading GASNet
- `dekk carts build --arts --no-rdma` — rebuild ARTS runtime for TCP/debug transport
- `dekk carts build --arts --legacy-rsocket` — rebuild ARTS on the legacy rsocket
  RDMA data plane
- `dekk carts build --arts --debug 3` — rebuild ARTS runtime with full debug logging
- `dekk carts build --arts --counters 2` — rebuild ARTS with workload counters
- `dekk carts build --arts --arts-tests` — rebuild ARTS with runtime test binaries enabled

The build respects `CARTS_HOME` first, then the local untracked `carts.config`
file, then the checkout root. Do not hardcode machine-local install paths in
tracked files. Builds default to all visible CPUs; set `CARTS_BUILD_JOBS` if a
machine needs a lower or higher explicit limit.

`dekk carts doctor` sees CARTS-managed LLVM tools through `tools/dekk-shims`.
If `llvm-lit`, `FileCheck`, or `clang-format` is missing there, rebuild LLVM
with `dekk carts build --llvm`.

## Build targets

| Flag | What it builds | When to use |
|------|---------------|-------------|
| (none) | CARTS compiler only | After changing `lib/carts/` or `include/carts/` |
| `--arts` | ARTS runtime | After changing `external/arts/`; defaults to GASNet-EX (auto-downloaded+built; `--no-rdma` for TCP, `--legacy-rsocket` for rsocket, `--gasnet-no-bootstrap` to require a prebuilt prefix) |
| `--polygeist` | Polygeist frontend | After changing `external/Polygeist/` |
| `--llvm` | LLVM/MLIR | After changing `external/Polygeist/llvm-project/` |
| `--clean` | Full clean rebuild | When incremental build fails or after branch switch |

## Troubleshooting

If the build fails:
1. Run `dekk carts doctor` to verify environment health
2. Try `dekk carts build --clean` for a fresh build
3. Check submodules: `carts update` (or `git submodule update --init --recursive`)
4. Read the error — CMake errors often point to missing dependencies
5. Check if the error is in CARTS code vs external (Polygeist/LLVM) code

## Instructions

When the user asks to build:
1. Run `dekk carts build $ARGUMENTS`
2. Report success/failure with relevant output
3. If build fails, analyze the error and suggest fixes
4. For C++ compilation errors, check the relevant source file and fix
