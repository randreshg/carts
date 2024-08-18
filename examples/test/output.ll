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


- EDT #0: edt.0.parallel
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


- EDT #3: edt.3.task
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


- EDT #1: edt.1.main
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


- EDT #2: edt.2.task
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
- EDT #3 - "edt.3.task"
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
- EDT #1 - "edt.1.main"
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
- EDT #2 - "edt.2.task"
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
- EDT #0 - "edt.0.parallel"
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
[arts-codegen] Inserting Call for EDT #1
 - Inserting EDT Call

Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
   - ParamVGuid[0]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
   - DepV[1]: ptr %1 ->   %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
[arts-codegen] Inserting Call for EDT #0
 - ParamVGuid[0]: EDT2
 - Inserting EDT Call

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1     - Value is an Integer
 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %4 = load i32, ptr %depv.0, align 4, !tbaa !6
 - ParamVGuid[1]: EDT2
 - Inserting EDT Call

Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
   - DepV[1]: ptr %random_number ->   %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
[arts-codegen] Inserting Call for EDT #2
 - Inserting EDT Call

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #0
define internal void @edt.0.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.3.task_guid.addr, align 8
  %1 = load i64, ptr %edt.3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  %4 = load i32, ptr %depv.0, align 4, !tbaa !15
  %edt.3.task_paramc = alloca i32, align 4
  store i32 2, ptr %edt.3.task_paramc, align 4
  %5 = load i32, ptr %edt.3.task_paramc, align 4
  %edt.3.task_paramv = alloca i64, i32 %5, align 8
  %edt.3.task_paramv.0 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt.3.task_paramv.0, align 8
  %edt.3.task_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 1
  store i64 %3, ptr %edt.3.task_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.3.task, i64 %1, i32 %5, ptr %edt.3.task_paramv, i32 %9)
  call void @carts.edt.2(ptr nocapture %depv.1, i32 %4) #8
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}
Signal DB   %random_number = alloca i32, align 4 from EDT #0 to EDT #2
 - FromEDT: 0 / DBContextEDT: 0
Signal DB ptr %1 from EDT #0 to EDT #3
 - FromEDT: 0 / DBContextEDT: 3
[arts-codegen] Inserting Signals from EDT #2
define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.shared_number, align 4, !tbaa !15
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %depv.shared_number, align 4, !tbaa !15
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !15
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc, i32 noundef %1) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  ret void
}
[arts-codegen] Inserting Signals from EDT #1
define internal void @edt.1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.0.parallel_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.0.parallel_guid.addr, align 8
  %1 = load i64, ptr %edt.0.parallel_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([29 x i8], ptr @2, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.2.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt.2.task_guid.addr, align 8
  %4 = load i64, ptr %edt.2.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %4)
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
  %edt.0.parallel_paramc = alloca i32, align 4
  store i32 1, ptr %edt.0.parallel_paramc, align 4
  %6 = load i32, ptr %edt.0.parallel_paramc, align 4
  %edt.0.parallel_paramv = alloca i64, i32 %6, align 8
  %edt.0.parallel_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.0.parallel_paramv, i64 0
  store i64 %4, ptr %edt.0.parallel_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 2, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @7, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.0.parallel, i64 %1, i32 %6, ptr %edt.0.parallel_paramv, i32 %9)
  call void @carts.edt.1(ptr nocapture %random_number, ptr nocapture %shared_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt.2.task_paramc = alloca i32, align 4
  store i32 0, ptr %edt.2.task_paramc, align 4
  %11 = load i32, ptr %edt.2.task_paramc, align 4
  %edt.2.task_paramv = alloca i64, i32 %11, align 8
  %12 = alloca i32, align 4
  store i32 2, ptr %12, align 4
  %13 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %14 = load i32, ptr %12, align 4
  %15 = call i64 @artsEdtCreateWithGuid(ptr @edt.2.task, i64 %4, i32 %11, ptr %edt.2.task_paramv, i32 %14)
  call void @carts.edt.3(ptr nocapture %shared_number, ptr nocapture %random_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}
Signal DB   %shared_number = alloca i32, align 4 from EDT #1 to EDT #0
 - FromEDT: 1 / DBContextEDT: 0
Signal DB   %random_number = alloca i32, align 4 from EDT #1 to EDT #0
 - FromEDT: 1 / DBContextEDT: 0
[arts-codegen] Inserting Signals from EDT #3
define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %3 = load i32, ptr %depv.0, align 4, !tbaa !15, !noalias !19
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %depv.0, align 4, !tbaa !15, !noalias !19
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %inc.i, i32 noundef %inc1.i) #5, !noalias !19
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}
Signal DB ptr %1 from EDT #3 to EDT #2
 - FromEDT: 3 / DBContextEDT: 3
[arts-codegen] Inserting Init Functions
[arts-codegen] Inserting ARTS Shutdown Function

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
@2 = private unnamed_addr constant [29 x i8] c"Guid for edt.0.parallel: %u\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt.2.task: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt.1.main: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt.3.task: %u\0A\00", align 1
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

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !6 internal void @carts.edt.1(ptr nocapture readonly, ptr nocapture) #3

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt.2(ptr nocapture, i32) #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !10 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: norecurse nounwind
declare !carts !12 internal void @carts.edt() #6

; Function Attrs: norecurse nounwind
declare !carts !14 internal void @carts.edt.3(ptr nocapture, ptr nocapture readonly) #6

define internal void @edt.0.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.3.task_guid.addr, align 8
  %1 = load i64, ptr %edt.3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  %4 = load i32, ptr %depv.0, align 4, !tbaa !15
  %edt.3.task_paramc = alloca i32, align 4
  store i32 2, ptr %edt.3.task_paramc, align 4
  %5 = load i32, ptr %edt.3.task_paramc, align 4
  %edt.3.task_paramv = alloca i64, i32 %5, align 8
  %edt.3.task_paramv.0 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt.3.task_paramv.0, align 8
  %edt.3.task_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 1
  store i64 %3, ptr %edt.3.task_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.3.task, i64 %1, i32 %5, ptr %edt.3.task_paramv, i32 %9)
  call void @carts.edt.2(ptr nocapture %depv.1, i32 %4) #7
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.0.slot.1, align 4
  %11 = load i32, ptr %edt.0.slot.1, align 4
  %12 = load i64, ptr %depv.0, align 8
  call void @artsSignalEdtValue(i64 %3, i32 %11, i64 %12)
  %edt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.0.slot.0, align 4
  %13 = load i32, ptr %edt.0.slot.0, align 4
  %14 = load i64, ptr %depv.1, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %13, i64 %14)
  ret void
}

define internal void @edt.1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.0.parallel_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.0.parallel_guid.addr, align 8
  %1 = load i64, ptr %edt.0.parallel_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([29 x i8], ptr @2, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.2.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt.2.task_guid.addr, align 8
  %4 = load i64, ptr %edt.2.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %4)
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
  store i32 %add, ptr %shared_number, align 4, !tbaa !15
  %call2 = tail call i32 @rand() #4
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !15
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #4
  %edt.0.parallel_paramc = alloca i32, align 4
  store i32 1, ptr %edt.0.parallel_paramc, align 4
  %6 = load i32, ptr %edt.0.parallel_paramc, align 4
  %edt.0.parallel_paramv = alloca i64, i32 %6, align 8
  %edt.0.parallel_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.0.parallel_paramv, i64 0
  store i64 %4, ptr %edt.0.parallel_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 2, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @7, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.0.parallel, i64 %1, i32 %6, ptr %edt.0.parallel_paramv, i32 %9)
  call void @carts.edt.1(ptr nocapture %random_number, ptr nocapture %shared_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt.2.task_paramc = alloca i32, align 4
  store i32 0, ptr %edt.2.task_paramc, align 4
  %11 = load i32, ptr %edt.2.task_paramc, align 4
  %edt.2.task_paramv = alloca i64, i32 %11, align 8
  %12 = alloca i32, align 4
  store i32 2, ptr %12, align 4
  %13 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %14 = load i32, ptr %12, align 4
  %15 = call i64 @artsEdtCreateWithGuid(ptr @edt.2.task, i64 %4, i32 %11, ptr %edt.2.task_paramv, i32 %14)
  call void @carts.edt.3(ptr nocapture %shared_number, ptr nocapture %random_number) #4
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %edt.1.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.1.slot.1, align 4
  %16 = load i32, ptr %edt.1.slot.1, align 4
  %17 = load i64, ptr %shared_number, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %16, i64 %17)
  %edt.1.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.1.slot.0, align 4
  %18 = load i32, ptr %edt.1.slot.0, align 4
  %19 = load i64, ptr %random_number, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %18, i64 %19)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.shared_number, align 4, !tbaa !15
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %depv.shared_number, align 4, !tbaa !15
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !15
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc, i32 noundef %1) #4
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %3 = load i32, ptr %depv.0, align 4, !tbaa !15, !noalias !19
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %depv.0, align 4, !tbaa !15, !noalias !19
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %inc.i, i32 noundef %inc1.i) #4, !noalias !19
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.3.slot.0, align 4
  %4 = load i32, ptr %edt.3.slot.0, align 4
  %5 = load i64, ptr %depv.0, align 8
  call void @artsSignalEdtValue(i64 %2, i32 %4, i64 %5)
  ret void
}

declare i64 @artsEdtCreateWithGuid(ptr, i64, i32, ptr, i32)

declare void @artsSignalEdtValue(i64, i32, i64)

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
  %edt.1.main_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt.1.main_guid.addr, align 8
  %4 = load i64, ptr %edt.1.main_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @4, i32 0, i32 0), i64 %4)
  br label %body

body:                                             ; preds = %then
  %edt.1.main_paramc = alloca i32, align 4
  store i32 0, ptr %edt.1.main_paramc, align 4
  %6 = load i32, ptr %edt.1.main_paramc, align 4
  %edt.1.main_paramv = alloca i64, i32 %6, align 8
  %7 = alloca i32, align 4
  store i32 0, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @6, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.1.main, i64 %4, i32 %6, ptr %edt.1.main_paramv, i32 %9)
  call void @carts.edt()
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
attributes #3 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #4 = { nounwind }
attributes #5 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #6 = { norecurse nounwind }
attributes #7 = { nounwind memory(readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"parallel", !7}
!7 = !{!"dep", !"dep"}
!8 = !{!"task", !9}
!9 = !{!"dep", !"param"}
!10 = !{!11}
!11 = !{i64 2, i64 -1, i64 -1, i1 true}
!12 = !{!"main", !13}
!13 = !{}
!14 = !{!"task", !7}
!15 = !{!16, !16, i64 0}
!16 = !{!"int", !17, i64 0}
!17 = !{!"omnipotent char", !18, i64 0}
!18 = !{!"Simple C++ TBAA"}
!19 = !{!20}
!20 = distinct !{!20, !21, !".omp_outlined.: %__context"}
!21 = distinct !{!21, !".omp_outlined."}


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
@2 = private unnamed_addr constant [29 x i8] c"Guid for edt.0.parallel: %u\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt.2.task: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt.1.main: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt.3.task: %u\0A\00", align 1
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

define internal void @edt.0.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.3.task_guid.addr, align 8
  %1 = load i64, ptr %edt.3.task_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @5, i32 0, i32 0), i64 %1)
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  %4 = load i32, ptr %depv.0, align 4, !tbaa !8
  %edt.3.task_paramc = alloca i32, align 4
  store i32 2, ptr %edt.3.task_paramc, align 4
  %5 = load i32, ptr %edt.3.task_paramc, align 4
  %edt.3.task_paramv = alloca i64, i32 %5, align 8
  %edt.3.task_paramv.0 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 0
  %6 = sext i32 %4 to i64
  store i64 %6, ptr %edt.3.task_paramv.0, align 8
  %edt.3.task_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 1
  store i64 %3, ptr %edt.3.task_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.3.task, i64 %1, i32 %5, ptr %edt.3.task_paramv, i32 %9)
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.0.slot.1, align 4
  %11 = load i32, ptr %edt.0.slot.1, align 4
  %12 = load i64, ptr %depv.0, align 8
  call void @artsSignalEdtValue(i64 %3, i32 %11, i64 %12)
  %edt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.0.slot.0, align 4
  %13 = load i32, ptr %edt.0.slot.0, align 4
  %14 = load i64, ptr %depv.1, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %13, i64 %14)
  ret void
}

define internal void @edt.1.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.0.parallel_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt.0.parallel_guid.addr, align 8
  %1 = load i64, ptr %edt.0.parallel_guid.addr, align 8
  %2 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([29 x i8], ptr @2, i32 0, i32 0), i64 %1)
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt.2.task_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt.2.task_guid.addr, align 8
  %4 = load i64, ptr %edt.2.task_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @3, i32 0, i32 0), i64 %4)
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
  %edt.0.parallel_paramc = alloca i32, align 4
  store i32 1, ptr %edt.0.parallel_paramc, align 4
  %6 = load i32, ptr %edt.0.parallel_paramc, align 4
  %edt.0.parallel_paramv = alloca i64, i32 %6, align 8
  %edt.0.parallel_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.0.parallel_paramv, i64 0
  store i64 %4, ptr %edt.0.parallel_paramv_guid.edt_2, align 8
  %7 = alloca i32, align 4
  store i32 2, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @7, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.0.parallel, i64 %1, i32 %6, ptr %edt.0.parallel_paramv, i32 %9)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt.2.task_paramc = alloca i32, align 4
  store i32 0, ptr %edt.2.task_paramc, align 4
  %11 = load i32, ptr %edt.2.task_paramc, align 4
  %edt.2.task_paramv = alloca i64, i32 %11, align 8
  %12 = alloca i32, align 4
  store i32 2, ptr %12, align 4
  %13 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %14 = load i32, ptr %12, align 4
  %15 = call i64 @artsEdtCreateWithGuid(ptr @edt.2.task, i64 %4, i32 %11, ptr %edt.2.task_paramv, i32 %14)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %edt.1.slot.1 = alloca i32, align 4
  store i32 1, ptr %edt.1.slot.1, align 4
  %16 = load i32, ptr %edt.1.slot.1, align 4
  %17 = load i64, ptr %shared_number, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %16, i64 %17)
  %edt.1.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.1.slot.0, align 4
  %18 = load i32, ptr %edt.1.slot.0, align 4
  %19 = load i64, ptr %random_number, align 8
  call void @artsSignalEdtValue(i64 %1, i32 %18, i64 %19)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.shared_number, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %depv.shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc, i32 noundef %1) #3
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %paramv.guid.edt_2, align 8
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %3 = load i32, ptr %depv.0, align 4, !tbaa !8, !noalias !12
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %depv.0, align 4, !tbaa !8, !noalias !12
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %inc.i, i32 noundef %inc1.i) #3, !noalias !12
  br label %exit

exit:                                             ; preds = %edt.body
  %edt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %edt.3.slot.0, align 4
  %4 = load i32, ptr %edt.3.slot.0, align 4
  %5 = load i64, ptr %depv.0, align 8
  call void @artsSignalEdtValue(i64 %2, i32 %4, i64 %5)
  ret void
}

declare i64 @artsEdtCreateWithGuid(ptr, i64, i32, ptr, i32)

declare void @artsSignalEdtValue(i64, i32, i64)

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
  %edt.1.main_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt.1.main_guid.addr, align 8
  %4 = load i64, ptr %edt.1.main_guid.addr, align 8
  %5 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([25 x i8], ptr @4, i32 0, i32 0), i64 %4)
  br label %body

body:                                             ; preds = %then
  %edt.1.main_paramc = alloca i32, align 4
  store i32 0, ptr %edt.1.main_paramc, align 4
  %6 = load i32, ptr %edt.1.main_paramc, align 4
  %edt.1.main_paramv = alloca i64, i32 %6, align 8
  %7 = alloca i32, align 4
  store i32 0, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @6, i32 0, i32 0))
  %9 = load i32, ptr %7, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt.1.main, i64 %4, i32 %6, ptr %edt.1.main_paramv, i32 %9)
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
llvm-dis test_arts_analysis.bc
opt -O3 test_arts_analysis.bc -o test_opt.bc
llvm-dis test_opt.bc
clang++ test_opt.bc -O3 -g3 -march=native -o test_opt -lstdc++ -I/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/include -L/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm
