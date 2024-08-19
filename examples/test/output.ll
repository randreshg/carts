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

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #3

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
        - Number of DependentChildEDTs: 0
     - EDT #0 signals to EDT #2
        - Inserting DataBlockEdge from "EDT #0" to "EDT #2" in Slot #1

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
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
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
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #3
DataBlock ->
     - Context: EDT #3 / Slot 0
     - SignalEDT: EDT #2
     - ParentCtx: EDT #0 / Slot 0
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
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 0
     - ChildDBss: 1
      - ChildDB: EDT #3 / Slot 0
     - DependentSiblingEDT: EDT #2 / Slot #0
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - SignalEDT: EDT #2
     - DependentSiblingEDT: EDT #2 / Slot #1
     - #DependentChildEDTs: 0{}
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
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #2" in Slot #0
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
    - [DataBlock] "EDT #0" in Slot #1
      -   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #0
      -   %shared_number = alloca i32, align 4
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
      - [DataBlock] ptr %0 /   %shared_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
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
      - [DataBlock]   %shared_number = alloca i32, align 4
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4
  - Outgoing Edges:
    - [Creation] "EDT #3"
    - [DataBlock] "EDT #3" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
    - [DataBlock] "EDT #2" in Slot #1
      -   %random_number = alloca i32, align 4

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
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
 - Inserting EDT Call
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
 - Inserting EDT Call
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
 - EDT Call:   %14 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %4, i32 %10, ptr %edt_2_paramv, i32 %13)

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
 - EDT Call:   %14 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.parallel, i64 %1, i32 %10, ptr %edt_0_paramv, i32 %13)

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %6 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !6
 - ParamVGuid[1]: EDT2
 - Inserting EDT Call
 - EDT Call:   %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %7, ptr %edt_3_paramv, i32 %11)

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #0

Signal DB ptr %0 from EDT #0 to EDT #3
 - We have a parent
 - DBPtr:   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
 - DBGuid:   %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8

Signal DB   %random_number = alloca i32, align 4 from EDT #0 to EDT #2
 - Using DepSlot of FromEDT
 - DBPtr:   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - DBGuid:   %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8
[arts-codegen] Inserting Signals from EDT #2
[arts-codegen] Inserting Signals from EDT #1

Signal DB   %random_number = alloca i32, align 4 from EDT #1 to EDT #0
 - We created the EDT
 - DBPtr:   %db.random_number.ptr = alloca ptr, align 8
 - DBGuid:   %6 = call i64 @artsDbCreate(ptr %db.random_number.ptr, i64 %db.random_number.size.ld, i32 7)

Signal DB   %shared_number = alloca i32, align 4 from EDT #1 to EDT #0
 - We created the EDT
 - DBPtr:   %db.shared_number.ptr = alloca ptr, align 8
 - DBGuid:   %8 = call i64 @artsDbCreate(ptr %db.shared_number.ptr, i64 %db.shared_number.size.ld, i32 7)
[arts-codegen] Inserting Signals from EDT #3

Signal DB ptr %0 from EDT #3 to EDT #2
 - Using DepSlot of FromEDT
 - DBPtr:   %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
 - DBGuid:   %edt_3.depv_0.guid.load = load i64, ptr %edt_3.depv_0.guid, align 8
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

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1
@2 = private unnamed_addr constant [29 x i8] c"Guid for edt_0.parallel: %u\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt_2.task: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt_1.main: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt_3.task: %u\0A\00", align 1
@6 = private unnamed_addr constant [43 x i8] c"Creating DB \22db.random_number\22 - Guid: %u\0A\00", align 1
@7 = private unnamed_addr constant [43 x i8] c"Creating DB \22db.shared_number\22 - Guid: %u\0A\00", align 1
@8 = private unnamed_addr constant [17 x i8] c"Creating EDT #1\0A\00", align 1
@9 = private unnamed_addr constant [17 x i8] c"Creating EDT #2\0A\00", align 1
@10 = private unnamed_addr constant [17 x i8] c"Creating EDT #0\0A\00", align 1
@11 = private unnamed_addr constant [17 x i8] c"Creating EDT #3\0A\00", align 1
@12 = private unnamed_addr constant [91 x i8] c"Signaling db.shared_number with guid: %u and Value %d from EDT #0 to EDT #3 with guid: %u\0A\00", align 1
@13 = private unnamed_addr constant [91 x i8] c"Signaling db.random_number with guid: %u and Value %d from EDT #0 to EDT #2 with guid: %u\0A\00", align 1
@14 = private unnamed_addr constant [91 x i8] c"Signaling db.random_number with guid: %u and Value %d from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@15 = private unnamed_addr constant [91 x i8] c"Signaling db.shared_number with guid: %u and Value %d from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@16 = private unnamed_addr constant [91 x i8] c"Signaling db.shared_number with guid: %u and Value %d from EDT #3 to EDT #2 with guid: %u\0A\00", align 1

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
  %edt_0.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %3 = load i64, ptr %edt_0.paramv_0.guid.edt_2, align 8
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
  %4 = load i32, ptr %edt_0.depv_0.ptr.load, align 4, !tbaa !8
  %5 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %4, i32 noundef %5) #3
  %6 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !8
  %edt_3_paramc = alloca i32, align 4
  store i32 2, ptr %edt_3_paramc, align 4
  %7 = load i32, ptr %edt_3_paramc, align 4
  %edt_3_paramv = alloca i64, i32 %7, align 8
  %edt_3.paramv_0 = getelementptr inbounds i64, ptr %edt_3_paramv, i64 0
  %8 = sext i32 %6 to i64
  store i64 %8, ptr %edt_3.paramv_0, align 8
  %edt_3.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %edt_3_paramv, i64 1
  store i64 %3, ptr %edt_3.paramv_1.guid.edt_2, align 8
  %9 = alloca i32, align 4
  store i32 1, ptr %9, align 4
  %10 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @11, i32 0, i32 0))
  %11 = load i32, ptr %9, align 4
  %12 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %1, i32 %7, ptr %edt_3_paramv, i32 %11)
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.3.slot.0, align 4
  %13 = load i32, ptr %toedt.3.slot.0, align 4
  %14 = load i32, ptr %edt_0.depv_0.ptr.load, align 4
  %15 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([91 x i8], ptr @12, i32 0, i32 0), i64 %edt_0.depv_0.guid.load, i32 %14, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %13, i64 %edt_0.depv_0.guid.load)
  %toedt.2.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.2.slot.1, align 4
  %16 = load i32, ptr %toedt.2.slot.1, align 4
  %17 = load i32, ptr %edt_0.depv_1.ptr.load, align 4
  %18 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([91 x i8], ptr @13, i32 0, i32 0), i64 %edt_0.depv_1.guid.load, i32 %17, i64 %3)
  call void @artsSignalEdt(i64 %3, i32 %16, i64 %edt_0.depv_1.guid.load)
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
  %db.random_number.ptr = alloca ptr, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %6 = call i64 @artsDbCreate(ptr %db.random_number.ptr, i64 %db.random_number.size.ld, i32 7)
  %7 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([43 x i8], ptr @6, i32 0, i32 0), i64 %6)
  %db.shared_number.ptr = alloca ptr, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %8 = call i64 @artsDbCreate(ptr %db.shared_number.ptr, i64 %db.shared_number.size.ld, i32 7)
  %9 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([43 x i8], ptr @7, i32 0, i32 0), i64 %8)
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
  store i32 %add, ptr %db.shared_number.ptr, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #3
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %db.random_number.ptr, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #3
  %edt_0_paramc = alloca i32, align 4
  store i32 1, ptr %edt_0_paramc, align 4
  %10 = load i32, ptr %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %10, align 8
  %edt_0.paramv_0.guid.edt_2 = getelementptr inbounds i64, ptr %edt_0_paramv, i64 0
  store i64 %4, ptr %edt_0.paramv_0.guid.edt_2, align 8
  %11 = alloca i32, align 4
  store i32 2, ptr %11, align 4
  %12 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @10, i32 0, i32 0))
  %13 = load i32, ptr %11, align 4
  %14 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.parallel, i64 %1, i32 %10, ptr %edt_0_paramv, i32 %13)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt_2_paramc = alloca i32, align 4
  store i32 0, ptr %edt_2_paramc, align 4
  %15 = load i32, ptr %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %15, align 8
  %16 = alloca i32, align 4
  store i32 2, ptr %16, align 4
  %17 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @9, i32 0, i32 0))
  %18 = load i32, ptr %16, align 4
  %19 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.task, i64 %4, i32 %15, ptr %edt_2_paramv, i32 %18)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %toedt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.0.slot.1, align 4
  %20 = load i32, ptr %toedt.0.slot.1, align 4
  %21 = load i32, ptr %db.random_number.ptr, align 4
  %22 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([91 x i8], ptr @14, i32 0, i32 0), i64 %6, i32 %21, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %20, i64 %6)
  %toedt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.0.slot.0, align 4
  %23 = load i32, ptr %toedt.0.slot.0, align 4
  %24 = load i32, ptr %db.shared_number.ptr, align 4
  %25 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([91 x i8], ptr @15, i32 0, i32 0), i64 %8, i32 %24, i64 %1)
  call void @artsSignalEdt(i64 %1, i32 %23, i64 %8)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

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
  %0 = load i32, ptr %edt_2.depv_0.ptr.load, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %edt_2.depv_0.ptr.load, align 4, !tbaa !8
  %1 = load i32, ptr %edt_2.depv_1.ptr.load, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1) #3
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt_3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_3.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt_3.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt_3.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_3.paramv_1.guid.edt_2, align 8
  %edt_3.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_3.depv_0.guid.load = load i64, ptr %edt_3.depv_0.guid, align 8
  %edt_3.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %3 = load i32, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !8, !noalias !12
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !8, !noalias !12
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #3, !noalias !12
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.2.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.2.slot.0, align 4
  %4 = load i32, ptr %toedt.2.slot.0, align 4
  %5 = load i32, ptr %edt_3.depv_0.ptr.load, align 4
  %6 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([91 x i8], ptr @16, i32 0, i32 0), i64 %edt_3.depv_0.guid.load, i32 %5, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt_3.depv_0.guid.load)
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
  %edt_1_paramc = alloca i32, align 4
  store i32 0, ptr %edt_1_paramc, align 4
  %6 = load i32, ptr %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %6, align 8
  %7 = alloca i32, align 4
  store i32 0, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([17 x i8], ptr @8, i32 0, i32 0))
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
