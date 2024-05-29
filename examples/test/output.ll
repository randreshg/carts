clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
noelle-norm test.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
# noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc

-------------------------------------------------
[carts] Running CARTS on Module: 
test_with_metadata.bc

-------------------------------------------------
-------------------------------------------------
[edt-graph] Building the EDT Graph
-------------------------------------------------
-------------------------------------------------
[edt-graph] Creating the EDT Nodes

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Processing function: main
- - - - - - - - - - - - - - - -
[edt-graph] Parallel Region Found:
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18
[arts] Setting data environment for ParallelEDT
[edt-graph] Creating edge from "MainFunction" to ".omp_outlined."
[edt-graph] The edge doesn't exist yet

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Processing function: .omp_outlined.
- - - - - - - - - - - - - - - -
[edt-graph] Task Region Found:
  %1 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !73
[arts]  - Setting data environment for TaskEDT
[edt-graph] Creating edge from ".omp_outlined." to ".omp_task_entry."
[edt-graph] The edge doesn't exist yet
- - - - - - - - - - - - - - - -
[edt-graph] Task Region Found:
  %11 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !22
[arts]  - Setting data environment for TaskEDT
[edt-graph] Creating edge from ".omp_outlined." to ".omp_task_entry..5"
[edt-graph] The edge doesn't exist yet

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Processing function: .omp_task_entry.

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Processing function: .omp_task_entry..5

-------------------------------------------------
[edt-graph] 4 EDT Nodes were created
-------------------------------------------------
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- EDT "MainFunction"
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Dependencies:
    - [creation] ".omp_outlined."
- EDT ".omp_outlined."
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 4
      -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
      -   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
      -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
      -   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
  - Dependencies:
    - [creation] ".omp_task_entry."
    - [creation] ".omp_task_entry..5"
- EDT ".omp_task_entry."
  - Data Environment:
    - Number of ParamV = 2
      -   %6 = load i32, i32* %random_number, align 4, !tbaa !169, !noelle.pdg.inst.id !101
      -   %9 = load i32, i32* %NewRandom, align 4, !tbaa !169, !noelle.pdg.inst.id !105
    - Number of DepV = 2
      - i32* %number
      - i32* %shared_number
  - Dependencies:
    - The EDT has no outgoing edges
- EDT ".omp_task_entry..5"
  - Data Environment:
    - Number of ParamV = 1
      -   %16 = load i32, i32* %number, align 4, !tbaa !169, !noelle.pdg.inst.id !31
    - Number of DepV = 1
      - i32* %shared_number
  - Dependencies:
    - The EDT has no outgoing edges
- - - - - - - - - - - - - - - - - - - - - - - -

[edt-graph] Analyzing dependencies
- EDT ".omp_task_entry..5"
   - The EDT doesn't have outgoing edges
- EDT ".omp_task_entry."
   - The EDT doesn't have outgoing edges
- EDT ".omp_outlined."
   - EDT ".omp_task_entry." depends on: 
   i32* %number - DATA
   i32* %shared_number - DATA
   - EDT ".omp_task_entry..5" depends on: 
   i32* %shared_number - DATA
- EDT "MainFunction"
   - EDT ".omp_outlined." depends on: 
     %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68 -  DATA  RAW 
        Adding Data Edge
[edt-graph] Creating edge from "MainFunction" to ".omp_outlined."
[edt-graph] The edge already exists
[edt-graph] The edge is a Control Edge
[edt-graph] Converting the edge to a Data Edge

     %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67 -  DATA  RAW 
        Adding Data Edge
[edt-graph] Creating edge from "MainFunction" to ".omp_outlined."
[edt-graph] The edge already exists
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -enable-new-pm=0 -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libSCAFUtilities.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libMemoryAnalysisModules.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/Noelle.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/MetadataCleaner.so --disable-basic-aa -globals-aa -cfl-steens-aa -tbaa -scev-aa -cfl-anders-aa --objc-arc-aa -scalar-evolution -loops -domtree -postdomtree -basic-loop-aa -scev-loop-aa -auto-restrict-aa -intrinsic-aa -global-malloc-aa -pure-fun-aa -semi-local-fun-aa -phi-maze-aa -no-capture-global-aa -no-capture-src-aa -type-aa -no-escape-fields-aa -acyclic-aa -disjoint-fields-aa -field-malloc-aa -loop-variant-allocation-aa -std-in-out-err-aa -array-of-structures-aa -kill-flow-aa -callsite-depth-combinator-aa -unique-access-paths-aa -llvm-aa-results -noelle-scaf -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
1.	Running pass 'Compiler for ARTS' on module 'test_with_metadata.bc'.
#0 0x00007fd9e3e84e13 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a6e13)
#1 0x00007fd9e3e82c0e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a4c0e)
#2 0x00007fd9e3e852df SignalHandler(int) Signals.cpp:0:0
#3 0x00007fd9e66ac910 __restore_rt (/lib64/libpthread.so.0+0x16910)
#4 0x0000000001005148 
/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/bin/n-eval: line 7: 114632 Segmentation fault      $@
error: noelle-load: line 5
make: *** [Makefile:23: test_opt.bc] Error 1
