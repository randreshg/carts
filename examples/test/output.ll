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
   - All ReachedEDTs are fixed for EDT #3
   - EDT #3 is async. No updates to push
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %2 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentChildEDTs on ptr %2 (ptr %0)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %3 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentChildEDTs on ptr %3 (ptr %1)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #3

[AAEDTInfoFunction::updateImpl] EDT #4
   - All ReachedEDTs are fixed for EDT #4
   - EDT #4 is async. No updates to push
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4

[AAEDTInfoCallsite::initialize] EDT #4

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %3 from EDT #4

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentChildEDTs on ptr %3 (ptr %0)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4

[AAEDTInfoFunction::updateImpl] EDT #2
   - All ReachedEDTs are fixed for EDT #2
   - EDT #2 is async. No updates to push
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::initialize]   %number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #2
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %number)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %random_number)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %shared_number)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoFunction::updateImpl] EDT #0
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

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %NewRandom = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %NewRandom = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on   %NewRandom = alloca i32, align 4
        - Number of DependentSiblingEDTs: 0

[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %2)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

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

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentChildEDTs on ptr %2 (ptr %0)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentChildEDTs on ptr %3 (ptr %1)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoFunction::updateImpl] EDT #0
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

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %2)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentChildEDTs on ptr %2 (ptr %0)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentChildEDTs on ptr %3 (ptr %1)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %2)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentChildEDTs on ptr %2 (ptr %0)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentChildEDTs on ptr %3 (ptr %1)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %2)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentChildEDTs on ptr %3 (ptr %1)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentSiblingEDTs on ptr %3
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentChildEDTs on ptr %2 (ptr %0)
        - Number of DependentChildEDTs: 0
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %3)
        - Number of DependentChildEDTs: 2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #3
   - Analyzing DependentSiblingEDTs on ptr %3
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %3 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %3
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentSiblingEDTs on ptr %2
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #4

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentChildEDTs on   %number = alloca i32, align 4 (ptr %2)
        - Number of DependentChildEDTs: 1
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %2 from EDT #3
   - Analyzing DependentSiblingEDTs on ptr %2
        - Number of DependentSiblingEDTs: 0

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
-------------------------------
[AAEDTInfoFunction::manifest] 
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
-------------------------------
[AAEDTInfoFunction::manifest] 
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
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: EDT #1
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}

- EDT #3: edt.3.task
Ty: task
Data environment for EDT: 
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - ptr %0
  - ptr %1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #4 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: EDT #1
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}

- EDT #4: edt.4.task
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0
-------------------------------
[AAEDTInfoFunction::manifest] 
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
-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %2 from EDT #3
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 0{}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %3 from EDT #3
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{4}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %3 from EDT #4
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{3}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{1}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{1}
     - DependentSiblingDescendantEDTs invalid with 0 EDTs

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{2}
     - DependentSiblingDescendantEDTs invalid with 0 EDTs

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{1}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %NewRandom = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 0{}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 1{3}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{2}
     - #DependentSiblingDescendantEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 2{3, 4}
     - #DependentChildDescendantEDTs: 0{}
     - #DependentSiblingEDTs: 1{2}
     - #DependentSiblingDescendantEDTs: 0{}

[Attributor] Done with 5 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
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
    - [control/ creation] "EDT #3"
    - [control/ creation] "EDT #4"

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

- EDT #0 - "edt.0.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #1"
    - [control/ creation] "EDT #2"

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
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts !6 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, ptr %number, align 4, !tbaa !8
  store i32 10000, ptr %shared_number, align 4, !tbaa !8
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !8
  %call1 = tail call i32 @rand() #5
  store i32 %call1, ptr %NewRandom, align 4, !tbaa !8
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number, ptr nocapture %shared_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt(ptr nocapture readonly %0, ptr nocapture readonly %1, ptr nocapture %2, ptr nocapture %3) #3 !carts !12 {
entry:
  %4 = load i32, ptr %0, align 4, !tbaa !8
  %5 = load i32, ptr %1, align 4, !tbaa !8
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #8
  %6 = load i32, ptr %3, align 4, !tbaa !8
  %inc = add nsw i32 %6, 1
  store i32 %inc, ptr %3, align 4, !tbaa !8
  %7 = load i32, ptr %2, align 4, !tbaa !8
  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.1(ptr nocapture %0, ptr nocapture %1, i32 %2, i32 %3) #3 !carts !14 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !16)
  %4 = load i32, ptr %0, align 4, !tbaa !8, !noalias !16
  %5 = load i32, ptr %1, align 4, !tbaa !8, !noalias !16
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !16
  %6 = load i32, ptr %0, align 4, !tbaa !8, !noalias !16
  %inc.i = add nsw i32 %6, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !8, !noalias !16
  %7 = load i32, ptr %1, align 4, !tbaa !8, !noalias !16
  %dec.i = add nsw i32 %7, -1
  store i32 %dec.i, ptr %1, align 4, !tbaa !8, !noalias !16
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.2(ptr nocapture readonly %0, i32 %1) #3 !carts !19 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !21)
  %2 = load i32, ptr %0, align 4, !tbaa !8, !noalias !21
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !21
  ret void
}

; Function Attrs: nounwind
declare !callback !24 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @carts.edt.3(ptr nocapture readonly %number, ptr nocapture readonly %random_number, ptr nocapture readonly %shared_number) #0 !carts !26 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %2 = load i32, ptr %shared_number, align 4, !tbaa !8
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1, i32 noundef %2) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"parallel", !13}
!13 = !{!"dep", !"dep", !"dep", !"dep"}
!14 = !{!"task", !15}
!15 = !{!"dep", !"dep", !"param", !"param"}
!16 = !{!17}
!17 = distinct !{!17, !18, !".omp_outlined.: %__context"}
!18 = distinct !{!18, !".omp_outlined."}
!19 = !{!"task", !20}
!20 = !{!"dep", !"param"}
!21 = !{!22}
!22 = distinct !{!22, !23, !".omp_outlined..1: %__context"}
!23 = distinct !{!23, !".omp_outlined..1"}
!24 = !{!25}
!25 = !{i64 2, i64 -1, i64 -1, i1 true}
!26 = !{!"task", !27}
!27 = !{!"dep", !"dep", !"dep"}


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
