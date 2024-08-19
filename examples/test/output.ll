clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" test_arts_ir.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_arts_ir.bc
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
[arts] Creating Parallel EDT for function: carts.edt.1

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt.1"
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Main EDT for function: carts.edt
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.3
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.3
   - DoneEDT: EDT #2
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.2
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.2

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.2"

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
[arts] Function: main doesn't have CARTS Metadata
   - The ContextEDT is the MainEDT

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.3"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #3 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #3"

[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #0

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
        - Inserting DataBlockEdge from "EDT #0" to "EDT #3" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #3

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
   - All DataBlocks were fixed for EDT #3

[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #0 is a child of EDT #1
        - Inserting CreationEdge from "EDT #1" to "EDT #0"
   - EDT #2 is a child of EDT #1
        - Inserting CreationEdge from "EDT #1" to "EDT #2"

[AAEDTInfoFunction::updateImpl] EDT #2
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

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

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #3 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0

[AAEDTInfoCallsite::initialize] EDT #0

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 0
     - EDT #0 signals to EDT #2
        - Inserting DataBlockEdge from "EDT #0" to "EDT #2" in Slot #1

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
        - Inserting DataBlockEdge from "EDT #1" to "EDT #0" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #0 is a child of EDT #1
   - EDT #2 is a child of EDT #1
   - All ReachedEDTs are fixed for EDT #1

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1
   - All DataBlocks were fixed for EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
        - Inserting DataBlockEdge from "EDT #3" to "EDT #2" in Slot #0
-------------------------------
[AAEDTInfoFunction::manifest] <invalid>

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #1
     -ParentSyncEDT: <null>
     #Child EDTs: 1{3}
     #Reached DescendantEDTs: 0{}


- EDT #0: edt_0.parallel
Ty: parallel
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - ptr %1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #0
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
State for EDT #1 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{0, 2}
     #Reached DescendantEDTs: 1{3}


- EDT #1: edt_1.main
Ty: main
Number of slots: 0
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0
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
        - Inserting Guid of child "EDT #2" in the edge from "EDT #1" to "EDT #0"
        - Inserting Guid of "EDT #2" in the  from "EDT #0" to "EDT #3"
-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #3
DataBlock ->
     - Context: EDT #3 / Slot 0
     - SignalEDT: EDT #2
     - ParentCtx: EDT #0 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

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
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 0
     - SignalEDT: EDT #2
     - DependentSiblingEDT: EDT #2 / Slot #1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - ChildDBss: 1
      - ChildDB: EDT #3 / Slot 0
     - DependentSiblingEDT: EDT #2 / Slot #0
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{2}

[Attributor] Done with 5 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt_3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %1 /   %shared_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #2" in Slot #0
      - ptr %1 /   %shared_number = alloca i32, align 4
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
      - [DataBlock] ptr %1 /   %shared_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
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
    - [Creation] "EDT #2"
    - [Creation] "EDT #0"
    - [DataBlock] "EDT #0" in Slot #1
      -   %shared_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #0
      -   %random_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt_0.parallel"
  - Type: parallel
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [Creation] "EDT #1"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock]   %random_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock]   %shared_number = alloca i32, align 4
  - Outgoing Edges:
    - [Creation] "EDT #3"
    - [DataBlock] "EDT #2" in Slot #1
      -   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #3" in Slot #0
      - ptr %1 /   %shared_number = alloca i32, align 4

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Reserving GUID for EDT #1
     EDT #1 doesn't have a parent EDT, using parent function
[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #3

All EDT Guids have been reserved

Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
     EDT #1 doesn't have a parent EDT
 - Inserting DepV
     EDT #1 doesn't have incoming slot nodes
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
[arts-codegen] Inserting Call for EDT #1
 - Inserting EDT Call
 - EDT Call:   %7 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.main, i64 %1, i32 %3, ptr %edt1_paramv, i32 %6)

Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
   - ParamVGuid[0]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt0depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
   - DepV[1]: ptr %1 ->   %edt0depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
[arts-codegen] Inserting Call for EDT #0
 - ParamVGuid[0]: EDT2
 - Inserting EDT Call
 - EDT Call:   %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.parallel, i64 %1, i32 %8, ptr %edt0_paramv, i32 %11)

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt3depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %4 = load i32, ptr %edt0depv_0.ptr, align 4, !tbaa !6
 - ParamVGuid[1]: EDT2
 - Inserting EDT Call
 - EDT Call:   %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %5, ptr %edt3_paramv, i32 %9)

Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %edt2depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
   - DepV[1]: ptr %random_number ->   %edt2depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
[arts-codegen] Inserting Call for EDT #2
 - Inserting EDT Call
 - EDT Call:   %17 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %4, i32 %13, ptr %edt2_paramv, i32 %16)

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #0
define internal void @edt_0.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_3.task_guid.addr, align 8
  %1 = load i64, ptr %edt_3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %edt0paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %edt0paramv_0.guid.edt_2, align 8
  %edt0depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt0depv_0.guid.load = load i64, ptr %edt0depv_0.guid, align 8
  %edt0depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  %edt0depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt0depv_1.guid.load = load i64, ptr %edt0depv_1.guid, align 8
  %edt0depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  %4 = load i32, ptr %edt0depv_0.ptr, align 4, !tbaa !15
  %edt3_paramc = alloca i32, align 4
  store i32 2, ptr %edt3_paramc, align 4
  %5 = load i32, ptr %edt3_paramc, align 4
  %edt3_paramv = alloca i64, i32 %5, align 8
  %edt3.paramv_0 = getelementptr inbounds i64, ptr %edt3_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt3.paramv_0, align 8
  %"edt3.paramv_1.guid.edt_ 2" = getelementptr inbounds i64, ptr %edt3_paramv, i64 1
  store i64 %3, ptr %"edt3.paramv_1.guid.edt_ 2", align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %5, ptr %edt3_paramv, i32 %9)
  call void @carts.edt.2(ptr nocapture %edt0depv_1.ptr, i32 %4) #8
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}


Signal DB   %random_number = alloca i32, align 4 from EDT #0 to EDT #2
 - DBPtr:   %edt0depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
 - DBGuid:   %edt0depv_0.guid.load = load i64, ptr %edt0depv_0.guid, align 8

Signal DB ptr %1 from EDT #0 to EDT #3
 - DBPtr:   %edt3depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
 - DBGuid:   %edt3depv_0.guid.load = load i64, ptr %edt3depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #2
define internal void @edt_2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt2depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt2depv_0.guid.load = load i64, ptr %edt2depv_0.guid, align 8
  %edt2depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  %edt2depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt2depv_1.guid.load = load i64, ptr %edt2depv_1.guid, align 8
  %edt2depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %edt2depv_0.ptr, align 4, !tbaa !15
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %edt2depv_0.ptr, align 4, !tbaa !15
  %1 = load i32, ptr %edt2depv_1.ptr, align 4, !tbaa !15
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc, i32 noundef %1) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  ret void
}

[arts-codegen] Inserting Signals from EDT #1
define internal void @edt_1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0.parallel_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_0.parallel_guid.addr, align 8
  %1 = load i64, ptr %edt_0.parallel_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([29 x i8], ptr @2, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_2.task_guid.addr, align 8
  %4 = load i64, ptr %edt_2.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %4)
  %db.shared_number.ptr = alloca ptr, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %6 = call i64 @artsDbCreate(ptr %db.shared_number.ptr, i64 %db.shared_number.size.ld, i32 7)
  %db.random_number.ptr = alloca ptr, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %7 = call i64 @artsDbCreate(ptr %db.random_number.ptr, i64 %db.random_number.size.ld, i32 7)
  br label %edt.body

edt.body:                                         ; preds = %entry
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #5
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #5
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !15
  %call2 = tail call i32 @rand() #5
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !15
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #5
  %edt0_paramc = alloca i32, align 4
  store i32 1, ptr %edt0_paramc, align 4
  %8 = load i32, ptr %edt0_paramc, align 4
  %edt0_paramv = alloca i64, i32 %8, align 8
  %"edt0.paramv_0.guid.edt_ 2" = getelementptr inbounds i64, ptr %edt0_paramv, i64 0
  store i64 %4, ptr %"edt0.paramv_0.guid.edt_ 2", align 8
  %9 = alloca i32, align 4
  store i32 2, ptr %9, align 4
  %10 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @7, i32 0, i32 0))
  %11 = load i32, ptr %9, align 4
  %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.parallel, i64 %1, i32 %8, ptr %edt0_paramv, i32 %11)
  call void @carts.edt.1(ptr nocapture %random_number, ptr nocapture %shared_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt2_paramc = alloca i32, align 4
  store i32 0, ptr %edt2_paramc, align 4
  %13 = load i32, ptr %edt2_paramc, align 4
  %edt2_paramv = alloca i64, i32 %13, align 8
  %14 = alloca i32, align 4
  store i32 2, ptr %14, align 4
  %15 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %16 = load i32, ptr %14, align 4
  %17 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %4, i32 %13, ptr %edt2_paramv, i32 %16)
  call void @carts.edt.3(ptr nocapture %shared_number, ptr nocapture %random_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}


Signal DB   %shared_number = alloca i32, align 4 from EDT #1 to EDT #0
 - DBPtr:   %db.shared_number.ptr = alloca ptr, align 8
 - DBGuid:   %6 = call i64 @artsDbCreate(ptr %db.shared_number.ptr, i64 %db.shared_number.size.ld, i32 7)

Signal DB   %random_number = alloca i32, align 4 from EDT #1 to EDT #0
 - DBPtr:   %db.random_number.ptr = alloca ptr, align 8
 - DBGuid:   %7 = call i64 @artsDbCreate(ptr %db.random_number.ptr, i64 %db.random_number.size.ld, i32 7)
[arts-codegen] Inserting Signals from EDT #3
define internal void @edt_3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt3.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt3.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt3paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt3paramv_1.guid.edt_2, align 8
  %edt3depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt3depv_0.guid.load = load i64, ptr %edt3depv_0.guid, align 8
  %edt3depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %3 = load i32, ptr %edt3depv_0.ptr, align 4, !tbaa !15, !noalias !19
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt3depv_0.ptr, align 4, !tbaa !15, !noalias !19
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %inc.i, i32 noundef %inc1.i) #5, !noalias !19
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}


Signal DB ptr %1 from EDT #3 to EDT #2
 - DBPtr:   %edt3depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
 - DBGuid:   %edt3depv_0.guid.load = load i64, ptr %edt3depv_0.guid, align 8
[arts-codegen] Inserting Init Functions
[arts-codegen] Inserting ARTS Shutdown Function

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'test_arts_ir.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { i64, i32, ptr }

@.str = private unnamed_addr constant [36 x i8] c"EDT 0: The initial number is %d/%d\0A\00", align 1
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.2 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1
@2 = private unnamed_addr constant [29 x i8] c"Guid for edt_0.parallel: %u\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt_2.task: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt_1.main: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt_3.task: %u\0A\00", align 1
@6 = private unnamed_addr constant [17 x i8] c"Creating EDT #1\0A\00", align 1
@7 = private unnamed_addr constant [17 x i8] c"Creating EDT #0\0A\00", align 1
@8 = private unnamed_addr constant [17 x i8] c"Creating EDT #3\0A\00", align 1
@9 = private unnamed_addr constant [17 x i8] c"Creating EDT #2\0A\00", align 1

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

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare !callback !6 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #4

define internal void @edt_0.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_3.task_guid.addr, align 8
  %1 = load i64, ptr %edt_3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %edt0paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %edt0paramv_0.guid.edt_2, align 8
  %edt0depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt0depv_0.guid.load = load i64, ptr %edt0depv_0.guid, align 8
  %edt0depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  %edt0depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt0depv_1.guid.load = load i64, ptr %edt0depv_1.guid, align 8
  %edt0depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  %4 = load i32, ptr %edt0depv_0.ptr, align 4, !tbaa !8
  %edt3_paramc = alloca i32, align 4
  store i32 2, ptr %edt3_paramc, align 4
  %5 = load i32, ptr %edt3_paramc, align 4
  %edt3_paramv = alloca i64, i32 %5, align 8
  %edt3.paramv_0 = getelementptr inbounds i64, ptr %edt3_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt3.paramv_0, align 8
  %"edt3.paramv_1.guid.edt_ 2" = getelementptr inbounds i64, ptr %edt3_paramv, i64 1
  store i64 %3, ptr %"edt3.paramv_1.guid.edt_ 2", align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %5, ptr %edt3_paramv, i32 %9)
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.0.slot.1, align 4
  %11 = load i32, ptr %edt.0.slot.1, align 4
  call void @artsSignalEdt(i64 %3, i32 %11, i64 %edt0depv_0.guid.load)
  %edt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.0.slot.0, align 4
  %12 = load i32, ptr %edt.0.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %12, i64 %edt3depv_0.guid.load)
  ret void
}

define internal void @edt_1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0.parallel_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_0.parallel_guid.addr, align 8
  %1 = load i64, ptr %edt_0.parallel_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([29 x i8], ptr @2, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_2.task_guid.addr, align 8
  %4 = load i64, ptr %edt_2.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %4)
  %db.shared_number.ptr = alloca ptr, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %6 = call i64 @artsDbCreate(ptr %db.shared_number.ptr, i64 %db.shared_number.size.ld, i32 7)
  %db.random_number.ptr = alloca ptr, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %7 = call i64 @artsDbCreate(ptr %db.random_number.ptr, i64 %db.random_number.size.ld, i32 7)
  br label %edt.body

edt.body:                                         ; preds = %entry
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #3
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #3
  %call1 = tail call i32 @rand() #3
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #3
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #3
  %edt0_paramc = alloca i32, align 4
  store i32 1, ptr %edt0_paramc, align 4
  %8 = load i32, ptr %edt0_paramc, align 4
  %edt0_paramv = alloca i64, i32 %8, align 8
  %"edt0.paramv_0.guid.edt_ 2" = getelementptr inbounds i64, ptr %edt0_paramv, i64 0
  store i64 %4, ptr %"edt0.paramv_0.guid.edt_ 2", align 8
  %9 = alloca i32, align 4
  store i32 2, ptr %9, align 4
  %10 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @7, i32 0, i32 0))
  %11 = load i32, ptr %9, align 4
  %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.parallel, i64 %1, i32 %8, ptr %edt0_paramv, i32 %11)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt2_paramc = alloca i32, align 4
  store i32 0, ptr %edt2_paramc, align 4
  %13 = load i32, ptr %edt2_paramc, align 4
  %edt2_paramv = alloca i64, i32 %13, align 8
  %14 = alloca i32, align 4
  store i32 2, ptr %14, align 4
  %15 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %16 = load i32, ptr %14, align 4
  %17 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %4, i32 %13, ptr %edt2_paramv, i32 %16)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %edt.1.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.1.slot.1, align 4
  %18 = load i32, ptr %edt.1.slot.1, align 4
  call void @artsSignalEdt(i64 %1, i32 %18, i64 %6)
  %edt.1.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.1.slot.0, align 4
  %19 = load i32, ptr %edt.1.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %19, i64 %7)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt_2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt2depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt2depv_0.guid.load = load i64, ptr %edt2depv_0.guid, align 8
  %edt2depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  %edt2depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt2depv_1.guid.load = load i64, ptr %edt2depv_1.guid, align 8
  %edt2depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %edt2depv_0.ptr, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %edt2depv_0.ptr, align 4, !tbaa !8
  %1 = load i32, ptr %edt2depv_1.ptr, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc, i32 noundef %1) #3
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt_3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt3.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt3.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt3paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt3paramv_1.guid.edt_2, align 8
  %edt3depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt3depv_0.guid.load = load i64, ptr %edt3depv_0.guid, align 8
  %edt3depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 1
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %3 = load i32, ptr %edt3depv_0.ptr, align 4, !tbaa !8, !noalias !12
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt3depv_0.ptr, align 4, !tbaa !8, !noalias !12
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %inc.i, i32 noundef %inc1.i) #3, !noalias !12
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.3.slot.0, align 4
  %4 = load i32, ptr %edt.3.slot.0, align 4
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt3depv_0.guid.load)
  ret void
}

declare i64 @artsDbCreate(ptr, i64, i32)

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
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @4, i32 0, i32 0), i64 %4)
  br label %body

body:                                             ; preds = %then
  %edt1_paramc = alloca i32, align 4
  store i32 0, ptr %edt1_paramc, align 4
  %6 = load i32, ptr %edt1_paramc, align 4
  %edt1_paramv = alloca i64, i32 %6, align 8
  %7 = alloca i32, align 4
  store i32 0, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @6, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.main, i64 %4, i32 %6, ptr %edt1_paramv, i32 %9)
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
attributes #3 = { nounwind }
attributes #4 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!13}
!13 = distinct !{!13, !14, !".omp_outlined.: %__context"}
!14 = distinct !{!14, !".omp_outlined."}


-------------------------------------------------
[arts-graph] Destroying the CARTS Graph
Instruction does not dominate all uses!
  %edt3depv_0.guid.load = load i64, ptr %edt3depv_0.guid, align 8
  call void @artsSignalEdt(i64 %1, i32 %12, i64 %edt3depv_0.guid.load)
LLVM ERROR: Broken module found, compilation aborted!
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007f57bdb7cef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f57bdb7ab7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f57bdb7d5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f57c08be910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f57bd487d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f57bd4893e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f57bdac8d3c llvm::report_fatal_error(llvm::Twine const&, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddd3c)
 #7 0x00007f57bdac8b66 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddb66)
 #8 0x00007f57bdfa9bca (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2f9bca)
 #9 0x000055ba8f77b32d llvm::detail::PassModel<llvm::Module, llvm::VerifierPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x2032d)
#10 0x00007f57bdf702a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#11 0x000055ba8f774293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#12 0x000055ba8f781aaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#13 0x00007f57bd47224d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x000055ba8f76da3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
