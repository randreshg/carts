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
   - DepV: ptr %0
   - DepV: ptr %1
   - DepV: ptr %2
   - DepV: ptr %3
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Sync EDT for function: carts.edt
[arts] Creating Parallel EDT for function: carts.edt
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.1
   - DepV: ptr %0
   - DepV: ptr %1
   - ParamV: i32 %2
   - ParamV: i32 %3
[arts] Creating Task EDT for function: carts.edt.1
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.2
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.2
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.3
   - DepV: ptr %number
   - DepV: ptr %random_number
   - DepV: ptr %shared_number
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
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #2
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
[AAEDTDataBlockInfoVal::initialize] Value #ptr %0
[AAEDTDataBlockInfoVal::updateImpl] ptr %0
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
[AAEDTDataBlockInfoVal::initialize] Value #ptr %1
[AAEDTDataBlockInfoVal::updateImpl] ptr %1
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
   - All DataBlocks were fixed for EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #3
   - All DataBlocks were fixed for EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
   - Analyzing local dependencies
     - No local deps, EDT is asynchronous!
   - Analyzing sync remote dependencies
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
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
[AAEDTInfoFunction::updateImpl] EDT #3
   - Getting updates for EDT #3
   - Finished getting updates for EDT #3
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing sync remote dependencies
     - EDT #2 must signal the value to EDT #4
     - EDT #3 must signal the value to EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - ReachedLocalEDTs: EDT #1
   - ReachedLocalEDTs: EDT #4
[AAEDTDataBlockInfoVal::initialize] Value #ptr %number
[AAEDTDataBlockInfoVal::updateImpl] ptr %number
[AAEDTInfoCallsite::updateImpl] EDT #2
   - All DataBlocks were fixed for EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #2
   - All DataBlocks were fixed for EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoFunction::updateImpl] EDT #2
   - Getting updates for EDT #2
   - Finished getting updates for EDT #2
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
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - ptr %0
  - ptr %1

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
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0

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

AAEDTInfoFunction::manifest: ptr %0
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 0{}
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
     - ParentEDT: EDT #1
     - #ReachedLocalEDTs: 0{}
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
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- EDT #2 - "carts.edt.1"
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
    - [data] "EDT #4"
        -   %number = alloca i32, align 4
        -   %shared_number = alloca i32, align 4

- EDT #3 - "carts.edt.2"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [control/ creation] "EDT #1"
  - Outgoing Edges:
    - [data] "EDT #4"
        -   %shared_number = alloca i32, align 4

- EDT #1 - "carts.edt"
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
    - [control/ creation] "EDT #2"

- EDT #4 - "carts.edt.3"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 3
      - ptr %number
      - ptr %random_number
      - ptr %shared_number
  - Incoming Edges:
    - [data] "EDT #3"
    - [control/ creation] "EDT #0"
    - [data] "EDT #2"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #0 - "main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #4"
    - [control/ creation] "EDT #1"

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
[arts-codegen] Creating function for EDT #2
[arts-codegen] Created ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function edt.2.task created
[arts-codegen] Inserting Entry for EDT #2
 - EntryBB: entry
 - Inserting ParamV
   - ParamV[0]: i32 %2
     - Value is an Integer
     - Casted Value:   %1 = trunc i64 %0 to i32
   - ParamV[1]: i32 %3
     - Value is an Integer
     - Casted Value:   %3 = trunc i64 %2 to i32
 - Inserting DepV
   - DepV[0]: ptr %0
   - DepV[1]: ptr %1
[arts-codegen] Creating function for EDT #3
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function edt.3.task created
[arts-codegen] Inserting Entry for EDT #3
 - EntryBB: entry
 - Inserting ParamV
   - ParamV[0]: i32 %1
     - Value is an Integer
     - Casted Value:   %1 = trunc i64 %0 to i32
 - Inserting DepV
   - DepV[0]: ptr %0
[arts-codegen] Creating function for EDT #1
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function edt.1.parallel created
[arts-codegen] Inserting Entry for EDT #1
 - EntryBB: entry
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %0
   - DepV[1]: ptr %1
   - DepV[2]: ptr %2
   - DepV[3]: ptr %3
[arts-codegen] Creating function for EDT #4
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function edt.4.task created
[arts-codegen] Inserting Entry for EDT #4
 - EntryBB: entry
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %number
   - DepV[1]: ptr %random_number
   - DepV[2]: ptr %shared_number
[arts-codegen] Creating function for EDT #0
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-codegen] EDT Function edt.0.main created
[arts-codegen] Inserting Entry for EDT #0
 - EntryBB: entry
 - Inserting ParamV
 - Inserting DepV

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'test_arts_ir.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { ptr, i32, ptr }

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

declare ptr @artsEdtCreateWithGuid(ptr, ptr, i32, ptr, i32)

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.1 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %paramv.1, align 8
  %3 = trunc i64 %2 to i32
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !18)
  %4 = load i32, ptr %depv.0, align 4, !tbaa !21, !noalias !18
  %5 = load i32, ptr %depv.1, align 4, !tbaa !21, !noalias !18
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %1, i32 noundef %3) #5, !noalias !18
  %6 = load i32, ptr %depv.0, align 4, !tbaa !21, !noalias !18
  %inc.i = add nsw i32 %6, 1
  store i32 %inc.i, ptr %depv.0, align 4, !tbaa !21, !noalias !18
  %7 = load i32, ptr %depv.1, align 4, !tbaa !21, !noalias !18
  %dec.i = add nsw i32 %7, -1
  store i32 %dec.i, ptr %depv.1, align 4, !tbaa !21, !noalias !18
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %2 = load i32, ptr %depv.0, align 4, !tbaa !21, !noalias !25
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !25
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.1.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %depv.2 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 2, i32 2
  %depv.3 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 3, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  %0 = load i32, ptr %depv.0, align 4, !tbaa !21
  %1 = load i32, ptr %depv.1, align 4, !tbaa !21
  call void @carts.edt.1(ptr nocapture %depv.2, ptr nocapture %depv.3, i32 %0, i32 %1) #7
  %2 = load i32, ptr %depv.3, align 4, !tbaa !21
  %inc = add nsw i32 %2, 1
  store i32 %inc, ptr %depv.3, align 4, !tbaa !21
  %3 = load i32, ptr %depv.2, align 4, !tbaa !21
  call void @carts.edt.2(ptr nocapture readonly %depv.3, i32 %3) #7
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.4.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 2, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.number, align 4, !tbaa !21
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !21
  %2 = load i32, ptr %depv.shared_number, align 4, !tbaa !21
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1, i32 noundef %2) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  ret void
}

define internal void @edt.0.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, ptr %number, align 4, !tbaa !21
  store i32 10000, ptr %shared_number, align 4, !tbaa !21
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !21
  %call1 = tail call i32 @rand() #5
  store i32 %call1, ptr %NewRandom, align 4, !tbaa !21
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number, ptr nocapture %shared_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #7 = { nounwind memory(readwrite) }
attributes #8 = { nounwind memory(argmem: readwrite) }

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
!18 = !{!19}
!19 = distinct !{!19, !20, !".omp_outlined.: %__context"}
!20 = distinct !{!20, !".omp_outlined."}
!21 = !{!22, !22, i64 0}
!22 = !{!"int", !23, i64 0}
!23 = !{!"omnipotent char", !24, i64 0}
!24 = !{!"Simple C++ TBAA"}
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
LLVM ERROR: Broken module found, compilation aborted!
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007f4b9045def8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f4b9045bb7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f4b9045e5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f4b9319f910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f4b8fd68d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f4b8fd6a3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f4b903a9d3c llvm::report_fatal_error(llvm::Twine const&, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddd3c)
 #7 0x00007f4b903a9b66 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddb66)
 #8 0x00007f4b9088abca (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2f9bca)
 #9 0x000056328e95432d llvm::detail::PassModel<llvm::Module, llvm::VerifierPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x2032d)
#10 0x00007f4b908512a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#11 0x000056328e94d293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#12 0x000056328e95aaaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#13 0x00007f4b8fd5324d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x000056328e946a3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
