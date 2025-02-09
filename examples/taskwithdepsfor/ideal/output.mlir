USAGE: cgeist [options] <Specify input file>

OPTIONS:

Color Options:

  --color                                                    - Use colors in output (default=autodetect)

General options:

  --O0                                                       - Opt level 0
  --O1                                                       - Opt level 1
  --O2                                                       - Opt level 2
  --O3                                                       - Opt level 3
  -S                                                         - Emit Assembly
  --abort-on-max-devirt-iterations-reached                   - Abort when the max iterations for devirtualization CGSCC repeat pass is reached
  --allow-ginsert-as-artifact                                - Allow G_INSERT to be considered an artifact. Hack around AMDGPU test infinite loops.
  --amd-gpu-arch=<string>                                    - AMD GPU arch
  --atomic-counter-update-promoted                           - Do counter update using atomic fetch add  for promoted counters only
  --atomic-first-counter                                     - Use atomic fetch add for first counter in a function (usually the entry counter)
  --barrier-opt                                              - Optimize barriers
  --bounds-checking-single-trap                              - Use one trap block per function
  --c-style-memref                                           - Use c style memrefs when possible
  --canonicalizeiters=<int>                                  - Number of canonicalization iterations
  --cfg-hide-cold-paths=<number>                             - Hide blocks with relative frequency below the given value
  --cfg-hide-deoptimize-paths                                - 
  --cfg-hide-unreachable-paths                               - 
  --cost-kind=<value>                                        - Target cost kind
    =throughput                                              -   Reciprocal throughput
    =latency                                                 -   Instruction latency
    =code-size                                               -   Code size
    =size-latency                                            -   Code size and latency
  --cpuify=<string>                                          - Convert to cpu
  --cuda-gpu-arch=<string>                                   - CUDA GPU arch
  --cuda-lower                                               - Add parallel loops around cuda
  --cuda-path=<string>                                       - CUDA Path
  --debug-info-correlate                                     - Use debug info to correlate profiles.
  --debugify-func-limit=<ulong>                              - Set max number of processed functions per pass.
  --debugify-level=<value>                                   - Kind of debug info to add
    =locations                                               -   Locations only
    =location+variables                                      -   Locations and Variables
  --debugify-quiet                                           - Suppress verbose debugify output
  --detect-reduction                                         - Detect reduction in inner most loop
  --device-opt-level=<int>                                   - Optimization level for ptxas
  --disable-auto-upgrade-debug-info                          - Disable autoupgrade of debug info
  --disable-i2p-p2i-opt                                      - Disables inttoptr/ptrtoint roundtrip optimization
  --do-counter-promotion                                     - Do counter register promotion
  --dot-cfg-mssa=<file name for generated dot file>          - file name for generated dot file
  --early-inner-serialize                                    - Perform early inner serialization
  --early-verifier                                           - Enable verifier ASAP
  --emit-cuda                                                - Emit CUDA code
  --emit-gpu-kernel-launch-bounds                            - Emit GPU kernel launch bounds where possible
  --emit-llvm                                                - Emit llvm
  --emit-llvm-dialect                                        - Emit LLVM Dialect
  --emit-openmpir                                            - Emit OpenMP IR
  --emit-rocm                                                - Emit ROCM code
  --enable-buffer-elim                                       - Enable buffer elimination
  --enable-cse-in-irtranslator                               - Should enable CSE in irtranslator
  --enable-cse-in-legalizer                                  - Should enable CSE in Legalizer
  --enable-gvn-hoist                                         - Enable the GVN hoisting pass (default = off)
  --enable-gvn-memdep                                        - 
  --enable-gvn-sink                                          - Enable the GVN sinking pass (default = off)
  --enable-load-in-loop-pre                                  - 
  --enable-load-pre                                          - 
  --enable-loop-simplifycfg-term-folding                     - 
  --enable-name-compression                                  - Enable name/filename string compression
  --enable-split-backedge-in-load-pre                        - 
  --experimental-debug-variable-locations                    - Use experimental new value-tracking variable locations
  --fopenmp                                                  - Enable OpenMP
  --force-tail-folding-style=<value>                         - Force the tail folding style
    =none                                                    -   Disable tail folding
    =data                                                    -   Create lane mask for data only, using active.lane.mask intrinsic
    =data-without-lane-mask                                  -   Create lane mask with compare/stepvector
    =data-and-control                                        -   Create lane mask using active.lane.mask intrinsic, and use it for both data and control flow
    =data-and-control-without-rt-check                       -   Similar to data-and-control, but remove the runtime check
  --fs-profile-debug-bw-threshold=<uint>                     - Only show debug message if the source branch weight is greater  than this value.
  --fs-profile-debug-prob-diff-threshold=<uint>              - Only show debug message if the branch probility is greater than this value (in percentage).
  --generate-merged-base-profiles                            - When generating nested context-sensitive profiles, always generate extra base profile for function with all its context profiles merged into it.
  --gpu-kernel-structure-mode=<value>                        - How to hangle the original gpu kernel parallel structure
    =discard                                                 -   Discard the structure
    =block_thread_wrappers                                   -   Wrap blocks and thread in operations
    =thread_noop                                             -   Put noop in the thread
    =block_thread_noops                                      -   Put noops in the block and thread
  --hash-based-counter-split                                 - Rename counter variable of a comdat function based on cfg hash
  --hot-cold-split                                           - Enable hot-cold splitting pass
  --immediate                                                - Emit immediate mlir
  --import-all-index                                         - Import all external functions in index.
  --inner-serialize                                          - Turn on parallel licm
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
  --iterative-counter-promotion                              - Allow counter promotion across the whole loop nest.
  --lto-embed-bitcode=<value>                                - Embed LLVM bitcode in object files produced by LTO
    =none                                                    -   Do not embed
    =optimized                                               -   Embed after all optimization passes
    =post-merge-pre-opt                                      -   Embed post merge, but before optimizations
  --march=<string>                                           - Architecture
  --matrix-default-layout=<value>                            - Sets the default matrix layout
    =column-major                                            -   Use column-major layout
    =row-major                                               -   Use row-major layout
  --matrix-print-after-transpose-opt                         - 
  --max-counter-promotions=<int>                             - Max number of allowed counter promotions
  --max-counter-promotions-per-loop=<uint>                   - Max number counter promotions per loop to avoid increasing register pressure too much
  --memref-abi                                               - Use memrefs when possible
  --memref-fullrank                                          - Get the full rank of the memref.
  --mir-strip-debugify-only                                  - Should mir-strip-debug only strip debug info from debugified modules by default
  --misexpect-tolerance=<uint>                               - Prevents emiting diagnostics when profile counts are within N% of the threshold..
  --no-discriminators                                        - Disable generation of discriminator information.
  --nocudainc                                                - Do not include CUDA headers
  --nocudalib                                                - Do not link CUDA libdevice
  -o <string>                                                - Output file
  --object-size-offset-visitor-max-visit-instructions=<uint> - Maximum number of instructions for ObjectSizeOffsetVisitor to look at
  --openmp-opt                                               - Turn on openmp opt
  --output-intermediate-gpu                                  - Output intermediate gpu code
  --parallel-licm                                            - Turn on parallel licm
  --pgo-block-coverage                                       - Use this option to enable basic block coverage instrumentation
  --pgo-temporal-instrumentation                             - Use this option to enable temporal instrumentation
  --pgo-view-block-coverage-graph                            - Create a dot file of CFGs with block coverage inference information
  --pm-enable-printing                                       - Enable printing of IR before and after all passes
  --poison-checking-function-local                           - Check that returns are non-poison (for testing)
  --polygeist-alternatives-mode=<value>                      - Polygeist alternatives op mode
    =static                                                  -   Pick at compile time
    =pgo_prof                                                -   Profile Guided Optimization - profiling mode
    =pgo_opt                                                 -   Profile Guided Optimization - optimization mode
  --polyhedral-opt                                           - Use polymer to optimize affine regions
  --prefix-abi=<string>                                      - Prefix for emitted symbols
  --print-debug-info                                         - Print debug info from MLIR
  --print-pipeline-passes                                    - Print a '-passes' compatible string describing the pipeline (best-effort only).
  --raise-scf-to-affine                                      - Raise SCF to Affine
  --resource-dir=<string>                                    - Resource-dir
  --rocm-path=<string>                                       - ROCM Path
  --runtime-counter-relocation                               - Enable relocating counters at runtime.
  --safepoint-ir-verifier-print-only                         - 
  --sample-profile-check-record-coverage=<N>                 - Emit a warning if less than N% of records in the input profile are matched to the IR.
  --sample-profile-check-sample-coverage=<N>                 - Emit a warning if less than N% of samples in the input profile are matched to the IR.
  --sample-profile-max-propagate-iterations=<uint>           - Maximum number of iterations to go through when propagating sample block/edge weights through the CFG.
  --sanitizer-early-opt-ep                                   - Insert sanitizers on OptimizerEarlyEP.
  --scal-rep                                                 - Raise SCF to Affine
  --scf-openmp                                               - Emit llvm
  --show-ast                                                 - Show AST
  --skip-ret-exit-block                                      - Suppress counter promotion if exit blocks contain ret.
  --speculative-counter-promotion-max-exiting=<uint>         - The max number of exiting blocks of a loop to allow  speculative counter promotion
  --speculative-counter-promotion-to-loop                    - When the option is false, if the target block is in a loop, the promotion will be disallowed unless the promoted counter  update can be further/iteratively promoted into an acyclic  region.
  --std=<string>                                             - C/C++ std
  --struct-abi                                               - Use literal LLVM ABI for structs
  --summary-file=<string>                                    - The summary file to use for function importing.
  --sysroot=<string>                                         - sysroot
  --thinlto-assume-merged                                    - Assume the input has already undergone ThinLTO function importing and the other pre-optimization pipeline changes.
  --type-based-intrinsic-cost                                - Calculate intrinsics cost based only on argument types
  --unroll-loops                                             - Unroll Affine Loops
  --use-original-gpu-block-size                              - Try not to alter the GPU kernel block sizes originally used in the code
  -v                                                         - Verbose
  --verify-legalizer-debug-locs=<value>                      - Verify that debug locations are handled
    =none                                                    -   No verification
    =legalizations                                           -   Verify legalizations
    =legalizations+artifactcombiners                         -   Verify legalizations and artifact combines
  --verify-region-info                                       - Verify region info (time consuming)
  --vp-counters-per-site=<number>                            - The average number of profile counters allocated per value profiling site.
  --vp-static-alloc                                          - Do static counter allocation for value profiler
  -x <string>                                                - Treat input as language
  --x86-align-branch=<string>                                - Specify types of branches to align (plus separated list of types):
                                                               jcc      indicates conditional jumps
                                                               fused    indicates fused conditional jumps
                                                               jmp      indicates direct unconditional jumps
                                                               call     indicates direct and indirect calls
                                                               ret      indicates rets
                                                               indirect indicates indirect unconditional jumps
  --x86-align-branch-boundary=<uint>                         - Control how the assembler should align branches with NOP. If the boundary's size is not 0, it should be a power of 2 and no less than 32. Branches will be aligned to prevent from being across or against the boundary of specified size. The default value 0 does not align branches.
  --x86-branches-within-32B-boundaries                       - Align selected instructions to mitigate negative performance impact of Intel's micro code update for errata skx102.  May break assumptions about labels corresponding to particular instructions, and should be used with caution.
  --x86-pad-max-prefix-size=<uint>                           - Maximum number of prefixes to use for padding

Generic Options:

  --help                                                     - Display available options (--help-hidden for more)
  --help-list                                                - Display list of available options (--help-list-hidden for more)
  --version                                                  - Display the version of this program

clang to mlir - tool options:

  -D <string>                                                - defines
  -I <string>                                                - include search path
  --function=<string>                                        - <Specify function>
  --inbounds-gep                                             - Use inbounds GEP operations
  --include=<string>                                         - includes
  --mcpu=<string>                                            - Target CPU
  --target=<string>                                          - Target triple
