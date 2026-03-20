# OpenMP Task Support in CARTS/Polygeist: Investigation Report

**Date**: 2026-03-19
**Scope**: OMP baseline benchmark failure for task-based benchmarks, LLVM migration feasibility

---

## Executive Summary

**The OMP baseline comparison for task-based benchmarks fails** because Polygeist's
`ConvertPolygeistToLLVM` pass doesn't know about `omp::TaskOp`. The conversion framework
can't descend into task regions to legalize `scf.for`/`scf.execute_region` ops, causing:

```
error: failed to legalize operation 'scf.for' that was explicitly marked illegal
```

Even after fixing that, `OpenMPToLLVMIRTranslation.cpp` in LLVM 18 will reject `omp.task`
with `untied`/`mergeable`/`priority` clauses, and `omp.taskloop` has **no LLVM IR translation
handler at all**.

**LLVM 23 (latest) has fixed all of these issues**: 9/11 task clauses lowered, full taskloop
support, `TaskContextStructManager` for privatization. The question is whether to patch LLVM 18
or upgrade.

---

## 1. Root Cause: OMP Baseline Failure

### 1.1 The OMP Baseline Compilation Path

The benchmark system (`common/carts.mk:102-107`) compiles the OMP reference binary as:

```bash
# Step 1: C → MLIR → LLVM IR  (via Polygeist cgeist)
carts cgeist source.c -O3 -S --emit-llvm -fopenmp -o source.ll

# Step 2: LLVM IR → native binary  (via LLVM clang + libomp)
carts clang source.ll -O3 -fopenmp -o binary
```

Step 1 internally runs:
1. Clang parser → `OMPTaskDirective` AST
2. `VisitOMPTaskDirective` → `omp.task { scf.execute_region { scf.for { ... } } }`
3. `ConvertSCFToOpenMP` pass (pm2)
4. `ConvertPolygeistToLLVM` pass (pm3) — **FAILS HERE**
5. `translateModuleToLLVMIR` → `OpenMPToLLVMIRTranslation` — **WOULD FAIL HERE TOO**

### 1.2 Blocker 1: `ConvertPolygeistToLLVM` Doesn't Know About TaskOp

**File**: `external/Polygeist/lib/polygeist/Passes/ConvertPolygeistToLLVM.cpp`

At line 3262-3267:
```cpp
// Only ParallelOp and WsLoopOp are registered as dynamically legal:
target.addDynamicallyLegalOp<omp::ParallelOp, omp::WsLoopOp>(
    [&](Operation *op) { return converter.isLegal(&op->getRegion(0)); });

// scf ops are marked ILLEGAL:
target.addIllegalOp<scf::ForOp, scf::IfOp, scf::ParallelOp, scf::WhileOp,
                    scf::ExecuteRegionOp, func::FuncOp>();

// Some OMP ops marked legal, but NO TaskOp, TaskGroupOp, TaskLoopOp:
target.addLegalOp<omp::TerminatorOp, omp::TaskyieldOp, omp::FlushOp,
                  omp::YieldOp, omp::BarrierOp, omp::TaskwaitOp>();
```

**Result**: `omp::TaskOp` is not registered at all. The conversion framework cannot descend
into task regions to convert the `scf.for` and `scf.execute_region` ops inside. Those ops
remain unconverted but are marked illegal → legalization failure.

**Error**: `failed to legalize operation 'scf.for' that was explicitly marked illegal`

### 1.3 Blocker 2: OpenMPToLLVMIRTranslation Rejects Task Clauses

**File**: `external/Polygeist/llvm-project/mlir/lib/Target/LLVMIR/Dialect/OpenMP/OpenMPToLLVMIRTranslation.cpp`

At line 670-673:
```cpp
if (taskOp.getUntiedAttr() || taskOp.getMergeableAttr() ||
    taskOp.getInReductions() || taskOp.getPriority() ||
    !taskOp.getAllocateVars().empty()) {
  return taskOp.emitError("unhandled clauses for translation to LLVM IR");
}
```

If cgeist generates any of `untied`, `mergeable`, or `priority` on the task, LLVM IR
translation will error out. The benchmarks' `#pragma omp task` doesn't explicitly use these
but cgeist may emit default attributes.

### 1.4 Blocker 3: No TaskLoopOp Handler

At line 2442-2444 of `OpenMPToLLVMIRTranslation.cpp`, the dispatch table has **no `.Case`
for `omp::TaskLoopOp`**, so it hits:
```cpp
.Default([&](Operation *inst) {
  return inst->emitError("unsupported OpenMP operation: ") << inst->getName();
});
```

This means `poisson-task` or any benchmark using `#pragma omp taskloop` cannot compile
to the OMP baseline.

### 1.5 Affected Benchmarks

14 test examples use `#pragma omp task`:
- `tests/examples/task/`, `stencil/`, `smith-waterman/`, `rows/deps/`, `matrixmul/`,
  `matrix/`, `jacobi/deps/`, `deps/`, `convolution/`, `concurrent/`, `cholesky/` (2),
  `array/` (2)

2 actual benchmarks in `carts-benchmarks` use tasks:
- `kastors-jacobi/jacobi-task-dep/jacobi-task-dep.c`
- `kastors-jacobi/poisson-task/poisson-task.c`

### 1.6 Verified Reproduction

```
$ cd external/carts-benchmarks/kastors-jacobi/jacobi-task-dep
$ make small-openmp CARTS=carts
error: failed to legalize operation 'scf.for' that was explicitly marked illegal
make: *** Error 10
```

---

## 2. LLVM 18 vs LLVM 23 Task Support Comparison

### 2.1 omp.task Clause Lowering to LLVM IR

| Clause | LLVM 18 | LLVM 23 |
|--------|---------|---------|
| `if` | LOWERED | LOWERED |
| `final` | LOWERED | LOWERED |
| `depend` (in/out/inout) | LOWERED | LOWERED + mutexinoutset/inoutset |
| `untied` | **ERROR** | LOWERED |
| `mergeable` | **ERROR** | LOWERED |
| `priority` | **ERROR** | LOWERED |
| `private`/`firstprivate` | **NOT IN DIALECT** | LOWERED (TaskContextStructManager) |
| `detach` | **NOT IN DIALECT** | LOWERED |
| `affinity` | **NOT IN DIALECT** | LOWERED (incl. iterated form) |
| `allocate` | **ERROR** | NOT LOWERED |
| `in_reduction` | **ERROR** | NOT LOWERED |

### 2.2 omp.taskloop Lowering to LLVM IR

| Feature | LLVM 18 | LLVM 23 |
|---------|---------|---------|
| Basic taskloop | **NO HANDLER** | FULLY LOWERED |
| grainsize | N/A | LOWERED |
| num_tasks | N/A | LOWERED |
| nogroup | N/A | LOWERED |
| priority | N/A | LOWERED |
| untied/mergeable | N/A | LOWERED |
| final | N/A | LOWERED |
| collapse (multi-loop) | N/A | LOWERED |
| cancel | N/A | LOWERED |
| private/firstprivate | N/A | LOWERED |
| reduction | N/A | NOT LOWERED |

### 2.3 Other Task Ops

| Operation | LLVM 18 | LLVM 23 |
|-----------|---------|---------|
| `omp.taskwait` (basic) | FULL | FULL |
| `omp.taskwait` (depend) | N/A | NOT LOWERED |
| `omp.taskyield` | FULL | FULL |
| `omp.taskgroup` (basic) | FULL | FULL |
| `omp.taskgroup` (task_reduction) | **ERROR** | NOT LOWERED |

---

## 3. Fix Options

### Option A: Patch LLVM 18 (Fastest — days, not weeks)

Minimal patches to unblock the OMP baseline:

**Patch 1** — `ConvertPolygeistToLLVM.cpp` (2 lines):
```cpp
// Line 3262: Add TaskOp, TaskGroupOp to dynamically legal ops
target.addDynamicallyLegalOp<omp::ParallelOp, omp::WsLoopOp,
                             omp::TaskOp, omp::TaskGroupOp>(
    [&](Operation *op) { return converter.isLegal(&op->getRegion(0)); });
```

**Patch 2** — `OpenMPToLLVMIRTranslation.cpp` (relax clause checks):
```cpp
// Line 670-673: Remove untied/mergeable/priority from error check
// These are handled by libomp via flags anyway
if (taskOp.getInReductions() || !taskOp.getAllocateVars().empty()) {
  return taskOp.emitError("unhandled clauses for translation to LLVM IR");
}
// Then at line 713-714, pass untied/priority to OMPIRBuilder:
builder.restoreIP(moduleTranslation.getOpenMPBuilder()->createTask(
    ompLoc, allocaIP, bodyCB, !taskOp.getUntied(),
    moduleTranslation.lookupValue(taskOp.getFinalExpr()),
    moduleTranslation.lookupValue(taskOp.getIfExpr()), dds,
    /*Mergeable=*/taskOp.getMergeableAttr() != nullptr,
    moduleTranslation.lookupValue(taskOp.getPriority())));
```

**Patch 3** — `OpenMPToLLVMIRTranslation.cpp` (add TaskLoopOp handler):
This is more involved. Would need to implement `convertOmpTaskLoopOp` that:
1. Creates a loop emitting individual task calls per iteration (or batch)
2. Wraps in implicit taskgroup unless `nogroup` is set
3. Handles grainsize/num_tasks scheduling

**Pros**: Fast, minimal risk, no API migration
**Cons**: Carrying patches against LLVM, taskloop handler is substantial work

### Option B: Backport Task Lowering from LLVM 23 to LLVM 18 (Moderate — 2-4 weeks)

Cherry-pick the relevant commits from `llvm-project-latest/` into the Polygeist-bundled
`llvm-project/`. The key files:
- `OpenMPToLLVMIRTranslation.cpp` (2459 lines in 18 → ~7500 lines in 23)
- `OMPIRBuilder.cpp` (`createTask()` signature changed significantly)
- `OpenMPOps.td` (clause-based ODS redesign in LLVM 19)

**Pros**: Production-quality task lowering, matching upstream behavior
**Cons**: ODS redesign in LLVM 19 makes clean cherry-picks impossible;
would effectively require rewriting the translation layer

### Option C: Full LLVM Upgrade to 23 (6-10 weeks)

As detailed in Section 4 of this report. ~3,800 call site changes across 12 categories.

**Pros**: Everything works, future-proof, access to all LLVM improvements
**Cons**: Massive effort, highest risk (especially cgeist's 13K lines of Clang internals)

### Option D: Bypass Polygeist for OMP Baseline — Use GCC/System Clang (Immediate)

Instead of `cgeist → LLVM IR → clang`, compile the OMP baseline with system clang or gcc:

```bash
gcc -O3 -fopenmp source.c -o binary   # or
clang -O3 -fopenmp source.c -o binary
```

**Pros**: Immediate, zero code changes, production-quality OMP task support
**Cons**: Different frontend (not Polygeist) — arguably a fairer comparison since it shows
CARTS (Polygeist+ARTS) vs native OMP compiler. The current 2-step Polygeist baseline was
designed to isolate the runtime difference, but for task benchmarks it's broken anyway.

---

## 4. LLVM Migration Cost Assessment

**Current LLVM**: 18.0.0git (commit `26eb4285`, September 28, 2023)
**Target LLVM**: 23.0.0 (latest trunk, March 2026)
**Gap**: 5 major versions, ~2.5 years

### 4.1 Breaking Changes Summary

| Category | Call Sites | Difficulty | Automatable |
|----------|-----------|------------|-------------|
| Member cast removal (`x.cast<T>()` → `cast<T>(x)`) | 758 | Mechanical | YES (sed) |
| `OpBuilder::create<T>` → `OpTy::create` | 2,272 | Mechanical | YES (clang-tidy) |
| `GEN_PASS_CLASSES` → `GEN_PASS_DEF_*` | ~90 files | Moderate | Partial |
| Typed LLVM pointer removal | ~136 | Hard (semantic) | NO |
| `applyPatternsAndFoldGreedily` rename | 25 | Trivial | YES |
| Clang API changes (cgeist frontend) | ~490 refs | HARD | NO |
| LLVM dialect op signature changes | ~150 | Medium | NO |
| GPU serialize pass rewrite | 2 files | Full rewrite | NO (skip if no GPU) |

**Total**: ~3,800 call site changes
**Estimated effort**: 6-10 weeks for one experienced developer

### 4.2 Highest Risks

1. **cgeist frontend** (13K lines of Clang internals, 5 major versions of API churn)
2. **Typed pointer removal** (semantic, not mechanical)
3. **No upstream support** (Polygeist project hasn't upgraded past LLVM 18)

---

## 5. CARTS Path: What Already Works

The CARTS compilation path (OMP→ARTS) already has **full end-to-end task support**:

```
#pragma omp task depend(in:A) depend(out:B)
  → cgeist → omp.task (custom CARTS additions: 928 lines in CGStmt.cc)
  → ConvertOpenMPToArts (TaskToARTSPattern, 11 patterns)
  → arts.edt<task> + arts.db_control
  → ARTS optimization pipeline (DbPartitioning, EdtTransforms, EpochOpt)
  → EdtLowering → arts_edt_create() + arts_add_dependence()
  → ARTS runtime execution
```

Missing CARTS-path gaps (NO LLVM upgrade needed):
- `#pragma omp taskgroup` — no visitor in cgeist, no pattern in ConvertOpenMPToArts
- `#pragma omp taskyield` — no visitor in cgeist
- `if`/`final`/`priority` clauses — captured but not lowered to ARTS

---

## 6. Recommendation

**For unblocking benchmarks now**: Use **Option D** (system compiler baseline) as immediate
workaround + **Option A** (patch LLVM 18) for Polygeist-consistent baseline.

**For long-term**: **Option C** (full LLVM upgrade) when there's a development window,
using incremental version hops (18→19→20→...→23).

---

## 7. Key Files Reference

| Component | File | Lines |
|-----------|------|-------|
| **ROOT CAUSE** | `external/Polygeist/lib/polygeist/Passes/ConvertPolygeistToLLVM.cpp` | 3262-3267 |
| Task clause rejection | `external/Polygeist/llvm-project/mlir/lib/Target/LLVMIR/Dialect/OpenMP/OpenMPToLLVMIRTranslation.cpp` | 670-673 |
| Missing taskloop handler | Same file | 2442-2444 (`.Default` case) |
| Task frontend | `external/Polygeist/tools/cgeist/Lib/CGStmt.cc` | 525-1050 |
| OMP→ARTS conversion | `lib/arts/passes/transforms/ConvertOpenMPToArts.cpp` | 1-825 |
| OMP baseline build rules | `external/carts-benchmarks/common/carts.mk` | 102-107 |
| cgeist driver (emit-llvm) | `external/Polygeist/tools/cgeist/driver.cc` | 1088, 1106-1108 |
| LLVM 23 task lowering | `external/llvm-project-latest/mlir/lib/Target/LLVMIR/Dialect/OpenMP/OpenMPToLLVMIRTranslation.cpp` | ~2700-3200 |
| Task benchmarks | `external/carts-benchmarks/kastors-jacobi/jacobi-task-dep/` | - |
| Task benchmarks | `external/carts-benchmarks/kastors-jacobi/poisson-task/` | - |
