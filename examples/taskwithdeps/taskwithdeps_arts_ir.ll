; ModuleID = 'taskwithdeps_arts_ir.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }

@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z16long_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 {
entry:
  %x.promoted = load i32, ptr %x, align 4, !tbaa !6
  %0 = add i32 %x.promoted, 1783293664
  store i32 %0, ptr %x, align 4, !tbaa !6
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z17short_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 {
entry:
  %x.promoted = load i32, ptr %x, align 4, !tbaa !6
  %0 = add i32 %x.promoted, 499500
  store i32 %0, ptr %x, align 4, !tbaa !6
  ret void
}

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #2 {
entry:
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #3

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.parallel(ptr nocapture %0, ptr nocapture %1, ptr nocapture %2) #4 !carts !10 {
pre_entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  br label %entry

entry:                                            ; preds = %pre_entry
  call void @carts.edt.task(ptr nocapture writeonly %0) #13
  %3 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  %4 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  call void @carts.edt.task.1(ptr nocapture %1) #13
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  call void @carts.edt.task.2(ptr nocapture %2) #13
  %7 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  call void @carts.edt.task.3(ptr nocapture %0, ptr nocapture %1) #13
  %9 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  %10 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  call void @carts.edt.task.4(ptr nocapture %0, ptr nocapture readonly %2) #13
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  call void @carts.edt.task.5(ptr nocapture %0) #13
  %13 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task(ptr nocapture writeonly %0) #6 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  store i32 0, ptr %0, align 4, !tbaa !6, !noalias !14
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.1(ptr nocapture %0) #8 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  tail call void @_Z16long_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0) #13, !noalias !17
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.2(ptr nocapture %0) #8 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
  tail call void @_Z17short_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0) #13, !noalias !20
  ret void
}

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.3(ptr nocapture %0, ptr nocapture %1) #6 !carts !23 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !25
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !25
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !25
  %4 = load i32, ptr %1, align 4, !tbaa !6, !noalias !25
  %inc.i = add nsw i32 %4, 1
  store i32 %inc.i, ptr %1, align 4, !tbaa !6, !noalias !25
  ret void
}

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.4(ptr nocapture %0, ptr nocapture readonly %1) #6 !carts !23 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !28)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !28
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !28
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !28
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #9

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.task.5(ptr nocapture readonly %0) #4 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !31)
  %1 = load i32, ptr %0, align 4, !tbaa !6, !noalias !31
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %1) #7, !noalias !31
  ret void
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !34 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #10

; Function Attrs: norecurse nounwind
define internal void @carts.edt.main() #11 !carts !36 {
entry:
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %res = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #7
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #7
  %call1 = tail call i32 @rand() #7
  store i32 %call1, ptr %x, align 4, !tbaa !6
  %call2 = tail call i32 @rand() #7
  store i32 %call2, ptr %y, align 4, !tbaa !6
  store i32 0, ptr %res, align 4, !tbaa !6
  call void @carts.edt.parallel(ptr nocapture %res, ptr nocapture %x, ptr nocapture %y) #13
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: nounwind memory(none)
define internal void @carts.edt.sync.done() #12 !carts !38 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #5 = { convergent nounwind }
attributes #6 = { nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) }
attributes #7 = { nounwind }
attributes #8 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) }
attributes #9 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #11 = { norecurse nounwind }
attributes #12 = { nounwind memory(none) }
attributes #13 = { nounwind memory(readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = !{!"sync", !11}
!11 = !{!"dep", !"dep", !"dep"}
!12 = !{!"task", !13}
!13 = !{!"dep"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..1: %__context"}
!19 = distinct !{!19, !".omp_outlined..1"}
!20 = !{!21}
!21 = distinct !{!21, !22, !".omp_outlined..3: %__context"}
!22 = distinct !{!22, !".omp_outlined..3"}
!23 = !{!"task", !24}
!24 = !{!"dep", !"dep"}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..5: %__context"}
!27 = distinct !{!27, !".omp_outlined..5"}
!28 = !{!29}
!29 = distinct !{!29, !30, !".omp_outlined..7: %__context"}
!30 = distinct !{!30, !".omp_outlined..7"}
!31 = !{!32}
!32 = distinct !{!32, !33, !".omp_outlined..9: %__context"}
!33 = distinct !{!33, !".omp_outlined..9"}
!34 = !{!35}
!35 = !{i64 2, i64 -1, i64 -1, i1 true}
!36 = !{!"main", !37}
!37 = !{}
!38 = !{!"task", !37}
