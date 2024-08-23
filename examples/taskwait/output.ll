clang++ -fopenmp -O3 -g0 -emit-llvm -c taskwait.cpp -o taskwait.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis taskwait.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder\
	-passes="omp-transform" taskwait.bc -o taskwait_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
taskwait.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 
[omp-transform] Processing main function

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt
- - - - - - - - - - - - - - - -
[omp-transform] Parallel Region Found:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.1(ptr, ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.1(ptr %shared_number, ptr %random_number)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.1(ptr %0, ptr %1) #2 {
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
  %11 = tail call i32 @__kmpc_omp_taskwait(ptr nonnull @1, i32 undef)
  %12 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..6)
  %13 = load ptr, ptr %12, align 8, !tbaa !10
  store ptr %1, ptr %13, align 8, !tbaa.struct !15
  %14 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %12, i64 0, i32 1
  %15 = load i32, ptr %0, align 4, !tbaa !6
  store i32 %15, ptr %14, align 8, !tbaa !17
  %16 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %12)
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  tail call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.1
- - - - - - - - - - - - - - - -
[omp-transform] Single Region Found:
  %3 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %4 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.2(ptr, i32)
[arts-ir-builder] New callsite:   call void @carts.edt.2(ptr %0, i32 %7)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.2(ptr %0, i32 %1) #2 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !12
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
  %4 = load ptr, ptr undef, align 8, !tbaa !23, !alias.scope !20, !noalias !25
  %5 = load i32, ptr %0, align 4, !tbaa !8, !noalias !20
  %inc.i = add nsw i32 %5, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !8, !noalias !20
  %6 = load i32, ptr undef, align 4, !tbaa !8, !noalias !20
  %inc1.i = add nsw i32 %1, 1
  store i32 %inc1.i, ptr undef, align 4, !tbaa !8, !noalias !20
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !20
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.2
[omp-transform] Processing function: carts.edt.2 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Taskwait Found:
  %5 = tail call i32 @__kmpc_omp_taskwait(ptr nonnull @1, i32 undef)
[omp-transform] New Function: ; Function Attrs: nocallback nofree nounwind
define internal void @carts.edt.3(ptr %0, ptr %1) #8 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..6)
  %3 = load ptr, ptr %2, align 8, !tbaa !14
  store ptr %0, ptr %3, align 8, !tbaa.struct !37
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %2, i64 0, i32 1
  %5 = load i32, ptr %1, align 4, !tbaa !8
  store i32 %5, ptr %4, align 8, !tbaa !39
  %6 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %2)
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %exit

exit:                                             ; preds = %entry.split
  br label %exit.ret.exitStub

exit.ret.exitStub:                                ; preds = %exit
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.3
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..6)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.4(ptr, i32)
[arts-ir-builder] New callsite:   call void @carts.edt.4(ptr %0, i32 %5)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.4(ptr %0, i32 %1) #2 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !14
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !26)
  %4 = load i32, ptr undef, align 4, !tbaa !8, !noalias !26
  %inc.i = add nsw i32 %1, 1
  store i32 %inc.i, ptr undef, align 4, !tbaa !8, !noalias !26
  %5 = load ptr, ptr undef, align 8, !tbaa !29, !alias.scope !26, !noalias !31
  %6 = load i32, ptr %0, align 4, !tbaa !8, !noalias !26
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %6), !noalias !26
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.4
[omp-transform] Processing function: carts.edt.4 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Single End Region Found:
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[omp-transform] Processing function: carts.edt.3 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] SyncEDTFn: ; Function Attrs: nocallback nofree nounwind
define internal void @carts.edt.1.entry.split(ptr %0, ptr %1) #7 {
newFuncRoot:
  br label %entry.split

codeRepl:                                         ; preds = %entry.split
  call void @carts.edt.3(ptr %0, ptr %1)
  br label %exit.ret

exit.ret:                                         ; preds = %codeRepl
  br label %exit.ret.ret.exitStub

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !25
  %3 = load i32, ptr %0, align 4, !tbaa !25
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3)
  %4 = load i32, ptr %0, align 4, !tbaa !25
  call void @carts.edt.2(ptr %1, i32 %4) #8
  br label %codeRepl

exit.ret.ret.exitStub:                            ; preds = %exit.ret
  ret void
}

[omp-transform] SyncCB:   call void @carts.edt.5(ptr %1, ptr %0)
[omp-transform] Processing function: carts.edt.1 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] New Function: define internal void @carts.edt.6(ptr %shared_number, ptr %random_number) {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !25
  %1 = load i32, ptr %random_number, align 4, !tbaa !25
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1)
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.6
[omp-transform] Processing function: carts.edt.6 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'taskwait.bc'
source_filename = "taskwait.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@.str.4 = private unnamed_addr constant [28 x i8] c"EDT 4: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.7 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  call void @carts.edt()
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
define internal void @carts.edt.1(ptr %0, ptr %1) #2 !carts !6 {
entry:
  br label %codeRepl1

codeRepl1:                                        ; preds = %entry
  call void @carts.edt.5(ptr %1, ptr %0)
  br label %exit.ret.ret

exit.ret.ret:                                     ; preds = %codeRepl1
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.2(ptr %0, i32 %1) #2 !carts !8 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !10
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !18)
  %4 = load ptr, ptr undef, align 8, !tbaa !21, !alias.scope !18, !noalias !23
  %5 = load i32, ptr %0, align 4, !tbaa !25, !noalias !18
  %inc.i = add nsw i32 %5, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !25, !noalias !18
  %6 = load i32, ptr undef, align 4, !tbaa !25, !noalias !18
  %inc1.i = add nsw i32 %1, 1
  store i32 %inc1.i, ptr undef, align 4, !tbaa !25, !noalias !18
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !18
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.4(ptr %0, i32 %1) #2 !carts !8 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !10
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !26)
  %4 = load i32, ptr undef, align 4, !tbaa !25, !noalias !26
  %inc.i = add nsw i32 %1, 1
  store i32 %inc.i, ptr undef, align 4, !tbaa !25, !noalias !26
  %5 = load ptr, ptr undef, align 8, !tbaa !29, !alias.scope !26, !noalias !31
  %6 = load i32, ptr %0, align 4, !tbaa !25, !noalias !26
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %6), !noalias !26
  ret void
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !33 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

define internal void @carts.edt() !carts !35 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #5
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #5
  %call1 = tail call i32 @rand() #5
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !25
  %call2 = tail call i32 @rand() #5
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !25
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4)
  call void @carts.edt.1(ptr %shared_number, ptr %random_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.6(ptr %shared_number, ptr %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: nocallback nofree nounwind
define internal void @carts.edt.3(ptr %0, ptr %1) #7 !carts !37 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !25
  call void @carts.edt.4(ptr %0, i32 %2) #8
  br label %exit

exit:                                             ; preds = %entry.split
  br label %exit.ret.exitStub

exit.ret.exitStub:                                ; preds = %exit
  ret void
}

; Function Attrs: nocallback nofree nounwind
define internal void @carts.edt.5(ptr %0, ptr %1) #7 !carts !6 {
newFuncRoot:
  br label %entry.split

codeRepl:                                         ; preds = %entry.split
  call void @carts.edt.3(ptr %0, ptr %1)
  br label %exit.ret

exit.ret:                                         ; preds = %codeRepl
  br label %exit.ret.ret.exitStub

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !25
  %3 = load i32, ptr %0, align 4, !tbaa !25
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3)
  %4 = load i32, ptr %0, align 4, !tbaa !25
  call void @carts.edt.2(ptr %1, i32 %4) #8
  br label %codeRepl

exit.ret.ret.exitStub:                            ; preds = %exit.ret
  ret void
}

define internal void @carts.edt.6(ptr %shared_number, ptr %random_number) !carts !37 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !25
  %1 = load i32, ptr %random_number, align 4, !tbaa !25
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1)
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
attributes #7 = { nocallback nofree nounwind }
attributes #8 = { memory(argmem: readwrite) }

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
!8 = !{!"task", !9}
!9 = !{!"dep", !"param"}
!10 = !{!11, !13, i64 0}
!11 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !17, i64 40}
!12 = !{!"_ZTS10kmp_task_t", !13, i64 0, !13, i64 8, !16, i64 16, !14, i64 24, !14, i64 32}
!13 = !{!"any pointer", !14, i64 0}
!14 = !{!"omnipotent char", !15, i64 0}
!15 = !{!"Simple C++ TBAA"}
!16 = !{!"int", !14, i64 0}
!17 = !{!"_ZTS15.kmp_privates.t", !16, i64 0}
!18 = !{!19}
!19 = distinct !{!19, !20, !".omp_outlined.: %__context"}
!20 = distinct !{!20, !".omp_outlined."}
!21 = !{!22, !13, i64 0}
!22 = !{!"_ZTSZ4mainE3$_0", !13, i64 0}
!23 = !{!24}
!24 = distinct !{!24, !20, !".omp_outlined.: %.privates."}
!25 = !{!16, !16, i64 0}
!26 = !{!27}
!27 = distinct !{!27, !28, !".omp_outlined..3: %__context"}
!28 = distinct !{!28, !".omp_outlined..3"}
!29 = !{!30, !13, i64 0}
!30 = !{!"_ZTSZ4mainE3$_1", !13, i64 0}
!31 = !{!32}
!32 = distinct !{!32, !28, !".omp_outlined..3: %.privates."}
!33 = !{!34}
!34 = !{i64 2, i64 -1, i64 -1, i1 true}
!35 = !{!"main", !36}
!36 = !{}
!37 = !{!"task", !7}


-------------------------------------------------
[omp-transform] [Attributor] Done with 8 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'taskwait.bc'
source_filename = "taskwait.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 3: The number is %d/%d\0A\00", align 1
@.str.4 = private unnamed_addr constant [28 x i8] c"EDT 4: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.7 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  call void @carts.edt()
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
define internal void @carts.edt.1(ptr nocapture %0, ptr nocapture readonly %1) #4 !carts !6 {
entry:
  br label %codeRepl1

codeRepl1:                                        ; preds = %entry
  call void @carts.edt.5(ptr nocapture readonly %1, ptr nocapture %0) #6
  br label %exit.ret.ret

exit.ret.ret:                                     ; preds = %codeRepl1
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.2(ptr nocapture %0, i32 %1) #4 !carts !8 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !10)
  %2 = load i32, ptr %0, align 4, !tbaa !13, !noalias !10
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !13, !noalias !10
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #6, !noalias !10
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #6

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.4(ptr nocapture readonly %0, i32 %1) #4 !carts !8 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  %inc.i = add nsw i32 %1, 1
  %2 = load i32, ptr %0, align 4, !tbaa !13, !noalias !17
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %2) #6, !noalias !17
  ret void
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !20 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

; Function Attrs: norecurse nounwind
define internal void @carts.edt() #8 !carts !22 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #6
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #6
  %call1 = tail call i32 @rand() #6
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !13
  %call2 = tail call i32 @rand() #6
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !13
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #6
  call void @carts.edt.1(ptr nocapture %shared_number, ptr nocapture %random_number) #11
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.6(ptr nocapture %shared_number, ptr nocapture %random_number) #6
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: nocallback nofree norecurse nosync nounwind
define internal void @carts.edt.3(ptr nocapture readonly %0, ptr nocapture readonly %1) #9 !carts !24 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !13
  call void @carts.edt.4(ptr nocapture readonly %0, i32 %2) #11
  br label %exit

exit:                                             ; preds = %entry.split
  br label %exit.ret.exitStub

exit.ret.exitStub:                                ; preds = %exit
  ret void
}

; Function Attrs: nocallback nofree norecurse nounwind
define internal void @carts.edt.5(ptr nocapture readonly %0, ptr nocapture %1) #10 !carts !6 {
newFuncRoot:
  br label %entry.split

codeRepl:                                         ; preds = %entry.split
  call void @carts.edt.3(ptr nocapture readonly %0, ptr nocapture %1) #6
  br label %exit.ret

exit.ret:                                         ; preds = %codeRepl
  br label %exit.ret.ret.exitStub

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !13
  %3 = load i32, ptr %0, align 4, !tbaa !13
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3) #6
  %4 = load i32, ptr %0, align 4, !tbaa !13
  call void @carts.edt.2(ptr nocapture %1, i32 %4) #12
  br label %codeRepl

exit.ret.ret.exitStub:                            ; preds = %exit.ret
  ret void
}

; Function Attrs: norecurse nounwind
define internal void @carts.edt.6(ptr nocapture readonly %shared_number, ptr nocapture readonly %random_number) #8 !carts !24 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !13
  %1 = load i32, ptr %random_number, align 4, !tbaa !13
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1) #6
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
attributes #9 = { nocallback nofree norecurse nosync nounwind }
attributes #10 = { nocallback nofree norecurse nounwind }
attributes #11 = { nounwind memory(readwrite) }
attributes #12 = { memory(argmem: readwrite) }

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
!8 = !{!"task", !9}
!9 = !{!"dep", !"param"}
!10 = !{!11}
!11 = distinct !{!11, !12, !".omp_outlined.: %__context"}
!12 = distinct !{!12, !".omp_outlined."}
!13 = !{!14, !14, i64 0}
!14 = !{!"int", !15, i64 0}
!15 = !{!"omnipotent char", !16, i64 0}
!16 = !{!"Simple C++ TBAA"}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..3: %__context"}
!19 = distinct !{!19, !".omp_outlined..3"}
!20 = !{!21}
!21 = !{i64 2, i64 -1, i64 -1, i1 true}
!22 = !{!"main", !23}
!23 = !{}
!24 = !{!"task", !7}


-------------------------------------------------
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
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/carts/src/analysis/ARTSAnalysisPass.cpp:582: auto AAEDTInfoFunction::initialize(Attributor &)::(anonymous class)::operator()(AbstractCallSite) const: Assertion `CB && "Next instruction is not a CallBase!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis taskwait_arts_ir.bc -o taskwait_arts_analysis.bc
 #0 0x00007f1679f1bef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f1679f19b7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f1679f1c5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f167cc5d910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f1679826d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f16798283e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f167981ec6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007f167981ecf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007f1678487b41 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x16b41)
 #9 0x00007f1678487726 bool llvm::function_ref<bool (llvm::AbstractCallSite)>::callback_fn<AAEDTInfoFunction::initialize(llvm::Attributor&)::'lambda'(llvm::AbstractCallSite)>(long, llvm::AbstractCallSite) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x16726)
#10 0x00007f167c69592e llvm::Attributor::checkForAllCallSites(llvm::function_ref<bool (llvm::AbstractCallSite)>, llvm::Function const&, bool, llvm::AbstractAttribute const*, bool&, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x8e92e)
#11 0x00007f167c6962dd llvm::Attributor::checkForAllCallSites(llvm::function_ref<bool (llvm::AbstractCallSite)>, llvm::AbstractAttribute const&, bool, bool&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x8f2dd)
#12 0x00007f1678486d07 AAEDTInfoFunction::initialize(llvm::Attributor&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x15d07)
#13 0x00007f16784862d2 AAEDTInfo const* llvm::Attributor::getOrCreateAAFor<AAEDTInfo>(llvm::IRPosition, llvm::AbstractAttribute const*, llvm::DepClassTy, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x152d2)
#14 0x00007f16784903ae llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#15 0x00007f167a30f2a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#16 0x0000565098ae3293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#17 0x0000565098af0aaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#18 0x00007f167981124d __libc_start_main (/lib64/libc.so.6+0x3524d)
#19 0x0000565098adca3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:24: taskwait_arts_analysis.bc] Aborted
