; ModuleID = 'taskwait_arts_ir.bc'
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
