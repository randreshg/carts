clang++ -fopenmp -O3 -g0 -emit-llvm -c simple.cpp -o simple.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis simple.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder\
	-passes="omp-transform" simple.bc -o simple_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
simple.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 
[omp-transform] Processing main function
[omp-transform] Main EDT Function: define internal void @carts.edt.main() !carts !27 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #7
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #7
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %shared_number) #7
  %call1 = tail call i32 @rand() #7
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %random_number) #7
  %call2 = tail call i32 @rand() #7
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !6
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4)
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
  %0 = load i32, ptr %shared_number, align 4, !tbaa !6
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !6
  %1 = load i32, ptr %random_number, align 4, !tbaa !6
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %random_number) #7
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %shared_number) #7
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.main
[omp-transform] Other Function Found:
  %call = tail call i64 @time(ptr noundef null) #7
[omp-transform] Other Function Found:
  tail call void @srand(i32 noundef %conv) #7
[omp-transform] Other Function Found:
  %call1 = tail call i32 @rand() #7
[omp-transform] Other Function Found:
  %call2 = tail call i32 @rand() #7
[omp-transform] Other Function Found:
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4)
- - - - - - - - - - - - - - - -
[omp-transform] Parallel Region Found:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.parallel(ptr, ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.parallel(ptr %shared_number, ptr %random_number)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1) #2 {
entry:
  %2 = load i32, ptr undef, align 4, !tbaa !6
  %3 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
  %.not = icmp eq i32 %3, 0
  br i1 %.not, label %omp_if.end, label %omp_if.then

omp_if.then:                                      ; preds = %entry
  %4 = load i32, ptr %0, align 4, !tbaa !6
  %5 = load i32, ptr %1, align 4, !tbaa !6
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %4, i32 noundef %5)
  %6 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
  %7 = load ptr, ptr %6, align 8, !tbaa !10
  store ptr %0, ptr %7, align 8, !tbaa.struct !15
  %8 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %6, i64 0, i32 1
  %9 = load i32, ptr %1, align 4, !tbaa !6
  store i32 %9, ptr %8, align 8, !tbaa !17
  %10 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %6)
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  tail call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.parallel
- - - - - - - - - - - - - - - -
[omp-transform] Single Region Found:
  %3 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
[omp-transform] Other Function Found:
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3)
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %4 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task(ptr, i32)
[arts-ir-builder] New callsite:   call void @carts.edt.task(ptr %0, i32 %7)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0, i32 %1) #2 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !12
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  %4 = load ptr, ptr undef, align 8, !tbaa !20, !alias.scope !17, !noalias !22
  %5 = load i32, ptr %0, align 4, !tbaa !8, !noalias !17
  %inc.i = add nsw i32 %5, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !8, !noalias !17
  %6 = load i32, ptr undef, align 4, !tbaa !8, !noalias !17
  %inc1.i = add nsw i32 %1, 1
  store i32 %inc1.i, ptr undef, align 4, !tbaa !8, !noalias !17
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !17
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
[omp-transform] Other Function Found:
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !16
[omp-transform] Processing function: carts.edt.task - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Single End Region Found:
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[omp-transform] Other Function Found:
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3)
[omp-transform] Other Function Found:
  call void @carts.edt.task(ptr %0, i32 %4) #7
[omp-transform] Single End Region Found:
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[omp-transform] Processing function: carts.edt.parallel - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.sync.done
[omp-transform] Other Function Found:
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
[omp-transform] Processing function: carts.edt.sync.done - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.main - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'simple.bc'
source_filename = "simple.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #1

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1) #2 !carts !6 {
entry:
  %2 = load i32, ptr %0, align 4, !tbaa !8
  %3 = load i32, ptr %1, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3)
  %4 = load i32, ptr %1, align 4, !tbaa !8
  call void @carts.edt.task(ptr %0, i32 %4) #7
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0, i32 %1) #2 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  %2 = load i32, ptr %0, align 4, !tbaa !8, !noalias !14
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !8, !noalias !14
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !14
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !17 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

define internal void @carts.edt.main() !carts !19 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #5
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #5
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #5
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4)
  call void @carts.edt.parallel(ptr %shared_number, ptr %random_number) #7
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.sync.done(ptr %shared_number, ptr %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

define internal void @carts.edt.sync.done(ptr %shared_number, ptr %random_number) !carts !21 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { convergent nounwind }
attributes #5 = { nounwind }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #7 = { memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"sync", !7}
!7 = !{!"dep", !"dep"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"task", !13}
!13 = !{!"dep", !"param"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!18}
!18 = !{i64 2, i64 -1, i64 -1, i1 true}
!19 = !{!"main", !20}
!20 = !{}
!21 = !{!"task", !7}


-------------------------------------------------
[omp-transform] [Attributor] Done with 5 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'simple.bc'
source_filename = "simple.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #1

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.parallel(ptr nocapture %0, ptr nocapture readonly %1) #4 !carts !6 {
entry:
  %2 = load i32, ptr %0, align 4, !tbaa !8
  %3 = load i32, ptr %1, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3) #6
  %4 = load i32, ptr %1, align 4, !tbaa !8
  call void @carts.edt.task(ptr nocapture %0, i32 %4) #9
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.task(ptr nocapture %0, i32 %1) #4 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  %2 = load i32, ptr %0, align 4, !tbaa !8, !noalias !14
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !8, !noalias !14
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #6, !noalias !14
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #6

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !17 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

; Function Attrs: norecurse nounwind
define internal void @carts.edt.main() #8 !carts !19 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #6
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #6
  %call1 = tail call i32 @rand() #6
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #6
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #6
  call void @carts.edt.parallel(ptr nocapture %shared_number, ptr nocapture %random_number) #10
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture %random_number) #6
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: norecurse nounwind
define internal void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture readonly %random_number) #8 !carts !21 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1) #6
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #5 = { convergent nounwind }
attributes #6 = { nounwind }
attributes #7 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #8 = { norecurse nounwind }
attributes #9 = { memory(argmem: readwrite) }
attributes #10 = { nounwind memory(readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"sync", !7}
!7 = !{!"dep", !"dep"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"task", !13}
!13 = !{!"dep", !"param"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!18}
!18 = !{i64 2, i64 -1, i64 -1, i1 true}
!19 = !{!"main", !20}
!20 = !{}
!21 = !{!"task", !7}


-------------------------------------------------
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis simple_arts_ir.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" simple_arts_ir.bc -o simple_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
simple_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Initializing ARTS Cache
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
 - Analyzing Functions
-- Function: main
[arts] Function: main doesn't have CARTS Metadata

-- Function: carts.edt.parallel
[arts] Function: carts.edt.parallel has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt.parallel
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.parallel
[arts] Creating Sync EDT for function: carts.edt.parallel

-- Function: carts.edt.task
[arts] Function: carts.edt.task has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt.task
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.task

-- Function: carts.edt.main
[arts] Function: carts.edt.main has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.main
[arts] Creating Task EDT for function: carts.edt.main
[arts] Creating Main EDT for function: carts.edt.main

-- Function: carts.edt.sync.done
[arts] Function: carts.edt.sync.done has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.sync.done
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.sync.done

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt.parallel"
   - Call to EDTFunction:
      call void @carts.edt.parallel(ptr nocapture %shared_number, ptr nocapture %random_number) #9
   - DoneEDT: EDT #3

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt.task"
   - Call to EDTFunction:
      call void @carts.edt.task(ptr nocapture %0, i32 %4) #9

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.main"
   - Call to EDTFunction:
      call void @carts.edt.main()
[arts] Function: main doesn't have CARTS Metadata
   - The ContextEDT is the MainEDT

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.sync.done"
   - Call to EDTFunction:
      call void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture %random_number) #6

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #1"

[AAEDTInfoFunction::updateImpl] EDT #1
   - All ReachedEDTs are fixed for EDT #1
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #0

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
        - Inserting CreationEdge from "EDT #2" to "EDT #0"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0

[AAEDTInfoCallsite::initialize] EDT #0

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #0" to "EDT #1" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #1

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #1

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 0

[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #3

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #3
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #3
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #3

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #3

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #3

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
   - EDT #3 is a child of EDT #2
        - Inserting CreationEdge from "EDT #2" to "EDT #3"
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoCallsite::updateImpl] EDT #3

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #2
   - All DataBlocks were fixed for EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #3

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #3

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #0
        - Inserting DataBlockEdge from "EDT #0" to "EDT #3" in Slot #1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #0
        - Inserting DataBlockEdge from "EDT #1" to "EDT #3" in Slot #0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #2
     -ParentSyncEDT: <null>
     #Child EDTs: 1{1}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #0
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #2 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{0, 3}
     #Reached DescendantEDTs: 1{1}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentEDT: EDT #2
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

        - Inserting Guid of child "EDT #3" in the edge from "EDT #2" to "EDT #0"
        - Inserting Guid of "EDT #3" in the  from "EDT #0" to "EDT #1"
-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #1
DataBlock ->
     - Context: EDT #1 / Slot 0
     - SignalEDT: EDT #3
     - ParentCtx: EDT #0 / Slot 0
     - ParentSync: EDT #0 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 0
     - ChildDBss: 1
      - EDT #1 / Slot 0
     - DependentSiblingEDT: EDT #3 / Slot #0
     - #DependentChildEDTs: 1{1}
     - #DependentSiblingEDTs: 1{3}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - SignalEDT: EDT #3
     - DependentSiblingEDT: EDT #3 / Slot #1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 1{3}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #3
DataBlock ->
     - Context: EDT #3 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #3
DataBlock ->
     - Context: EDT #3 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

[Attributor] Done with 5 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph
- - - - - - - - - - - - - - - - - - 
- EDT #1 - "edt_1.task"
  - Type: task
  - Parent Sync: "EDT #0"
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Creation Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 from "EDT #0" /   %shared_number = alloca i32, align 4 [Parent EDT #0]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #3" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt_3.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Creation Edges:
    - [Creation] "EDT #2"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 from "EDT #1" /   %shared_number = alloca i32, align 4 [Parent EDT #0]
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4 from "EDT #0"/ No parent
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt_0.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Creation Edges:
    - [Creation] "EDT #2"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock]   %shared_number = alloca i32, align 4 from "EDT #2"/ No parent
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4 from "EDT #2"/ No parent
  - Outgoing Edges:
    - [Creation] "EDT #1"
    - [DataBlock] "EDT #1" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
    - [DataBlock] "EDT #3" in Slot #1
      -   %random_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt_2.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Creation Edges:
    - The EDT has no incoming Creation edges
    - The EDT has no incoming DataBlock slots
  - Outgoing Edges:
    - [Creation] "EDT #3"
    - [Creation] "EDT #0"
    - [DataBlock] "EDT #0" in Slot #0
      -   %shared_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #1
      -   %random_number = alloca i32, align 4

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Creating function for EDT #2
[arts-codegen] Reserving GUID for EDT #2
     EDT #2 doesn't have a parent EDT, using parent function
[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #3
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Reserving GUID for EDT #1

All EDT Guids have been reserved

Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
 - Inserting ParamV
     EDT #2 doesn't have a parent EDT
 - Inserting DepV
     EDT #2 doesn't have incoming slot nodes
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
[arts-codegen] Inserting Call for EDT #2
 - EDT Call:   %5 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.main, i64 %1, i32 %2, ptr %edt_2_paramv, i32 %4)

Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
   - ParamVGuid[0]: EDT3
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
   - DepV[1]: ptr %1 ->   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #0
 - ParamVGuid[0]: EDT3
 - EDT Call:   %9 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.sync, i64 %1, i32 %6, ptr %edt_0_paramv, i32 %8)

Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT3
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #1
 - ParamV[0]:   %5 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !6
 - ParamVGuid[1]: EDT3
 - EDT Call:   %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.task, i64 %1, i32 %6, ptr %edt_1_paramv, i32 %9)

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
   - DepV[1]: ptr %random_number ->   %edt_3.depv_1.ptr.load = load ptr, ptr %edt_3.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #3
 - EDT Call:   %13 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %3, i32 %10, ptr %edt_3_paramv, i32 %12)

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #2

Signal DB   %shared_number = alloca i32, align 4 from EDT #2 to EDT #0
 - ContextEDT: 0
 - We created the EDT
 - DBPtr:   %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
 - DBGuid:   %4 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)

Signal DB   %random_number = alloca i32, align 4 from EDT #2 to EDT #0
 - ContextEDT: 0
 - We created the EDT
 - DBPtr:   %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
 - DBGuid:   %5 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)
[arts-codegen] Inserting Signals from EDT #0

Signal DB ptr %0 from EDT #0 to EDT #1
 - ContextEDT: 1
 - We have a parent
 - DBPtr:   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
 - DBGuid:   %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8

Signal DB   %random_number = alloca i32, align 4 from EDT #0 to EDT #3
 - ContextEDT: 0
 - Using DepSlot of FromEDT - Slot: 1 - DBPtr:   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - DBGuid:   %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8
[arts-codegen] Inserting Signals from EDT #3
[arts-codegen] Inserting Signals from EDT #1

Signal DB ptr %0 from EDT #1 to EDT #3
 - ContextEDT: 1
 - Using DepSlot of FromEDT - Slot: 0 - DBPtr:   %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
 - DBGuid:   %edt_1.depv_0.guid.load = load i64, ptr %edt_1.depv_0.guid, align 8
[arts-codegen] Inserting Init Functions
[arts-codegen] Inserting ARTS Shutdown Function

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'simple_arts_ir.bc'
source_filename = "simple.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { i64, i32, ptr }

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

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
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #3

; Function Attrs: nounwind
declare !callback !6 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

define internal void @edt_2.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0.sync_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_0.sync_guid.addr, align 8
  %1 = load i64, ptr %edt_0.sync_guid.addr, align 8
  %2 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3.task_guid.addr = alloca i64, align 8
  store i64 %2, ptr %edt_3.task_guid.addr, align 8
  %3 = load i64, ptr %edt_3.task_guid.addr, align 8
  %db.shared_number.addr = alloca i64, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %4 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)
  %db.shared_number.addr.ld = load i64, ptr %db.shared_number.addr, align 8
  %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
  %db.random_number.addr = alloca i64, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %5 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)
  %db.random_number.addr.ld = load i64, ptr %db.random_number.addr, align 8
  %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
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
  store i32 %add, ptr %db.shared_number.ptr, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #4
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %db.random_number.ptr, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #4
  %edt_0_paramc = alloca i32, align 4
  store i32 1, ptr %edt_0_paramc, align 4
  %6 = load i32, ptr %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %6, align 8
  %edt_0.paramv_0.guid.edt_3 = getelementptr inbounds i64, ptr %edt_0_paramv, i64 0
  store i64 %3, ptr %edt_0.paramv_0.guid.edt_3, align 8
  %7 = alloca i32, align 4
  store i32 2, ptr %7, align 4
  %8 = load i32, ptr %7, align 4
  %9 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.sync, i64 %1, i32 %6, ptr %edt_0_paramv, i32 %8)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt_3_paramc = alloca i32, align 4
  store i32 0, ptr %edt_3_paramc, align 4
  %10 = load i32, ptr %edt_3_paramc, align 4
  %edt_3_paramv = alloca i64, i32 %10, align 8
  %11 = alloca i32, align 4
  store i32 2, ptr %11, align 4
  %12 = load i32, ptr %11, align 4
  %13 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.task, i64 %3, i32 %10, ptr %edt_3_paramv, i32 %12)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %toedt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.0.slot.0, align 4
  %14 = load i32, ptr %toedt.0.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %14, i64 %4)
  %toedt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.0.slot.1, align 4
  %15 = load i32, ptr %toedt.0.slot.1, align 4
  call void @artsSignalEdt(i64 %1, i32 %15, i64 %5)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt_0.sync(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_1.task_guid.addr, align 8
  %1 = load i64, ptr %edt_1.task_guid.addr, align 8
  %edt_0.paramv_0.guid.edt_3 = getelementptr inbounds i64, ptr %paramv, i64 0
  %2 = load i64, ptr %edt_0.paramv_0.guid.edt_3, align 8
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
  %3 = load i32, ptr %edt_0.depv_0.ptr.load, align 4, !tbaa !8
  %4 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %3, i32 noundef %4) #4
  %5 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, ptr %edt_1_paramc, align 4
  %6 = load i32, ptr %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %6, align 8
  %edt_1.paramv_0 = getelementptr inbounds i64, ptr %edt_1_paramv, i64 0
  %7 = sext i32 %5 to i64
  store i64 %7, ptr %edt_1.paramv_0, align 8
  %edt_1.paramv_1.guid.edt_3 = getelementptr inbounds i64, ptr %edt_1_paramv, i64 1
  store i64 %2, ptr %edt_1.paramv_1.guid.edt_3, align 8
  %8 = alloca i32, align 4
  store i32 1, ptr %8, align 4
  %9 = load i32, ptr %8, align 4
  %10 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.task, i64 %1, i32 %6, ptr %edt_1_paramv, i32 %9)
  br label %exit1

exit1:                                            ; preds = %edt.body
  br label %exit

exit:                                             ; preds = %exit1
  %toedt.1.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.1.slot.0, align 4
  %11 = load i32, ptr %toedt.1.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %11, i64 %edt_0.depv_0.guid.load)
  %toedt.3.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.3.slot.1, align 4
  %12 = load i32, ptr %toedt.3.slot.1, align 4
  call void @artsSignalEdt(i64 %2, i32 %12, i64 %edt_0.depv_1.guid.load)
  ret void
}

define internal void @edt_3.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_3.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_3.depv_0.guid.load = load i64, ptr %edt_3.depv_0.guid, align 8
  %edt_3.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
  %edt_3.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_3.depv_1.guid.load = load i64, ptr %edt_3.depv_1.guid, align 8
  %edt_3.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_3.depv_1.ptr.load = load ptr, ptr %edt_3.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !8
  %1 = load i32, ptr %edt_3.depv_1.ptr.load, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1) #4
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt_1.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_1.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt_1.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt_1.paramv_1.guid.edt_3 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_1.paramv_1.guid.edt_3, align 8
  %edt_1.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_1.depv_0.guid.load = load i64, ptr %edt_1.depv_0.guid, align 8
  %edt_1.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %3 = load i32, ptr %edt_1.depv_0.ptr.load, align 4, !tbaa !8, !noalias !12
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt_1.depv_0.ptr.load, align 4, !tbaa !8, !noalias !12
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #4, !noalias !12
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.3.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.3.slot.0, align 4
  %4 = load i32, ptr %toedt.3.slot.0, align 4
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt_1.depv_0.guid.load)
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
  %edt_2.main_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_2.main_guid.addr, align 8
  %4 = load i64, ptr %edt_2.main_guid.addr, align 8
  br label %body

body:                                             ; preds = %then
  %edt_2_paramc = alloca i32, align 4
  store i32 0, ptr %edt_2_paramc, align 4
  %5 = load i32, ptr %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %5, align 8
  %6 = alloca i32, align 4
  store i32 0, ptr %6, align 4
  %7 = load i32, ptr %6, align 4
  %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.main, i64 %4, i32 %5, ptr %edt_2_paramv, i32 %7)
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
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!13}
!13 = distinct !{!13, !14, !".omp_outlined.: %__context"}
!14 = distinct !{!14, !".omp_outlined."}


-------------------------------------------------
[arts-graph] Destroying the CARTS Graph
llvm-dis simple_arts_analysis.bc
opt -O3 simple_arts_analysis.bc -o simple_opt.bc
llvm-dis simple_opt.bc
clang++ simple_opt.bc -O3 -g3 -march=native -o simple_opt -I/home/randres/projects/carts/carts/include -L/home/randres/projects/carts/.install/arts/lib -larts -lrdmacm
