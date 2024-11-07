; ModuleID = 'taskwithdeps.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }
%struct.anon.4 = type { ptr, ptr }
%struct.anon.6 = type { ptr, ptr }

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
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %res = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #7
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #7
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %x) #7
  %call1 = tail call i32 @rand() #7
  store i32 %call1, ptr %x, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %y) #7
  %call2 = tail call i32 @rand() #7
  store i32 %call2, ptr %y, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %res) #7
  store i32 0, ptr %res, align 4, !tbaa !6
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 3, ptr nonnull @main.omp_outlined, ptr nonnull %res, ptr nonnull %x, ptr nonnull %y)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %res) #7
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %y) #7
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %x) #7
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #3

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %res, ptr noundef nonnull align 4 dereferenceable(4) %x, ptr noundef nonnull align 4 dereferenceable(4) %y) #4 {
entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  %0 = load i32, ptr %.global_tid., align 4, !tbaa !6
  %1 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 %0)
  %.not = icmp eq i32 %1, 0
  br i1 %.not, label %omp_if.end, label %omp_if.then

omp_if.then:                                      ; preds = %entry
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry.)
  %3 = load ptr, ptr %2, align 8, !tbaa !10
  store ptr %res, ptr %3, align 8, !tbaa.struct !14
  %4 = ptrtoint ptr %res to i64
  store i64 %4, ptr %.dep.arr.addr, align 8, !tbaa !16
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  store i64 4, ptr %5, align 8, !tbaa !19
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  store i8 3, ptr %6, align 8, !tbaa !20
  %7 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %2, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null)
  %8 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2)
  %9 = load ptr, ptr %8, align 8, !tbaa !10
  store ptr %x, ptr %9, align 8, !tbaa.struct !14
  %10 = ptrtoint ptr %x to i64
  store i64 %10, ptr %.dep.arr.addr2, align 8, !tbaa !16
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  store i64 4, ptr %11, align 8, !tbaa !19
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  store i8 3, ptr %12, align 8, !tbaa !20
  %13 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %8, i32 1, ptr nonnull %.dep.arr.addr2, i32 0, ptr null)
  %14 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4)
  %15 = load ptr, ptr %14, align 8, !tbaa !10
  store ptr %y, ptr %15, align 8, !tbaa.struct !14
  %16 = ptrtoint ptr %y to i64
  store i64 %16, ptr %.dep.arr.addr5, align 8, !tbaa !16
  %17 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  store i64 4, ptr %17, align 8, !tbaa !19
  %18 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  store i8 3, ptr %18, align 8, !tbaa !20
  %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %14, i32 1, ptr nonnull %.dep.arr.addr5, i32 0, ptr null)
  %20 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..6)
  %21 = load ptr, ptr %20, align 8, !tbaa !10
  store ptr %res, ptr %21, align 8, !tbaa.struct !21
  %agg.captured7.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %21, i64 8
  store ptr %x, ptr %agg.captured7.sroa.2.0..sroa_idx, align 8, !tbaa.struct !14
  store i64 %10, ptr %.dep.arr.addr8, align 8, !tbaa !16
  %22 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  store i64 4, ptr %22, align 8, !tbaa !19
  %23 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  store i8 1, ptr %23, align 8, !tbaa !20
  %24 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %20, i32 1, ptr nonnull %.dep.arr.addr8, i32 0, ptr null)
  %25 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..8)
  %26 = load ptr, ptr %25, align 8, !tbaa !10
  store ptr %res, ptr %26, align 8, !tbaa.struct !21
  %agg.captured10.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %26, i64 8
  store ptr %y, ptr %agg.captured10.sroa.2.0..sroa_idx, align 8, !tbaa.struct !14
  store i64 %16, ptr %.dep.arr.addr11, align 8, !tbaa !16
  %27 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  store i64 4, ptr %27, align 8, !tbaa !19
  %28 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  store i8 1, ptr %28, align 8, !tbaa !20
  %29 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %25, i32 1, ptr nonnull %.dep.arr.addr11, i32 0, ptr null)
  %30 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..10)
  %31 = load ptr, ptr %30, align 8, !tbaa !10
  store ptr %res, ptr %31, align 8, !tbaa.struct !14
  store i64 %4, ptr %.dep.arr.addr14, align 8, !tbaa !16
  %32 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  store i64 4, ptr %32, align 8, !tbaa !19
  %33 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  store i8 1, ptr %33, align 8, !tbaa !20
  %34 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr nonnull %30, i32 1, ptr nonnull %.dep.arr.addr14, i32 0, ptr null)
  call void @__kmpc_end_single(ptr nonnull @1, i32 %0)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  call void @__kmpc_barrier(ptr nonnull @2, i32 %0)
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !22)
  %3 = load ptr, ptr %2, align 8, !tbaa !25, !alias.scope !22
  store i32 0, ptr %3, align 4, !tbaa !6, !noalias !22
  ret i32 0
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #7

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..2(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !27)
  %3 = load ptr, ptr %2, align 8, !tbaa !30, !alias.scope !27
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !noalias !27
  ret i32 0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..4(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !32)
  %3 = load ptr, ptr %2, align 8, !tbaa !35, !alias.scope !32
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !noalias !32
  ret i32 0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..6(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !37)
  %3 = getelementptr inbounds %struct.anon.4, ptr %2, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !40, !alias.scope !37
  %5 = load i32, ptr %4, align 4, !tbaa !6, !noalias !37
  %6 = load ptr, ptr %2, align 8, !tbaa !42, !alias.scope !37
  %7 = load i32, ptr %6, align 4, !tbaa !6, !noalias !37
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %6, align 4, !tbaa !6, !noalias !37
  ret i32 0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..8(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !43)
  %3 = getelementptr inbounds %struct.anon.6, ptr %2, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !46, !alias.scope !43
  %5 = load i32, ptr %4, align 4, !tbaa !6, !noalias !43
  %6 = load ptr, ptr %2, align 8, !tbaa !48, !alias.scope !43
  %7 = load i32, ptr %6, align 4, !tbaa !6, !noalias !43
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %6, align 4, !tbaa !6, !noalias !43
  ret i32 0
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #8

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..10(i32 %0, ptr noalias nocapture noundef readonly %1) #9 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  tail call void @llvm.experimental.noalias.scope.decl(metadata !49)
  %3 = load ptr, ptr %2, align 8, !tbaa !52, !alias.scope !49
  %4 = load i32, ptr %3, align 4, !tbaa !6, !noalias !49
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4), !noalias !49
  ret i32 0
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !54 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #10

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { convergent nounwind }
attributes #6 = { mustprogress nofree norecurse nosync nounwind willreturn uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }
attributes #8 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

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
!10 = !{!11, !13, i64 0}
!11 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0}
!12 = !{!"_ZTS10kmp_task_t", !13, i64 0, !13, i64 8, !7, i64 16, !8, i64 24, !8, i64 32}
!13 = !{!"any pointer", !8, i64 0}
!14 = !{i64 0, i64 8, !15}
!15 = !{!13, !13, i64 0}
!16 = !{!17, !18, i64 0}
!17 = !{!"_ZTS15kmp_depend_info", !18, i64 0, !18, i64 8, !8, i64 16}
!18 = !{!"long", !8, i64 0}
!19 = !{!17, !18, i64 8}
!20 = !{!17, !8, i64 16}
!21 = !{i64 0, i64 8, !15, i64 8, i64 8, !15}
!22 = !{!23}
!23 = distinct !{!23, !24, !".omp_outlined.: %__context"}
!24 = distinct !{!24, !".omp_outlined."}
!25 = !{!26, !13, i64 0}
!26 = !{!"_ZTSZ4mainE3$_0", !13, i64 0}
!27 = !{!28}
!28 = distinct !{!28, !29, !".omp_outlined..1: %__context"}
!29 = distinct !{!29, !".omp_outlined..1"}
!30 = !{!31, !13, i64 0}
!31 = !{!"_ZTSZ4mainE3$_1", !13, i64 0}
!32 = !{!33}
!33 = distinct !{!33, !34, !".omp_outlined..3: %__context"}
!34 = distinct !{!34, !".omp_outlined..3"}
!35 = !{!36, !13, i64 0}
!36 = !{!"_ZTSZ4mainE3$_2", !13, i64 0}
!37 = !{!38}
!38 = distinct !{!38, !39, !".omp_outlined..5: %__context"}
!39 = distinct !{!39, !".omp_outlined..5"}
!40 = !{!41, !13, i64 8}
!41 = !{!"_ZTSZ4mainE3$_3", !13, i64 0, !13, i64 8}
!42 = !{!41, !13, i64 0}
!43 = !{!44}
!44 = distinct !{!44, !45, !".omp_outlined..7: %__context"}
!45 = distinct !{!45, !".omp_outlined..7"}
!46 = !{!47, !13, i64 8}
!47 = !{!"_ZTSZ4mainE3$_4", !13, i64 0, !13, i64 8}
!48 = !{!47, !13, i64 0}
!49 = !{!50}
!50 = distinct !{!50, !51, !".omp_outlined..9: %__context"}
!51 = distinct !{!51, !".omp_outlined..9"}
!52 = !{!53, !13, i64 0}
!53 = !{!"_ZTSZ4mainE3$_5", !13, i64 0}
!54 = !{!55}
!55 = !{i64 2, i64 -1, i64 -1, i1 true}
