---
name: carts-dialect-trace
description: Use when debugging lowering paths, understanding operation placement across SDE/ARTS/ARTS-RT, or verifying dialect boundary invariants.
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

Trace an operation's lifecycle across the pipeline stages and dialect layers
(SDE, ARTS, ARTS-RT). Understand where ops are created, transformed, and
destroyed.

Use [[carts-vision]] when the trace affects layer placement, real
transformations, distributed shape, or DB/CU grain.

## Dialects

| Dialect | Namespace | Stages | Purpose |
|---------|-----------|--------|---------|
| SDE | `sde::` | 3 (`sde-planning`) | Real source/layout/tiling transforms, HPF-style `DISTRIBUTE`/`ALIGN`, MU/CU/SU facts |
| ARTS | `arts::` | 4-12 | Direct SDE boundary materialization, graph structure over SDE movement facts, per-block DB/EDT/owner-map realization, and grouped compute/bridge/communication CUs |
| ARTS-RT | `arts_rt::` | 12-13 | Mechanical runtime ABI and LLVM-facing call mapping |

## Dialect Boundaries

```
C/OMP source
  → [sde-planning]    OMP → SDE (ConvertOpenMPToSde) →
                 SDE commits real layout/tiling/source facts
  → [sde-to-arts]    SDE MU/SU/CU facts → ARTS DB/acquire/EDT objects
                 ARTS represents SDE movement facts on graph edges
  → [ARTS stages]   ARTS realizes owner maps, DB/EDT graphs, grouped CUs
  → [pre-lowering]    ARTS → ARTS-RT lowering
                 (EdtLowering.cpp, EpochLowering.cpp, DbLowering.cpp)
  → [arts-rt-to-llvm] ARTS-RT → LLVM lowering
                 (ConvertArtsRtToLLVM + rt-specific RuntimeCallOpt etc.)
```

## Verification Barriers

| Verifier | After Stage | Checks |
|----------|-------------|--------|
| VerifyArts | 4 (sde-to-arts) | ARTS deps/params/yields are explicit and isolated |
| VerifySdeLowered | 5 (sde-to-arts) | No SDE ops survive |
| VerifyArtsObjectsOnly | 5 (sde-to-arts) | No transient semantic carrier survives into ARTS refinement |
| VerifyEdtCreated | 5 (sde-to-arts) | EDTs created from ARTS codelets |
| VerifyPreLowered | 12 (pre-lowering) | No arts.edt/epoch/db_* survive |
| VerifyLowered | 13 (arts-rt-to-llvm) | No ARTS/ARTS-RT ops survive |

## Op Lifecycle Quick Reference

### ARTS Ops
| Op | Created | Lowered | Stages Active |
|----|---------|---------|---------------|
| `arts.edt` | sde-to-arts (5) | pre-lowering (12) | 5-12 |
| `arts.db_alloc` | sde-to-arts (5) or create-dbs materialization (7) | pre-lowering (12) | 5/7-12 |
| `arts.db_acquire` | sde-to-arts (5) or create-dbs materialization (7) | pre-lowering (12) | 5/7-12 |
| `arts.db_ref` | create-dbs (7) | pre-lowering (12) | 7-12 |
| `arts.epoch` | sde-to-arts (5) or epochs (11) | pre-lowering (12) | 5-12 |
| `arts.barrier` | sde-to-arts (5) | epochs (11) | 5-11 |

### ARTS-RT Ops
| Op | Created | Lowered | Stages Active |
|----|---------|---------|---------------|
| `arts_rt.edt_create` | pre-lowering (12) | arts-rt-to-llvm (13) | 12-13 |
| `arts_rt.create_epoch` | pre-lowering (12) | arts-rt-to-llvm (13) | 12-13 |
| `arts_rt.db_create` | pre-lowering (12) | arts-rt-to-llvm (13) | 12-13 |
| `arts_rt.rec_dep` | pre-lowering (12) | arts-rt-to-llvm (13) | 12-13 |

### SDE Ops
SDE planning ops are created in `sde-planning`; codelet ops are consumed by
`sde-to-arts`, and no SDE op may survive `sde-to-arts`:

| Op | Created | Lowered |
|----|---------|---------|
| `sde.cu_codelet` | ConvertOpenMPToSde / `sde-planning` | `sde-to-arts` |
| `sde.cu_task` | ConvertOpenMPToSde / `sde-planning` | `sde-to-arts`; leftovers fail VerifySdeLowered |
| `sde.su_iterate` | ConvertOpenMPToSde / `sde-planning` | SDE-to-ARTS scheduling-unit materialization; leftovers fail VerifySdeLowered |
| `sde.mu_reduction_decl` | ConvertOpenMPToSde / `sde-planning` | SDE reduction materialization; leftovers fail VerifySdeLowered |

## Tracing Commands

### Find where an op is created
```bash
grep -rn 'create<.*OpName>' lib/carts/ --include='*.cpp'
grep -rn 'builder\.create<.*OpName>' lib/carts/ --include='*.cpp'
```

### Find where an op is consumed/matched
```bash
grep -rn 'isa<.*OpName>\|dyn_cast<.*OpName>\|cast<.*OpName>' lib/carts/ --include='*.cpp'
```

### Find where an op is erased/replaced
```bash
grep -rn 'replaceOp\|eraseOp' lib/carts/ --include='*.cpp' | grep -i 'OpName'
```

### Find conversion patterns for an op
```bash
grep -rn 'OpName.*Pattern\|convert.*OpName' lib/carts/ --include='*.cpp'
```

## Key Source Locations

### Dialect Definitions
```
include/carts/dialect/arts/IR/Ops.td          # ARTS op definitions
include/carts/dialect/arts-rt/IR/RtOps.td          # ARTS-RT op definitions
include/carts/dialect/sde/IR/SdeOps.td        # SDE op definitions
include/carts/dialect/arts/IR/ArtsOps.td   # ARTS op definitions
```

### Conversion Passes
```
lib/carts/dialect/arts/Transforms/              # SDE → ARTS → ARTS boundary
lib/carts/dialect/arts-rt/Conversion/ArtsToRt/   # pre-lowering implementation
lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ # ARTS-RT → LLVM
```

## Instructions

When asked to trace an op:

1. Identify the op and its dialect (SDE, ARTS, or ARTS-RT)
2. Search for creation sites (`builder.create<OpType>`)
3. Search for transformation sites (`isa<>`, pattern matches)
4. Search for lowering/erasure sites (`replaceOp`, conversion patterns)
5. Map findings to pipeline stages using the pass-placement reference
6. Report the complete lifecycle: created at stage X, transformed by Y, lowered at Z
7. Flag any unexpected cross-dialect references (ops used outside their expected stage range)
8. Flag downstream recomputation of committed facts: owner dims, block shape,
   movement family, owner maps, DB grain, or runtime policy.
