clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" test_arts_ir.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized


[Attributor] Initializing AAEDTInfo: 
[arts] Function: main has CARTS Metadata
[arts] Creating EDT #0 for function: main
[arts] Creating Task EDT for function: main
[arts] Creating Main EDT for function: main
[AAEDTInfoFunction::initialize] EDT #0 for function "main"
   - Failed to visit all Callsites!
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt
   - DepV: ptr %0
   - DepV: ptr %1
   - DepV: ptr %2
   - DepV: ptr %3
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Sync EDT for function: carts.edt
[arts] Creating Parallel EDT for function: carts.edt
[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
   - ParentEDT: EDT #0
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.3
   - DepV: ptr %number
   - DepV: ptr %random_number
   - DepV: ptr %shared_number
[arts] Creating Task EDT for function: carts.edt.3
   - DoneEDT: EDT #2
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.1
   - DepV: ptr %0
   - DepV: ptr %1
   - ParamV: i32 %2
   - ParamV: i32 %3
[arts] Creating Task EDT for function: carts.edt.1
[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.1"
   - ParentEDT: EDT #1
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.2
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.2
[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.2"
   - ParentEDT: EDT #1
[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.3"
   - ParentEDT: EDT #0
[AAEDTInfoFunction::updateImpl] EDT #0
[arts] Function: rand doesn't have CARTS Metadata
[arts] Function: rand doesn't have CARTS Metadata
   - EDT #1 is a child of EDT #0
        - Creating edge from "EDT #0" to "EDT #1"
          Control Edge
   - EDT #2 is a child of EDT #0
        - Creating edge from "EDT #0" to "EDT #2"
          Control Edge
[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #3 is a child of EDT #1
        - Creating edge from "EDT #1" to "EDT #3"
          Control Edge
   - EDT #4 is a child of EDT #1
        - Creating edge from "EDT #1" to "EDT #4"
          Control Edge
[AAEDTInfoFunction::updateImpl] EDT #3
[arts] Function: llvm.experimental.noalias.scope.decl doesn't have CARTS Metadata
[arts] Function: printf doesn't have CARTS Metadata
   - All ReachedEDTs are fixed for EDT #3
   - EDT #3 is async. No updates to push
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoCallsite::initialize] EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTDataBlockInfoCtxAndVal::initialize] Value ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoVal::initialize] Value ptr %2
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTDataBlockInfoCtxAndVal::initialize] Value ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoVal::initialize] Value ptr %3
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #3
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
[arts] Function: llvm.experimental.noalias.scope.decl doesn't have CARTS Metadata
[arts] Function: printf doesn't have CARTS Metadata
   - All ReachedEDTs are fixed for EDT #4
   - EDT #4 is async. No updates to push
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTInfoCallsite::initialize] EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTDataBlockInfoCtxAndVal::initialize] Value ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
[arts] Function: printf doesn't have CARTS Metadata
   - All ReachedEDTs are fixed for EDT #2
   - EDT #2 is async. No updates to push
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoCallsite::initialize] EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value   %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %random_number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #2
[AAEDTDataBlockInfoVal::initialize] Value ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
[AAEDTDataBlockInfoVal::initialize] Value ptr %random_number
[AAEDTDataBlockInfoVal::updateImpl] ptr %random_number
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoFunction::updateImpl] EDT #0
[arts] Function: rand doesn't have CARTS Metadata
[arts] Function: rand doesn't have CARTS Metadata
   - EDT #1 is a child of EDT #0
   - EDT #2 is a child of EDT #0
[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #3 is a child of EDT #1
   - EDT #4 is a child of EDT #1
   - All ReachedEDTs are fixed for EDT #1
   - Pushing updates from EDT #1
   - EDT #1 must sync ChildEDTs: 2
   - EDT #1 must sync DescendantEDTs: 0
      - Pushing update to EDT #3
      - Pushing update to EDT #4
   - Getting updates for EDT #1
   - Finished getting updates for EDT #1
[AAEDTInfoCallsite::initialize] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %NewRandom = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
[AAEDTDataBlockInfoVal::initialize] Value ptr %1
[AAEDTDataBlockInfoVal::updateImpl] ptr %1
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::initialize] Value   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoFunction::updateImpl] EDT #0
[arts] Function: rand doesn't have CARTS Metadata
[arts] Function: rand doesn't have CARTS Metadata
   - EDT #1 is a child of EDT #0
   - EDT #2 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0
   - EDT #0 is async. No updates to push
[AAEDTInfoFunction::updateImpl] EDT #1
   - Getting updates for EDT #1
   - Finished getting updates for EDT #1
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - ReachedLocalEDTs: EDT #3
   - ReachedLocalEDTs: EDT #4
[AAEDTDataBlockInfoVal::initialize] Value ptr %1
[AAEDTDataBlockInfoVal::updateImpl] ptr %1
[AAEDTDataBlockInfoVal::initialize] Value ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - ReachedLocalEDTs: EDT #3
[AAEDTDataBlockInfoVal::initialize] Value ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #2
[AAEDTDataBlockInfoVal::initialize] Value ptr %shared_number
[AAEDTDataBlockInfoVal::updateImpl] ptr %shared_number
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #2
[AAEDTDataBlockInfoVal::initialize] Value ptr %number
[AAEDTDataBlockInfoVal::updateImpl] ptr %number
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
-------------------------------
AAEDTInfoFunction::manifest: 
State for EDT #0 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{1, 2}
     #Reached DescendantEDTs: 2{3, 4}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}

- EDT #0: edt.0.main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
     EDT #0 doesn't have a parent EDT
-------------------------------
AAEDTInfoFunction::manifest: 
State for EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 2{3, 4}
     #Reached DescendantEDTs: 0{}
     -MaySignalLocal EDTs invalid with 0 EDTs
     -MaySignalRemote EDTs invalid with 0 EDTs
     -Dependent EDTs invalid with 0 EDTs

- EDT #1: edt.1.parallel
Ty: parallel
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  - ptr %0
  - ptr %1
  - ptr %2
  - ptr %3
[arts-codegen] Creating function for EDT #1
[arts-codegen] Reserving GUID for EDT #1
[arts-codegen] Created ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
-------------------------------
AAEDTInfoFunction::manifest: 
State for EDT #3 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT invalid
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     -MaySignalLocal EDTs invalid with 0 EDTs
     -MaySignalRemote EDTs invalid with 0 EDTs
     -Dependent EDTs invalid with 0 EDTs

- EDT #3: edt.3.task
Ty: task
Data environment for EDT: 
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - ptr %0
  - ptr %1
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #3
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
-------------------------------
AAEDTInfoFunction::manifest: 
State for EDT #4 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT invalid
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     -MaySignalLocal EDTs invalid with 0 EDTs
     -MaySignalRemote EDTs invalid with 0 EDTs
     -Dependent EDTs invalid with 0 EDTs

- EDT #4: edt.4.task
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0
[arts-codegen] Creating function for EDT #4
[arts-codegen] Reserving GUID for EDT #4
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
-------------------------------
AAEDTInfoFunction::manifest: 
State for EDT #2 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT invalid
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     -MaySignalLocal EDTs invalid with 0 EDTs
     -MaySignalRemote EDTs invalid with 0 EDTs
     -Dependent EDTs invalid with 0 EDTs

- EDT #2: edt.2.task
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 3
  - ptr %number
  - ptr %random_number
  - ptr %shared_number
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
[Attributor] Done with 5 functions, result: changed.
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- EDT #1 - "edt.1.parallel"
  - Type: parallel
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 4
      - ptr %0
      - ptr %1
      - ptr %2
      - ptr %3
  - Incoming Edges:
    - [control/ creation] "EDT #0"
  - Outgoing Edges:
    - [control/ creation] "EDT #4"
    - [control/ creation] "EDT #3"

- EDT #0 - "edt.0.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #2"
    - [control/ creation] "EDT #1"

- EDT #4 - "edt.4.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [control/ creation] "EDT #1"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #2 - "edt.2.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 3
      - ptr %number
      - ptr %random_number
      - ptr %shared_number
  - Incoming Edges:
    - [control/ creation] "EDT #0"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #3 - "edt.3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 2
      - i32 %2
      - i32 %3
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [control/ creation] "EDT #1"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- - - - - - - - - - - - - - - - - - - - - - - -


-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'test_arts_ir.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.2 = private unnamed_addr constant [32 x i8] c"I think the number is %d - %d.\0A\00", align 1
@.str.5 = private unnamed_addr constant [36 x i8] c"The final number is %d - % d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !6 dso_local noundef i32 @main() local_unnamed_addr #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt(ptr nocapture readonly, ptr nocapture readonly, ptr nocapture, ptr nocapture) #3

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !10 internal void @carts.edt.1(ptr nocapture, ptr nocapture, i32, i32) #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !12 internal void @carts.edt.2(ptr nocapture readonly, i32) #3

; Function Attrs: nounwind
declare !callback !14 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !16 internal void @carts.edt.3(ptr nocapture readonly, ptr nocapture readonly, ptr nocapture readonly) #0

define internal void @edt.0.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.1.parallel_guid.addr = alloca ptr, align 8
  store ptr %0, ptr %edt.1.parallel_guid.addr, align 8
  %1 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.2.task_guid.addr = alloca ptr, align 8
  store ptr %1, ptr %edt.2.task_guid.addr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, ptr %number, align 4, !tbaa !18
  store i32 10000, ptr %shared_number, align 4, !tbaa !18
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !18
  %call1 = tail call i32 @rand() #5
  store i32 %call1, ptr %NewRandom, align 4, !tbaa !18
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number, ptr nocapture %shared_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}

define internal void @edt.1.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca ptr, align 8
  store ptr %0, ptr %edt.3.task_guid.addr, align 8
  %1 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.4.task_guid.addr = alloca ptr, align 8
  store ptr %1, ptr %edt.4.task_guid.addr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  %2 = load i32, ptr %0, align 4, !tbaa !18
  %3 = load i32, ptr %1, align 4, !tbaa !18
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %2, i32 %3) #8
  %4 = load i32, ptr %3, align 4, !tbaa !18
  %inc = add nsw i32 %4, 1
  store i32 %inc, ptr %3, align 4, !tbaa !18
  %5 = load i32, ptr %2, align 4, !tbaa !18
  call void @carts.edt.2(ptr nocapture readonly %3, i32 %5) #8
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

declare ptr @artsReserveGuidRoute(i32, i32)

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !22)
  %0 = load i32, ptr %0, align 4, !tbaa !18, !noalias !22
  %1 = load i32, ptr %1, align 4, !tbaa !18, !noalias !22
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #5, !noalias !22
  %2 = load i32, ptr %0, align 4, !tbaa !18, !noalias !22
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !18, !noalias !22
  %3 = load i32, ptr %1, align 4, !tbaa !18, !noalias !22
  %dec.i = add nsw i32 %3, -1
  store i32 %dec.i, ptr %1, align 4, !tbaa !18, !noalias !22
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.4.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %0 = load i32, ptr %0, align 4, !tbaa !18, !noalias !25
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %0) #5, !noalias !25
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %number, align 4, !tbaa !18
  %1 = load i32, ptr %random_number, align 4, !tbaa !18
  %2 = load i32, ptr %shared_number, align 4, !tbaa !18
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1, i32 noundef %2) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #7 = { nounwind memory(argmem: readwrite) }
attributes #8 = { nounwind memory(readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"main", !7}
!7 = !{}
!8 = !{!"parallel", !9}
!9 = !{!"dep", !"dep", !"dep", !"dep"}
!10 = !{!"task", !11}
!11 = !{!"dep", !"dep", !"param", !"param"}
!12 = !{!"task", !13}
!13 = !{!"dep", !"param"}
!14 = !{!15}
!15 = !{i64 2, i64 -1, i64 -1, i1 true}
!16 = !{!"task", !17}
!17 = !{!"dep", !"dep", !"dep"}
!18 = !{!19, !19, i64 0}
!19 = !{!"int", !20, i64 0}
!20 = !{!"omnipotent char", !21, i64 0}
!21 = !{!"Simple C++ TBAA"}
!22 = !{!23}
!23 = distinct !{!23, !24, !".omp_outlined.: %__context"}
!24 = distinct !{!24, !".omp_outlined."}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..1: %__context"}
!27 = distinct !{!27, !".omp_outlined..1"}


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt.1
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt.2
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt.3
Referring to an argument in another function!
  %2 = load i32, ptr %0, align 4, !tbaa !18
Referring to an argument in another function!
  %3 = load i32, ptr %1, align 4, !tbaa !18
Referring to an argument in another function!
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %2, i32 %3) #7
Referring to an argument in another function!
  %4 = load i32, ptr %3, align 4, !tbaa !18
Referring to an argument in another function!
  store i32 %inc, ptr %3, align 4, !tbaa !18
Referring to an argument in another function!
  %5 = load i32, ptr %2, align 4, !tbaa !18
Referring to an argument in another function!
  call void @carts.edt.2(ptr nocapture readonly %3, i32 %5) #7
Referring to an argument in another function!
  %0 = load i32, ptr %0, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  %1 = load i32, ptr %1, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #5, !noalias !22
Referring to an argument in another function!
  %2 = load i32, ptr %0, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  store i32 %inc.i, ptr %0, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  %3 = load i32, ptr %1, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  store i32 %dec.i, ptr %1, align 4, !tbaa !18, !noalias !22
Referring to an argument in another function!
  %0 = load i32, ptr %0, align 4, !tbaa !18, !noalias !25
Referring to an argument in another function!
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %0) #5, !noalias !25
Referring to an argument in another function!
  %0 = load i32, ptr %number, align 4, !tbaa !18
Referring to an argument in another function!
  %1 = load i32, ptr %random_number, align 4, !tbaa !18
Referring to an argument in another function!
  %2 = load i32, ptr %shared_number, align 4, !tbaa !18
LLVM ERROR: Broken module found, compilation aborted!
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007f3560b0eef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f3560b0cb7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f3560b0f5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f3563850910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f3560419d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f356041b3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f3560a5ad3c llvm::report_fatal_error(llvm::Twine const&, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddd3c)
 #7 0x00007f3560a5ab66 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddb66)
 #8 0x00007f3560f3bbca (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2f9bca)
 #9 0x00005574a2de832d llvm::detail::PassModel<llvm::Module, llvm::VerifierPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x2032d)
#10 0x00007f3560f022a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#11 0x00005574a2de1293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#12 0x00005574a2deeaaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#13 0x00007f356040424d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x00005574a2ddaa3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
