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
[arts] Function: main has CARTS Metadata
[arts] Creating EDT #0 for function: main
[arts] Creating Task EDT for function: main
[arts] Creating Main EDT for function: main
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Sync EDT for function: carts.edt
[arts] Creating Parallel EDT for function: carts.edt
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.1
[arts] Creating Task EDT for function: carts.edt.1
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.2
[arts] Creating Task EDT for function: carts.edt.2
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.3
[arts] Creating Task EDT for function: carts.edt.3


[Attributor] Initializing AAEDTInfo: 
[AAEDTInfoFunction::initialize] EDT #0 for function "main"
   - Failed to visit all Callsites!
[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
   - ParentEDT: EDT #0
   - DoneEDT: EDT #4
[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.1"
   - ParentEDT: EDT #1
[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.2"
   - ParentEDT: EDT #1
[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.3"
   - ParentEDT: EDT #0
[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
        - Creating edge from "EDT #0" to "EDT #1"
          Control Edge
   - EDT #4 is a child of EDT #0
        - Creating edge from "EDT #0" to "EDT #4"
          Control Edge
[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #2 is a child of EDT #1
        - Creating edge from "EDT #1" to "EDT #2"
          Control Edge
   - EDT #3 is a child of EDT #1
        - Creating edge from "EDT #1" to "EDT #3"
          Control Edge
[AAEDTInfoFunction::updateImpl] EDT #2
   - All ReachedEDTs are fixed for EDT #2
   - EDT #2 is async. No updates to push
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoCallsite::initialize] EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
[AAEDTDataBlockInfoVal::initialize] Value #ptr %2
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #2
[AAEDTDataBlockInfoVal::initialize] Value #ptr %3
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %4 = load i32, ptr %0, align 4, !tbaa !8 from EDT #2
[AAEDTDataBlockInfoVal::initialize] Value #ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %4 = load i32, ptr %0, align 4, !tbaa !8 from EDT #2
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %5 = load i32, ptr %1, align 4, !tbaa !8 from EDT #2
[AAEDTDataBlockInfoVal::initialize] Value #ptr %1
[AAEDTDataBlockInfoVal::updateImpl] ptr %1
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %5 = load i32, ptr %1, align 4, !tbaa !8 from EDT #2
   - Analyzing sync remote dependencies
[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3
   - EDT #3 is async. No updates to push
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoCallsite::initialize] EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl]   %7 = load i32, ptr %2, align 4, !tbaa !8 from EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
   - All ReachedEDTs are fixed for EDT #4
   - EDT #4 is async. No updates to push
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTInfoCallsite::initialize] EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #  %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #  %random_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %random_number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #ptr %random_number
[AAEDTDataBlockInfoVal::updateImpl] ptr %random_number
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #  %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #4 is a child of EDT #0
[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #2 is a child of EDT #1
   - EDT #3 is a child of EDT #1
   - All ReachedEDTs are fixed for EDT #1
   - Pushing updates from EDT #1
   - EDT #1 must sync ChildEDTs: 2
   - EDT #1 must sync DescendantEDTs: 0
      - Pushing update to EDT #2
      - Pushing update to EDT #3
   - Getting updates for EDT #1
   - Finished getting updates for EDT #1
[AAEDTInfoCallsite::initialize] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing local dependencies
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::initialize] Value #  %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %NewRandom = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - Analyzing local dependencies
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #2
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl]   %7 = load i32, ptr %2, align 4, !tbaa !8 from EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #4
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #4 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0
   - EDT #0 is async. No updates to push
[AAEDTInfoFunction::updateImpl] EDT #1
   - Getting updates for EDT #1
   - Finished getting updates for EDT #1
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
[AAEDTDataBlockInfoVal::updateImpl] ptr %3
   - ReachedLocalEDTs: EDT #2
   - ReachedLocalEDTs: EDT #3
[AAEDTDataBlockInfoVal::initialize] Value #ptr %1
[AAEDTDataBlockInfoVal::updateImpl] ptr %1
[AAEDTDataBlockInfoVal::initialize] Value #ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #2
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #2
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
   - Analyzing sync remote dependencies
[AAEDTDataBlockInfoVal::updateImpl] ptr %2
   - ReachedLocalEDTs: EDT #2
[AAEDTDataBlockInfoVal::initialize] Value #ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #ptr %shared_number
[AAEDTDataBlockInfoVal::updateImpl] ptr %shared_number
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %7 = load i32, ptr %2, align 4, !tbaa !8 from EDT #3
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %7 = load i32, ptr %2, align 4, !tbaa !8 from EDT #3
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing local dependencies
   - Analyzing sync remote dependencies
        - Creating edge from "EDT #2" to "EDT #4"
          Data Edge
     - EDT #2 must signal the value to EDT #4
        - Creating edge from "EDT #3" to "EDT #4"
          Data Edge
     - EDT #3 must signal the value to EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
     - EDT #3 must signal the value to EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #ptr %number
[AAEDTDataBlockInfoVal::updateImpl] ptr %number
[AAEDTInfoCallsite::updateImpl] EDT #2
   - All DataBlocks were fixed for EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #2
   - All DataBlocks were fixed for EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #3
   - All DataBlocks were fixed for EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #3
   - All DataBlocks were fixed for EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing local dependencies
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #4
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #4
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
     - EDT #3 must signal the value to EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #4
   - All DataBlocks were fixed for EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #4
   - All DataBlocks were fixed for EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoFunction::updateImpl] EDT #1
   - Getting updates for EDT #1
   - Finished getting updates for EDT #1
[AAEDTInfoFunction::updateImpl] EDT #4
   - Getting updates for EDT #4
   - Finished getting updates for EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
     - EDT #3 must signal the value to EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
AAEDTInfoFunction::manifest: 
EDT #0 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{1, 4}
     #Reached DescendantEDTs: 2{2, 3}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #0: main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0

AAEDTInfoFunction::manifest: 
EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 2{2, 3}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 1{4}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #1: carts.edt
Ty: parallel
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  - ptr %0
  - ptr %1
  - ptr %2
  - ptr %3

AAEDTInfoFunction::manifest: 
EDT #2 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: EDT #1
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #2: carts.edt.1
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  - ptr %0
  - ptr %1
  - i32 %2
  - i32 %3

AAEDTInfoFunction::manifest: 
EDT #3 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: EDT #1
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #3: carts.edt.2
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - i32 %1

AAEDTInfoFunction::manifest: 
EDT #4 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #MaySignalLocal EDTs: 0{}
     #MaySignalRemote EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #4: carts.edt.3
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 3
  - ptr %number
  - ptr %random_number
  - ptr %shared_number

AAEDTInfoFunction::manifest: ptr %2
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 1{2}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %3
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 2{2, 3}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %0
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %1
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest:   %number = alloca i32, align 4
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #ReachedLocalEDTs: 2{1, 4}
     - #ReachedRemoteEDTs: 1{2}

AAEDTInfoFunction::manifest:   %random_number = alloca i32, align 4
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #ReachedLocalEDTs: 2{1, 4}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %random_number
EDTDataBlock ->
     - ParentEDT: EDT #4
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest:   %shared_number = alloca i32, align 4
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #ReachedLocalEDTs: 2{1, 4}
     - #ReachedRemoteEDTs: 2{2, 3}

AAEDTInfoFunction::manifest:   %NewRandom = alloca i32, align 4
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #ReachedLocalEDTs: 1{1}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %1
EDTDataBlock ->
     - ParentEDT: EDT #2
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %0
EDTDataBlock ->
     - ParentEDT: EDT #3
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %0
EDTDataBlock ->
     - ParentEDT: EDT #2
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %shared_number
EDTDataBlock ->
     - ParentEDT: EDT #4
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

AAEDTInfoFunction::manifest: ptr %number
EDTDataBlock ->
     - ParentEDT: EDT #4
     - #ReachedLocalEDTs: 0{}
     - #ReachedRemoteEDTs: 0{}

[Attributor] Done with 5 functions, result: unchanged.
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
[arts-codegen] Creating function for EDT #3
[arts-codegen] Created ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function carts.edt.2_edt created
[arts-codegen] Inserting Entry for EDT #3
 - EntryBB: entry
 - Inserting Entry
 - Inserting ParamV
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/external/llvm/llvm/lib/IR/Instructions.cpp:3335: static CastInst *llvm::CastInst::Create(Instruction::CastOps, Value *, Type *, const Twine &, Instruction *): Assertion `castIsValid(op, S, Ty) && "Invalid cast!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007ffb155f5ef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007ffb155f3b7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007ffb155f65ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007ffb18337910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007ffb14f00d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007ffb14f023e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007ffb14ef8c6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007ffb14ef8cf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007ffb15985334 llvm::CastInst::Create(llvm::Instruction::CastOps, llvm::Value*, llvm::Type*, llvm::Twine const&, llvm::Instruction*) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x25c334)
 #9 0x00007ffb13b8ec79 arts::ARTSCodegen::insertEDTEntry(arts::EDT&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x23c79)
#10 0x00007ffb13b8a2ce llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#11 0x00007ffb159e92a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#12 0x000055f66f360293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#13 0x000055f66f36daaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#14 0x00007ffb14eeb24d __libc_start_main (/lib64/libc.so.6+0x3524d)
#15 0x000055f66f359a3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
