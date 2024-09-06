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
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #4" to "EDT #3" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #5

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #5
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #6" to "EDT #5" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #5

[AAEDTInfoCallsite::updateImpl] EDT #6

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #6
   - Creating DepSlot #0 for value: ptr %1

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #6
   - Creating DepSlot #1 for value: ptr %0

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
   - Creating DepSlot #1 for value: ptr %1
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #1

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #1

[AAEDTInfoCallsite::updateImpl] EDT #1
   - All DataBlocks were fixed for EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #5

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #5

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #6

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #6

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #4

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #5

[AAEDTInfoCallsite::updateImpl] EDT #6

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #5

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #6
   - Analyzing DependentChildEDTs on ptr %1 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #6

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #6
   - Analyzing DependentChildEDTs on ptr %0 (ptr %1)
        - Number of DependentChildEDTs: 0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #6

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %0
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on ptr %0 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #4
   - Analyzing DependentSiblingEDTs on ptr %1
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on ptr %1 (ptr %1)
        - Number of DependentChildEDTs: 0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #5

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #6

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #4
  The signalDB is: ptr %1 in slot 0 of EDT #6
     - EDT #4 signals to EDT #6
        - Inserting DataBlockEdge from "EDT #4" to "EDT #6" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
  The signalDB is:   %random_number = alloca i32, align 4 in slot 1 of EDT #2
   The signaledDB is: ptr %0 in slot 1
        - Inserting DataBlockEdge from "EDT #5" to "EDT #2" in Slot #1

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
  The signalDB is: ptr %0 in slot 1 of EDT #6
   The signaledDB is: ptr %0 in slot 1
        - Inserting DataBlockEdge from "EDT #3" to "EDT #6" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
  The signalDB is:   %shared_number = alloca i32, align 4 in slot 0 of EDT #2
   The signaledDB is: ptr %0 in slot 0
        - Inserting DataBlockEdge from "EDT #6" to "EDT #2" in Slot #0
-------------------------------
[AAEDTInfoFunction::manifest] <invalid>
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: <null>
     #Child EDTs: 2{4, 6}
     #Reached DescendantEDTs: 2{3, 5}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentEDT: EDT #4
     -ParentSyncEDT: EDT #4
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #5 -> 
     -ParentEDT: EDT #6
     -ParentSyncEDT: EDT #0
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{0, 2}
     #Reached DescendantEDTs: 4{4, 6, 3, 5}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #6 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #0
     #Child EDTs: 1{5}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #4 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 1{3}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #2 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

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
     - ParentSync: EDT #4 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #5
DataBlock ->
     - Context: EDT #5 / Slot 0
     - SignalEDT: EDT #2
     - ParentCtx: EDT #6 / Slot 0
     - ParentSync: EDT #0 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #6
DataBlock ->
     - Context: EDT #6 / Slot 0
     - ParentCtx: EDT #0 / Slot 1
     - ChildDBss: 1
      - EDT #5 / Slot 0
     - #DependentChildEDTs: 1{5}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #6
DataBlock ->
     - Context: EDT #6 / Slot 1
     - SignalEDT: EDT #2
     - ParentCtx: EDT #0 / Slot 0
     - ParentSync: EDT #0 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 0
     - ParentCtx: EDT #0 / Slot 0
     - ChildDBss: 1
      - EDT #3 / Slot 0
     - DependentSiblingEDT: EDT #6 / Slot #1
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{6}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 1
     - SignalEDT: EDT #6
     - ParentCtx: EDT #0 / Slot 1
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
      - EDT #6 / Slot 1
      - EDT #4 / Slot 0
     - DependentSiblingEDT: EDT #2 / Slot #0
     - #DependentChildEDTs: 2{6, 4}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - ChildDBss: 2
      - EDT #6 / Slot 0
      - EDT #4 / Slot 1
     - DependentSiblingEDT: EDT #2 / Slot #1
     - #DependentChildEDTs: 2{6, 4}
     - #DependentSiblingEDTs: 1{2}

[Attributor] Done with 8 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph
- - - - - - - - - - - - - - - - - - 
- EDT #6 - "edt_6.task"
  - Type: task
  - Parent Sync: "EDT #0"
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Creation Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %1 /   %random_number = alloca i32, align 4 [Parent EDT #0]
    - EDTSlot #1
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - [Creation] "EDT #5"
    - [DataBlock] "EDT #5" in Slot #0
      - ptr %0 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #2" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #1 - "edt_1.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Creation Edges:
    - The EDT has no incoming Creation edges
    - The EDT has no incoming DataBlock slots
  - Outgoing Edges:
    - [Creation] "EDT #0"
    - [Creation] "EDT #2"
    - [DataBlock] "EDT #0" in Slot #1
      -   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #0
      -   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt_0.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Creation Edges:
    - [Creation] "EDT #1"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock]   %shared_number = alloca i32, align 4/ No parent
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4/ No parent
  - Outgoing Edges:
    - [Creation] "EDT #4"
    - [Creation] "EDT #6"
    - [DataBlock] "EDT #4" in Slot #1
      - ptr %1 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #4" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt_2.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Creation Edges:
    - [Creation] "EDT #1"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4 [Parent EDT #0]
    - EDTSlot #1
      - [DataBlock] ptr %0 /   %random_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
- - - - - - - - - - - - - - - - - - 
- EDT #5 - "edt_5.task"
  - Type: task
  - Parent Sync: "EDT #0"
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Creation Edges:
    - [Creation] "EDT #6"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %random_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #2" in Slot #1
      - ptr %0 /   %random_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #4 - "edt_4.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Creation Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4 [Parent EDT #0]
    - EDTSlot #1
      - [DataBlock] ptr %1 /   %random_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - [Creation] "EDT #3"
    - [DataBlock] "EDT #6" in Slot #0
      - ptr %1 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #3" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt_3.task"
  - Type: task
  - Parent Sync: "EDT #4"
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Creation Edges:
    - [Creation] "EDT #4"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #6" in Slot #1
      - ptr %0 /   %shared_number = alloca i32, align 4

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #3
[arts-codegen] Creating codegen for EDT #4
[arts-codegen] Creating function for EDT #4
[arts-codegen] Creating codegen for EDT #5
[arts-codegen] Creating function for EDT #5
[arts-codegen] Reserving GUID for EDT #5
[arts-codegen] Creating codegen for EDT #6
[arts-codegen] Creating function for EDT #6
[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
[arts-codegen] Reserving GUID for EDT #4
[arts-codegen] Reserving GUID for EDT #1
     EDT #1 doesn't have a parent EDT, using parent function
[arts-codegen] Reserving GUID for EDT #6

All EDT Guids have been reserved

Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
     EDT #1 doesn't have a parent EDT
 - Inserting DepV
     EDT #1 doesn't have incoming slot nodes
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
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
[arts-codegen] Inserting Call for EDT #0
 - ParamVGuid[0]: EDT2
 - Inserting EDT Call
 - EDT Call:   %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.sync, i64 %4, i32 %8, ptr %edt_0_paramv, i32 %11)

Generating Code for EDT #6
[arts-codegen] Inserting Entry for EDT #6
 - Inserting ParamV
   - ParamVGuid[0]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_6.depv_0.ptr.load = load ptr, ptr %edt_6.depv_0.ptr, align 8
   - DepV[1]: ptr %1 ->   %edt_6.depv_1.ptr.load = load ptr, ptr %edt_6.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #6
 - ParamVGuid[0]: EDT2
 - Inserting EDT Call
 - EDT Call:   %11 = call i64 @artsEdtCreateWithGuid(ptr @edt_6.task, i64 %4, i32 %7, ptr %edt_6_paramv, i32 %10)

Generating Code for EDT #5
[arts-codegen] Inserting Entry for EDT #5
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_5.depv_0.ptr.load = load ptr, ptr %edt_5.depv_0.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #5
 - ParamV[0]:   %4 = load i32, ptr %edt_6.depv_1.ptr.load, align 4, !tbaa !6
 - ParamVGuid[1]: EDT2
 - Inserting EDT Call
 - EDT Call:   %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_5.task, i64 %1, i32 %5, ptr %edt_5_paramv, i32 %9)

Generating Code for EDT #4
[arts-codegen] Inserting Entry for EDT #4
 - Inserting ParamV
   - ParamVGuid[0]: EDT6
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
   - DepV[1]: ptr %1 ->   %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #4
 - ParamVGuid[0]: EDT6
 - Inserting EDT Call
 - EDT Call:   %11 = call i64 @artsEdtCreateWithGuid(ptr @edt_4.sync, i64 %1, i32 %7, ptr %edt_4_paramv, i32 %10)

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT6
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %6 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !6
 - ParamVGuid[1]: EDT6
 - Inserting EDT Call
 - EDT Call:   %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %7, ptr %edt_3_paramv, i32 %11)

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #3

Signal DB ptr %0 from EDT #3 to EDT #6
 - ContextEDT: 3
 - Using DepSlot of FromEDT - Slot: 0 - DBPtr:   %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
 - DBGuid:   %edt_3.depv_0.guid.load = load i64, ptr %edt_3.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #5

Signal DB ptr %0 from EDT #5 to EDT #2
 - ContextEDT: 5
 - Using DepSlot of FromEDT - Slot: 0 - DBPtr:   %edt_5.depv_0.ptr.load = load ptr, ptr %edt_5.depv_0.ptr, align 8
 - DBGuid:   %edt_5.depv_0.guid.load = load i64, ptr %edt_5.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #2
[arts-codegen] Inserting Signals from EDT #0

Signal DB ptr %1 from EDT #0 to EDT #4
 - ContextEDT: 4
 - We have a parent
 - DBPtr:   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - DBGuid:   %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8

Signal DB ptr %0 from EDT #0 to EDT #4
 - ContextEDT: 4
 - We have a parent
 - DBPtr:   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
 - DBGuid:   %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #4

Signal DB ptr %1 from EDT #4 to EDT #6
 - ContextEDT: 4
 - Using DepSlot of FromEDT - Slot: 1 - DBPtr:   %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
 - DBGuid:   %edt_4.depv_1.guid.load = load i64, ptr %edt_4.depv_1.guid, align 8

Signal DB ptr %0 from EDT #4 to EDT #3
 - ContextEDT: 3
 - We have a parent
 - DBPtr:   %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
 - DBGuid:   %edt_4.depv_0.guid.load = load i64, ptr %edt_4.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #1

Signal DB   %random_number = alloca i32, align 4 from EDT #1 to EDT #0
 - ContextEDT: 0
 - We created the EDT
 - DBPtr:   %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
 - DBGuid:   %6 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)

Signal DB   %shared_number = alloca i32, align 4 from EDT #1 to EDT #0
 - ContextEDT: 0
 - We created the EDT
 - DBPtr:   %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
 - DBGuid:   %7 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)
[arts-codegen] Inserting Signals from EDT #6

Signal DB ptr %0 from EDT #6 to EDT #5
 - ContextEDT: 5
 - We have a parent
 - DBPtr:   %edt_6.depv_0.ptr.load = load ptr, ptr %edt_6.depv_0.ptr, align 8
 - DBGuid:   %edt_6.depv_0.guid.load = load i64, ptr %edt_6.depv_0.guid, align 8

Signal DB ptr %0 from EDT #6 to EDT #2
 - ContextEDT: 6
 - Using DepSlot of FromEDT - Slot: 1 - DBPtr:   %edt_6.depv_1.ptr.load = load ptr, ptr %edt_6.depv_1.ptr, align 8
 - DBGuid:   %edt_6.depv_1.guid.load = load i64, ptr %edt_6.depv_1.guid, align 8
[arts-codegen] Inserting Init Functions
[arts-codegen] Inserting ARTS Shutdown Function

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'taskwait_arts_ir.bc'
source_filename = "taskwait.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { i64, i32, ptr }

@.str = private unnamed_addr constant [39 x i8] c"** EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [31 x i8] c"** EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [31 x i8] c"** EDT 3: The number is %d/%d\0A\00", align 1
@.str.4 = private unnamed_addr constant [31 x i8] c"** EDT 4: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.7 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt_3.task: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt_5.task: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt_2.task: %u\0A\00", align 1
@6 = private unnamed_addr constant [25 x i8] c"Guid for edt_0.sync: %u\0A\00", align 1
@7 = private unnamed_addr constant [25 x i8] c"Guid for edt_4.sync: %u\0A\00", align 1
@8 = private unnamed_addr constant [25 x i8] c"Guid for edt_1.main: %u\0A\00", align 1
@9 = private unnamed_addr constant [25 x i8] c"Guid for edt_6.task: %u\0A\00", align 1
@10 = private unnamed_addr constant [17 x i8] c"Creating EDT #1\0A\00", align 1
@11 = private unnamed_addr constant [17 x i8] c"Creating EDT #2\0A\00", align 1
@12 = private unnamed_addr constant [17 x i8] c"Creating EDT #0\0A\00", align 1
@13 = private unnamed_addr constant [17 x i8] c"Creating EDT #6\0A\00", align 1
@14 = private unnamed_addr constant [17 x i8] c"Creating EDT #5\0A\00", align 1
@15 = private unnamed_addr constant [17 x i8] c"Creating EDT #4\0A\00", align 1
@16 = private unnamed_addr constant [17 x i8] c"Creating EDT #3\0A\00", align 1
@17 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #3 to EDT #6 with guid: %u\0A\00", align 1
@18 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #5 to EDT #2 with guid: %u\0A\00", align 1
@19 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #0 to EDT #4 with guid: %u\0A\00", align 1
@20 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #0 to EDT #4 with guid: %u\0A\00", align 1
@21 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #4 to EDT #6 with guid: %u\0A\00", align 1
@22 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #4 to EDT #3 with guid: %u\0A\00", align 1
@23 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@24 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@25 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #6 to EDT #5 with guid: %u\0A\00", align 1
@26 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #6 to EDT #2 with guid: %u\0A\00", align 1

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #0

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #0

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #3

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #3

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) local_unnamed_addr #3

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #3

; Function Attrs: nounwind
declare !callback !6 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

define internal void @edt_3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_3.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt_3.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt_3.paramv_1.guid.edt_6 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_3.paramv_1.guid.edt_6, align 8
  %edt_3.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_3.depv_0.guid.load = load i64, ptr %edt_3.depv_0.guid, align 8
  %edt_3.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !8)
  %3 = load i32, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !11, !noalias !8
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !11, !noalias !8
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #4, !noalias !8
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.6.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.6.slot.1, align 4
  %4 = load i32, ptr %toedt.6.slot.1, align 4
  %5 = load i32, ptr %edt_3.depv_0.ptr.load, align 4
  %6 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @17, i32 0, i32 0), i64 %edt_3.depv_0.guid.load, i32 %5, ptr %edt_3.depv_0.ptr.load, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt_3.depv_0.guid.load)
  ret void
}

define internal void @edt_4.sync(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_3.task_guid.addr, align 8
  %1 = load i64, ptr %edt_3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %1)
  %edt_4.paramv_0.guid.edt_6 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %edt_4.paramv_0.guid.edt_6, align 8
  %edt_4.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_4.depv_0.guid.load = load i64, ptr %edt_4.depv_0.guid, align 8
  %edt_4.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
  %edt_4.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_4.depv_1.guid.load = load i64, ptr %edt_4.depv_1.guid, align 8
  %edt_4.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %4 = load i32, ptr %edt_4.depv_0.ptr.load, align 4, !tbaa !11
  %5 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !11
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %4, i32 noundef %5) #4
  %6 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !11
  %edt_3_paramc = alloca i32, align 4
  store i32 2, ptr %edt_3_paramc, align 4
  %7 = load i32, ptr %edt_3_paramc, align 4
  %edt_3_paramv = alloca i64, i32 %7, align 8
  %edt_3.paramv_0 = getelementptr inbounds i64, ptr %edt_3_paramv, i64 0
  %8 = sext i32 %6 to i64
  store i64 %8, ptr %edt_3.paramv_0, align 8
  %edt_3.paramv_1.guid.edt_6 = getelementptr inbounds i64, ptr %edt_3_paramv, i64 1
  store i64 %3, ptr %edt_3.paramv_1.guid.edt_6, align 8
  %9 = alloca i32, align 4
  store i32 1, ptr %9, align 4
  %10 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @16, i32 0, i32 0))
  %11 = load i32, ptr %9, align 4
  %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %7, ptr %edt_3_paramv, i32 %11)
  br label %codeRepl.exitStub

codeRepl.exitStub:                                ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %codeRepl.exitStub
  %toedt.6.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.6.slot.0, align 4
  %13 = load i32, ptr %toedt.6.slot.0, align 4
  %14 = load i32, ptr %edt_4.depv_1.ptr.load, align 4
  %15 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @21, i32 0, i32 0), i64 %edt_4.depv_1.guid.load, i32 %14, ptr %edt_4.depv_1.ptr.load, i64 %3)
  call void @artsSignalEdt(i64 %3, i32 %13, i64 %edt_4.depv_1.guid.load)
  %toedt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.3.slot.0, align 4
  %16 = load i32, ptr %toedt.3.slot.0, align 4
  %17 = load i32, ptr %edt_4.depv_0.ptr.load, align 4
  %18 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @22, i32 0, i32 0), i64 %edt_4.depv_0.guid.load, i32 %17, ptr %edt_4.depv_0.ptr.load, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %16, i64 %edt_4.depv_0.guid.load)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt_5.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_5.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt_5.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt_5.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_5.paramv_1.guid.edt_2, align 8
  %edt_5.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_5.depv_0.guid.load = load i64, ptr %edt_5.depv_0.guid, align 8
  %edt_5.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_5.depv_0.ptr.load = load ptr, ptr %edt_5.depv_0.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
  %inc.i = add nsw i32 %1, 1
  %3 = load i32, ptr %edt_5.depv_0.ptr.load, align 4, !tbaa !11, !noalias !15
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %3) #4, !noalias !15
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.2.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.2.slot.1, align 4
  %4 = load i32, ptr %toedt.2.slot.1, align 4
  %5 = load i32, ptr %edt_5.depv_0.ptr.load, align 4
  %6 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @18, i32 0, i32 0), i64 %edt_5.depv_0.guid.load, i32 %5, ptr %edt_5.depv_0.ptr.load, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt_5.depv_0.guid.load)
  ret void
}

define internal void @edt_6.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_5.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_5.task_guid.addr, align 8
  %1 = load i64, ptr %edt_5.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @4, i32 0, i32 0), i64 %1)
  %edt_6.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %edt_6.paramv_0.guid.edt_2, align 8
  %edt_6.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_6.depv_0.guid.load = load i64, ptr %edt_6.depv_0.guid, align 8
  %edt_6.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_6.depv_0.ptr.load = load ptr, ptr %edt_6.depv_0.ptr, align 8
  %edt_6.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_6.depv_1.guid.load = load i64, ptr %edt_6.depv_1.guid, align 8
  %edt_6.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_6.depv_1.ptr.load = load ptr, ptr %edt_6.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %4 = load i32, ptr %edt_6.depv_1.ptr.load, align 4, !tbaa !11
  %edt_5_paramc = alloca i32, align 4
  store i32 2, ptr %edt_5_paramc, align 4
  %5 = load i32, ptr %edt_5_paramc, align 4
  %edt_5_paramv = alloca i64, i32 %5, align 8
  %edt_5.paramv_0 = getelementptr inbounds i64, ptr %edt_5_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt_5.paramv_0, align 8
  %edt_5.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %edt_5_paramv, i64 1
  store i64 %3, ptr %edt_5.paramv_1.guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @14, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_5.task, i64 %1, i32 %5, ptr %edt_5_paramv, i32 %9)
  br label %exit1

exit1:                                            ; preds = %entry.split
  br label %exit.ret.exitStub

exit.ret.exitStub:                                ; preds = %exit1
  br label %exit

exit:                                             ; preds = %exit.ret.exitStub
  %toedt.5.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.5.slot.0, align 4
  %11 = load i32, ptr %toedt.5.slot.0, align 4
  %12 = load i32, ptr %edt_6.depv_0.ptr.load, align 4
  %13 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @25, i32 0, i32 0), i64 %edt_6.depv_0.guid.load, i32 %12, ptr %edt_6.depv_0.ptr.load, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %11, i64 %edt_6.depv_0.guid.load)
  %toedt.2.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.2.slot.0, align 4
  %14 = load i32, ptr %toedt.2.slot.0, align 4
  %15 = load i32, ptr %edt_6.depv_1.ptr.load, align 4
  %16 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @26, i32 0, i32 0), i64 %edt_6.depv_1.guid.load, i32 %15, ptr %edt_6.depv_1.ptr.load, i64 %3)
  call void @artsSignalEdt(i64 %3, i32 %14, i64 %edt_6.depv_1.guid.load)
  ret void
}

define internal void @edt_2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_2.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_2.depv_0.guid.load = load i64, ptr %edt_2.depv_0.guid, align 8
  %edt_2.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
  %edt_2.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_2.depv_1.guid.load = load i64, ptr %edt_2.depv_1.guid, align 8
  %edt_2.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %edt_2.depv_0.ptr.load, align 4, !tbaa !11
  %1 = load i32, ptr %edt_2.depv_1.ptr.load, align 4, !tbaa !11
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1) #4
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt_1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_2.task_guid.addr, align 8
  %1 = load i64, ptr %edt_2.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0.sync_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_0.sync_guid.addr, align 8
  %4 = load i64, ptr %edt_0.sync_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @6, i32 0, i32 0), i64 %4)
  %db.random_number.addr = alloca i64, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %6 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)
  %db.random_number.addr.ld = load i64, ptr %db.random_number.addr, align 8
  %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
  %db.shared_number.addr = alloca i64, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %7 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)
  %db.shared_number.addr.ld = load i64, ptr %db.shared_number.addr, align 8
  %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
  br label %edt.body

edt.body:                                         ; preds = %entry
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #4
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #4
  %call1 = tail call i32 @rand() #4
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %db.shared_number.ptr, align 4, !tbaa !11
  %call2 = tail call i32 @rand() #4
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %db.random_number.ptr, align 4, !tbaa !11
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #4
  %edt_0_paramc = alloca i32, align 4
  store i32 1, ptr %edt_0_paramc, align 4
  %8 = load i32, ptr %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %8, align 8
  %edt_0.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %edt_0_paramv, i64 0
  store i64 %1, ptr %edt_0.paramv_0.guid.edt_2, align 8
  %9 = alloca i32, align 4
  store i32 2, ptr %9, align 4
  %10 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @12, i32 0, i32 0))
  %11 = load i32, ptr %9, align 4
  %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.sync, i64 %4, i32 %8, ptr %edt_0_paramv, i32 %11)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt_2_paramc = alloca i32, align 4
  store i32 0, ptr %edt_2_paramc, align 4
  %13 = load i32, ptr %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %13, align 8
  %14 = alloca i32, align 4
  store i32 2, ptr %14, align 4
  %15 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @11, i32 0, i32 0))
  %16 = load i32, ptr %14, align 4
  %17 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %1, i32 %13, ptr %edt_2_paramv, i32 %16)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %toedt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.0.slot.1, align 4
  %18 = load i32, ptr %toedt.0.slot.1, align 4
  %19 = load i32, ptr %db.random_number.ptr, align 4
  %20 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @23, i32 0, i32 0), i64 %6, i32 %19, ptr %db.random_number.ptr, i64 %4)
  call void @artsSignalEdt(i64 %4, i32 %18, i64 %6)
  %toedt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.0.slot.0, align 4
  %21 = load i32, ptr %toedt.0.slot.0, align 4
  %22 = load i32, ptr %db.shared_number.ptr, align 4
  %23 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @24, i32 0, i32 0), i64 %7, i32 %22, ptr %db.shared_number.ptr, i64 %4)
  call void @artsSignalEdt(i64 %4, i32 %21, i64 %7)
  ret void
}

define internal void @edt_0.sync(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_4.sync_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_4.sync_guid.addr, align 8
  %1 = load i64, ptr %edt_4.sync_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @7, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_6.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_6.task_guid.addr, align 8
  %4 = load i64, ptr %edt_6.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @9, i32 0, i32 0), i64 %4)
  %edt_0.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %6 = load i64, ptr %edt_0.paramv_0.guid.edt_2, align 8
  %edt_0.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8
  %edt_0.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
  %edt_0.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8
  %edt_0.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %codeRepl1

codeRepl1:                                        ; preds = %edt.body
  %edt_4_paramc = alloca i32, align 4
  store i32 1, ptr %edt_4_paramc, align 4
  %7 = load i32, ptr %edt_4_paramc, align 4
  %edt_4_paramv = alloca i64, i32 %7, align 8
  %edt_4.paramv_0.guid.edt_6 = getelementptr inbounds i64, ptr %edt_4_paramv, i64 0
  store i64 %4, ptr %edt_4.paramv_0.guid.edt_6, align 8
  %8 = alloca i32, align 4
  store i32 2, ptr %8, align 4
  %9 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @15, i32 0, i32 0))
  %10 = load i32, ptr %8, align 4
  %11 = call i64 @artsEdtCreateWithGuid(ptr @edt_4.sync, i64 %1, i32 %7, ptr %edt_4_paramv, i32 %10)
  br label %codeRepl

codeRepl:                                         ; preds = %codeRepl1
  %edt_6_paramc = alloca i32, align 4
  store i32 1, ptr %edt_6_paramc, align 4
  %12 = load i32, ptr %edt_6_paramc, align 4
  %edt_6_paramv = alloca i64, i32 %12, align 8
  %edt_6.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %edt_6_paramv, i64 0
  store i64 %6, ptr %edt_6.paramv_0.guid.edt_2, align 8
  %13 = alloca i32, align 4
  store i32 2, ptr %13, align 4
  %14 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @13, i32 0, i32 0))
  %15 = load i32, ptr %13, align 4
  %16 = call i64 @artsEdtCreateWithGuid(ptr @edt_6.task, i64 %4, i32 %12, ptr %edt_6_paramv, i32 %15)
  br label %exit.ret

exit.ret:                                         ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %exit.ret
  %toedt.4.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.4.slot.1, align 4
  %17 = load i32, ptr %toedt.4.slot.1, align 4
  %18 = load i32, ptr %edt_0.depv_1.ptr.load, align 4
  %19 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @19, i32 0, i32 0), i64 %edt_0.depv_1.guid.load, i32 %18, ptr %edt_0.depv_1.ptr.load, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %17, i64 %edt_0.depv_1.guid.load)
  %toedt.4.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.4.slot.0, align 4
  %20 = load i32, ptr %toedt.4.slot.0, align 4
  %21 = load i32, ptr %edt_0.depv_0.ptr.load, align 4
  %22 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([107 x i8], ptr @20, i32 0, i32 0), i64 %edt_0.depv_0.guid.load, i32 %21, ptr %edt_0.depv_0.ptr.load, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %20, i64 %edt_0.depv_0.guid.load)
  ret void
}

declare i64 @artsDbCreatePtr(ptr, i64, i32)

declare i64 @artsEdtCreateWithGuid(ptr, i64, i32, ptr, i32)

declare void @artsSignalEdt(i64, i32, i64)

define dso_local void @initPerNode(i32 %nodeId, i32 %argc, ptr %argv) {
entry:
  ret void
}

define dso_local void @initPerWorker(i32 %nodeId, i32 %workerId, i32 %argc, ptr %argv) {
entry:
  %0 = icmp eq i32 %nodeId, 0
  %1 = icmp eq i32 %workerId, 0
  %2 = and i1 %0, %1
  br i1 %2, label %then, label %exit

then:                                             ; preds = %entry
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1.main_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_1.main_guid.addr, align 8
  %4 = load i64, ptr %edt_1.main_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @8, i32 0, i32 0), i64 %4)
  br label %body

body:                                             ; preds = %then
  %edt_1_paramc = alloca i32, align 4
  store i32 0, ptr %edt_1_paramc, align 4
  %6 = load i32, ptr %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %6, align 8
  %7 = alloca i32, align 4
  store i32 0, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @10, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.main, i64 %4, i32 %6, ptr %edt_1_paramv, i32 %9)
  br label %exit

exit:                                             ; preds = %body, %entry
  ret void
}

define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @artsRT(i32 %argc, ptr %argv)
  ret i32 0
}

declare void @artsRT(i32, ptr)

declare void @artsShutdown()

attributes #0 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { convergent nounwind }
attributes #4 = { nounwind }
attributes #5 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!7}
!7 = !{i64 2, i64 -1, i64 -1, i1 true}
!8 = !{!9}
!9 = distinct !{!9, !10, !".omp_outlined.: %__context"}
!10 = distinct !{!10, !".omp_outlined."}
!11 = !{!12, !12, i64 0}
!12 = !{!"int", !13, i64 0}
!13 = !{!"omnipotent char", !14, i64 0}
!14 = !{!"Simple C++ TBAA"}
!15 = !{!16}
!16 = distinct !{!16, !17, !".omp_outlined..3: %__context"}
!17 = distinct !{!17, !".omp_outlined..3"}


-------------------------------------------------
[arts-graph] Destroying the CARTS Graph
llvm-dis taskwait_arts_analysis.bc
opt -O3 taskwait_arts_analysis.bc -o taskwait_opt.bc
llvm-dis taskwait_opt.bc
clang++ taskwait_opt.bc -O3 -g3 -march=native -o taskwait_opt -lstdc++ -I/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/include -L/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm
