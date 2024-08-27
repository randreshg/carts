clang++ -fopenmp -O3 -g0 -emit-llvm -c taskwait.cpp -o taskwait.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis taskwait.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" taskwait.bc -o taskwait_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis taskwait_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" taskwait_arts_ir.bc -o taskwait_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
taskwait_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized


[Attributor] Initializing AAEDTInfo: 
[arts] Function: main doesn't have CARTS Metadata
[AAEDTInfoFunction::initialize] No context EDT for the function.
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt.1
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.1
[arts] Creating Sync EDT for function: carts.edt.1

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt.1"
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Main EDT for function: carts.edt
[arts] Function: carts.edt.6 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.6
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.6
   - DoneEDT: EDT #2
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.2
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.2

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.2"
[arts] Function: carts.edt.5 has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.5
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.5
[arts] Creating Sync EDT for function: carts.edt.5
[arts] Function: carts.edt.4 has CARTS Metadata
[arts] Creating EDT #5 for function: carts.edt.4
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.4

[AAEDTInfoFunction::initialize] EDT #5 for function "carts.edt.4"
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #6 for function: carts.edt.3
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.3

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
[arts] Function: main doesn't have CARTS Metadata
   - The ContextEDT is the MainEDT

[AAEDTInfoFunction::initialize] EDT #6 for function "carts.edt.3"

[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.5"
   - DoneEDT: EDT #6

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.6"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #4 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #4"
   - EDT #6 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #6"

[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #4

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoFunction::updateImpl] EDT #5
   - All ReachedEDTs are fixed for EDT #5
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #0

[AAEDTInfoCallsite::initialize] EDT #5

[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #0 is a child of EDT #1
        - Inserting CreationEdge from "EDT #1" to "EDT #0"
   - EDT #2 is a child of EDT #1
        - Inserting CreationEdge from "EDT #1" to "EDT #2"

[AAEDTInfoFunction::updateImpl] EDT #6
   - EDT #5 is a child of EDT #6
        - Inserting CreationEdge from "EDT #6" to "EDT #5"
   - All ReachedEDTs are fixed for EDT #6
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #0

[AAEDTInfoCallsite::initialize] EDT #6

[AAEDTInfoFunction::updateImpl] EDT #4
   - EDT #3 is a child of EDT #4
        - Inserting CreationEdge from "EDT #4" to "EDT #3"
   - All ReachedEDTs are fixed for EDT #4

[AAEDTInfoCallsite::initialize] EDT #4

[AAEDTInfoFunction::updateImpl] EDT #2
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #4 is a child of EDT #0
   - EDT #6 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0

[AAEDTInfoCallsite::initialize] EDT #0

[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #0 is a child of EDT #1
   - EDT #2 is a child of EDT #1
   - All ReachedEDTs are fixed for EDT #1

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
        - Inserting DataBlockEdge from "EDT #4" to "EDT #3" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
   - All DataBlocks were fixed for EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #5

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #5
        - Inserting DataBlockEdge from "EDT #6" to "EDT #5" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #5

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #5

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #5

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #5
   - All DataBlocks were fixed for EDT #5

[AAEDTInfoCallsite::updateImpl] EDT #6

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #6

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #6

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #6
   - Analyzing DependentChildEDTs on ptr %1 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #6

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #6

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #6

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #6
   - Analyzing DependentChildEDTs on ptr %0 (ptr %1)
        - Number of DependentChildEDTs: 0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #6
   - All DataBlocks were fixed for EDT #6

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on ptr %0 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #4

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %1
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on ptr %1 (ptr %1)
        - Number of DependentChildEDTs: 0
     - EDT #4 signals to EDT #6
        - Inserting DataBlockEdge from "EDT #4" to "EDT #6" in Slot #0

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - All DataBlocks were fixed for EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 2
        - Inserting DataBlockEdge from "EDT #5" to "EDT #2" in Slot #0

[AAEDTInfoCallsite::updateImpl] EDT #1
   - All DataBlocks were fixed for EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on ptr %0 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on ptr %0 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
        - Number of DependentSiblingEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 2

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4
        - Inserting DataBlockEdge from "EDT #3" to "EDT #6" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
        - Inserting DataBlockEdge from "EDT #6" to "EDT #2" in Slot #1
-------------------------------
[AAEDTInfoFunction::manifest] <invalid>

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: <null>
     #Child EDTs: 2{4, 6}
     #Reached DescendantEDTs: 2{3, 5}


- EDT #0: edt_0.sync
Ty: sync
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - ptr %1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentEDT: EDT #4
     -ParentSyncEDT: EDT #4
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}


- EDT #3: edt_3.task
Ty: task
Number of slots: 1
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #5 -> 
     -ParentEDT: EDT #6
     -ParentSyncEDT: EDT #0
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}


- EDT #5: edt_5.task
Ty: task
Number of slots: 1
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{0, 2}
     #Reached DescendantEDTs: 4{4, 6, 3, 5}


- EDT #1: edt_1.main
Ty: main
Number of slots: 0
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #6 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #0
     #Child EDTs: 1{5}
     #Reached DescendantEDTs: 0{}


- EDT #6: edt_6.task
Ty: task
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - ptr %1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #4 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 1{3}
     #Reached DescendantEDTs: 0{}


- EDT #4: edt_4.sync
Ty: sync
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - ptr %1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #2 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}


- EDT #2: edt_2.task
Ty: task
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %shared_number
  - ptr %random_number
        - Inserting Guid of child "EDT #6" in the edge from "EDT #0" to "EDT #4"
        - Inserting Guid of "EDT #6" in the  from "EDT #4" to "EDT #3"
        - Inserting Guid of child "EDT #2" in the edge from "EDT #1" to "EDT #0"
        - Inserting Guid of "EDT #2" in the  from "EDT #0" to "EDT #6"
        - Inserting Guid of "EDT #2" in the  from "EDT #6" to "EDT #5"
-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #3
DataBlock ->
     - Context: EDT #3 / Slot 0
     - SignalEDT: EDT #6
     - ParentCtx: EDT #4 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #5
DataBlock ->
     - Context: EDT #5 / Slot 0
     - SignalEDT: EDT #2
     - ParentCtx: EDT #0 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #6
DataBlock ->
     - Context: EDT #6 / Slot 0
     - ChildDBss: 1
      - ChildDB: EDT #5 / Slot 0
     - #DependentChildEDTs: 1{5}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #6
DataBlock ->
     - Context: EDT #6 / Slot 1
     - SignalEDT: EDT #2
     - ParentCtx: EDT #0 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 0
     - ChildDBss: 1
      - ChildDB: EDT #3 / Slot 0
     - DependentSiblingEDT: EDT #6 / Slot #1
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{6}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 1
     - SignalEDT: EDT #6
     - DependentSiblingEDT: EDT #6 / Slot #0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 1{6}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #2
DataBlock ->
     - Context: EDT #2 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #2
DataBlock ->
     - Context: EDT #2 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 0
     - ChildDBss: 2
      - ChildDB: EDT #6 / Slot 1
      - ChildDB: EDT #4 / Slot 0
     - DependentSiblingEDT: EDT #2 / Slot #0
     - #DependentChildEDTs: 2{6, 4}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - ChildDBss: 2
      - ChildDB: EDT #6 / Slot 0
      - ChildDB: EDT #4 / Slot 1
     - DependentSiblingEDT: EDT #2 / Slot #1
     - #DependentChildEDTs: 2{6, 4}
     - #DependentSiblingEDTs: 1{2}

[Attributor] Done with 8 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph
- - - - - - - - - - - - - - - - - - 
- EDT #5 - "edt_5.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [Creation] "EDT #6"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %random_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #2" in Slot #0
      - ptr %0 /   %random_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #6 - "edt_6.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %1
      - [DataBlock] ptr %0 / ptr %0
  - Outgoing Edges:
    - [Creation] "EDT #5"
    - [DataBlock] "EDT #5" in Slot #0
      - ptr %0 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #2" in Slot #1
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #1 - "edt_1.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming Creation edges
    - The EDT has no incoming DataBlock slots
  - Outgoing Edges:
    - [Creation] "EDT #0"
    - [Creation] "EDT #2"
    - [DataBlock] "EDT #0" in Slot #0
      -   %shared_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #1
      -   %random_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt_3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [Creation] "EDT #4"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 / ptr %0
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #6" in Slot #0
      - ptr %0 / ptr %0
- - - - - - - - - - - - - - - - - - 
- EDT #4 - "edt_4.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0
    - EDTSlot #1
      - [DataBlock] ptr %1
  - Outgoing Edges:
    - [Creation] "EDT #3"
    - [DataBlock] "EDT #3" in Slot #0
      - ptr %0 / ptr %0
    - [DataBlock] "EDT #6" in Slot #0
      - ptr %1
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt_0.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [Creation] "EDT #1"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock]   %shared_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4
  - Outgoing Edges:
    - [Creation] "EDT #4"
    - [Creation] "EDT #6"
    - [DataBlock] "EDT #4" in Slot #1
      - ptr %1
    - [DataBlock] "EDT #4" in Slot #0
      - ptr %0
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt_2.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Edges:
    - [Creation] "EDT #1"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %random_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
[arts-codegen] Creating codegen for EDT #4
[arts-codegen] Creating function for EDT #4
[arts-codegen] Reserving GUID for EDT #4
[arts-codegen] Reserving GUID for EDT #1
     EDT #1 doesn't have a parent EDT, using parent function
[arts-codegen] Creating codegen for EDT #6
[arts-codegen] Creating function for EDT #6
[arts-codegen] Reserving GUID for EDT #6
[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #3
[arts-codegen] Creating codegen for EDT #5
[arts-codegen] Creating function for EDT #5
[arts-codegen] Reserving GUID for EDT #5

All EDT Guids have been reserved

Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
     EDT #1 doesn't have a parent EDT
 - Inserting DepV
     EDT #1 doesn't have incoming slot nodes
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
[arts-codegen] Inserting Call for EDT #1
 - Inserting EDT Call
 - EDT Call:   %7 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.main, i64 %1, i32 %3, ptr %edt_1_paramv, i32 %6)

Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
   - DepV[1]: ptr %random_number ->   %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #2
 - Inserting EDT Call
 - EDT Call:   %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %1, i32 %8, ptr %edt_2_paramv, i32 %11)

Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
   - ParamVGuid[0]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
   - DepV[1]: ptr %1 ->   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen ptr %1
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/carts/src/utils/ARTS.cpp:126: Type *arts::DataBlock::getType(): Assertion `Ty && "Type is null"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis taskwait_arts_ir.bc -o taskwait_arts_analysis.bc
 #0 0x00007ff123d2def8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007ff123d2bb7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007ff123d2e5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007ff126a6f910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007ff123638d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007ff12363a3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007ff123630c6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007ff123630cf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007ff1222b1e78 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x2ee78)
 #9 0x00007ff1222aaaaf arts::ARTSCodegen::getOrCreateDBCodegen(arts::DataBlock&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x27aaf)
#10 0x00007ff1222aee74 std::_Function_handler<void (arts::DataBlockGraphEdge*), arts::ARTSCodegen::insertEDTEntry(arts::EDT&)::$_2>::_M_invoke(std::_Any_data const&, arts::DataBlockGraphEdge*&&) ARTSCodegen.cpp:0:0
#11 0x00007ff1222bf058 arts::ARTSGraph::forEachOutgoingDataBlockEdge(arts::EDTGraphNode*, std::function<void (arts::DataBlockGraphEdge*)>) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x3c058)
#12 0x00007ff1222a855a arts::ARTSCodegen::insertEDTEntry(arts::EDT&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x2555a)
#13 0x00007ff1222a27a8 llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#14 0x00007ff1241212a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#15 0x000055df5d6dd293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#16 0x000055df5d6eaaaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#17 0x00007ff12362324d __libc_start_main (/lib64/libc.so.6+0x3524d)
#18 0x000055df5d6d6a3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: taskwait_arts_analysis.bc] Aborted
