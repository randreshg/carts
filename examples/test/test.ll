; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32, i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }
%struct.anon = type { ptr, ptr }

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.2 = private unnamed_addr constant [32 x i8] c"I think the number is %d - %d.\0A\00", align 1
@.str.5 = private unnamed_addr constant [36 x i8] c"The final number is %d - % d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %number) #6
  store i32 1, ptr %number, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %shared_number) #6
  store i32 10000, ptr %shared_number, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %random_number) #6
  %call = tail call i32 @rand() #6
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !6
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %NewRandom) #6
  %call1 = tail call i32 @rand() #6
  store i32 %call1, ptr %NewRandom, align 4, !tbaa !6
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 4, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
  %0 = load i32, ptr %number, align 4, !tbaa !6
  %1 = load i32, ptr %random_number, align 4, !tbaa !6
  %2 = load i32, ptr %shared_number, align 4, !tbaa !6
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1, i32 noundef %2)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %NewRandom) #6
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %random_number) #6
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %shared_number) #6
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %number) #6
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, ptr noundef nonnull align 4 dereferenceable(4) %number, ptr noundef nonnull align 4 dereferenceable(4) %shared_number) #3 {
entry:
  %0 = load i32, ptr %.global_tid., align 4, !tbaa !6
  %1 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 16, ptr nonnull @.omp_task_entry.)
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  store ptr %number, ptr %2, align 8, !tbaa.struct !15
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %2, i64 8
  store ptr %shared_number, ptr %agg.captured.sroa.2.0..sroa_idx, align 8, !tbaa.struct !17
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1
  %4 = load i32, ptr %random_number, align 4, !tbaa !6
  store i32 %4, ptr %3, align 8, !tbaa !18
  %5 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1, i32 1
  %6 = load i32, ptr %NewRandom, align 4, !tbaa !6
  store i32 %6, ptr %5, align 4, !tbaa !19
  %7 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull %1)
  %8 = load i32, ptr %shared_number, align 4, !tbaa !6
  %inc = add nsw i32 %8, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !6
  %9 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..4)
  %10 = load ptr, ptr %9, align 8, !tbaa !20
  store ptr %shared_number, ptr %10, align 8, !tbaa.struct !17
  %11 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %9, i64 0, i32 1
  %12 = load i32, ptr %number, align 4, !tbaa !6
  store i32 %12, ptr %11, align 8, !tbaa !23
  %13 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull %9)
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #5 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !10
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !24)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1, i32 1
  %5 = load ptr, ptr %2, align 8, !tbaa !27, !alias.scope !24, !noalias !29
  %6 = load i32, ptr %5, align 4, !tbaa !6, !noalias !24
  %7 = getelementptr inbounds %struct.anon, ptr %2, i64 0, i32 1
  %8 = load ptr, ptr %7, align 8, !tbaa !31, !alias.scope !24, !noalias !29
  %9 = load i32, ptr %8, align 4, !tbaa !6, !noalias !24
  %10 = load i32, ptr %3, align 4, !tbaa !6, !noalias !24
  %11 = load i32, ptr %4, align 4, !tbaa !6, !noalias !24
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %6, i32 noundef %9, i32 noundef %10, i32 noundef %11), !noalias !24
  %12 = load i32, ptr %5, align 4, !tbaa !6, !noalias !24
  %inc.i = add nsw i32 %12, 1
  store i32 %inc.i, ptr %5, align 4, !tbaa !6, !noalias !24
  %13 = load i32, ptr %8, align 4, !tbaa !6, !noalias !24
  %dec.i = add nsw i32 %13, -1
  store i32 %dec.i, ptr %8, align 4, !tbaa !6, !noalias !24
  ret i32 0
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #6

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..4(i32 %0, ptr noalias nocapture noundef %1) #5 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !20
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !32)
  %4 = load i32, ptr %3, align 4, !tbaa !6, !noalias !32
  %5 = load ptr, ptr %2, align 8, !tbaa !35, !alias.scope !32, !noalias !37
  %6 = load i32, ptr %5, align 4, !tbaa !6, !noalias !32
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %4, i32 noundef %6), !noalias !32
  %inc.i = add nsw i32 %4, 1
  store i32 %inc.i, ptr %3, align 4, !tbaa !6, !noalias !32
  ret i32 0
}

; Function Attrs: nounwind
declare !callback !39 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

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
!11 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !14, i64 40}
!12 = !{!"_ZTS10kmp_task_t", !13, i64 0, !13, i64 8, !7, i64 16, !8, i64 24, !8, i64 32}
!13 = !{!"any pointer", !8, i64 0}
!14 = !{!"_ZTS15.kmp_privates.t", !7, i64 0, !7, i64 4}
!15 = !{i64 0, i64 8, !16, i64 8, i64 8, !16}
!16 = !{!13, !13, i64 0}
!17 = !{i64 0, i64 8, !16}
!18 = !{!11, !7, i64 40}
!19 = !{!11, !7, i64 44}
!20 = !{!21, !13, i64 0}
!21 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !22, i64 40}
!22 = !{!"_ZTS15.kmp_privates.t", !7, i64 0}
!23 = !{!21, !7, i64 40}
!24 = !{!25}
!25 = distinct !{!25, !26, !".omp_outlined.: %__context"}
!26 = distinct !{!26, !".omp_outlined."}
!27 = !{!28, !13, i64 0}
!28 = !{!"_ZTSZ4mainE3$_0", !13, i64 0, !13, i64 8}
!29 = !{!30}
!30 = distinct !{!30, !26, !".omp_outlined.: %.privates."}
!31 = !{!28, !13, i64 8}
!32 = !{!33}
!33 = distinct !{!33, !34, !".omp_outlined..1: %__context"}
!34 = distinct !{!34, !".omp_outlined..1"}
!35 = !{!36, !13, i64 0}
!36 = !{!"_ZTSZ4mainE3$_1", !13, i64 0}
!37 = !{!38}
!38 = distinct !{!38, !34, !".omp_outlined..1: %.privates."}
!39 = !{!40}
!40 = !{i64 2, i64 -1, i64 -1, i1 true}
