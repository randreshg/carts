clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	-passes="omp-transform" test.bc -o test_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
test.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: main
- - - - - - - - - - - - - - - -
[omp-transform] Parallel Region Found:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %shared_number)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt(ptr, ptr)
Rewiring new function arguments:
  - Rewiring: ptr %shared_number -> ptr %1
  - Rewiring: ptr %random_number -> ptr %0
[arts-ir-builder] New callsite:   call void @carts.edt(ptr %random_number, ptr %shared_number)
[arts-utils]   - Replacing uses of:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %shared_number)
[arts-utils]    - Removing instruction:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %shared_number)
[arts-utils]   - Replacing uses of: ptr %.global_tid.
[arts-utils]    - Replacing:   %2 = load i32, ptr undef, align 4, !tbaa !6
[arts-utils]   - Replacing uses of: ptr %.bound_tid.
[arts-utils]   - Replacing uses of: ptr %random_number
[arts-utils]   - Replacing uses of: ptr %shared_number
[arts-utils]   - Replacing uses of: ; Function Attrs: alwaysinline norecurse nounwind uwtable
declare dso_local void @main.omp_outlined(ptr noalias nocapture noundef readonly, ptr noalias nocapture readnone, ptr noundef nonnull align 4 dereferenceable(4), ptr noundef nonnull align 4 dereferenceable(4)) #3

[arts-utils]    - Removing function: ; Function Attrs: alwaysinline norecurse nounwind uwtable
declare dso_local void @main.omp_outlined(ptr noalias nocapture noundef readonly, ptr noalias nocapture readnone, ptr noundef nonnull align 4 dereferenceable(4), ptr noundef nonnull align 4 dereferenceable(4)) #3

Current Function after undefining OldCB:
; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i32 @rand() #5
  store i32 %call, ptr %shared_number, align 4, !tbaa !6
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !6
  call void @carts.edt(ptr %random_number, ptr %shared_number)
  %0 = load i32, ptr %shared_number, align 4, !tbaa !6
  %1 = load i32, ptr %random_number, align 4, !tbaa !6
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1)
  ret i32 0
}

[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt(ptr %0, ptr %1) #1 {
entry:
  %2 = load i32, ptr undef, align 4, !tbaa !6
  %3 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry.)
  %4 = load ptr, ptr %3, align 8, !tbaa !10
  store ptr %1, ptr %4, align 8, !tbaa.struct !14
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %4, i64 8
  store ptr %0, ptr %agg.captured.sroa.2.0..sroa_idx, align 8, !tbaa.struct !16
  %5 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %3)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %3 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry.)
[omp-transform] LoadInst:   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
[omp-transform] Offset: 0
[omp-transform] LoadInst:   %4 = load i32, ptr %3, align 4, !tbaa !18, !noalias !13
[omp-transform] BasePointer:   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] Offset: 0
[omp-transform] LoadInst:   %6 = load ptr, ptr %5, align 8, !tbaa !19, !alias.scope !13
[omp-transform] BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
[omp-transform] Offset: 8
[omp-transform] OffsetToValueOF: 0 ->   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] OffsetToValueOF: 8 ->   %6 = load ptr, ptr %5, align 8, !tbaa !19, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %1 -> 0
[omp-transform] ValueToOffsetTD: ptr %0 -> 8
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.1(ptr, ptr)
Rewiring new function arguments:
  - Rewiring:   %6 = load ptr, ptr %5, align 8, !tbaa !19, !alias.scope !13 -> ptr %1
  - Rewiring:   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13 -> ptr %0
[arts-ir-builder] New callsite:   call void @carts.edt.1(ptr %1, ptr %0)
[arts-utils]   - Replacing uses of:   %3 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry.)
[arts-utils]    - Replacing:   %5 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull undef)
[arts-utils]    - Removing instruction:   %5 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull undef)
[arts-utils]    - Replacing:   %4 = load ptr, ptr undef, align 8, !tbaa !12
[arts-utils]    - Removing instruction:   %4 = load ptr, ptr undef, align 8, !tbaa !12
[arts-utils]    - Replacing:   store ptr %1, ptr undef, align 8, !tbaa.struct !12
[arts-utils]    - Removing instruction:   store ptr %1, ptr undef, align 8, !tbaa.struct !12
[arts-utils]    - Replacing:   %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr undef, i64 8
[arts-utils]    - Removing instruction:   %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr undef, i64 8
[arts-utils]    - Replacing:   store ptr %0, ptr undef, align 8, !tbaa.struct !12
[arts-utils]    - Removing instruction:   store ptr %0, ptr undef, align 8, !tbaa.struct !12
[arts-utils]    - Removing instruction:   %3 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry.)
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Replacing uses of: ptr %1
[arts-utils]    - Replacing:   %2 = load ptr, ptr undef, align 8, !tbaa !6
[arts-utils]   - Replacing uses of: ; Function Attrs: nofree norecurse nounwind uwtable
declare dso_local noundef i32 @.omp_task_entry.(i32, ptr noalias nocapture noundef readonly) #4

[arts-utils]    - Removing function: ; Function Attrs: nofree norecurse nounwind uwtable
declare dso_local noundef i32 @.omp_task_entry.(i32, ptr noalias nocapture noundef readonly) #4

Current Function after undefining OldCB:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt(ptr %0, ptr %1) #1 !carts !10 {
entry:
  %2 = load i32, ptr undef, align 4, !tbaa !6
  call void @carts.edt.1(ptr %1, ptr %0)
  ret void
}

[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.1(ptr %0, ptr %1) #1 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !12
  tail call void @llvm.experimental.noalias.scope.decl(metadata !16)
  %3 = load ptr, ptr undef, align 8, !tbaa !19, !alias.scope !16
  %4 = load i32, ptr %0, align 4, !tbaa !6, !noalias !16
  %5 = getelementptr inbounds %struct.anon, ptr undef, i64 0, i32 1
  %6 = load ptr, ptr %5, align 8, !tbaa !21, !alias.scope !16
  %7 = load i32, ptr %1, align 4, !tbaa !6, !noalias !16
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %7), !noalias !16
  %8 = load i32, ptr %0, align 4, !tbaa !6, !noalias !16
  %dec.i = add nsw i32 %8, -1
  store i32 %dec.i, ptr %0, align 4, !tbaa !6, !noalias !16
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.1
[omp-transform] Processing function: carts.edt.1 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.2
[omp-transform] Processing function: carts.edt.2 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: main - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.anon = type { ptr, ptr }

@.str = private unnamed_addr constant [29 x i8] c"I think the number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [30 x i8] c"The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts !6 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i32 @rand() #4
  store i32 %call, ptr %shared_number, align 4, !tbaa !8
  %call1 = tail call i32 @rand() #4
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !8
  call void @carts.edt(ptr %random_number, ptr %shared_number) #6
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.2(ptr %shared_number, ptr %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt(ptr %0, ptr %1) #1 !carts !12 {
entry:
  %2 = load i32, ptr undef, align 4, !tbaa !8
  call void @carts.edt.1(ptr %1, ptr %0) #6
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #3

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.1(ptr %0, ptr %1) #1 !carts !14 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !15
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %3 = load ptr, ptr undef, align 8, !tbaa !22, !alias.scope !19
  %4 = load i32, ptr %0, align 4, !tbaa !8, !noalias !19
  %5 = getelementptr inbounds %struct.anon, ptr undef, i64 0, i32 1
  %6 = load ptr, ptr %5, align 8, !tbaa !24, !alias.scope !19
  %7 = load i32, ptr %1, align 4, !tbaa !8, !noalias !19
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %7), !noalias !19
  %8 = load i32, ptr %0, align 4, !tbaa !8, !noalias !19
  %dec.i = add nsw i32 %8, -1
  store i32 %dec.i, ptr %0, align 4, !tbaa !8, !noalias !19
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !25 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @carts.edt.2(ptr %shared_number, ptr %random_number) #0 !carts !14 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1)
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }
attributes #5 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #6 = { memory(argmem: readwrite) }

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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"parallel", !13}
!13 = !{!"dep", !"dep"}
!14 = !{!"task", !13}
!15 = !{!16, !18, i64 0}
!16 = !{!"_ZTS24kmp_task_t_with_privates", !17, i64 0}
!17 = !{!"_ZTS10kmp_task_t", !18, i64 0, !18, i64 8, !9, i64 16, !10, i64 24, !10, i64 32}
!18 = !{!"any pointer", !10, i64 0}
!19 = !{!20}
!20 = distinct !{!20, !21, !".omp_outlined.: %__context"}
!21 = distinct !{!21, !".omp_outlined."}
!22 = !{!23, !18, i64 0}
!23 = !{!"_ZTSZ4mainE3$_0", !18, i64 0, !18, i64 8}
!24 = !{!23, !18, i64 8}
!25 = !{!26}
!26 = !{i64 2, i64 -1, i64 -1, i1 true}


-------------------------------------------------
[arts-utils] Removing dead instructions from: main
[arts-utils] Removing 0 dead instructions

[arts-utils] Removing dead instructions from: carts.edt
[arts-utils] - Removing:   %2 = load i32, ptr undef, align 4, !tbaa !8
[arts-utils] Removing 1 dead instructions
[arts-utils]   - Replacing uses of:   %2 = load i32, ptr undef, align 4, !tbaa !8
[arts-utils]    - Removing instruction:   %2 = load i32, ptr undef, align 4, !tbaa !8

[arts-utils] Removing dead instructions from: carts.edt.1
[arts-utils] - Removing:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils] - Removing:   %3 = load ptr, ptr undef, align 8, !tbaa !18, !alias.scope !15
[arts-utils] - Removing:   %5 = getelementptr inbounds %struct.anon, ptr undef, i64 0, i32 1
[arts-utils] - Removing:   %6 = load ptr, ptr %5, align 8, !tbaa !21, !alias.scope !15
[arts-utils] Removing 4 dead instructions
[arts-utils]   - Replacing uses of:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils]    - Removing instruction:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils]   - Replacing uses of:   %2 = load ptr, ptr undef, align 8, !tbaa !11, !alias.scope !8
[arts-utils]    - Removing instruction:   %2 = load ptr, ptr undef, align 8, !tbaa !11, !alias.scope !8
[arts-utils]   - Replacing uses of:   %3 = getelementptr inbounds %struct.anon, ptr undef, i64 0, i32 1
[arts-utils]    - Replacing:   %4 = load ptr, ptr undef, align 8, !tbaa !15, !alias.scope !8
[arts-utils]    - Removing instruction:   %4 = load ptr, ptr undef, align 8, !tbaa !15, !alias.scope !8
[arts-utils]    - Removing instruction:   %3 = getelementptr inbounds %struct.anon, ptr undef, i64 0, i32 1

[arts-utils] Removing dead instructions from: carts.edt.2
[arts-utils] Removing 0 dead instructions

[omp-transform] [Attributor] Done with 4 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [29 x i8] c"I think the number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [30 x i8] c"The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts !6 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i32 @rand() #5
  store i32 %call, ptr %shared_number, align 4, !tbaa !8
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !8
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %shared_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt(ptr nocapture readonly %0, ptr nocapture %1) #3 !carts !12 {
entry:
  call void @carts.edt.1(ptr nocapture %1, ptr nocapture readonly %0) #8
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.1(ptr nocapture %0, ptr nocapture readonly %1) #3 !carts !14 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
  %2 = load i32, ptr %0, align 4, !tbaa !8, !noalias !15
  %3 = load i32, ptr %1, align 4, !tbaa !8, !noalias !15
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %2, i32 noundef %3) #5, !noalias !15
  %4 = load i32, ptr %0, align 4, !tbaa !8, !noalias !15
  %dec.i = add nsw i32 %4, -1
  store i32 %dec.i, ptr %0, align 4, !tbaa !8, !noalias !15
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !18 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @carts.edt.2(ptr nocapture readonly %shared_number, ptr nocapture readonly %random_number) #0 !carts !14 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #7 = { memory(argmem: readwrite) }
attributes #8 = { nounwind memory(readwrite) }

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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"parallel", !13}
!13 = !{!"dep", !"dep"}
!14 = !{!"task", !13}
!15 = !{!16}
!16 = distinct !{!16, !17, !".omp_outlined.: %__context"}
!17 = distinct !{!17, !".omp_outlined."}
!18 = !{!19}
!19 = !{i64 2, i64 -1, i64 -1, i1 true}


-------------------------------------------------
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
   - DepV: ptr %1
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

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #3
     - EDT #3 signals to EDT #2
        - Converting Control edge to Data edge ["edt.3.task" -> "edt.2.task"]

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #3

[AAEDTDataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #3
     - EDT #3 signals to EDT #2
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

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 1

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - EDT #2 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
   - AAPointerInfo is not at fixpoint!

[AAEDTDataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1
   - All DataBlocks were fixed for EDT #1
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{1, 2}
     #Reached DescendantEDTs: 1{3}
     #Dependent EDTs: 0{}

- EDT #0: edt.0.main
Ty: main
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
     #Dependent EDTs: 0{}

- EDT #1: edt.1.parallel
Ty: parallel
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
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %0
  - ptr %1
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
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %shared_number
  - ptr %random_number
-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #3
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #3
EDTDataBlock ->
     - ParentEDT: EDT #1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #2
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 1{2}

-------------------------------
[AAEDTDataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #1
EDTDataBlock ->
     - ParentEDT: EDT #0
     - #DependentChildEDTs: 1{3}
     - #DependentSiblingEDTs: 0{}

[Attributor] Done with 4 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- EDT #3 - "edt.3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [control/ creation] "EDT #1"
  - Outgoing Edges:
    - [data] "EDT #2"
      - Parameters:
      - DataBlocks:
        - ptr %1 / ReadWrite
        - ptr %0 / ReadWrite

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
    - [control] "EDT #1"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #0 - "edt.0.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #1"
    - [control/ creation] "EDT #2"

- EDT #1 - "edt.1.parallel"
  - Type: parallel
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Edges:
    - [control/ creation] "EDT #0"
  - Outgoing Edges:
    - [control/ creation] "EDT #3"
    - [control] "EDT #2"

- - - - - - - - - - - - - - - - - - - - - - - -


-------------------------------------------------
[arts-analysis] Process has finished

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
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts !6 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i32 @rand() #5
  store i32 %call, ptr %shared_number, align 4, !tbaa !8
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !8
  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %shared_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.2(ptr nocapture %shared_number, ptr nocapture %random_number) #5
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt(ptr nocapture readonly %0, ptr nocapture %1) #3 !carts !12 {
entry:
  call void @carts.edt.1(ptr nocapture %1, ptr nocapture readonly %0) #8
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.1(ptr nocapture %0, ptr nocapture readonly %1) #3 !carts !14 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
  %2 = load i32, ptr %0, align 4, !tbaa !8, !noalias !15
  %3 = load i32, ptr %1, align 4, !tbaa !8, !noalias !15
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %2, i32 noundef %3) #5, !noalias !15
  %4 = load i32, ptr %0, align 4, !tbaa !8, !noalias !15
  %dec.i = add nsw i32 %4, -1
  store i32 %dec.i, ptr %0, align 4, !tbaa !8, !noalias !15
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !18 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @carts.edt.2(ptr nocapture readonly %shared_number, ptr nocapture readonly %random_number) #0 !carts !14 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0, i32 noundef %1) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #7 = { nounwind memory(argmem: readwrite) }
attributes #8 = { nounwind memory(readwrite) }

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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"parallel", !13}
!13 = !{!"dep", !"dep"}
!14 = !{!"task", !13}
!15 = !{!16}
!16 = distinct !{!16, !17, !".omp_outlined.: %__context"}
!17 = distinct !{!17, !".omp_outlined."}
!18 = !{!19}
!19 = !{i64 2, i64 -1, i64 -1, i1 true}


-------------------------------------------------
[edt-graph] Destroying the EDT Graph
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
