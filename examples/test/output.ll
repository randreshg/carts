clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata\
		-passes="arts-analysis" test_arts_ir.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
[carts-metadata] Function: main has CARTS Metadata
[carts-metadata] Creating Main EDT Metadata
[arts] Creating EDT #0
[arts] Creating Main EDT for function: main
[carts-metadata] Function: carts.edt has CARTS Metadata
[carts-metadata] Creating Parallel EDT Metadata
[arts] Creating EDT #1
[arts] Creating Parallel EDT for function: carts.edt
[carts-metadata] Function: carts.edt.1 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #2
[arts] Creating Task EDT for function: carts.edt.1
[carts-metadata] Function: carts.edt.2 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #3
[arts] Creating Task EDT for function: carts.edt.2
[carts-metadata] Function: carts.edt.3 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #4
[arts] Creating Task EDT for function: carts.edt.3


[arts-analysis] Initializing AAEDTInfo: 
[AAEDTInfoFunction::initialize] EDT #0 for function "main"
   - Failed to visit all Callsites!
[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.1"
[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.2"
[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.3"
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
[AAEDTInfoCallsite::initialize] EDT #1
[AAEDTInfoCallsite::updateImpl] EDT #1
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Underlying object:   %random_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value #  %random_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %random_number = alloca i32, align 4
   - ReachedChildEDTs: EDT #1
   - ReachedChildEDTs: EDT #4
   - Underlying object belongs to the parent EDT!
   - MaySignalChildEDTs: EDT #4
[AAEDTInfoCallsiteArg::updateImpl] EDT #1 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %NewRandom = alloca i32, align 4 from EDT #1
   - Underlying object:   %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value #  %NewRandom = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %NewRandom = alloca i32, align 4
   - ReachedChildEDTs: EDT #1
   - Underlying object belongs to the parent EDT!
[AAEDTInfoCallsiteArg::updateImpl] EDT #1 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #1
   - Underlying object:   %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value #  %number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - ReachedChildEDTs: EDT #1
   - ReachedChildEDTs: EDT #4
   - Underlying object belongs to the parent EDT!
   - MaySignalChildEDTs: EDT #4
[AAEDTInfoCallsiteArg::updateImpl] EDT #1 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #1
[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Underlying object:   %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::initialize] Value #  %shared_number = alloca i32, align 4
[AAEDTDataBlockInfoVal::updateImpl]   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - ReachedChildEDTs: EDT #1
   - Underlying object belongs to the parent EDT!
[AAEDTInfoCallsiteArg::updateImpl] EDT #1 Changed: YES
   - All DataBlocks were fixed for EDT #1
[AAEDTInfoFunction::updateImpl] EDT #2
[AAEDTInfoCallsite::initialize] EDT #2
[AAEDTInfoCallsite::updateImpl] EDT #2
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl] ptr %2 from EDT #2
   - Underlying object:   %number = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 2
   - DependentEDTs: EDT #4
        - Creating edge from "EDT #2" to "EDT #4"
          Data Edge
[AAEDTInfoCallsiteArg::updateImpl] EDT #2 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #2
   - Underlying object:   %shared_number = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 2
[AAEDTInfoCallsiteArg::updateImpl] EDT #2 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #2 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %4 = load i32, ptr %0, align 4, !tbaa !8 from EDT #2
   - Underlying object:   %random_number = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 2
   - DependentEDTs: EDT #4
[AAEDTInfoCallsiteArg::updateImpl] EDT #2 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #3 from EDT #2
[AAEDTInfoCallsiteArg::updateImpl]   %5 = load i32, ptr %1, align 4, !tbaa !8 from EDT #2
   - Underlying object:   %NewRandom = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 2
[AAEDTInfoCallsiteArg::updateImpl] EDT #2 Changed: YES
   - All DataBlocks were fixed for EDT #2
[AAEDTInfoFunction::updateImpl] EDT #3
[AAEDTInfoCallsite::initialize] EDT #3
[AAEDTInfoCallsite::updateImpl] EDT #3
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl] ptr %3 from EDT #3
   - Underlying object:   %shared_number = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 3
[AAEDTInfoCallsiteArg::updateImpl] EDT #3 Changed: YES
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
[AAEDTInfoCallsiteArg::updateImpl]   %7 = load i32, ptr %2, align 4, !tbaa !8 from EDT #3
   - Underlying object:   %number = alloca i32, align 4
   - Underlying object does not belong to the parent EDT!. It belongs to EDT #0
   - EDT #1 can reach 3
   - DependentEDTs: EDT #4
        - Creating edge from "EDT #3" to "EDT #4"
          Data Edge
[AAEDTInfoCallsiteArg::updateImpl] EDT #3 Changed: YES
   - All DataBlocks were fixed for EDT #3
[AAEDTInfoFunction::updateImpl] EDT #4
[AAEDTInfoCallsite::initialize] EDT #4
[AAEDTInfoCallsite::updateImpl] EDT #4
[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %number = alloca i32, align 4 from EDT #4
   - Underlying object:   %number = alloca i32, align 4
   - Underlying object belongs to the parent EDT!
   - CalledEDT is asynchronous!
[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4
   - Underlying object:   %random_number = alloca i32, align 4
   - Underlying object belongs to the parent EDT!
   - CalledEDT is asynchronous!
   - All DataBlocks were fixed for EDT #4
[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #4 is a child of EDT #0
[AAEDTInfoFunction::updateImpl] EDT #0
AAEDTInfoFunction::manifest: EDT #0 -> 
     #Child EDTs: 2{1, 4}
     #Reached ChildEDTs: 4{1, 4, 2, 3}
     #MaySignal EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #0: main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0

AAEDTInfoFunction::manifest: EDT #1 -> 
     #Child EDTs: 2{2, 3}
     #Reached ChildEDTs: 2{2, 3}
     #MaySignal EDTs: 1{4}
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

AAEDTInfoFunction::manifest: EDT #2 -> 
     #Child EDTs: 0{}
     #Reached ChildEDTs: 0{}
     #MaySignal EDTs: 0{}
     #Dependent EDTs: 1{4}
- EDT #2: carts.edt.1
Ty: task
Data environment for EDT: 
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - ptr %0
  - ptr %1

AAEDTInfoFunction::manifest: EDT #3 -> 
     #Child EDTs: 0{}
     #Reached ChildEDTs: 0{}
     #MaySignal EDTs: 0{}
     #Dependent EDTs: 1{4}
- EDT #3: carts.edt.2
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0

AAEDTInfoFunction::manifest: EDT #4 -> 
     #Child EDTs: 0{}
     #Reached ChildEDTs: 0{}
     #MaySignal EDTs: 0{}
     #Dependent EDTs: 0{}
- EDT #4: carts.edt.3
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %number
  - ptr %random_number

[Attributor] Done with 5 functions, result: unchanged.

-------------------------------------------------
[arts-analysis] Process has finished
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph

- EDT #0 - "main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #1"
    - [control/ creation] "EDT #4"

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
    - [control/ creation] "main"
  - Outgoing Edges:
    - [control/ creation] "EDT #2"
    - [control/ creation] "EDT #3"

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
    - [control/ creation] "carts.edt"
  - Outgoing Edges:
    - [data] "EDT #4"
        -   %random_number = alloca i32, align 4
        -   %number = alloca i32, align 4

- EDT #3 - "carts.edt.2"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [control/ creation] "carts.edt"
  - Outgoing Edges:
    - [data] "EDT #4"
        -   %number = alloca i32, align 4

- EDT #4 - "carts.edt.3"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %number
      - ptr %random_number
  - Incoming Edges:
    - [data] "carts.edt.1"
    - [control/ creation] "main"
    - [data] "carts.edt.2"
  - Outgoing Edges:
    - The EDT has no outgoing edges
- - - - - - - - - - - - - - - - - - - - - - - -


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
