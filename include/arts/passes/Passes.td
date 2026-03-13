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

def CanonicalizeMemrefs : Pass<"canonicalize-memrefs", "mlir::ModuleOp"> {
  let summary = "Canonicalize memref allocations and accesses";
  let description = [{
    This pass canonicalizes memref allocations and accesses for downstream passes.
    It converts nested array-of-pointer allocations to multi-dimensional memrefs and
    rewrites pointer-based accesses to direct memref indexed accesses.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCanonicalizeMemrefsPass()";
}

def ConvertOpenMPToArts : Pass<"convert-openmp-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP functions to ARTS";
  let dependentDialects = [
    "memref::MemRefDialect", "omp::OpenMPDialect", "LLVM::LLVMDialect",
    "arts::ArtsDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertOpenMPtoArtsPass()";
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

def Db : Pass<"db", "mlir::ModuleOp"> {
  let summary = "Adjust DB modes based on access patterns";
  let description =
      [{Adjusts the mode(in / out / inout)
            of DB allocations and acquires based on the actual load /
        store operations performed on them.}];
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createDbPass(nullptr)";
}

def DbOpts : Pass<"db-opts", "mlir::ModuleOp"> {
  let summary = "Apply DB-local cleanup optimizations";
  let description = [{
    Performs DB-specific cleanups after mode and partition decisions are known.
    Current optimizations:
    - Replace task-private single-element scratch DBs with local allocas
  }];
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createDbOptsPass()";
}

def DbPartitioning : Pass<"db-partitioning", "mlir::ModuleOp"> {
  let summary = "Partition DBs for fine-grained parallel access";
  let description = [{
    Transforms coarse-grained DB allocations to fine-grained (chunked)
    allocations for better parallelism. Also handles:
    - Stencil bounds analysis and runtime bounds checking

    Partition Fallback (--partition-fallback):
    - coarse (default): Non-affine accesses block partitioning, use coarse allocation
    - fine: Non-affine accesses use element-wise (fine-grained) partitioning
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createDbPartitioningPass(nullptr)";
}

def DistributedDbOwnership
    : Pass<"distributed-db-ownership", "mlir::ModuleOp"> {
  let summary = "Mark eligible DB allocations for distributed ownership";
  let description = [{
    Selects host-level DB allocations that are safe to distribute across
    nodes and annotates them with the `distributed` attribute for lowering.
  }];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect", "memref::MemRefDialect"];
  let constructor = "mlir::arts::createDistributedDbOwnershipPass(nullptr)";
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

def Edt : Pass<"edt", "mlir::ModuleOp"> {
  let summary = "Optimize EDTs";
  let dependentDialects = [
    "memref::MemRefDialect", "LLVM::LLVMDialect", "func::FuncDialect",
    "arts::ArtsDialect", "arith::ArithDialect", "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtPass(nullptr, true)";
  let options = [Option<"runAnalysis", "run-analysis", "bool",
                        /*default=*/"true", "Enable EDT analysis.">];
}

def EdtAllocaSinking : Pass<"edt-alloca-sinking", "mlir::ModuleOp"> {
  let summary = "Clone external stack allocas into EDT regions";
  let dependentDialects =
      ["memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createEdtAllocaSinkingPass()";
}

def Concurrency : Pass<"concurrency", "mlir::ModuleOp"> {
  let summary =
      "Build EDT concurrency graph and apply concurrency-driven cleanups";
  let dependentDialects = ["func::FuncDialect", "arts::ArtsDialect"];
  let constructor = "mlir::arts::createConcurrencyPass(nullptr)";
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

def ArtsDataPtrHoisting
    : Pass<"arts-data-ptr-hoisting", "mlir::ModuleOp"> {
  let summary = "Hoist data pointer loads from deps struct out of loops";
  let description = [{
    This pass hoists llvm.load operations that extract data pointers from the
    ARTS deps struct (artsEdtDep_t.ptr field) to before enclosing loops.

    The deps struct contains pointers to data arrays. Without hoisting, these
    pointer loads happen inside innermost loops (O(n^3) loads for 3-nested loops).
    With hoisting, the pointers are loaded once before the loop nest (O(1) loads).

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

def ArtsLoopFusion : Pass<"arts-loop-fusion", "mlir::ModuleOp"> {
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

def ArtsEpochOpt : Pass<"arts-epoch-opt", "mlir::ModuleOp"> {
  let summary = "Combined epoch optimizations (fusion + loop fusion)";
  let description = [{
    Runs epoch - related
    optimizations:
        1. Epoch fusion -
            merge independent consecutive epochs when their DB accesses
                are independent(no shared writes) 2. Worker loop fusion -
            merge compatible worker loops within epochs when they have identical
                bounds and no loop -
            carried values
  }];
  let options = [
    Option<"enableEpochFusion", "epoch-fusion", "bool", "true",
           "Enable epoch fusion">,
    Option<"enableLoopFusion", "loop-fusion", "bool", "true",
           "Enable worker loop fusion">
  ];
  let dependentDialects =
      ["func::FuncDialect", "arts::ArtsDialect", "scf::SCFDialect"];
  let constructor = "mlir::arts::createEpochOptPass()";
}

def ArtsHoisting : Pass<"arts-hoisting", "mlir::ModuleOp"> {
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
  let summary = "Strip-mine innermost loops with block-partitioned indexing";
  let description = [{Matches innermost loops that use div /
                          rem by a constant block size and block -
                      partitioned db_ref patterns.Rewrites them into a block
                          loop and local index loop to remove per -
                      iteration div / rem overhead and reuse db_ref.}];
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

    This pass is automatically run when --diagnose is enabled to ensure the
    diagnose output contains complete information for all entities.
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

#endif // CARTS_PASSES
