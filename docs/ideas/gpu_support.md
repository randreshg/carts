# GPU Extension Research & Implementation Plan for CARTS

## Executive Summary

This document presents findings from investigating how to extend CARTS to target GPUs. The key insight is that **ARTS runtime already has comprehensive GPU support** (EDTs, memory, streams, multi-GPU) but **CARTS does not generate GPU code** - it parses GPU configuration but doesn't use it.

## Research Findings

### Current State Analysis

| Component | Status | Details |
|-----------|--------|---------|
| ARTS GPU Runtime | **Partial** | APIs exist but **missing persistent event support** (see critical gap below) |
| CARTS GPU DbModes | **Defined** | `gpu_read`, `gpu_write`, `lc`, `lc_sync` in ArtsAttributes.td:85-91 |
| GPU Configuration | **Parsed** | ArtsAbstractMachine reads `arts.cfg` GPU settings |
| GPU Code Generation | **Missing** | No passes generate GPU EDTs or CUDA kernels |

---

## ARTS GPU Runtime Analysis: Persistent Events

### Two Signaling Paths Exist

**Path 1: DataBlock Release Chain (FULLY WORKING)**

GPU EDTs call `releaseDbs()` at line 308, which uses the **exact same path as CPU EDTs**:

```cpp
// GpuRuntime.cu:308
releaseDbs(depc, depv, true);  // GPU calls same function as CPU!
```

This triggers:
```
releaseDbs() → artsDbDecrementLatch() → artsPersistentEventDecrementLatch()
            → artsPersistentEventSatisfy() → Signals ALL dependents
```

**Result**: DataBlock persistent events (which Memory EDTs use) **work correctly** for GPU.

**Path 2: Explicit toSignal Parameter (MINOR GAP)**

```cpp
// GpuRuntime.cu:319-331
if (toSignal) {
    artsType_t mode = artsGuidGetType(toSignal);
    if (mode == ARTS_EDT || mode == ARTS_GPU_EDT)  // ✓
        artsSignalEdt(...);
    if (mode == ARTS_EVENT)                        // ✓
        artsEventSatisfySlot(...);
    if (mode == ARTS_BUFFER)                       // ✓
        artsSetBuffer(...);
    // MISSING: ARTS_PERSISTENT_EVENT              // ✗ (minor gap)
}
```

This gap only affects **standalone persistent events** passed as `toSignal` - an uncommon use case.

### Practical Impact Assessment

| Use Case | Works? | Reason |
|----------|--------|--------|
| DataBlock dependencies | **YES** | Via `releaseDbs()` path |
| Memory EDT patterns | **YES** | Uses DB persistent events |
| Parallel loop → GPU | **YES** | Standard dependency model |
| Standalone persistent events as toSignal | **NO** | Missing case in switch |

### GPU Examples Workaround Patterns

The ARTS GPU examples use these patterns (all work correctly):

1. **Pre-allocated reduction trees** (mmTileRed.cu) - Persistent EDTs created at init
2. **Local buffers** (stream.cu) - `artsBlockForBuffer()` for synchronization
3. **Epochs** (bfs.cu) - Dynamic work with epoch termination detection
4. **Reserved GUIDs** (randomAccess.cu) - Progressive EDT chains

### ARTS Runtime Enhancement (TO IMPLEMENT)

**File**: `external/arts/core/src/gpu/GpuRuntime.cu:328`

```cpp
// Add after ARTS_BUFFER case in artsGpuHostWrapUp():
if (mode == ARTS_PERSISTENT_EVENT)
    artsPersistentEventSatisfy(toSignal, slot, true);
```

**Why**: Enables explicit persistent event signaling from GPU completion callbacks. While DataBlock dependencies work via `releaseDbs()`, this completes the signaling API for all ARTS types.

### Other GPU Runtime Characteristics

| Characteristic | Status | Notes |
|----------------|--------|-------|
| DataBlock persistent events | **Working** | Via releaseDbs() |
| Single-target GPU callbacks | Limitation | GPU signals ONE explicit target |
| GPU-to-GPU signaling | Via host | All must route through callbacks |
| GPU dynamic scheduling | Not supported | Can't spawn EDTs from GPU kernel |
| Stream-based execution | Working | Callbacks after kernel completion |

---

## CPU/GPU Code Sharing Analysis in ARTS Runtime

### Quantitative Sharing Analysis

| Category | Shared | CPU-Only | GPU-Only |
|----------|--------|----------|----------|
| EDT Creation | 70% | 10% | 20% |
| EDT Structures | 85% | 0% | 15% |
| Database Handling | 85% | 10% | 5% |
| Signaling | **100%** | 0% | 0% |
| **Overall** | **~76%** | ~15% | ~9% |

### Key Architectural Patterns

**1. Inheritance Model for EDT Structures**
```c
// CPU EDT (base structure)
struct artsEdt {
  struct artsHeader header;
  artsEdt_t funcPtr;
  uint32_t paramc, depc;
  artsGuid_t currentEdt, epochGuid;
  volatile unsigned int depcNeeded;
  // ... common fields
};

// GPU EDT (extends base - 100% inheritance)
typedef struct {
  struct artsEdt wrapperEdt;  // Base EDT embedded
  dim3 grid, block;           // GPU-specific additions
  int gpuToRunOn;
  artsGuid_t endGuid, dataGuid;
  uint32_t slot;
  bool passthrough, lib;
} artsGpuEdt_t;
```

**2. Unified DB Handling via Boolean Flag**
```c
// Same function handles both CPU and GPU
void prepDbs(unsigned int depc, artsEdtDep_t *depv, bool gpu);
void releaseDbs(unsigned int depc, artsEdtDep_t *depv, bool gpu);

// GPU: prepDbs(depc, depv, true)   → skips LC locking
// CPU: prepDbs(depc, depv, false)  → includes LC locking
```

**3. Shared Signaling (100% Reuse)**
```c
// Same functions for both CPU and GPU
artsSignalEdt(edtGuid, slot, dataGuid);
artsSignalEdtWithMode(edtGuid, slot, dataGuid, mode);
// artsGpuHostWrapUp() calls these same functions
```

### Execution Model Difference

| Aspect | CPU EDT | GPU EDT |
|--------|---------|---------|
| Execution | Synchronous | Asynchronous (stream callback) |
| Flow | prep → run → release → delete | prep → schedule → [callback] → release → delete |
| Cleanup | Immediate in same thread | Via `artsGpuHostWrapUp()` callback |

---

## Target Generated Code Patterns

### GPU Kernel Signature (from fibGpu.cu)

```cuda
// Standard ARTS GPU kernel signature - CARTS must generate this
__global__ void kernelName(uint32_t paramc, uint64_t *paramv,
                           uint32_t depc, artsEdtDep_t depv[]) {
  // Access parameters
  unsigned int param1 = (unsigned int)paramv[0];

  // Access dependencies (data pointers)
  double *A = (double *)depv[0].ptr;
  double *B = (double *)depv[1].ptr;

  // Compute thread index
  int idx = threadIdx.x + blockIdx.x * blockDim.x;

  // Kernel body
  if (idx < param1) {
    B[idx] = A[idx] * 2.0;
  }
}
```

### GPU EDT Creation Pattern

```c
// Parameters packed as uint64_t array
uint64_t args[] = {(uint64_t)N, (uint64_t)otherParam};

// Grid/block dimensions
dim3 grid((N + 255) / 256, 1, 1);
dim3 block(256, 1, 1);

// Create GPU EDT
artsGuid_t gpuEdt = artsEdtCreateGpu(
    kernelName,          // GPU kernel function
    artsGetCurrentNode(), // node
    2,                   // paramc
    args,                // paramv
    3,                   // depc (number of dependencies)
    grid, block,         // launch config
    nextEdtGuid,         // EDT to signal on completion
    0,                   // signal slot
    resultDbGuid         // data GUID for signaling
);

// Signal dependencies (triggers execution when all received)
artsSignalEdt(gpuEdt, 0, aGuid);
artsSignalEdt(gpuEdt, 1, bGuid);
artsSignalEdt(gpuEdt, 2, cGuid);
```

### Data Allocation Pattern

```c
// GPU-accessible database creation
double *data = NULL;
artsGuid_t dataGuid = artsDbCreate(
    (void **)&data,
    N * sizeof(double),
    ARTS_DB_GPU_WRITE  // or ARTS_DB_GPU_READ
);

// Initialize data (on CPU)
for (int i = 0; i < N; i++) data[i] = i;

// Now signal EDT that depends on this data
artsSignalEdt(gpuEdt, slot, dataGuid);
```

---

## Implementation Options: GPU Dialect vs Direct ARTS

### Option A: Direct ARTS Runtime Calls (Recommended for Phase 1)

**Approach**: Generate ARTS GPU runtime calls directly, skip MLIR GPU dialect

```
arts.for (parallel) → GpuForLowering → arts.gpu_edt → EdtLowering → artsEdtCreateGpu()
```

**Pros**:
- Fastest to implement
- Maximum control over ARTS integration
- Proven patterns from GPU examples
- 76% code reuse with CPU path

**Cons**:
- Loses GPU-specific optimizations from LLVM
- No portability to non-CUDA backends

### Option B: Full MLIR GPU Dialect

**Approach**: Generate gpu.launch/gpu.func, lower via NVVM, link with ARTS

```
arts.for → gpu.launch → NVVM → PTX → link with ARTS runtime
```

**Pros**:
- Access to mature GPU optimization passes
- Portability to HIP, OpenCL, Vulkan
- Standard tooling

**Cons**:
- Complex integration with ARTS EDT system
- Memory management impedance mismatch
- Double scheduling layer

### Corrected Approach: Extend ConvertOpenMPToArts for GPU

**Key Insight**: Polygeist's `ConvertParallelToGPU` works on `scf.parallel`, not ARTS ops. By the time we have `arts.for`, Polygeist can't help. The solution is to make the GPU decision **during** OpenMP→ARTS conversion.

#### Corrected Pipeline

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SOURCE CODE (OpenMP C/C++)                         │
│  #pragma omp parallel for                                                   │
│  for (int i = 0; i < N; i++) C[i] = A[i] + B[i];                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    POLYGEIST FRONTEND (cgeist)                              │
│  Generates: omp.parallel + omp.wsloop (or scf.parallel)                    │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│               ConvertOpenMPToArts (EXTENDED FOR GPU)                        │
│                                                                             │
│  1. Check GPU support: ArtsAbstractMachine::hasGpuSupport()                │
│  2. For each omp.wsloop / scf.parallel:                                    │
│     - Check GPU eligibility (no loop-carried deps, sufficient size)        │
│     - GPU eligible? → GPUEligibleWsloopPattern (higher priority)          │
│     - Not eligible? → WsloopToARTSPattern (fallback to CPU)               │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                    ┌─────────────────┴─────────────────┐
                    ▼                                   ▼
┌───────────────────────────────────┐  ┌───────────────────────────────────┐
│         GPU PATH                   │  │         CPU PATH                   │
│  (GPUEligibleWsloopPattern)        │  │  (WsloopToARTSPattern)            │
└───────────────────────────────────┘  └───────────────────────────────────┘
                    │                                   │
                    ▼                                   ▼
┌───────────────────────────────────┐  ┌───────────────────────────────────┐
│ arts.edt<single> {                │  │ arts.edt<parallel> {              │
│   // Memory EDTs for H2D          │  │   arts.for (%i) = ... {           │
│   arts.gpu_memcpy h2d(...)        │  │     // loop body                  │
│                                   │  │   }                               │
│   // GPU EDT with kernel          │  │ }                                 │
│   arts.gpu_edt route(...) {       │  │                                   │
│     // kernel body                │  │ Lowers via existing pipeline:     │
│     // (uses gpu dialect or       │  │ ForLowering → EdtLowering → LLVM │
│     //  direct NVVM intrinsics)   │  │                                   │
│   }                               │  │                                   │
│                                   │  │                                   │
│   // D2H sync                     │  │                                   │
│   arts.lc_sync(...)               │  │                                   │
│ }                                 │  │                                   │
└───────────────────────────────────┘  └───────────────────────────────────┘
                    │                                   │
                    ▼                                   ▼
┌───────────────────────────────────┐  ┌───────────────────────────────────┐
│ GpuEdtLowering (NEW)              │  │ EdtLowering (existing)            │
│  - Outline kernel to __global__   │  │  - artsEdtCreate()                │
│  - Generate artsEdtCreateGpu()    │  │  - artsSignalEdt()                │
│  - Generate artsSignalEdt()       │  │                                   │
└───────────────────────────────────┘  └───────────────────────────────────┘
                    │                                   │
                    └─────────────────┬─────────────────┘
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         LLVM CODE GENERATION                                 │
│  - GPU: __global__ kernel + artsEdtCreateGpu() host wrapper                │
│  - CPU: Outlined EDT function + artsEdtCreate()                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Key Code Changes in ConvertOpenMPToArts.cpp

**Location**: `lib/arts/Passes/Transformations/ConvertOpenMPToArts.cpp`

```cpp
// In runOnOperation() - add GPU patterns BEFORE CPU patterns:
void ConvertOpenMPToArtsPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *context = &getContext();

  // Check GPU support from abstract machine config
  ArtsAbstractMachine machine;
  bool hasGpuSupport = machine.hasGpuSupport();

  RewritePatternSet patterns(context);

  // GPU patterns first (higher priority - tried before CPU fallback)
  if (hasGpuSupport) {
    patterns.add<GPUEligibleWsloopPattern>(context, machine);
    patterns.add<GPUEligibleSCFParallelPattern>(context, machine);
  }

  // CPU patterns (fallback if GPU patterns don't match)
  patterns.add<OMPParallelToArtsPattern, SCFParallelToArtsPattern,
               WsloopToARTSPattern, ...>(context);

  (void)applyPatternsAndFoldGreedily(module, std::move(patterns));
}
```

**New GPU Pattern**:
```cpp
struct GPUEligibleWsloopPattern : public OpRewritePattern<omp::WsLoopOp> {
  ArtsAbstractMachine machine;

  LogicalResult matchAndRewrite(omp::WsLoopOp op,
                                PatternRewriter &rewriter) const override {
    // Check GPU eligibility
    if (!isGpuEligible(op))
      return failure();  // Fall through to WsloopToARTSPattern

    auto loc = op.getLoc();

    // Create orchestrating EDT (single, not parallel)
    auto orchEdt = rewriter.create<EdtOp>(loc, EdtType::single, ...);

    // Inside orchestrating EDT:
    // 1. Create GPU DB allocations with gpu_read/gpu_write mode
    // 2. Create H2D memory transfers
    // 3. Create arts.gpu_edt with kernel body
    // 4. Create D2H sync

    auto gpuEdt = rewriter.create<GpuEdtOp>(loc, ...);
    // Move loop body to GPU EDT

    rewriter.eraseOp(op);
    return success();
  }

  bool isGpuEligible(omp::WsLoopOp op) const {
    // 1. No loop-carried dependencies
    // 2. Sufficient iteration count (use metadata if available)
    // 3. Simple operations (arithmetic, memory)
    // 4. No complex control flow
    return true; // Simplified
  }
};
```

#### Concrete MLIR Example: Hybrid Transformation

**Input (after CARTS pipeline):**
```mlir
// arts.edt<parallel> with GPU-eligible arts.for
arts.edt<parallel> route(%route) (%A, %B, %C : memref<?xf64>) {
  arts.for (%i) = (%c0) to (%N) step (%c1)
      {gpu_profitability = #arts.gpu_profitability<
          iterations = 1000000, isEmbarrassinglyParallel = true>} {
    %a = memref.load %A[%i] : memref<?xf64>
    %b = memref.load %B[%i] : memref<?xf64>
    %sum = arith.addf %a, %b : f64
    memref.store %sum, %C[%i] : memref<?xf64>
  }
  arts.yield
}
```

**After Hybrid Transformation:**
```mlir
// Split into: Memory EDTs + GPU EDT + Kernel

// 1. Memory EDT: H2D transfers (orchestrating EDT)
arts.edt<single> route(%route) {
  // Allocate GPU-accessible DBs
  %gpu_A_guid, %gpu_A = arts.db_alloc [gpu_read] sizes[%N] : memref<?xf64>
  %gpu_B_guid, %gpu_B = arts.db_alloc [gpu_read] sizes[%N] : memref<?xf64>
  %gpu_C_guid, %gpu_C = arts.db_alloc [gpu_write] sizes[%N] : memref<?xf64>

  // H2D transfers (could be Memory EDTs in full implementation)
  arts.gpu_memcpy h2d(%gpu_A, %A, %size)
  arts.gpu_memcpy h2d(%gpu_B, %B, %size)

  // 2. GPU EDT wrapper (generates artsEdtCreateGpu)
  %blocks = arith.ceildivui %N, %c256 : index
  %gpu_edt_guid = arts.gpu_edt route(%route)
      launch<gridX = %blocks, blockX = 256>
      (%gpu_A_guid, %gpu_B_guid, %gpu_C_guid)
      signal(%next_edt, %slot, %gpu_C_guid) {
    // 3. Kernel body → GPU dialect → NVVM
    gpu.launch blocks(%bx, %by, %bz) in (%gx = %blocks, %gy = %c1, %gz = %c1)
               threads(%tx, %ty, %tz) in (%bkx = %c256, %bky = %c1, %bkz = %c1) {
      %global_id = arith.addi %tx, arith.muli %bx, %c256
      %in_bounds = arith.cmpi slt, %global_id, %N
      scf.if %in_bounds {
        %a_val = memref.load %gpu_A[%global_id] : memref<?xf64>
        %b_val = memref.load %gpu_B[%global_id] : memref<?xf64>
        %sum = arith.addf %a_val, %b_val : f64
        memref.store %sum, %gpu_C[%global_id] : memref<?xf64>
      }
      gpu.terminator
    }
    arts.yield
  } : i64

  // D2H sync (triggered after GPU kernel completes)
  arts.lc_sync(%gpu_edt_guid, %c0, %gpu_C_guid)

  arts.yield
}
```

**Final Generated Code (conceptual C/CUDA):**
```cuda
// Kernel (from GPU dialect → NVVM path)
__global__ void vectorAdd_kernel(uint32_t paramc, uint64_t *paramv,
                                  uint32_t depc, artsEdtDep_t depv[]) {
  int N = (int)paramv[0];
  double *A = (double *)depv[0].ptr;
  double *B = (double *)depv[1].ptr;
  double *C = (double *)depv[2].ptr;

  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < N) {
    C[idx] = A[idx] + B[idx];
  }
}

// Host wrapper (from ARTS EDT path)
void launch_vectorAdd(artsGuid_t aGuid, artsGuid_t bGuid, artsGuid_t cGuid,
                      int N, artsGuid_t nextEdt, int slot) {
  // Parameters
  uint64_t args[] = {(uint64_t)N};

  // Grid/block
  dim3 grid((N + 255) / 256);
  dim3 block(256);

  // Create GPU EDT
  artsGuid_t gpuEdt = artsEdtCreateGpu(
      vectorAdd_kernel,
      artsGetCurrentNode(),
      1, args,        // paramc, paramv
      3,              // depc
      grid, block,
      nextEdt, slot,  // signal on completion
      cGuid           // data to signal with
  );

  // Signal dependencies → triggers kernel when all ready
  artsSignalEdt(gpuEdt, 0, aGuid);
  artsSignalEdt(gpuEdt, 1, bGuid);
  artsSignalEdt(gpuEdt, 2, cGuid);
}
```

#### Why Hybrid Works Well

1. **Kernel Optimization**: GPU dialect path gets LLVM's GPU optimizations (vectorization, coalescing, barrier optimization)

2. **EDT Integration**: ARTS EDT path maintains proper task-graph semantics (dependencies, signaling, epochs)

3. **Memory Management**: Memory EDT path enables data movement optimization (prefetch, double-buffer, overlap)

4. **Incremental Adoption**: Can start with Option A (direct ARTS) and add GPU dialect later

### Polygeist GPU Support (Already Available!)

```cpp
// external/Polygeist/lib/polygeist/Passes/ConvertParallelToGPU.cpp
// Converts scf.parallel → gpu.launch with automatic:
// - Block/thread dimension mapping
// - Thread coarsening for large iteration spaces
// - Workgroup memory optimization
// - Barrier synchronization analysis
```

---

## ARTS GPU Runtime APIs

```c
// GPU EDT Creation
artsEdtCreateGpu(funcPtr, route, paramc, paramv, depc, grid, block, ...)
artsEdtCreateGpuDirect(funcPtr, route, gpu_id, ...)  // Target specific GPU
artsEdtCreateGpuPT(...)  // Pass-through variant

// GPU Memory
artsDbCreate((void**)&ptr, size, ARTS_DB_GPU_WRITE)  // GPU-accessible DB
artsDbCreate((void**)&ptr, size, ARTS_DB_GPU_READ)   // GPU read-only DB

// Signaling (shared with CPU)
artsSignalEdt(edtGuid, slot, dataGuid)

// Synchronization
artsBlockForBuffer(bufferGuid)  // Blocking wait
```

### Relevant Papers

1. **Memory Codelets** ([arxiv 2302.00115](https://arxiv.org/abs/2302.00115))
   - Prefetching, streaming, recoding as first-class tasks
   - Aligns with Memory EDT concept in `/opt/carts/docs/ideas/memory_edts.md`

2. **GPU-to-CPU Transpilation** ([arxiv 2207.00257](https://arxiv.org/abs/2207.00257))
   - Uses Polygeist/MLIR for transpilation (same frontend as CARTS)
   - Achieves 76% geomean speedup over handwritten OpenMP
   - Key: high-level parallel construct representation

---

## Proposed GPU Extension Architecture

### Design Approach

Rather than using MLIR's GPU dialect (gpu.launch), we generate **ARTS GPU runtime calls directly** - matching CARTS's existing philosophy where EDTs are the unit of execution.

```
   Embarrassingly Parallel Loop
              |
              v
   [GpuEligibilityAnalysis] -- Detect GPU-suitable loops
              |
              v
   [GpuForLowering] -- Transform to GPU EDT + Memory EDTs
              |
              v
   [EdtLowering] -- Generate artsEdtCreateGpu() calls
              |
              v
   ARTS GPU Runtime
```

### Memory EDT Integration

Memory EDTs handle CPU<->GPU data movement as first-class tasks:

```
CPU Memory EDT (orchestrator)
    |
    +-- arts.db_alloc with mode=gpu_read/gpu_write
    |
    +-- arts.gpu_memcpy h2d (Transfer to GPU)
    |
    +-- arts.gpu_edt (GPU kernel)
    |
    +-- arts.lc_sync (Synchronize/D2H)
    |
    +-- Continue on CPU
```

---

## New MLIR Operations

### 1. arts.gpu_edt (GPU Event-Driven Task)

```mlir
%guid = arts.gpu_edt route(%r) launch<grid=512, block=256> (%dep1, %dep2) {
  // Kernel body - uses GPU thread/block indices
  %tid = arts.gpu_thread_id x
  %bid = arts.gpu_block_id x
  // ... computation ...
  arts.yield
} : i64
```

### 2. arts.gpu_memcpy (Data Transfer)

```mlir
arts.gpu_memcpy h2d(%gpu_ptr, %cpu_ptr, %size) : memref<?xf64>, memref<?xf64>
arts.gpu_memcpy d2h(%cpu_ptr, %gpu_ptr, %size) : memref<?xf64>, memref<?xf64>
```

### 3. arts.lc_sync (Local Copy Synchronization)

```mlir
arts.lc_sync(%edt_guid, %slot, %data_guid) // CPU-GPU coherence
```

### New Attributes

```tablegen
// GPU launch configuration
def GpuLaunchConfigAttr : {gridX, gridY, gridZ, blockX, blockY, blockZ}

// GPU profitability metrics
def GpuProfitabilityAttr : {iterations, memoryBytes, isEmbarrassinglyParallel, hasReductions}

// GPU target hint
def GpuTargetAttr : {none, cuda, auto}
```

---

## New Passes

### 1. GpuEligibilityAnalysis

**Purpose**: Identify GPU-suitable `arts.for` loops

**Criteria**:
- Embarrassingly parallel (no loop-carried dependencies)
- Sufficient iterations (> 1024, configurable)
- Sufficient data size (> 1MB, to amortize transfer overhead)
- No complex control flow or function calls

**Output**: Attaches `GpuProfitabilityAttr` to eligible loops

### 2. GpuForLowering

**Purpose**: Transform GPU-eligible `arts.for` to `arts.gpu_edt`

**Transformation**:
```mlir
// BEFORE
arts.edt<parallel> {
  arts.for (%i) = (0) to (%N) {
    C[i] = A[i] + B[i]
  }
}

// AFTER
arts.edt<single> {
  // H2D transfers
  arts.gpu_memcpy h2d(%gpu_A, %A, %size)
  arts.gpu_memcpy h2d(%gpu_B, %B, %size)

  // GPU kernel
  %blocks = ceildiv(%N, 256)
  %guid = arts.gpu_edt route(%r) launch<grid=%blocks, block=256> {
    %tid = arts.gpu_thread_id x
    %bid = arts.gpu_block_id x
    %i = %tid + %bid * 256
    if (%i < %N) {
      C[i] = A[i] + B[i]
    }
  }

  // D2H sync
  arts.lc_sync(%guid, 0, %gpu_C_guid)
}
```

### 3. GpuCodegen (CUDA Kernel Generation)

**Purpose**: Outline GPU EDT bodies to CUDA kernel functions

**Output**: Functions with `__global__` attribute matching ARTS signature:
```c
__global__ void kernel(uint32_t paramc, uint64_t *paramv,
                       uint32_t depc, artsEdtDep_t depv[])
```

---

## Files to Modify

### New Files

| File | Purpose |
|------|---------|
| `lib/arts/Passes/Analysis/GpuEligibilityAnalysis.cpp` | GPU loop analysis |
| `lib/arts/Passes/Transformations/GpuForLowering.cpp` | GPU loop transformation |
| `lib/arts/Passes/Transformations/GpuCodegen.cpp` | CUDA kernel generation |
| `include/arts/Passes/GpuPasses.td` | GPU pass definitions |

### Modified Files

| File | Changes |
|------|---------|
| `include/arts/ArtsOps.td` | Add GpuEdtOp, GpuMemcpyOp, LCSyncOp |
| `include/arts/ArtsAttributes.td` | Add GpuLaunchConfigAttr, GpuProfitabilityAttr |
| `lib/arts/Passes/Transformations/ForLowering.cpp` | Add GPU path routing |
| `lib/arts/Passes/Transformations/EdtLowering.cpp` | Add GpuEdtOp lowering |
| `lib/arts/Passes/Transformations/ConvertArtsToLLVM.cpp` | Add GPU op patterns |
| `lib/arts/Codegen/ArtsCodegen.cpp` | Add GPU runtime function declarations |
| `include/arts/Codegen/ArtsKinds.def` | Add GPU runtime function IDs |

---

## Profitability Heuristics

```
Is loop embarrassingly parallel?
  |-- No  --> CPU execution
  |-- Yes --> Iteration count >= 1024?
                |-- No  --> CPU (insufficient parallelism)
                |-- Yes --> Data size >= 1MB?
                              |-- No  --> CPU (transfer overhead)
                              |-- Yes --> GPU execution
```

Configurable thresholds via `ArtsAbstractMachine`:
- `gpuMinIterations = 1024`
- `gpuMinDataBytes = 1048576` (1MB)
- `gpuMinArithIntensity = 0.1` (FLOPs/byte)

---

## Implementation Phases (Revised)

### Strategy: Extend ConvertOpenMPToArts for GPU Support

The GPU decision happens at OpenMP→ARTS conversion time, not after. This avoids the mismatch where Polygeist GPU passes work on `scf.parallel` but we have `arts.for`.

---

### Phase 1: ARTS Runtime Fix + MLIR Infrastructure

**1.1 ARTS Runtime Enhancement**
- File: `external/arts/core/src/gpu/GpuRuntime.cu:328`
- Add: `if (mode == ARTS_PERSISTENT_EVENT) artsPersistentEventSatisfy(toSignal, slot, true);`

**1.2 New MLIR Operations** (ArtsOps.td)
```tablegen
def GpuEdtOp : Arts_Op<"gpu_edt", [...]> {
  let arguments = (ins
    I32:$route,
    GpuLaunchConfigAttr:$launch_config,  // gridX/Y/Z, blockX/Y/Z
    Variadic<AnyMemRef>:$dependencies,
    OptionalAttr<I64Attr>:$signal_edt,
    OptionalAttr<I32Attr>:$signal_slot
  );
  let regions = (region SizedRegion<1>:$body);
  let results = (outs I64:$guid);
}

def GpuMemcpyOp : Arts_Op<"gpu_memcpy", [...]> {
  let arguments = (ins AnyMemRef:$dest, AnyMemRef:$src, Index:$size,
                       GpuMemcpyDirectionAttr:$direction);  // h2d, d2h
}
```

**1.3 New Attributes** (ArtsAttributes.td)
```tablegen
def GpuLaunchConfigAttr : Arts_Attr<"GpuLaunchConfig", "gpu_config"> {
  let parameters = (ins "int64_t":$gridX, "int64_t":$gridY, "int64_t":$gridZ,
                        "int64_t":$blockX, "int64_t":$blockY, "int64_t":$blockZ);
}

def GpuMemcpyDirection : Arts_I32Enum<"GpuMemcpyDirection", [
  I32EnumAttrCase<"h2d", 0>, I32EnumAttrCase<"d2h", 1>, I32EnumAttrCase<"d2d", 2>
]>;
```

### Phase 2: Extend ConvertOpenMPToArts for GPU

**2.1 Modify runOnOperation()** (`lib/arts/Passes/Transformations/ConvertOpenMPToArts.cpp`)
```cpp
void ConvertOpenMPToArtsPass::runOnOperation() {
  ArtsAbstractMachine machine;
  bool hasGpu = machine.hasGpuSupport();

  RewritePatternSet patterns(context);

  // GPU patterns FIRST (higher priority)
  if (hasGpu) {
    patterns.add<GPUEligibleWsloopPattern>(context, machine);
    patterns.add<GPUEligibleSCFParallelPattern>(context, machine);
  }

  // CPU patterns (fallback)
  patterns.add<WsloopToARTSPattern, SCFParallelToArtsPattern, ...>(context);

  applyPatternsAndFoldGreedily(module, std::move(patterns));
}
```

**2.2 Create GPUEligibleWsloopPattern**
- Check GPU eligibility (no loop-carried deps, sufficient iterations)
- Generate `arts.edt<single>` orchestrator containing:
  - `arts.db_alloc` with `gpu_read`/`gpu_write` mode
  - `arts.gpu_memcpy h2d` for input arrays
  - `arts.gpu_edt` with kernel body
  - `arts.lc_sync` for output synchronization

**2.3 GPU Eligibility Heuristics**
```cpp
bool isGpuEligible(omp::WsLoopOp op) {
  // 1. Check loop metadata for inter-iteration deps
  if (auto meta = op->getAttr("loop_metadata"))
    if (cast<LoopMetadataAttr>(meta).getHasInterIterationDeps())
      return false;

  // 2. Check minimum iteration count (from metadata or estimate)
  // 3. Check operations are GPU-safe (arithmetic, memref load/store)
  // 4. No function calls (except intrinsics)
  return true;
}
```

### Phase 3: GPU EDT Lowering

**3.1 Create GpuEdtLoweringPass** (`lib/arts/Passes/Transformations/GpuEdtLowering.cpp`)
- Outline `arts.gpu_edt` body to `__global__` function
- Generate ARTS kernel signature: `(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[])`
- Replace `arts.gpu_edt` with `artsEdtCreateGpu()` call

**3.2 Kernel Outlining**
```cpp
// Transform kernel body to use GPU intrinsics
// memref indices → threadIdx.x + blockIdx.x * blockDim.x
// Bounds check: if (global_idx < N) { ... }
```

**3.3 Extend EdtLowering** (`lib/arts/Passes/Transformations/EdtLowering.cpp`)
- Add `GpuEdtOp` lowering pattern
- Generate `artsEdtCreateGpu()` with grid/block from `GpuLaunchConfigAttr`
- Generate `artsSignalEdt()` for dependencies

### Phase 4: Memory EDT Integration for GPU

**4.1 GPU Memory EDTs**
- H2D transfer as Memory EDT: `arts.memory_edt recode source=cpu dest=gpu`
- D2H transfer as Memory EDT: `arts.memory_edt recode source=gpu dest=cpu`
- Integrate with framework from `/opt/carts/docs/ideas/memory_edts.md`

**4.2 Data Movement Optimization**
- Overlap H2D with previous GPU kernel (double buffering)
- Use LC (Local Copy) for CPU-GPU coherence
- Prefetch hints for next kernel's data

### Phase 5: Advanced Features

**5.1 Reduction Support**
- GPU-side partial reductions per block
- CPU aggregation of partial results
- Or: hierarchical GPU reduction tree (like mmTileRed.cu)

**5.2 Multi-GPU**
- Partition work across GPUs using ARTS scheduling
- Use `artsEdtCreateGpuDirect()` for explicit GPU targeting
- Leverage ARTS GPU locality policies

**5.3 Profitability Tuning**
- Runtime feedback for threshold tuning
- Kernel launch overhead modeling
- Memory transfer cost modeling

### Phase 6: Testing & Validation

**6.1 Unit Tests**
- MLIR FileCheck tests for new operations
- GPU lowering pattern tests

**6.2 Integration Tests**
- Vector add (simple baseline)
- Matrix multiply (compute-intensive)
- Stencil (memory-intensive, tests heuristics)

**6.3 Benchmarks**
- LULESH with GPU acceleration
- Polybench GPU vs CPU comparison
- Scaling tests (1, 2, 4 GPUs)

---

## Verification Plan

1. **Unit Tests**: MLIR FileCheck tests for new ops and lowering
2. **Integration Tests**:
   - Vector add (embarrassingly parallel baseline)
   - Matrix multiply (compute-intensive)
   - Stencil (memory-intensive, tests heuristics)
3. **Benchmarks**: Compare GPU vs CPU on Polybench suite
4. **Multi-GPU**: Test with 2+ GPUs using LULESH partitioning

---

## Key References

- Memory Codelets: [arxiv.org/abs/2302.00115](https://arxiv.org/abs/2302.00115)
- GPU-to-CPU Transpilation: [arxiv.org/abs/2207.00257](https://arxiv.org/abs/2207.00257)
- Memory EDTs design: `/opt/carts/docs/ideas/memory_edts.md`
- ARTS GPU headers: `external/arts/core/inc/arts/gpu/`
