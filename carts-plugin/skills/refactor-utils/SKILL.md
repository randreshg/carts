---
name: carts-refactor-utils
description: Find and extract misplaced utility functions from pass files into shared locations. Use when auditing code quality, consolidating duplicates, or extracting reusable helpers from pass files.
user-invocable: true
allowed-tools: Read, Grep, Glob, Bash, Write, Edit, Agent
argument-hint: [audit | extract <function> | fix-strings | consolidate <file>]
parameters:
  - name: action
    type: str
    gather: "What to do: 'audit' (scan for misplaced utils), 'extract <function>' (move function to shared utils), 'fix-strings' (fix hardcoded attribute strings), or 'consolidate <file>' (extract all reusable helpers from a file)"
---

# CARTS Utility Refactoring

## Purpose

Find and extract misplaced utility functions from pass files into shared
locations. Prevents code duplication, enforces the single-source-of-truth
principle, and fixes hardcoded attribute string violations.

## Background

Audit of 161 source files (67.5K LOC) found:
- **38.4% of pass code (13,549 LOC) is static helpers**
- **9 exact duplicate functions** across files
- **60+ extractable helpers** in pass files
- **4 hardcoded attribute string violations**
- **10 god files** >1K LOC with extractable support code

## Shared Utility Targets

When extracting, place functions in the right shared location:

| Category | Header | Implementation | When to Use |
|----------|--------|----------------|-------------|
| General IR | `include/arts/utils/Utils.h` | `lib/arts/utils/Utils.cpp` | Value replacement, dominance, index creation, mode conversion |
| DataBlock | `include/arts/utils/DbUtils.h` | `lib/arts/utils/DbUtils.cpp` | DB tracing, sizes, memory access, stride computation |
| EDT | `include/arts/utils/EdtUtils.h` | `lib/arts/utils/EdtUtils.cpp` | EDT capture analysis, epoch detection, fusion |
| Loops | `include/arts/utils/LoopUtils.h` | `lib/arts/utils/LoopUtils.cpp` | Trip count, loop detection, IV extraction, depth |
| Loop Invariance | `include/arts/utils/LoopInvarianceUtils.h` | `lib/arts/utils/LoopInvarianceUtils.cpp` | Invariance check, hoist safety, dominance |
| Blocked Access | `include/arts/utils/BlockedAccessUtils.h` | (header-only) | Block alignment, local extraction, bounds proving |
| Contracts | `include/arts/utils/LoweringContractUtils.h` | `lib/arts/utils/LoweringContractUtils.cpp` | Contract query, merge, transfer |
| Patterns | `include/arts/utils/PatternSemantics.h` | `lib/arts/utils/PatternSemantics.cpp` | Pattern family classification |
| Attributes | `include/arts/utils/OperationAttributes.h` | (constants only) | Attribute name constants |
| Stencil Attrs | `include/arts/utils/StencilAttributes.h` | (constants + inline) | Stencil attribute getters/setters |
| Op Removal | `include/arts/utils/RemovalUtils.h` | `lib/arts/utils/RemovalUtils.cpp` | Deferred op removal |
| Value Analysis | `include/arts/utils/ValueAnalysis.h` | `lib/arts/dialect/core/Analysis/value/ValueAnalysis.cpp` | Constant detection, value tracing |

## Known Issues (Prioritized Backlog)

### Tier 1: Exact Duplicates (Fix Immediately)

| Function | Location 1 | Location 2 | Target |
|----------|-----------|-----------|--------|
| `isOneLikeValue` | `EdtLowering.cpp:128` | `DbAnalysis.cpp:1534` (lambda) | `ValueAnalysis::isOneLikeValue` |
| `hasWorkAfterInParentBlock` | `ConvertOpenMPToArts.cpp:86` | `ConvertOpenMPToSde.cpp:79` | `Utils.h` |
| `isPureOp` | `StencilTilingNDPattern.cpp:80` | `PatternDiscovery.cpp:152` | `Utils.h` (extend `isSideEffectFreeArithmeticLikeOp`) |
| `collectSpatialNestIvs` | `StencilTilingNDPattern.cpp:122` | `PatternDiscovery.cpp:167` | `LoopUtils.h` |
| `sortStoresInProgramOrder` | `EdtStructuralOpt.cpp` | `EdtAllocaSinking.cpp` | `Utils.h` |
| `findHoistTarget` | `RuntimeCallOpt.cpp:48` | `DataPtrHoistingSupport.cpp:1376` | `LoopInvarianceUtils.h` |
| `getMemoryAccessInfo` | `DataPtrHoistingSupport.cpp:604` | `DbUtils.cpp:855` | `DbUtils.h` (already canonical) |

### Tier 2: Hardcoded Attribute Strings

| File | Line | Violation | Fix |
|------|------|-----------|-----|
| `BlockLoopStripMiningSupport.cpp` | 18-19 | `"arts.block_loop_strip_mining.generated"` | Add to `AttrNames::Operation` |
| `RaiseToLinalg.cpp` | 240-295 | `"parallel"`, `"reduction"`, `"stencil"`, `"elementwise"`, `"matmul"` | Use enum or `AttrNames` constants |
| `EdtLowering.cpp` | 123-125 | `"llvm.mlir.undef"`, `"polygeist.undef"`, `"arts.undef"` | Use `isa<>` checks or centralized names |
| `MetadataAttacher.cpp` | 62, 97 | `ArtsMetadata::IdAttrName` instead of `AttrNames::Operation::ArtsId` | Replace with canonical constant |

### Tier 3: High-Value Extractions

| Function | Source File | Target | LOC |
|----------|-----------|--------|-----|
| `getLoadInfo` / `getStoreInfo` / `getStoredValue` | `ScalarReplacement.cpp` | `DbUtils.h` or new `MemoryAccessUtils.h` | ~30 |
| `getForwardedMemrefAliasSource/Result` | `RaiseMemRefDimensionality.cpp` | `Utils.h` | ~35 |
| `hoistInvariantOpsInLoop` + `collectLoops` | `Hoisting.cpp` | `LoopInvarianceUtils.h` | ~50 |
| `buildLoopInvariantI1Not/And` | `DataPtrHoistingSupport.cpp` | `Utils.h` | ~15 |
| `isUnitStrideLoop` | `PerfectNestLinearizationPattern.cpp` | `LoopUtils.h` | ~5 |
| `tryGetAffineExpr` + `tryBuildIndexingMap` | `RaiseToLinalg.cpp` | New `AffineUtils.h` | ~65 |
| `castToIndexType` | `WorkDistributionUtils.cpp` | `Utils.h` | ~5 |
| `ensureBlock` | `ConvertOpenMPToSde.cpp` | `Utils.h` | ~5 |

### Tier 4: Pass-Specific (Keep in Place)

These are correctly located — do NOT extract:
- Jacobi pattern helpers (29 in `JacobiAlternatingBuffersPattern.cpp`)
- Seidel wavefront helpers (12 in `Seidel2DWavefrontPattern.cpp`)
- Stencil tiling helpers (21 in `StencilTilingNDPattern.cpp`)
- Blocked neighbor cache patterns (30+ in `DataPtrHoistingSupport.cpp`)
- Mode-specific partition code (in `DbPartitionPlanner.cpp`)

## Extraction Procedure

When extracting a function from a pass file to a shared location:

1. **Verify no existing equivalent** — run `/check-utils` first
2. **Choose the right target file** — use the table above
3. **Add declaration to header** — in the appropriate namespace
4. **Move implementation** — to the `.cpp` file
5. **Update CMakeLists.txt** — if creating a new file (rarely needed)
6. **Replace all call sites** — `Grep` for all callers across `lib/arts/`
7. **Remove the old static definition** — from the pass file
8. **Run `dekk carts format`** — format changed files
9. **Run `dekk carts build`** — verify compilation
10. **Run `dekk carts test`** — verify no regressions

## Instructions

### `audit` — Scan for misplaced utilities

1. Spawn parallel agents to scan each directory:
   - `lib/arts/dialect/core/Transforms/`
   - `lib/arts/dialect/rt/Transforms/` + `rt/Conversion/`
   - `lib/arts/dialect/sde/Transforms/`
   - `lib/arts/dialect/core/Transforms/` (all subdirs)
2. For each static function found, check:
   - Does it duplicate a shared utility? → Flag as Tier 1
   - Does it hardcode attribute strings? → Flag as Tier 2
   - Is it generic and reusable? → Flag as Tier 3
   - Is it pass-specific? → Mark as Tier 4 (keep)
3. Report findings grouped by tier with file:line references

### `extract <function>` — Move a specific function

1. Read the function's current implementation
2. Run `/check-utils` to verify no existing equivalent
3. Choose target file based on category
4. Add header declaration + implementation
5. Update all call sites
6. Remove old static definition
7. Build and test

### `fix-strings` — Fix hardcoded attribute strings

1. Scan for the known violations listed in Tier 2
2. For each violation:
   - If the constant exists in `AttrNames::`, replace the string with it
   - If not, add a new constant to `OperationAttributes.h` first
3. Build and test

### `consolidate <file>` — Extract all reusable helpers from one file

1. Read the entire file
2. Classify every static function as Tier 1-4
3. For Tier 1-3 functions, propose extraction targets
4. Ask user for confirmation before proceeding
5. Extract approved functions one by one
6. Build and test after each extraction
