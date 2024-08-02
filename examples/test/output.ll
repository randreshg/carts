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
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Sync EDT for function: carts.edt
[arts] Creating Parallel EDT for function: carts.edt

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
   - ParentEDT: EDT #0
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.2
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.2
   - DoneEDT: EDT #2
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.1
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.1

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.1"
   - ParentEDT: EDT #1

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.2"
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

[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #1
        - Creating edge from "EDT #3" to "EDT #2"
          Control Edge
        - Converting Control edge to Data edge ["edt.1.parallel" -> "edt.3.task"]

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #3
     - EDT #3 signals to EDT #2
        - Converting Control edge to Data edge ["edt.3.task" -> "edt.2.task"]

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
   - All DataBlocks were fixed for EDT #3

[AAEDTInfoFunction::updateImpl] EDT #2
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - All DataBlocks were fixed for EDT #2

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #2 is a child of EDT #0

[AAEDTInfoFunction::updateImpl] EDT #1
   - EDT #3 is a child of EDT #1
   - All ReachedEDTs are fixed for EDT #1
        - Creating edge from "EDT #1" to "EDT #2"
          Control Edge
        - Converting Control edge to Data edge ["edt.0.main" -> "edt.1.parallel"]

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 0
     - EDT #1 signals to EDT #2
        - Converting Control edge to Data edge ["edt.1.parallel" -> "edt.2.task"]

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 1
   - All DataBlocks were fixed for EDT #1

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #2 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{1, 2}
     #Reached DescendantEDTs: 1{3}
     #Dependent EDTs: 0{}

- EDT #0: edt.0.main
Ty: main
Number of slots: 0
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 1{3}
     #Reached DescendantEDTs: 0{}
     #Dependent EDTs: 1{2}

- EDT #1: edt.1.parallel
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
     -ParentEDT: EDT #1
     -ParentSyncEDT: EDT #1
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #Dependent EDTs: 1{2}

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
State for EDT #2 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}
     #Dependent EDTs: 0{}

- EDT #2: edt.2.task
Ty: task
Number of slots: 2
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %shared_number
  - ptr %random_number
-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #3
ptr %1EDTDataBlock ->
     - Context: EDT #3 / Slot 0
     - SignalEDT: EDT #2
     - ParentDBCtx: EDT #1 / Slot 1
     - DoneDB: EDT #2
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - Context: EDT #2 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - Context: EDT #2 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - Context: EDT #1 / Slot 0
     - SignalEDT: EDT #2
     - DoneDB: EDT #2
     - DependentSiblingEDT: EDT #2 / Slot #1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - Context: EDT #1 / Slot 1
     - DoneDB: EDT #2
     - ChildrenDBs: 1
      - ChildDB: EDT #3 / Slot 0
     - DependentSiblingEDT: EDT #2 / Slot #0
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{2}

[Attributor] Done with 4 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt.3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Edges:
    - [data/ creation] "EDT #1"
  - Outgoing Edges:
    - [data] "EDT #2"
      - Parameters:
      - Guids:
      - DataBlocks:
        - ptr %1 / ReadWrite / from slot 0 to 0
- - - - - - - - - - - - - - - - - - 
- EDT #1 - "edt.1.parallel"
  - Type: parallel
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [data/ creation] "EDT #0"
  - Outgoing Edges:
    - [data] "EDT #2"
      - Parameters:
      - Guids:
      - DataBlocks:
        -   %random_number = alloca i32, align 4 / ReadWrite / from slot 0 to 1
    - [data/ creation] "EDT #3"
      - Parameters:
        -   %2 = load i32, ptr %0, align 4, !tbaa !8
      - Guids:
        - EDT #2
      - DataBlocks:
        - ptr %1 / ReadWrite / to slot 0
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt.0.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [data/ creation] "EDT #1"
      - Parameters:
      - Guids:
        - EDT #2
      - DataBlocks:
        -   %random_number = alloca i32, align 4 / ReadWrite / to slot 0
        -   %shared_number = alloca i32, align 4 / ReadWrite / to slot 1
    - [control/ creation] "EDT #2"
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt.2.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Edges:
    - [data] "EDT #1"
    - [control/ creation] "EDT #0"
    - [data] "EDT #3"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Reserving GUID for EDT #3
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Created ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
[arts-codegen] Reserving GUID for EDT #1
[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
[arts-codegen] Reserving GUID for EDT #0
     EDT #0 doesn't have a parent EDT
[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)

All EDT Guids have been reserved
- - - - - - - - - - - - - - - - - - -
Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
[arts-codegen] Creating function for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1
     - Value is an Integer
     - Casted Value:   %1 = trunc i64 %0 to i32
 - Inserting DepV
   - DepV[0]: ptr %0
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %1 = load i32, ptr %0, align 4, !tbaa !6
[arts-codegen] Created ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
- - - - - - - - - - - - - - - - - - -
Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
     EDT #1 doesn't have input data edges from the parent
 - Inserting DepV
   - DepV[1]: ptr %1
   - DepV[0]: ptr %0
[arts-codegen] Inserting Call for EDT #1
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
- - - - - - - - - - - - - - - - - - -
Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
     EDT #0 doesn't have a parent EDT
 - Inserting DepV
     EDT #0 doesn't have input edges
[arts-codegen] Inserting Call for EDT #0
     EDT #0 doesn't have a parent EDT
- - - - - - - - - - - - - - - - - - -
Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
[arts-codegen] Creating function for EDT #2
 - Inserting ParamV
     EDT #2 doesn't have input data edges from the parent
 - Inserting DepV
   - DepV[1]: ptr %random_number
   - DepV[0]: ptr %shared_number
[arts-codegen] Inserting Call for EDT #2
     EDT #2 doesn't have input data edges from the parent

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'test_arts_ir.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { ptr, i32, ptr }

@.str = private unnamed_addr constant [29 x i8] c"I think the number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [30 x i8] c"The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !6 dso_local noundef i32 @main() local_unnamed_addr #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt(ptr nocapture readonly, ptr nocapture readonly) #3

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !10 internal void @carts.edt.1(ptr nocapture readonly, i32) #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !12 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !14 internal void @carts.edt.2(ptr nocapture readonly, ptr nocapture readonly) #0

define internal void @edt.1.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca ptr, align 8
  store ptr %0, ptr %edt.3.task_guid.addr, align 8
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  %1 = load i32, ptr %depv.0, align 4, !tbaa !15
  %edt.3.task_paramc = alloca i32, align 4
  store i32 1, ptr %edt.3.task_paramc, align 4
  %2 = load i32, ptr %edt.3.task_paramc, align 4
  %edt.3.task_paramv = alloca i64, i32 %2, align 8
  %edt.3.task_paramv.0 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 0
  %3 = sext i32 %1 to i64
  store i64 %3, ptr %edt.3.task_paramv.0, align 8
  %4 = alloca i32, align 4
  store i32 1, ptr %4, align 4
  %5 = load i32, ptr %4, align 4
  %6 = call ptr @artsEdtCreateWithGuid(ptr @edt.3.task, ptr %edt.3.task_guid.addr, i32 %2, ptr %edt.3.task_paramv, i32 %5)
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

declare ptr @artsReserveGuidRoute(i32, i32)

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
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i32 @rand() #5
  store i32 %call, ptr %shared_number, align 4, !tbaa !15
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !15
  %edt.1.parallel_paramc = alloca i32, align 4
  store i32 0, ptr %edt.1.parallel_paramc, align 4
  %2 = load i32, ptr %edt.1.parallel_paramc, align 4
  %edt.1.parallel_paramv = alloca i64, i32 %2, align 8
  %3 = alloca i32, align 4
  store i32 2, ptr %3, align 4
  %4 = load i32, ptr %3, align 4
  %5 = call ptr @artsEdtCreateWithGuid(ptr @edt.1.parallel, ptr %edt.1.parallel_guid.addr, i32 %2, ptr %edt.1.parallel_paramv, i32 %4)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
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
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %2 = load i32, ptr %depv.0, align 4, !tbaa !15, !noalias !19
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %2, i32 noundef %1) #5, !noalias !19
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

declare ptr @artsEdtCreateWithGuid(ptr, ptr, i32, ptr, i32)

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.shared_number, align 4, !tbaa !15
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !15
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1) #5
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
!9 = !{!"dep", !"dep"}
!10 = !{!"task", !11}
!11 = !{!"dep", !"param"}
!12 = !{!13}
!13 = !{i64 2, i64 -1, i64 -1, i1 true}
!14 = !{!"task", !9}
!15 = !{!16, !16, i64 0}
!16 = !{!"int", !17, i64 0}
!17 = !{!"omnipotent char", !18, i64 0}
!18 = !{!"Simple C++ TBAA"}
!19 = !{!20}
!20 = distinct !{!20, !21, !".omp_outlined.: %__context"}
!21 = distinct !{!21, !".omp_outlined."}


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt.1
Global is external, but doesn't have external or weak linkage!
ptr @carts.edt.2
LLVM ERROR: Broken module found, compilation aborted!
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007fb9ec51def8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007fb9ec51bb7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007fb9ec51e5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fb9ef25f910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007fb9ebe28d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007fb9ebe2a3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007fb9ec469d3c llvm::report_fatal_error(llvm::Twine const&, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddd3c)
 #7 0x00007fb9ec469b66 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0xddb66)
 #8 0x00007fb9ec94abca (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2f9bca)
 #9 0x000056268ec1b32d llvm::detail::PassModel<llvm::Module, llvm::VerifierPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x2032d)
#10 0x00007fb9ec9112a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#11 0x000056268ec14293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#12 0x000056268ec21aaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#13 0x00007fb9ebe1324d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x000056268ec0da3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
