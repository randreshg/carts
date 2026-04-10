---
name: carts-dialect-trace
description: Trace an MLIR operation through the 18-stage pipeline to understand its lifecycle — where it is created, which passes transform it, and where it is lowered or erased. Use when debugging lowering paths, understanding op placement across the 3 dialects (SDE, core, RT), or verifying dialect boundary invariants.
user-invocable: true
allowed-tools: Read, Grep, Glob, Bash, Agent
argument-hint: [<op-name> | boundary | verify]
parameters:
  - name: op_name
    type: str
    gather: "Operation to trace (e.g., 'arts.db_acquire', 'arts_rt.edt_create', 'sde.cu_task')"
---

# CARTS Dialect Operation Tracer

## Purpose

Trace an operation's lifecycle across the 18 pipeline stages and 3 dialects
(SDE, ARTS core, ARTS RT). Understand where ops are created, transformed,
and destroyed.

## Three Dialects

| Dialect | Namespace | Stages | Purpose |
|---------|-----------|--------|---------|
| SDE | `sde::` | 1-4 | Semantic contracts, pattern classification |
| ARTS Core | `arts::` | 3-17 | Structural transforms, partitioning, distribution |
| ARTS RT | `arts_rt::` / `rt::` | 17-18 | 1:1 runtime call mapping |

## Dialect Boundaries

```
C/OMP source
  → [Stage 1-2] SDE ops created (sde.cu_task, sde.su_access, ...)
  → [Stage 4]   SDE → Core conversion (SdeToArtsPatterns.cpp)
  → [Stage 5-16] Core transforms (arts.edt, arts.db_*, arts.for, ...)
  → [Stage 17]  Core → RT lowering (EdtLowering.cpp, EpochLowering.cpp)
  → [Stage 18]  RT → LLVM lowering (RtToLLVMPatterns.cpp)
```

## Verification Barriers

| Verifier | After Stage | Checks |
|----------|-------------|--------|
| VerifySdeLowered | 4 (openmp-to-arts) | No SDE ops survive |
| VerifyEdtLowered | 17 (pre-lowering) | No arts.edt/epoch/for survive |
| VerifyLowered | 18 (arts-to-llvm) | No ARTS ops survive |

## Op Lifecycle Quick Reference

### Core Ops (18 ops)
| Op | Created | Lowered | Stages Active |
|----|---------|---------|---------------|
| `arts.edt` | openmp-to-arts (4) | pre-lowering (17) | 4-17 |
| `arts.db_alloc` | create-dbs (7) | pre-lowering (17) | 7-17 |
| `arts.db_acquire` | create-dbs (7) | pre-lowering (17) | 7-17 |
| `arts.db_ref` | create-dbs (7) | pre-lowering (17) | 7-17 |
| `arts.for` | openmp-to-arts (4) | edt-distribution (11) | 4-11 |
| `arts.epoch` | epochs (16) | pre-lowering (17) | 16-17 |
| `arts.barrier` | openmp-to-arts (4) | epochs (16) | 4-16 |

### RT Ops (14 ops)
| Op | Created | Lowered | Stages Active |
|----|---------|---------|---------------|
| `arts_rt.edt_create` | pre-lowering (17) | arts-to-llvm (18) | 17-18 |
| `arts_rt.create_epoch` | pre-lowering (17) | arts-to-llvm (18) | 17-18 |
| `arts_rt.db_create` | pre-lowering (17) | arts-to-llvm (18) | 17-18 |
| `arts_rt.rec_dep` | pre-lowering (17) | arts-to-llvm (18) | 17-18 |

### SDE Ops (8 ops)
| Op | Created | Lowered | Stages Active |
|----|---------|---------|---------------|
| `sde.cu_task` | openmp-to-sde (4) | sde-to-arts (4) | 4 |
| `sde.su_access` | openmp-to-sde (4) | sde-to-arts (4) | 4 |
| `sde.mu_reduction` | openmp-to-sde (4) | sde-to-arts (4) | 4 |

## Tracing Commands

### Find where an op is created
```bash
grep -rn 'create<.*OpName>' lib/arts/ --include='*.cpp'
grep -rn 'builder\.create<.*OpName>' lib/arts/ --include='*.cpp'
```

### Find where an op is consumed/matched
```bash
grep -rn 'isa<.*OpName>\|dyn_cast<.*OpName>\|cast<.*OpName>' lib/arts/ --include='*.cpp'
```

### Find where an op is erased/replaced
```bash
grep -rn 'replaceOp\|eraseOp' lib/arts/ --include='*.cpp' | grep -i 'OpName'
```

### Find conversion patterns for an op
```bash
grep -rn 'OpName.*Pattern\|convert.*OpName' lib/arts/ --include='*.cpp'
```

## Key Source Locations

### Dialect Definitions
```
include/arts/dialect/core/IR/Ops.td          # Core op definitions
include/arts/dialect/rt/IR/RtOps.td          # RT op definitions
include/arts/dialect/sde/IR/SdeOps.td        # SDE op definitions
```

### Conversion Passes
```
lib/arts/dialect/sde/Conversion/SdeToArts/   # SDE → Core
lib/arts/dialect/core/Conversion/ArtsToRt/   # Core → RT
lib/arts/dialect/rt/Conversion/RtToLLVM/     # RT → LLVM
lib/arts/dialect/core/Conversion/ArtsToLLVM/ # Core → LLVM (remaining)
```

## Instructions

When asked to trace an op:

1. Identify the op and its dialect (SDE, core, or RT)
2. Search for creation sites (`builder.create<OpType>`)
3. Search for transformation sites (`isa<>`, pattern matches)
4. Search for lowering/erasure sites (`replaceOp`, conversion patterns)
5. Map findings to pipeline stages using the pass-placement reference
6. Report the complete lifecycle: created at stage X, transformed by Y, lowered at Z
7. Flag any unexpected cross-dialect references (ops used outside their expected stage range)
