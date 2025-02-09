OVERVIEW: Polygeist modular optimizer driver
Available Dialects: affine, arith, async, builtin, cf, dlti, func, gpu, llvm, math, memref, nvvm, omp, polygeist, scf
USAGE: polygeist-opt [options] <input file>

OPTIONS:

Color Options:

  --color                                                    - Use colors in output (default=autodetect)

General options:

  --abort-on-max-devirt-iterations-reached                   - Abort when the max iterations for devirtualization CGSCC repeat pass is reached
  --allow-unregistered-dialect                               - Allow operation with no registered dialects
  --atomic-counter-update-promoted                           - Do counter update using atomic fetch add  for promoted counters only
  --atomic-first-counter                                     - Use atomic fetch add for first counter in a function (usually the entry counter)
  --barrier-opt                                              - Optimize barriers
  --bounds-checking-single-trap                              - Use one trap block per function
  --cfg-hide-cold-paths=<number>                             - Hide blocks with relative frequency below the given value
  --cfg-hide-deoptimize-paths                                - 
  --cfg-hide-unreachable-paths                               - 
  --cost-kind=<value>                                        - Target cost kind
    =throughput                                              -   Reciprocal throughput
    =latency                                                 -   Instruction latency
    =code-size                                               -   Code size
    =size-latency                                            -   Code size and latency
  --debug-info-correlate                                     - Use debug info to correlate profiles.
  --debugify-func-limit=<ulong>                              - Set max number of processed functions per pass.
  --debugify-level=<value>                                   - Kind of debug info to add
    =locations                                               -   Locations only
    =location+variables                                      -   Locations and Variables
  --debugify-quiet                                           - Suppress verbose debugify output
  --disable-auto-upgrade-debug-info                          - Disable autoupgrade of debug info
  --disable-i2p-p2i-opt                                      - Disables inttoptr/ptrtoint roundtrip optimization
  --do-counter-promotion                                     - Do counter register promotion
  --dot-cfg-mssa=<file name for generated dot file>          - file name for generated dot file
  --dump-pass-pipeline                                       - Print the pipeline that will be run
  --emit-bytecode                                            - Emit bytecode when generating output
  --emit-bytecode-version=<value>                            - Use specified bytecode when generating output
  --enable-buffer-elim                                       - Enable buffer elimination
  --enable-gvn-hoist                                         - Enable the GVN hoisting pass (default = off)
  --enable-gvn-memdep                                        - 
  --enable-gvn-sink                                          - Enable the GVN sinking pass (default = off)
  --enable-load-in-loop-pre                                  - 
  --enable-load-pre                                          - 
  --enable-loop-simplifycfg-term-folding                     - 
  --enable-name-compression                                  - Enable name/filename string compression
  --enable-split-backedge-in-load-pre                        - 
  --experimental-debug-variable-locations                    - Use experimental new value-tracking variable locations
  --force-tail-folding-style=<value>                         - Force the tail folding style
    =none                                                    -   Disable tail folding
    =data                                                    -   Create lane mask for data only, using active.lane.mask intrinsic
    =data-without-lane-mask                                  -   Create lane mask with compare/stepvector
    =data-and-control                                        -   Create lane mask using active.lane.mask intrinsic, and use it for both data and control flow
    =data-and-control-without-rt-check                       -   Similar to data-and-control, but remove the runtime check
  --fs-profile-debug-bw-threshold=<uint>                     - Only show debug message if the source branch weight is greater  than this value.
  --fs-profile-debug-prob-diff-threshold=<uint>              - Only show debug message if the branch probility is greater than this value (in percentage).
  --generate-merged-base-profiles                            - When generating nested context-sensitive profiles, always generate extra base profile for function with all its context profiles merged into it.
  --gpu-kernel-emit-coarsened-alternatives                   - Emit alternative kernels with coarsened threads
  --gpu-kernel-enable-block-coarsening                       - When emitting coarsened kernels, enable block coarsening
  --gpu-kernel-enable-coalescing-friendly-unroll             - When thread coarsening, do coalescing-friendly unrolling
  --hash-based-counter-split                                 - Rename counter variable of a comdat function based on cfg hash
  --hot-cold-split                                           - Enable hot-cold splitting pass
  --import-all-index                                         - Import all external functions in index.
  --instcombine-code-sinking                                 - Enable code sinking
  --instcombine-guard-widening-window=<uint>                 - How wide an instruction window to bypass looking for another guard
  --instcombine-max-num-phis=<uint>                          - Maximum number phis to handle in intptr/ptrint folding
  --instcombine-max-sink-users=<uint>                        - Maximum number of undroppable users for instruction sinking
  --instcombine-maxarray-size=<uint>                         - Maximum array size considered when doing a combine
  --instcombine-negator-enabled                              - Should we attempt to sink negations?
  --instcombine-negator-max-depth=<uint>                     - What is the maximal lookup depth when trying to check for viability of negation sinking.
  --instrprof-atomic-counter-update-all                      - Make all profile counter updates atomic (for testing only)
  --internalize-public-api-file=<filename>                   - A file containing list of symbol names to preserve
  --internalize-public-api-list=<list>                       - A list of symbol names to preserve
  --irdl-file=<filename>                                     - IRDL file to register before processing the input
  --iterative-counter-promotion                              - Allow counter promotion across the whole loop nest.
  --load-dialect-plugin=<string>                             - Load dialects from plugin library
  --load-pass-plugin=<string>                                - Load passes from plugin library
  --log-actions-to=<string>                                  - Log action execution to a file, or stderr if  '-' is passed
  --log-mlir-actions-filter=<string>                         - Comma separated list of locations to filter actions from logging
  --matrix-default-layout=<value>                            - Sets the default matrix layout
    =column-major                                            -   Use column-major layout
    =row-major                                               -   Use row-major layout
  --matrix-print-after-transpose-opt                         - 
  --max-counter-promotions=<int>                             - Max number of allowed counter promotions
  --max-counter-promotions-per-loop=<uint>                   - Max number counter promotions per loop to avoid increasing register pressure too much
  --mir-strip-debugify-only                                  - Should mir-strip-debug only strip debug info from debugified modules by default
  --misexpect-tolerance=<uint>                               - Prevents emiting diagnostics when profile counts are within N% of the threshold..
  --mlir-debug-counter=<string>                              - Comma separated list of debug counter skip and count arguments
  --mlir-disable-threading                                   - Disable multi-threading within MLIR, overrides any further call to MLIRContext::enableMultiThreading()
  --mlir-elide-elementsattrs-if-larger=<uint>                - Elide ElementsAttrs with "..." that have more elements than the given upper limit
  --mlir-elide-resource-strings-if-larger=<uint>             - Elide printing value of resources if string is too long in chars.
  --mlir-enable-debugger-hook                                - Enable Debugger hook for debugging MLIR Actions
  --mlir-pass-pipeline-crash-reproducer=<string>             - Generate a .mlir reproducer file at the given output path if the pass manager crashes or fails
  --mlir-pass-pipeline-local-reproducer                      - When generating a crash reproducer, attempt to generated a reproducer with the smallest pipeline.
  --mlir-pass-statistics                                     - Display the statistics of each pass
  --mlir-pass-statistics-display=<value>                     - Display method for pass statistics
    =list                                                    -   display the results in a merged list sorted by pass name
    =pipeline                                                -   display the results with a nested pipeline view
  --mlir-pretty-debuginfo                                    - Print pretty debug info in MLIR output
  --mlir-print-debug-counter                                 - Print out debug counter information after all counters have been accumulated
  --mlir-print-debuginfo                                     - Print debug info in MLIR output
  --mlir-print-elementsattrs-with-hex-if-larger=<long>       - Print DenseElementsAttrs with a hex string that have more elements than the given upper limit (use -1 to disable)
  --mlir-print-ir-after=<pass-arg>                           - Print IR after specified passes
  --mlir-print-ir-after-all                                  - Print IR after each pass
  --mlir-print-ir-after-change                               - When printing the IR after a pass, only print if the IR changed
  --mlir-print-ir-after-failure                              - When printing the IR after a pass, only print if the pass failed
  --mlir-print-ir-before=<pass-arg>                          - Print IR before specified passes
  --mlir-print-ir-before-all                                 - Print IR before each pass
  --mlir-print-ir-module-scope                               - When printing IR for print-ir-[before|after]{-all} always print the top-level operation
  --mlir-print-local-scope                                   - Print with local scope and inline information (eliding aliases for attributes, types, and locations
  --mlir-print-op-on-diagnostic                              - When a diagnostic is emitted on an operation, also print the operation as an attached note
  --mlir-print-stacktrace-on-diagnostic                      - When a diagnostic is emitted, also print the stack trace as an attached note
  --mlir-print-value-users                                   - Print users of operation results and block arguments as a comment
  --mlir-timing                                              - Display execution times
  --mlir-timing-display=<value>                              - Display method for timing data
    =list                                                    -   display the results in a list sorted by total time
    =tree                                                    -   display the results ina with a nested tree view
  --no-discriminators                                        - Disable generation of discriminator information.
  --no-implicit-module                                       - Disable implicit addition of a top-level module op during parsing
  -o <filename>                                              - Output filename
  --object-size-offset-visitor-max-visit-instructions=<uint> - Maximum number of instructions for ObjectSizeOffsetVisitor to look at
  --pass-pipeline=<string>                                   - Textual description of the pass pipeline to run
  --pgo-block-coverage                                       - Use this option to enable basic block coverage instrumentation
  --pgo-temporal-instrumentation                             - Use this option to enable temporal instrumentation
  --pgo-view-block-coverage-graph                            - Create a dot file of CFGs with block coverage inference information
  --poison-checking-function-local                           - Check that returns are non-poison (for testing)
  --polygeist-alternatives-mode=<value>                      - Polygeist alternatives op mode
    =static                                                  -   Pick at compile time
    =pgo_prof                                                -   Profile Guided Optimization - profiling mode
    =pgo_opt                                                 -   Profile Guided Optimization - optimization mode
  --print-pipeline-passes                                    - Print a '-passes' compatible string describing the pipeline (best-effort only).
  Compiler passes to run
    Passes:
      --affine-cfg                                           -   Replace scf.if and similar with affine.if
      --affine-data-copy-generate                            -   Generate explicit copying for affine memory operations
        --fast-mem-capacity=<ulong>                          - Set fast memory space capacity in KiB (default: unlimited)
        --fast-mem-space=<uint>                              - Fast memory space identifier for copy generation (default: 1)
        --generate-dma                                       - Generate DMA instead of point-wise copy
        --min-dma-transfer=<int>                             - Minimum DMA transfer size supported by the target in bytes
        --skip-non-unit-stride-loops                         - Testing purposes: avoid non-unit stride loop choice depths for copy placement
        --slow-mem-space=<uint>                              - Slow memory space identifier for copy generation (default: 0)
        --tag-mem-space=<uint>                               - Tag memory space identifier for copy generation (default: 0)
      --affine-expand-index-ops                              -   Lower affine operations operating on indices into more fundamental operations
      --affine-loop-coalescing                               -   Coalesce nested loops with independent bounds into a single loop
      --affine-loop-fusion                                   -   Fuse affine loop nests
        --fusion-compute-tolerance=<number>                  - Fractional increase in additional computation tolerated while fusing
        --fusion-fast-mem-space=<uint>                       - Faster memory space number to promote fusion buffers to
        --fusion-local-buf-threshold=<ulong>                 - Threshold size (KiB) for promoting local buffers to fast memory space
        --fusion-maximal                                     - Enables maximal loop fusion
        --mode=<value>                                       - fusion mode to attempt
    =greedy                                            -   Perform greedy (both producer-consumer and sibling)  fusion
    =producer                                          -   Perform only producer-consumer fusion
    =sibling                                           -   Perform only sibling fusion
      --affine-loop-invariant-code-motion                    -   Hoist loop invariant instructions outside of affine loops
      --affine-loop-normalize                                -   Apply normalization transformations to affine loop-like ops
        --promote-single-iter                                - Promote single iteration loops
      --affine-loop-tile                                     -   Tile affine loop nests
        --cache-size=<ulong>                                 - Set size of cache to tile for in KiB (default: 512)
        --separate                                           - Separate full and partial tiles (default: false)
        --tile-size=<uint>                                   - Use this tile size for all loops
        --tile-sizes=<uint>                                  - List of tile sizes for each perfect nest (overridden by -tile-size)
      --affine-loop-unroll                                   -   Unroll affine loops
        --cleanup-unroll                                     - Fully unroll the cleanup loop when possible.
        --unroll-factor=<uint>                               - Use this unroll factor for all loops being unrolled
        --unroll-full                                        - Fully unroll loops
        --unroll-full-threshold=<uint>                       - Unroll all loops with trip count less than or equal to this
        --unroll-num-reps=<uint>                             - Unroll innermost loops repeatedly this many times
        --unroll-up-to-factor                                - Allow unrolling up to the factor specified
      --affine-loop-unroll-jam                               -   Unroll and jam affine loops
        --unroll-jam-factor=<uint>                           - Use this unroll jam factor for all loops (default 4)
      --affine-parallelize                                   -   Convert affine.for ops into 1-D affine.parallel
        --max-nested=<uint>                                  - Maximum number of nested parallel loops to produce. Defaults to unlimited (UINT_MAX).
        --parallel-reductions                                - Whether to parallelize reduction loops. Defaults to false.
      --affine-pipeline-data-transfer                        -   Pipeline non-blocking data transfers between explicitly managed levels of the memory hierarchy
      --affine-scalrep                                       -   Replace affine memref accesses by scalars by forwarding stores to loads and eliminating redundant loads
      --affine-simplify-structures                           -   Simplify affine expressions in maps/sets and normalize memrefs
      --affine-super-vectorize                               -   Vectorize to a target independent n-D vector abstraction
        --test-fastest-varying=<long>                        - Specify a 1-D, 2-D or 3-D pattern of fastest varying memory dimensions to match. See defaultPatterns in Vectorize.cpp for a description and examples. This is used for testing purposes
        --vectorize-reductions                               - Vectorize known reductions expressed via iter_args. Switched off by default.
        --virtual-vector-size=<long>                         - Specify an n-D virtual vector size for vectorization
      --barrier-removal-continuation                         -   Remove scf.barrier using continuations
      --canonicalize                                         -   Canonicalize operations
        --disable-patterns=<string>                          - Labels of patterns that should be filtered out during application
        --enable-patterns=<string>                           - Labels of patterns that should be used during application, all other patterns are filtered out
        --max-iterations=<long>                              - Max. iterations between applying patterns / simplifying regions
        --max-num-rewrites=<long>                            - Max. number of pattern rewrites within an iteration
        --region-simplify                                    - Perform control flow optimizations to the region tree
        --test-convergence                                   - Test only: Fail pass on non-convergence to detect cyclic pattern
        --top-down                                           - Seed the worklist in general top-down order
      --canonicalize-polygeist                               -   
        --disable-patterns=<string>                          - Labels of patterns that should be filtered out during application
        --enable-patterns=<string>                           - Labels of patterns that should be used during application, all other patterns are filtered out
        --max-iterations=<long>                              - Max. iterations between applying patterns / simplifying regions
        --max-num-rewrites=<long>                            - Max. number of pattern rewrites within an iteration
        --region-simplify                                    - Perform control flow optimizations to the region tree
        --test-convergence                                   - Test only: Fail pass on non-convergence to detect cyclic pattern
        --top-down                                           - Seed the worklist in general top-down order
      --canonicalize-scf-for                                 -   Run some additional canonicalization for scf::for
      --collect-kernel-statistics                            -   Lower cudart functions to cpu versions
      --convert-cudart-to-cpu                                -   Lower cudart functions to cpu versions
      --convert-cudart-to-gpu                                -   Lower cudart functions to generic gpu versions
      --convert-parallel-to-gpu1                             -   Convert parallel loops to gpu
        --arch=<string>                                      - Target GPU architecture
      --convert-parallel-to-gpu2                             -   Convert parallel loops to gpu
      --convert-polygeist-to-llvm                            -   Convert scalar and vector operations from the Standard to the LLVM dialect
        --data-layout=<string>                               - String description (LLVM format) of the data layout that is expected on the produced module
        --index-bitwidth=<uint>                              - Bitwidth of the index type, 0 to use size of machine word
        --use-bare-ptr-memref-call-conv                      - Replace FuncOp's MemRef arguments with bare pointers to the MemRef element types
        --use-c-style-memref                                 - Use C-style nested-array lowering of memref instead of the default MLIR descriptor structure
      --convert-scf-to-openmp                                -   Convert SCF parallel loop to OpenMP parallel + workshare constructs.
        --use-opaque-pointers                                - Generate LLVM IR using opaque pointers instead of typed pointers
      --convert-to-opaque-ptr                                -   Convert typed llvm pointers to opaque
      --cpuify                                               -   remove scf.barrier
        --method=<string>                                    - Method of doing distribution
      --cse                                                  -   Eliminate common sub-expressions
      --detect-reduction                                     -   Detect reductions in affine.for
      --fix-gpu-func                                         -   Fix nested calls to gpu functions we generate in the frontend
      --for-break-to-while                                   -   Rewrite scf.for(scf.if) to scf.while
      --inline                                               -   Inline function calls
        --default-pipeline=<string>                          - The default optimizer pipeline used for callables
        --max-iterations=<uint>                              - Maximum number of iterations when inlining within an SCC
        --op-pipelines=<pass-manager>                        - Callable operation specific optimizer pipelines (in the form of `dialect.op(pipeline)`)
      --inner-serialize                                      -   remove scf.barrier
      --loop-invariant-code-motion                           -   Hoist loop invariant instructions outside of the loop
      --loop-restructure                                     -   
      --lower-affine                                         -   Lower Affine operations to a combination of Standard and SCF operations
      --lower-alternatives                                   -   Lower alternatives if in opt mode
      --merge-gpu-modules                                    -   Merge all gpu modules into one
      --openmp-opt                                           -   Optimize OpenMP
      --parallel-licm                                        -   Perform LICM on known parallel (and serial) loops
      --parallel-lower                                       -   Lower gpu launch op to parallel ops
      --polygeist-mem2reg                                    -   Replace scf.if and similar with affine.if
      --polyhedral-opt                                       -   Optimize affine regions with pluto
      --raise-scf-to-affine                                  -   Raise SCF to affine
      --sccp                                                 -   Sparse Conditional Constant Propagation
      --scf-parallel-loop-unroll                             -   Unroll and interleave scf parallel loops
        --unrollFactor=<int>                                 - Unroll factor
      --serialize                                            -   remove scf.barrier
      --symbol-dce                                           -   Eliminate dead symbols
      --trivialuse                                           -   
  --run-reproducer                                           - Run the pipeline stored in the reproducer
  --runtime-counter-relocation                               - Enable relocating counters at runtime.
  --safepoint-ir-verifier-print-only                         - 
  --sample-profile-check-record-coverage=<N>                 - Emit a warning if less than N% of records in the input profile are matched to the IR.
  --sample-profile-check-sample-coverage=<N>                 - Emit a warning if less than N% of samples in the input profile are matched to the IR.
  --sample-profile-max-propagate-iterations=<uint>           - Maximum number of iterations to go through when propagating sample block/edge weights through the CFG.
  --show-dialects                                            - Print the list of registered dialects and exit
  --skip-ret-exit-block                                      - Suppress counter promotion if exit blocks contain ret.
  --speculative-counter-promotion-max-exiting=<uint>         - The max number of exiting blocks of a loop to allow  speculative counter promotion
  --speculative-counter-promotion-to-loop                    - When the option is false, if the target block is in a loop, the promotion will be disallowed unless the promoted counter  update can be further/iteratively promoted into an acyclic  region.
  --split-input-file                                         - Split the input file into pieces and process each chunk independently
  --summary-file=<string>                                    - The summary file to use for function importing.
  --type-based-intrinsic-cost                                - Calculate intrinsics cost based only on argument types
  --verify-diagnostics                                       - Check that emitted diagnostics match expected-* lines on the corresponding line
  --verify-each                                              - Run the verifier after each transformation pass
  --verify-region-info                                       - Verify region info (time consuming)
  --verify-roundtrip                                         - Round-trip the IR after parsing and ensure it succeeds
  --vp-counters-per-site=<number>                            - The average number of profile counters allocated per value profiling site.
  --vp-static-alloc                                          - Do static counter allocation for value profiler

Generic Options:

  --help                                                     - Display available options (--help-hidden for more)
  --help-list                                                - Display list of available options (--help-list-hidden for more)
  --version                                                  - Display the version of this program
