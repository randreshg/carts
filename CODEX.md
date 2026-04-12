# CARTS - Codex Agent Guide

CARTS is an MLIR-based compiler that transforms C/C++ programs with OpenMP
pragmas into task-based parallel executables targeting the ARTS asynchronous
runtime.

## Critical Rules

- **ALWAYS** use the `carts` CLI (already in PATH) for all operations
- **NEVER** use `make`, `ninja`, or raw build commands
- **NEVER** use full paths like `python tools/carts_cli.py` — just use `carts`
- **NEVER** hardcode attribute name strings — use `AttrNames::Operation` from
  `include/arts/utils/OperationAttributes.h` and `AttrNames::Operation::Stencil`
  from `include/arts/utils/StencilAttributes.h`
- **NEVER** access DB/EDT graphs directly — always use `AM->getDbAnalysis()` and
  `AM->getEdtAnalysis()` interfaces
- Run `carts format` before committing C/C++ changes

## Essential Commands

```bash
carts build                    # Build CARTS compiler
carts build --arts             # Rebuild ARTS runtime
carts build --clean            # Full clean rebuild
carts compile <file> -O3       # Compile C/C++ to ARTS executable
carts compile <f>.mlir --pipeline=<stage>  # Stop at pipeline stage
carts test                     # Run all tests
carts lit lib/arts/dialect/core/test/X.mlir  # Run single lit test
carts format                   # Format C/C++/TableGen files
carts doctor                   # Validate environment
carts pipeline --json          # List pipeline stages (source of truth)
```

## Project Layout

```
include/arts/          C++ headers (dialect, passes, utilities)
lib/arts/              Core implementation
  dialect/             MLIR dialect definition (ArtsOps.td, ArtsTypes.td)
  analysis/            Analysis framework (graphs, heuristics)
  passes/opt/          Optimization passes (db/, edt/, general/)
  passes/transforms/   Transformation passes (lowering, conversion)
tools/                 CLI (carts_cli.py) and compilation driver
  compile/             C++ driver (Compile.cpp = pipeline definition)
  scripts/             Python CLI subcommand modules
  dialect/sde/test/    SDE dialect lit tests
  dialect/core/test/   Core dialect lit tests
  dialect/rt/test/     RT dialect lit tests
tests/cli/             CLI lit tests
tests/verify/          Verifier lit tests
samples/               End-to-end C/C++ integration tests
external/              Vendored: ARTS runtime, Polygeist, LLVM
docs/                  Architecture and developer docs
```

## Coding Conventions

- **Style**: LLVM (2-space indent, CamelCase types, camelCase variables)
- **Format**: `.clang-format` uses `BasedOnStyle: LLVM`
- **DB passes**: use `Db` prefix (e.g., `DbPartitioning`)
- **EDT passes**: use `Edt` prefix (e.g., `EdtStructuralOpt`)
- **New passes**: add source in `lib/arts/passes/`, declaration in
  `include/arts/passes/Passes.h`, register in `tools/compile/Compile.cpp`,
  add lit test in the appropriate per-dialect test dir (e.g. `lib/arts/dialect/core/test/`)

## Debugging

```bash
carts compile <f>.mlir --pipeline=<stage> --arts-debug=<channel> 2>&1
carts compile <f>.mlir --all-pipelines -o dir/       # Dump all stages
carts compile <f>.mlir --diagnose --diagnose-output d.json
carts build --arts --debug 3                         # ARTS runtime debug
```

Debug channel convention: `snake_case(PassFilename)` (e.g., `DbPartitioning.cpp` -> `db_partitioning`).

## Testing

```bash
carts test                                    # All contract tests
carts lit lib/arts/dialect/core/test/my_test.mlir  # Single test
carts lit -v lib/arts/dialect/sde/test/           # Verbose directory
```

Test format:
```mlir
// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline <stage> | %FileCheck %s
// CHECK: expected_output
module { ... }
```

## Key Architectural Concepts

- **EDT** (Event-Driven Task): ARTS unit of parallel execution
- **DataBlock (DB)**: ARTS memory abstraction for inter-task data
- **Epoch**: Synchronization barrier grouping related EDTs
- **AnalysisManager (AM)**: Central analysis cache — use `AM->getDbAnalysis()`,
  `AM->getEdtAnalysis()`, `AM->getLoopAnalysis()` (never access graphs directly)

## Things That Will Bite You

- **AnalysisManager is NOT thread-safe** — analysis invalidation must be serialized, never call `AM->invalidate()` from parallel passes
- **`external/` submodules have their own build systems** — never modify files in `external/` without understanding the submodule boundary; changes get lost on `git submodule update`
- **Pipeline stage ordering is load-bearing** — reordering passes in `Compile.cpp` can cause silent miscompilation; always verify with `carts test` after changes
- **DB mode constants are NOT the enum values you'd expect** — `ARTS_DB_PIN=2` maps to GPU memory, `ARTS_DB_ONCE=3` maps to local copy; see `include/arts/utils/ArtsDbModes.h`
- **`*.mlir` is gitignored EXCEPT per-dialect test dirs** — if you create MLIR test files outside `lib/arts/dialect/*/test/`, `tests/cli/`, or `tests/verify/`, they won't be committed
- **The `carts` CLI must be used for everything** — `make`/`ninja` skip critical environment setup from `.dekk.toml`
- **Hardcoded attribute strings break silently** — always use `AttrNames::Operation` constants; typos in string literals cause passes to silently skip operations

## Agent Skills

Custom skills in `.carts/skills/`: `/build`, `/test`, `/debug`, `/benchmark`,
`/benchmark-triage`, `/pass-dev`, `/carts`. Run `carts agents generate` to
sync to all agent systems (Claude, Codex, Copilot, Cursor).


## Skills

Run `carts agents install` to install repo-managed skills into `~/.codex/skills/`.
