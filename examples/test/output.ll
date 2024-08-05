clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen,arts-utils\
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
    - [data/ creation] "EDT #3"
      - Parameters:
        -   %2 = load i32, ptr %0, align 4, !tbaa !8
      - Guids:
        - EDT #2
      - DataBlocks:
        - ptr %1 / ReadWrite / to slot 0
    - [data] "EDT #2"
      - Parameters:
      - Guids:
      - DataBlocks:
        -   %random_number = alloca i32, align 4 / ReadWrite / from slot 0 to 1
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt.0.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #2"
    - [data/ creation] "EDT #1"
      - Parameters:
      - Guids:
        - EDT #2
      - DataBlocks:
        -   %random_number = alloca i32, align 4 / ReadWrite / to slot 0
        -   %shared_number = alloca i32, align 4 / ReadWrite / to slot 1
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt.2.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Edges:
    - [control/ creation] "EDT #0"
    - [data] "EDT #3"
    - [data] "EDT #1"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
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
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Found ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)

All EDT Guids have been reserved

; ModuleID = 'test_arts_ir.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

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

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
  %0 = load i32, ptr %0, align 4, !tbaa !18, !noalias !15
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %0, i32 noundef %1) #5, !noalias !15
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.1.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca ptr, align 8
  store ptr %0, ptr %edt.3.task_guid.addr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  %1 = load i32, ptr %0, align 4, !tbaa !18
  call void @carts.edt.1(ptr nocapture readonly %1, i32 %1) #7
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
  store i32 %call, ptr %shared_number, align 4, !tbaa !18
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !18
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %shared_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %shared_number, align 4, !tbaa !18
  %1 = load i32, ptr %random_number, align 4, !tbaa !18
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
!9 = !{!"dep", !"dep"}
!10 = !{!"task", !11}
!11 = !{!"dep", !"param"}
!12 = !{!13}
!13 = !{i64 2, i64 -1, i64 -1, i1 true}
!14 = !{!"task", !9}
!15 = !{!16}
!16 = distinct !{!16, !17, !".omp_outlined.: %__context"}
!17 = distinct !{!17, !".omp_outlined."}
!18 = !{!19, !19, i64 0}
!19 = !{!"int", !20, i64 0}
!20 = !{!"omnipotent char", !21, i64 0}
!21 = !{!"Simple C++ TBAA"}


Generating Code for EDT #0
[arts-codegen] Inserting Call for  for EDT #0
 - Inserting ParamV
     EDT #0 doesn't have a parent EDT
 - Inserting DepV
     EDT #0 doesn't have input edges
Rewiring new function arguments:
[arts-codegen] Inserting Call for EDT #0
     EDT #0 doesn't have a parent EDT

Generating Code for EDT #1
[arts-codegen] Inserting Call for  for EDT #1
 - Inserting ParamV
   - ParamVGuid[0]: EDT2
 - Inserting DepV
   - DepV[1]: ptr %1
   - DepV[0]: ptr %0
Rewiring new function arguments:
  - Rewiring: ptr %1 ->   %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  - Rewiring: ptr %0 ->   %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
[arts-codegen] Inserting Call for EDT #1
 - ParamVGuid[0]: EDT2
 - Creating EDT
[arts-codegen] Created ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-utils]   - Replacing uses of:   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %shared_number) #7
[arts-utils]    - Removing instruction:   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %shared_number) #7
[arts-utils]   - Replacing uses of: ; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt(ptr nocapture readonly, ptr nocapture readonly) #3

[arts-utils]    - Removing function: ; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt(ptr nocapture readonly, ptr nocapture readonly) #3


Generating Code for EDT #3
[arts-codegen] Inserting Call for  for EDT #3
 - Inserting ParamV
   - ParamV[0]: i32 %1
     - Value is an Integer
     - Casted Value:   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT2
 - Inserting DepV
   - DepV[0]: ptr %0
Rewiring new function arguments:
  - Rewiring: ptr %0 ->   %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  - Rewiring: i32 %1 ->   %1 = trunc i64 %0 to i32
[arts-codegen] Inserting Call for EDT #3
 - ParamV[0]:   %3 = load i32, ptr %depv.0, align 4, !tbaa !6
 - ParamVGuid[0]: EDT2
 - Creating EDT
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-utils]   - Replacing uses of:   call void @carts.edt.1(ptr nocapture readonly %depv.1, i32 %3) #7
[arts-utils]    - Removing instruction:   call void @carts.edt.1(ptr nocapture readonly %depv.1, i32 %3) #7
[arts-utils]   - Replacing uses of: ; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt.1(ptr nocapture readonly, i32) #4

[arts-utils]    - Removing function: ; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
declare !carts !8 internal void @carts.edt.1(ptr nocapture readonly, i32) #4


Generating Code for EDT #2
[arts-codegen] Inserting Call for  for EDT #2
 - Inserting ParamV
     EDT #2 doesn't have input data edges from the parent
 - Inserting DepV
   - DepV[0]: ptr %shared_number
   - DepV[1]: ptr %random_number
Rewiring new function arguments:
  - Rewiring: ptr %random_number ->   %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  - Rewiring: ptr %shared_number ->   %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
[arts-codegen] Inserting Call for EDT #2
 - Creating EDT
[arts-codegen] Found ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-utils]   - Replacing uses of:   call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number) #4
[arts-utils]    - Removing instruction:   call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number) #4
[arts-utils]   - Replacing uses of: ; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !10 internal void @carts.edt.2(ptr nocapture readonly, ptr nocapture readonly) #0

[arts-utils]    - Removing function: ; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !10 internal void @carts.edt.2(ptr nocapture readonly, ptr nocapture readonly) #0

[arts-codegen] Inserting Init Functions
[arts-utils]   - Replacing uses of: ; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !6 dso_local noundef i32 @2() local_unnamed_addr #0

[arts-utils]    - Removing function: ; Function Attrs: mustprogress norecurse nounwind uwtable
declare !carts !6 dso_local noundef i32 @2() local_unnamed_addr #0

[arts-codegen] Created ARTS runtime function artsRT with type void (i32, ptr)

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

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #0

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #1

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
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #4

define internal void @edt.3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %paramv.0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %paramv.guid.edt_2, align 8
  %3 = inttoptr i64 %2 to ptr
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !8)
  %4 = load i32, ptr %depv.0, align 4, !tbaa !11, !noalias !8
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %1) #3, !noalias !8
  br label %exit

exit:                                             ; preds = %edt.body
  ret void
}

define internal void @edt.1.parallel(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %edt.3.task_guid.addr = alloca ptr, align 8
  store ptr %0, ptr %edt.3.task_guid.addr, align 8
  %paramv.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 0
  %1 = load i64, ptr %paramv.guid.edt_2, align 8
  %2 = inttoptr i64 %1 to ptr
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  %3 = load i32, ptr %depv.0, align 4, !tbaa !11
  %edt.3.task_paramc = alloca i32, align 4
  store i32 2, ptr %edt.3.task_paramc, align 4
  %4 = load i32, ptr %edt.3.task_paramc, align 4
  %edt.3.task_paramv = alloca i64, i32 %4, align 8
  %edt.3.task_paramv.0 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 0
  %5 = sext i32 %3 to i64
  store i64 %5, ptr %edt.3.task_paramv.0, align 8
  %edt.3.task_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.3.task_paramv, i64 1
  %6 = ptrtoint ptr %2 to i32
  store i32 %6, ptr %edt.3.task_paramv_guid.edt_2, align 4
  %7 = alloca i32, align 4
  store i32 1, ptr %7, align 4
  %8 = load i32, ptr %7, align 4
  %9 = call ptr @artsEdtCreateWithGuid(ptr @edt.3.task, ptr %edt.3.task_guid.addr, i32 %4, ptr %edt.3.task_paramv, i32 %8)
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
  %call = tail call i32 @rand() #3
  store i32 %call, ptr %shared_number, align 4, !tbaa !11
  %call1 = tail call i32 @rand() #3
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !11
  %edt.1.parallel_paramc = alloca i32, align 4
  store i32 1, ptr %edt.1.parallel_paramc, align 4
  %2 = load i32, ptr %edt.1.parallel_paramc, align 4
  %edt.1.parallel_paramv = alloca i64, i32 %2, align 8
  %edt.1.parallel_paramv_guid.edt_2 = getelementptr inbounds i64, ptr %edt.1.parallel_paramv, i64 0
  %3 = ptrtoint ptr %edt.2.task_guid.addr to i32
  store i32 %3, ptr %edt.1.parallel_paramv_guid.edt_2, align 4
  %4 = alloca i32, align 4
  store i32 2, ptr %4, align 4
  %5 = load i32, ptr %4, align 4
  %6 = call ptr @artsEdtCreateWithGuid(ptr @edt.1.parallel, ptr %edt.1.parallel_guid.addr, i32 %2, ptr %edt.1.parallel_paramv, i32 %5)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt.2.task_paramc = alloca i32, align 4
  store i32 0, ptr %edt.2.task_paramc, align 4
  %7 = load i32, ptr %edt.2.task_paramc, align 4
  %edt.2.task_paramv = alloca i64, i32 %7, align 8
  %8 = alloca i32, align 4
  store i32 2, ptr %8, align 4
  %9 = load i32, ptr %8, align 4
  %10 = call ptr @artsEdtCreateWithGuid(ptr @edt.2.task, ptr %edt.2.task_guid.addr, i32 %7, ptr %edt.2.task_paramv, i32 %9)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  ret void
}

define internal void @edt.2.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %depv.shared_number, align 4, !tbaa !11
  %1 = load i32, ptr %depv.random_number, align 4, !tbaa !11
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1) #3
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  ret void
}

declare ptr @artsEdtCreateWithGuid(ptr, ptr, i32, ptr, i32)

define internal void @initPerNode(i32 %nodeId, i32 %argc, ptr %argv) {
entry:
  ret void
}

define internal void @initPerWorker(i32 %nodeId, i32 %workerId, i32 %argc, ptr %argv) {
entry:
  ret void
}

define i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @artsRT(i32 %argc, ptr %argv)
  ret i32 0
}

declare void @artsRT(i32, ptr)

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #1 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
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
!8 = !{!9}
!9 = distinct !{!9, !10, !".omp_outlined.: %__context"}
!10 = distinct !{!10, !".omp_outlined."}
!11 = !{!12, !12, i64 0}
!12 = !{!"int", !13, i64 0}
!13 = !{!"omnipotent char", !14, i64 0}
!14 = !{!"Simple C++ TBAA"}


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: /tmp/test_arts_analysis-c4c146.o: in function `main':
test.cpp:(.text+0x2): undefined reference to `artsRT'
clang++: error: linker command failed with exit code 1 (use -v to see invocation)
make: *** [Makefile:31: test_opt] Error 1
