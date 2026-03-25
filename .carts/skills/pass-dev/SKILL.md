---
name: carts-pass-dev
description: Guide for developing new CARTS compiler passes. Use when the user wants to create a new pass, modify an existing pass, understand pass architecture, or work on compiler transforms.
user-invocable: true
allowed-tools: Read, Grep, Glob, Bash, Write, Edit, Agent
argument-hint: [<pass-name> | new <pass-name>]
---

# CARTS Pass Development

## Architecture Conventions (MANDATORY)

**Analysis interface** — always use analysis classes, NEVER access graphs directly:
```cpp
AM->getDbAnalysis().getOrCreateGraph(func)     // DB analysis
AM->getEdtAnalysis().getOrCreateEdtGraph(func) // EDT analysis
AM->getEdtAnalysis().getEdtNode(edt)           // Node lookup
AM->getDbAnalysis().getDbAcquireNode(acquire)  // Node lookup
```

**Attribute names** — NEVER hardcode strings. Use centralized names from:
- `include/arts/utils/OperationAttributes.h` (`AttrNames::Operation`)
- `include/arts/utils/StencilAttributes.h` (`AttrNames::Operation::Stencil`)

**Naming** — DB passes: `Db` prefix. EDT passes: `Edt` prefix. LLVM style: 2-space indent, CamelCase types, camelCase variables.

## Key Source Locations

- Passes: `lib/arts/passes/opt/` and `lib/arts/passes/transforms/`
- Analysis: `lib/arts/analysis/`
- Pipeline setup: `tools/compile/Compile.cpp`
- Pass declarations: `include/arts/passes/Passes.h` and `Passes.td`

## Creating a New Pass

1. Create source in `lib/arts/passes/{opt,transforms}/`
2. Add declaration in `include/arts/passes/Passes.h`
3. Register in pipeline at appropriate stage in `tools/compile/Compile.cpp`
4. Add lit test in `tests/contracts/`
5. `carts format` then `carts test --suite contracts`

## Thread Safety

All passes must be thread-safe — no global/static mutable state. Use function-scoped graph access.

## Instructions

When the user asks to develop a pass:
1. Find similar existing passes for reference
2. Guide through the creation steps above
3. Ensure conventions are followed
4. Create test, build, and verify
