///==========================================================================///
/// File: Passes.td
///
/// This file defines pass declarations for the ARTS dialect.
///==========================================================================///
#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"

def ArtsInliner : Pass<"arts-inliner", "mlir::ModuleOp"> {
  let summary = "Inline functions in the program";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createArtsInlinerPass()";
}

def CollectMetadata : Pass<"collect-metadata", "mlir::ModuleOp"> {
  let summary = "Collect metadata for dual-compilation strategy";
  let description = [{
    This pass analyzes program structures (loops, array accesses, both affine
    and non-affine) and attaches comprehensive metadata for use in dual-compilation.
    It runs on sequential (non-OpenMP) code where analysis is cleaner, then the
    metadata is exported and injected into the parallel compilation.

    Metadata collected from both affine and non-affine operations:
    - Array dimensions inferred from access patterns (e.g., i*128+j → [128,128])
    - Loop bounds (static and dynamic), trip counts, and parallelism opportunities
    - Memory access patterns and regions (affine and non-affine loads/stores)
    - Data dependencies and access frequency statistics
    - Read/write ratios and lifetime information

    This preserves high-level semantic information through OpenMP transformations.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "affine::AffineDialect"
  ];
  let options = [
    Option<"exportMetadata", "export-metadata", "bool", "false",
           "Export metadata to JSON file after collection">,
    Option<"metadataFile", "metadata-file", "std::string",
           "\".carts-metadata.json\"", "Path to metadata JSON file for export">
  ];
  let constructor = "mlir::arts::createCollectMetadataPass()";
}

def RaiseMemRefDimensionality : Pass<"raise-memref-dimensionality", "mlir::ModuleOp"> {
  let summary = "Raise nested pointer allocations to N-dimensional memrefs";
  let description = [{
    This pass raises nested array-of-pointer allocations to multi-dimensional memrefs
    and rewrites pointer-based accesses to direct memref indexed accesses. It also
    converts OpenMP task dependencies into arts.omp_dep operations.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createRaiseMemRefDimensionalityPass()";
}

def HandleDeps : Pass<"handle-deps", "mlir::ModuleOp"> {
  let summary = "Convert residual OMP dependencies to arts.omp_dep";
  let description = [{
    Converts remaining OpenMP task dependency variables that were not handled
    by RaiseMemRefDimensionality into arts.omp_dep operations. Handles static
    arrays, token containers, subview chunks, direct allocations, and block
    arguments.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createHandleDepsPass()";
}

def ConvertOpenMPToArts : Pass<"convert-openmp-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP functions to ARTS";
  let dependentDialects = [
    "memref::MemRefDialect", "omp::OpenMPDialect", "LLVM::LLVMDialect",
    "arts::ArtsDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertOpenMPToArtsPass()";
}

def PatternDiscovery : Pass<"arts-pattern-discovery", "mlir::ModuleOp"> {
  let summary = "Seed or refine reusable pre-DB pattern contracts";
  let description = [{
    Runs metadata-guided pattern discovery on ARTS IR before DB creation.
    This pass does not perform the full schedule/kernel rewrite itself.
    Instead it:
    - seeds candidate pattern contracts after OpenMP-to-ARTS conversion
    - refines those contracts after structural cleanup exposes cleaner shapes
    - normalizes shared contract revision/family metadata for downstream
      DepTransforms and KernelTransforms

    The pass is intended to run multiple times inside the dedicated pattern
    pipeline, typically once in seed mode and once in refine mode.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let options = [
    Option<"refine", "refine", "bool", /*default=*/"false",
           "Run in refine mode instead of seed mode.">
  ];
  let constructor = "mlir::arts::createPatternDiscoveryPass(nullptr)";
}

def DeadCodeElimination : Pass<"arts-dce", "mlir::ModuleOp"> {
  let summary = "Remove dead loads, stores, allocas, and symbols";
  let description = [{
    Performs dead code elimination for memref operations:
    - Removes loads whose results are unused
    - Removes stores to allocas that are never loaded
    - Removes allocas with no remaining uses
    - Runs SymbolDCE for dead functions and globals

    This is particularly useful after openmp-to-arts conversion where OMP
    dependency proxy allocas become unused.
  }];
  let dependentDialects = ["memref::MemRefDialect"];
  let constructor = "mlir::arts::createDCEPass()";
}

def DbModeTightening : Pass<"db-mode-tightening", "mlir::ModuleOp"> {
  let summary = "Tighten DB access modes and infer storage types";
  let description =
      [{Tightens the access mode (in / out / inout)
            of DB allocations and acquires based on the actual load /
        store operations performed on them, and infers storage types.}];
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let options = [
    Option<"forceInout", "force-inout", "bool", "false",
           "Force all stores to use inout mode (disables mode tightening to out)">
  ];
  let constructor = "mlir::arts::createDbModeTighteningPass(nullptr)";
}

def DbScratchElimination : Pass<"db-scratch-elimination", "mlir::ModuleOp"> {
  let summary = "Eliminate task-private single-element scratch DBs";
  let description = [{
    Replaces task-private single-element scratch DBs with local allocas.
  }];
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createDbScratchEliminationPass()";
}

def DbPartitioning : Pass<"db-partitioning", "mlir::ModuleOp"> {
  let summary = "Partition DBs for fine-grained parallel access";
  let description = [{
    Transforms coarse-grained DB allocations to fine-grained (chunked)
    allocations for better parallelism. Also handles:
    - Stencil bounds analysis and runtime bounds checking
    - Unsupported/non-partitionable accesses remain coarse
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let options = [
    Option<"forceCoarseAllocId", "force-coarse-alloc-id", "int64_t", "-1",
           "Force the DB allocation with this arts.id to use coarse partitioning (-1 = disabled)">,
    Option<"maxOuterDbs", "max-outer-dbs", "int64_t", "1024",
           "Maximum number of outer DB partitions before coarsening">,
    Option<"maxDepsPerEdt", "max-deps-per-edt", "int64_t", "8",
           "Maximum dependencies per EDT before coarsening">,
    Option<"minInnerBytes", "min-inner-bytes", "int64_t", "64",
           "Minimum inner partition size in bytes">
  ];
  let constructor = "mlir::arts::createDbPartitioningPass(nullptr)";
}

def DbDistributedOwnership
    : Pass<"distributed-db-ownership", "mlir::ModuleOp"> {
  let summary = "Mark eligible DB allocations for distributed ownership";
  let description = [{
    Selects host-level DB allocations that are safe to distribute across
    nodes and annotates them with the `distributed` attribute for lowering.
  }];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect", "memref::MemRefDialect"];
  let constructor = "mlir::arts::createDbDistributedOwnershipPass(nullptr)";
}

def DbTransforms : Pass<"db-transforms", "mlir::ModuleOp"> {
  let summary = "Post-DB transforms (contracts, halo, GUID, cleanup)";
  let description = [{
    Hosts DB-level transformations that run after DB partitioning and
    distributed ownership decisions are known. Current transforms:
    - DT-1: Contract persistence -- persist refined AcquireContractSummary
      back to IR via upsertLoweringContract().
    - DT-2: Stencil halo consolidation
    - DT-4: GUID range allocation
    - DT-6: DB lifetime shortening
    - DT-7: Dead DB elimination

    Placeholder slots:
    - DT-3: ESD annotation
    - DT-5: Acquire coalescing
  }];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createDbTransformsPass(nullptr)";
}

def CreateDbs : Pass<"create-dbs", "mlir::ModuleOp"> {
  let summary = "Identify Dbs in the program";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateDbsPass(nullptr)";
}

def DbLowering : Pass<"db-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS DB handles to opaque pointer form";
  let description =
      [{This pass converts ARTS DataBlocks from typed memrefs to the opaque
            pointer representation required by the runtime
                .It assumes the DBs are already in canonical form.}];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createDbLoweringPass()";
}

def EdtStructuralOpt : Pass<"edt-structural-opt", "mlir::ModuleOp"> {
  let summary = "Optimize EDTs";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtStructuralOptPass(nullptr, true)";
  let options = [Option<"runAnalysis", "run-analysis", "bool",
                        /*default=*/"true", "Enable EDT analysis.">];
}

def EdtAllocaSinking : Pass<"edt-alloca-sinking", "mlir::ModuleOp"> {
  let summary = "Clone external stack allocas into EDT regions";
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createEdtAllocaSinkingPass()";
}

def EdtTransforms : Pass<"edt-transforms", "mlir::ModuleOp"> {
  let summary = "Post-DB EDT transforms (granularity, affinity, critical path)";
  let description = [{
    Hosts EDT-level transformations that run after DB partitioning and
    mode decisions are known. Current placeholder slots:
    - ET-1: Task granularity control
    - ET-2: Data affinity placement
    - ET-6: Critical path analysis
    - EXT-EDT-2: Dead dependency elimination
  }];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createEdtTransformsPass(nullptr)";
}

def StructuredKernelPlan : Pass<"structured-kernel-plan", "mlir::ModuleOp"> {
  let summary = "Classify regular kernels and stamp structured execution plans";
  let description = [{
    Runs StructuredKernelPlanAnalysis on parallel EDTs. For regular kernels
    with recognizable dep patterns (stencil, uniform, wavefront, matmul),
    stamps arts.plan.* attributes encoding the kernel family, iteration
    topology, async strategy, owner dims, block/halo shape, and cost signals.

    Downstream passes (EdtDistribution, ForLowering, DbPartitioning) consult
    these plan attrs when present and fall back to existing generic logic
    when absent.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "scf::SCFDialect",
    "arith::ArithDialect"
  ];
  let constructor = "mlir::arts::createStructuredKernelPlanPass(nullptr)";
}

def Concurrency : Pass<"concurrency", "mlir::ModuleOp"> {
  let summary =
      "Build EDT concurrency graph and apply concurrency-driven cleanups";
  let dependentDialects = ["func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createConcurrencyPass(nullptr)";
}

def PersistentStructuredRegion : Pass<"persistent-structured-region", "mlir::ModuleOp"> {
  let summary = "Mark proven regular epochs for persistent structured region lowering";
  let description = [{
    Evaluates ownership proofs and cost model gates for epochs containing
    structured kernel plans. Epochs that pass both gates are marked with
    arts.persistent_region = true, enabling downstream EpochLowering to
    use a persistent owner-strip or owner-tile execution shape.

    This pass runs before EpochOpt in the epochs pipeline. When the gate
    is not met, the epoch is left unchanged and follows the default
    per-step lowering path.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createPersistentStructuredRegionPass(nullptr)";
}

def CreateEpochs : Pass<"create-epochs", "mlir::ModuleOp"> {
  let summary = "Analyzes the program and creates ARTS epochs";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateEpochsPass()";
}

def ConvertArtsToLLVM : Pass<"convert-arts-to-llvm", "mlir::ModuleOp"> {
  let summary = "Convert ARTS Dialect to LLVM Dialect";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertArtsToLLVMPass()";
  let options = [
    Option<"debug", "debug", "bool", /*default=*/"false", "Enable debug mode.">,
  ];
}

def EdtICM : Pass<"arts-edt-icm", "mlir::ModuleOp"> {
  let summary = "Hoist invariant operations from arts edt regions";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createEdtICMPass()";
}

def DataPtrHoisting
    : Pass<"arts-data-ptr-hoisting", "mlir::ModuleOp"> {
  let summary =
      "Hoist invariant dep/db views and recover loop-local blocked slices";
  let description = [{
    This pass hoists invariant llvm.load operations that extract dependency or
    datablock pointers to before enclosing loops.

    It also recognizes blocked worker loops that already stay inside a single
    owned slice and rewrites them to reuse one hoisted dep view plus the local
    loop IV, instead of rebuilding dep_gep/div/rem selection on every element.

    Without hoisting, these pointer loads happen inside innermost loops
    (O(n^3) loads for 3-nested loops). With hoisting, the pointers are loaded
    once before the loop nest (O(1) loads).

    This is critical for performance because LLVM's LICM cannot hoist these loads
    without TBAA metadata proving they don't alias with data stores.

    Example transformation:
    ```mlir
    scf.for %i = ... {
      scf.for %j = ... {
        % ptr = llvm.load % dep_ptr_addr : !llvm.ptr->!llvm.ptr % val =
                    polygeist.load % ptr[% i, % j] : memref < ? x ? xf64 >
      }
    }
    ```
    becomes:
    ```mlir
    %ptr = llvm.load %dep_ptr_addr : !llvm.ptr -> !llvm.ptr
    scf.for %i = ... {
      scf.for %j = ... {
        % val = polygeist.load % ptr[% i, % j] : memref < ? x ? xf64 >
      }
    }
    ```
  }];
  let dependentDialects =
      ["arts::ArtsDialect", "LLVM::LLVMDialect", "scf::SCFDialect"];
  let constructor = "mlir::arts::createDataPtrHoistingPass()";
}

def GuidRangCallOpt
    : Pass<"arts-guid-range-call-opt", "mlir::ModuleOp"> {
  let summary = "Rewrite per-iteration GUID reserve calls into GUID-range calls";
  let description = [{
    Runs after ARTS-to-LLVM lowering and rewrites canonical loops of the form:

      scf.for %i = 0 to N step 1 {
        %g = call @arts_guid_reserve(...)
        ...
      }

    into:

      %range = call @arts_guid_reserve_range(..., N, ...)
      scf.for %i = 0 to N step 1 {
        %g = call @arts_guid_from_index(%range, %i)
        ...
      }

    This pass is conservative and requires lower-bound 0, step 1,
    and route(0) to preserve runtime semantics.

    For dynamic trip counts it emits a guarded rewrite:
    use range reservation only when upperBound > 1 and
    upperBound <= UINT32_MAX,
    otherwise preserve per-iteration reserve behavior.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arith::ArithDialect", "scf::SCFDialect",
    "LLVM::LLVMDialect"
  ];
  let constructor = "mlir::arts::createGuidRangCallOptPass()";
}

def RuntimeCallOpt
    : Pass<"arts-runtime-call-opt", "mlir::ModuleOp"> {
  let summary = "Hoist and deduplicate selected pure ARTS runtime calls";
  let description = [{
    Runs after ARTS-to-LLVM lowering and optimizes only a conservative set of
    runtime topology/identity queries that are safe to treat as loop-invariant
    within a function:

      - arts_get_total_workers
      - arts_get_total_nodes
      - arts_get_current_worker
      - arts_get_current_node

    The pass intentionally ignores stateful runtime calls (guid reserve, DB
    create, EDT/event signaling, etc.) and only rewrites zero-operand query
    calls.

    Transformations:
      1) Hoist loop-invariant calls out of nested `scf.for` loops.
      2) Deduplicate dominated duplicate calls per callee within a function.
  }];
  let dependentDialects = [
    "func::FuncDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createRuntimeCallOptPass()";
}

def EdtPtrRematerialization
    : Pass<"edt-ptr-rematerialization", "mlir::ModuleOp"> {
  let summary = "Rematerialize pointer dependencies in EDTs.";
  let description =
      [{This pass rematerializes pointer dependencies in event -
        driven tasks(EDTs) to optimize memory usage and data movement.}];
  let constructor = "mlir::arts::createEdtPtrRematerializationPass()";
}

def EpochLowering : Pass<"epoch-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS epochs and propagate epoch GUIDs to EDTs";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEpochLoweringPass()";
}


def ParallelEdtLowering : Pass<"parallel-edt-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS parallel EDTs to task graphs";
  let description = [{This pass transforms parallel EDT operations that contain
                          loop constructs into epoch -
                      wrapped task graphs
                          .It must execute before DB lowering to ensure
                              proper dependency management.}];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createParallelEdtLoweringPass()";
}

def ForOpt : Pass<"arts-for-opt", "mlir::ModuleOp"> {
  let summary = "Apply pre-lowering optimization hints to arts.for";
  let description = [{
    Applies optimization hints to arts.for prior to lowering by writing explicit
    `arts.partition_hint` metadata (block mode + block size). This pass can
    coarsen work for both intra-node and inter-node execution while preserving
    distributed worker utilization constraints for internode mode.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "arith::ArithDialect",
    "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createForOptPass(nullptr)";
}

def DistributedHostLoopOutlining
    : Pass<"distributed-host-loop-outlining", "mlir::ModuleOp"> {
  let summary =
      "Outline eligible host producer loops into distributed ARTS EDTs";
  let description = [{
    Converts eligible host-side scf.for loops (outside existing EDT regions)
    into arts.edt<task><internode> containing arts.for. This is used by
    --distributed-db to lift host producer loops into the regular distributed
    distribution/lowering pipeline.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "arith::ArithDialect",
    "scf::SCFDialect", "memref::MemRefDialect"
  ];
  let constructor =
      "mlir::arts::createDistributedHostLoopOutliningPass(nullptr)";
}

def EdtDistribution : Pass<"edt-distribution", "mlir::ModuleOp"> {
  let summary = "Select and annotate distribution strategy for arts.for loops";
  let description =
      [{Classifies loop access patterns inside parallel EDTs and selects an
            initial distribution strategy,
        recording the decision as attributes consumed by ForLowering.}];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "arith::ArithDialect",
    "scf::SCFDialect", "memref::MemRefDialect"
  ];
  let constructor = "mlir::arts::createEdtDistributionPass(nullptr)";
}

def EdtOrchestrationOpt : Pass<"edt-orchestration-opt", "mlir::ModuleOp"> {
  let summary = "Recognize repeated sibling waves and stamp orchestration contracts";
  let description = [{
    Matches repeated multi-wave timestep loops where several sibling
    arts.edt<parallel> regions inside one enclosing scf.for represent ordered
    waves of the same repeated kernel family. The pass records a canonical
    orchestration contract on the participating EDTs/arts.for ops so
    ForLowering can preserve the group structure instead of lowering each wave
    in isolation.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "arith::ArithDialect",
    "scf::SCFDialect", "memref::MemRefDialect"
  ];
  let constructor = "mlir::arts::createEdtOrchestrationOptPass()";
}

def ForLowering : Pass<"for-lowering", "mlir::ModuleOp"> {
  let summary = "Lower arts.for inside parallel EDTs to chunked task EDTs";
  let description = [{
    Lowers arts.for operations nested within arts.edt<parallel> by creating a
    symbolic worker-id placeholder, computing chunk bounds, acquiring required
    DBs, and generating a task EDT with a local scf.for over the chunk.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createForLoweringPass()";
}

def EdtLowering : Pass<"edt-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS EDTs to LLVM";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtLoweringPass()";
}

def LoweringContractCleanup : Pass<"lowering-contract-cleanup", "mlir::ModuleOp"> {
  let summary = "Erase arts.lowering_contract metadata ops before LLVM lowering";
  let description = [{
    Removes `arts.lowering_contract` operations after all DB/EDT optimization
    passes have consumed them. This keeps semantic contracts available through
    post-db-refinement and late-concurrency-cleanup while ensuring
    pre-lowering/LLVM translation does not see dialect-only metadata ops.
  }];
  let dependentDialects = ["arts::ArtsDialect", "func::FuncDialect"];
  let constructor = "mlir::arts::createLoweringContractCleanupPass()";
}

def LoopFusion : Pass<"arts-loop-fusion", "mlir::ModuleOp"> {
  let summary = "Fuse independent arts.for loops within parallel EDTs";
  let description = [{
    Identifies consecutive arts.for operations within parallel EDTs that:
    1. Have compatible iteration bounds
    2. Access disjoint sets of memory locations (no data dependencies)
    Uses LoopAnalysis for enhanced dependency checking.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect", "omp::OpenMPDialect",
    "affine::AffineDialect"
  ];
  let constructor = "mlir::arts::createLoopFusionPass(nullptr)";
}

def EpochOpt : Pass<"arts-epoch-opt", "mlir::ModuleOp"> {
  let summary = "Unified epoch optimizations (structural + scheduling)";
  let description = [{
    Runs all epoch-related optimizations:

    Structural optimizations:
        1. Epoch scope narrowing -
            split large epochs into smaller independent epochs at positions
                where no EDT after the cut depends on any EDT before the cut
        2. Epoch fusion -
            merge independent consecutive epochs when their DB accesses
                are independent (no shared writes)
        3. Worker loop fusion -
            merge compatible worker loops within epochs when they have identical
                bounds and no loop-carried values

    Scheduling optimizations (gated by --arts-epoch-finish-continuation):
        4. Repetition amortization -
            amortize epoch create/wait overhead for repeated epoch loops by
            wrapping EDT bodies with repeat loops
        5. Finish-EDT continuation -
            outline epoch tail code into continuation EDTs that fire via
            ARTS-native finish-EDT signaling instead of blocking waits
        6. CPS loop driver -
            wrap time-step loops containing epochs in a single outer
            epoch+driver EDT, eliminating repeated main-thread blocking
  }];
  let options = [
    Option<"enableEpochNarrowing", "epoch-narrowing", "bool", "true",
           "Enable epoch scope narrowing">,
    Option<"enableEpochFusion", "epoch-fusion", "bool", "true",
           "Enable epoch fusion">,
    Option<"enableLoopFusion", "loop-fusion", "bool", "true",
           "Enable worker loop fusion">,
    Option<"enableAmortization", "amortization", "bool", "false",
           "Enable repetition amortization for repeated epoch loops">,
    Option<"enableContinuation", "continuation", "bool", "false",
           "Enable finish-EDT continuation scheduling">,
    Option<"enableCPSDriver", "cps-driver", "bool", "false",
           "Enable CPS loop driver transform">,
    Option<"enableCPSChain", "cps-chain", "bool", "false",
           "Enable CPS continuation chain transform">
  ];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect", "scf::SCFDialect",
       "arith::ArithDialect"];
  let constructor = "mlir::arts::createEpochOptPass()";
}

def Hoisting : Pass<"arts-hoisting", "mlir::ModuleOp"> {
  let summary = "Hoist loop-invariant ARTS operations";
  let description = [{
    Unified hoisting for ARTS operations:
    - Hoists read-only db_acquire out of worker loops in epochs when operands
      do not depend on the worker induction variable
    - Hoists db_ref out of inner loops in EDTs when source and indices are
      loop-invariant
  }];
  let options = [
    Option<"enableAcquireHoisting", "acquire", "bool", "true",
           "Hoist db_acquire in epochs">,
    Option<"enableRefHoisting", "ref", "bool", "true", "Hoist db_ref in EDTs">
  ];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "scf::SCFDialect",
    "memref::MemRefDialect"
  ];
  let constructor = "mlir::arts::createHoistingPass()";
}

def BlockLoopStripMining : Pass<"block-stripmine", "mlir::func::FuncOp"> {
  let summary = "Strip-mine block-partitioned loops to remove div/rem overhead";
  let description = [{Matches block-partitioned loop indexing patterns and
                      rewrites them into block-local loops that reuse db_ref.
                      The legacy path handles innermost 1-D div/rem loops,
                      while the neighborhood-aware path can also rewrite
                      enclosing stencil loops that carry block-neighbor
                      indexing state.}];
  let dependentDialects = [
    "func::FuncDialect", "scf::SCFDialect", "arith::ArithDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createBlockLoopStripMiningPass()";
}

///===----------------------------------------------------------------------===///
/// Pattern Pipeline Passes
///===----------------------------------------------------------------------===///
///
/// These passes together form the dedicated pre-DB semantic pattern pipeline
/// in the compiler driver. They teach ARTS about kernel/dependence families
/// before DB creation and before distribution/partitioning lowering.

def LoopNormalization : Pass<"loop-normalization", "mlir::ModuleOp"> {
  let summary = "Pattern-based loop normalization for partitioning";
  let description =
      [{Pattern-pipeline pass. Applies loop normalization patterns to enable downstream
            DbPartitioning.Registered patterns include symmetric -
        triangular normalization(
            converts triangular loops with symmetric writes to rectangular form)
            and others.}];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createLoopNormalizationPass(nullptr)";
}

def DepTransforms : Pass<"arts-dep-transforms", "mlir::ModuleOp"> {
  let summary = "Apply dependence-preserving schedule rewrites before DB creation";
  let description = [{
    Pattern-pipeline pass.
    Groups exact dependence-shape transformations that run on ARTS/scf IR after
    OpenMP-to-ARTS conversion and expose scalable task and DB partitioning
    opportunities without changing program semantics.

    Current patterns:
    - Seidel2DWavefrontPattern
      Rewrites in-place Seidel-style 2D stencils into a tiled weighted
      wavefront schedule that avoids coarse full-range serialization.
    - JacobiAlternatingBuffersPattern
      Detects timestep loops that copy one buffer into another before launching
      a stencil update, and rewrites them into an alternating-buffer schedule.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect", "omp::OpenMPDialect"
  ];
  let constructor = "mlir::arts::createDepTransformsPass()";
}

def StencilBoundaryPeeling : Pass<"stencil-boundary-peeling",
                                  "mlir::ModuleOp"> {
  let summary = "Peel boundary iterations out of stencil inner loops";
  let description = [{
    Pattern-pipeline pass.
    Detects row-boundary plus column-boundary guard patterns inside inner
    stencil loops and rewrites them into:
    - a boundary-row loop that preserves the original boundary behavior
    - an interior-row path with peeled first/last iterations and a branch-free
      interior loop

    This reduces inner-loop control flow and exposes row-invariant stencil
    accesses to later hoisting and partitioning passes.
  }];
  let dependentDialects = [
    "func::FuncDialect", "arts::ArtsDialect", "arith::ArithDialect",
    "scf::SCFDialect", "memref::MemRefDialect"
  ];
  let constructor = "mlir::arts::createStencilBoundaryPeelingPass()";
}

def LoopReordering : Pass<"loop-reordering", "mlir::ModuleOp"> {
  let summary = "Reorder inner loops for cache-optimal access patterns";
  let description = [{
      Pattern-pipeline pass.
      Applies loop reordering transformations based on metadata collected during
          the CollectMetadata phase
              .This pass reads the `reorder_nest_to` field from LoopMetadata(
                  populated by LoopAnalyzer::analyzeLoopReordering)
                  which contains the target order as a sequence of arts.ids.}];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createLoopReorderingPass(nullptr)";
}

def KernelTransforms : Pass<"arts-kernel-transforms", "mlir::ModuleOp"> {
  let summary = "ARTS kernel-form transforms that expose better distribution";
  let description = [{
    Pattern-pipeline pass.
    Applies semantics-preserving kernel-form rewrites that make downstream
    ARTS distribution, partitioning, and locality optimizations more effective.

    This pass is intentionally separate from:
    - DepTransforms: exact schedule/dependence rewrites
    - LoopNormalization: structural cleanup
    - BlockLoopStripMining: late DB-aware strip-mining after partitioning

    Current patterns:
    - ElementwisePipeline
      Fuses consecutive sibling pointwise loops over the same iteration space
      into a single kernel pipeline before DB creation.
    - MatmulReductionPattern
      Rewrites dot-product matmul kernels into update form so later passes can
      tile, distribute, and localize them more effectively.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createKernelTransformsPass(nullptr)";
  let options = [
    Option<"enableElementwisePipeline", "enable-elementwise-pipeline", "bool",
           /*default=*/"true",
           "Enable pointwise pipeline fusion for sibling loops over the same iteration space.">,
    Option<"enableMatmul", "enable-matmul", "bool", /*default=*/"true",
           "Enable reduction distribution for matmul-like patterns.">,
    Option<"enableTiling", "enable-tiling", "bool", /*default=*/"true",
           "Enable inner-loop tiling (when patterns match).">,
    Option<"tileJ", "tile-j", "int64_t", /*default=*/"64",
           "Tile size for the j dimension (matmul-like inner loops).">,
    Option<"minTripCount", "min-trip-count", "int64_t", /*default=*/"128",
           "Minimum constant trip count required to apply tiling.">
  ];
}

def ScalarReplacement : Pass<"arts-scalar-replacement", "mlir::ModuleOp"> {
  let summary = "Transform memory-based reductions to register-based iter_args";
  let description = [{
    This pass transforms memory-based reduction patterns in scf.for loops
    to register-based iter_args patterns to enable LLVM vectorization.
  }];
  let dependentDialects = [
    "scf::SCFDialect", "memref::MemRefDialect", "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createScalarReplacementPass()";
}

def LoopVectorizationHints
    : Pass<"arts-loop-vectorization-hints", "mlir::ModuleOp"> {
  let summary = "Attach LLVM loop vectorization hints to EDT function loops";
  let description = [{
    This pass attaches LLVM loop optimization hints to loop backedges in EDT
    functions. It uses MLIR's native LoopAnnotationAttr, which automatically
    translates to !llvm.loop metadata during LLVM IR emission.
  }];
  let dependentDialects = ["LLVM::LLVMDialect"];
  let constructor = "mlir::arts::createLoopVectorizationHintsPass()";
}

def ContractValidation : Pass<"contract-validation", "mlir::ModuleOp"> {
  let summary = "Validate LoweringContractOp integrity across the module";
  let description = [{
    Lightweight verification pass that walks all arts.lowering_contract
    operations and checks structural invariants:
    - Target value exists and is a valid memref
    - supported_block_halo requires a stencil-family dep_pattern
    - min_offsets/max_offsets must have matching rank
    - block_shape rank > 0 if present
    - owner_dims values are non-negative

    Also warns about DbAcquireOps carrying stencil attributes with no
    corresponding LoweringContractOp on the source pointer (orphans).

    Collects per-pattern statistics and optionally fails on errors.
  }];
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let options = [
    Option<"failOnError", "fail-on-error", "bool", "false",
           "Fail compilation if any contract validation errors are found">,
    Option<"warnOnProofGaps", "warn-on-proof-gaps", "bool", "false",
           "Emit advisory warnings for ownership proof gaps and CPS pack-schema gaps">
  ];
  let constructor = "mlir::arts::createContractValidationPass()";
}

def VerifyMetadata : Pass<"verify-metadata", "mlir::ModuleOp"> {
  let summary = "Verify metadata coverage for loops and memrefs";
  let description = [{
    This pass verifies that all loops and memrefs in the program have associated
    metadata from the CollectMetadata pass. It counts:
    - affine.for, scf.for, scf.parallel operations
    - memref.alloc, memref.alloca operations

    And compares against entries in the MetadataManager. Emits warnings for
    any entities missing metadata, and optionally fails compilation if coverage
    is incomplete.

    This pass is automatically run from the `collect-metadata` stage when
    `carts-compile --diagnose` is enabled so the exported diagnose payload
    includes coverage information for loops and memrefs.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "affine::AffineDialect", "scf::SCFDialect"
  ];
  let options =
      [Option<"failOnMissing", "fail-on-missing", "bool", "false",
              "Fail compilation if any entities are missing metadata">];
  let constructor = "mlir::arts::createVerifyMetadataPass(nullptr)";
}

def VerifyMetadataIntegrity : Pass<"verify-metadata-integrity",
                                   "mlir::ModuleOp"> {
  let summary = "Verify metadata integrity, provenance, and trusted facts";
  let description = [{
    This pass validates semantic metadata invariants on operations that already
    carry ARTS metadata:
    - every metadata-bearing operation has a recognized provenance marker
    - transferred metadata records a source origin id
    - no duplicate active arts.id values exist across metadata-bearing ops
    - loop metadata trip_count matches current static bounds when statically
      knowable

    Unlike verify-metadata, this pass does not check coverage; it checks that
    the metadata that exists is still trustworthy after rewrites.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "affine::AffineDialect", "scf::SCFDialect"
  ];
  let options =
      [Option<"failOnError", "fail-on-error", "bool", "false",
              "Fail compilation if any metadata integrity errors are found">];
  let constructor = "mlir::arts::createVerifyMetadataIntegrityPass(nullptr)";
}

def AliasScopeGen : Pass<"arts-alias-scope-gen", "mlir::ModuleOp"> {
  let summary = "Generate LLVM alias scope metadata for ARTS data pointers";
  let dependentDialects = ["LLVM::LLVMDialect"];
  let constructor = "mlir::arts::createAliasScopeGenPass()";
}

///===----------------------------------------------------------------------===///
/// Verification Passes
///===----------------------------------------------------------------------===///

def VerifySdeLowered : Pass<"verify-sde-lowered", "mlir::ModuleOp"> {
  let summary = "Verify no SDE operations survive past SDE-to-ARTS conversion";
  let dependentDialects = ["arts::sde::ArtsSdeDialect"];
  let constructor = "mlir::arts::createVerifySdeLoweredPass()";
}

def VerifyEdtCreated : Pass<"verify-edt-created", "mlir::ModuleOp"> {
  let summary = "Verify EDT operations exist after OpenMP conversion";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyEdtCreatedPass()";
}

def VerifyEpochCreated : Pass<"verify-epoch-created", "mlir::ModuleOp"> {
  let summary = "Verify epoch operations exist after epoch creation";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyEpochCreatedPass()";
}

def VerifyForLowered : Pass<"verify-for-lowered", "mlir::ModuleOp"> {
  let summary = "Verify all arts.for ops have been lowered";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyForLoweredPass()";
}

def VerifyDbCreated : Pass<"verify-db-created", "mlir::ModuleOp"> {
  let summary = "Verify DB allocations exist after create-dbs";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyDbCreatedPass()";
}

def VerifyPreLowered : Pass<"verify-pre-lowered", "mlir::ModuleOp"> {
  let summary = "Verify pre-lowering removed high-level scheduler ops";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyPreLoweredPass()";
}

def VerifyLowered : Pass<"verify-lowered", "mlir::ModuleOp"> {
  let summary = "Verify no high-level ARTS ops survive past arts-to-llvm";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyLoweredPass()";
}

def VerifyEdtLowered : Pass<"verify-edt-lowered", "mlir::ModuleOp"> {
  let summary = "Verify all EDTs have been lowered";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyEdtLoweredPass()";
}

def VerifyDbLowered : Pass<"verify-db-lowered", "mlir::ModuleOp"> {
  let summary = "Verify all DB operations have been lowered";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyDbLoweredPass()";
}

def VerifyEpochLowered : Pass<"verify-epoch-lowered", "mlir::ModuleOp"> {
  let summary = "Verify all epoch operations have been lowered";
  let dependentDialects = ["arts::ArtsDialect"];
  let constructor = "mlir::arts::createVerifyEpochLoweredPass()";
}

#endif // CARTS_PASSES
