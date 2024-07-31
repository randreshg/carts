; ModuleID = 'output.ll'
source_filename = "example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }

@.str = private unnamed_addr constant [29 x i8] c"I think the number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [25 x i8] c";example.cpp;main;35;9;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 24, ptr @0 }, align 8
@2 = private unnamed_addr constant [25 x i8] c";example.cpp;main;28;3;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 24, ptr @2 }, align 8
@.str.2 = private unnamed_addr constant [30 x i8] c"The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4 !carts !8
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %shared_number) #6
  %call = tail call i32 @rand() #6
  store i32 %call, ptr %shared_number, align 4, !tbaa !8
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %random_number) #6
  %call1 = tail call i32 @rand() #6
  %rem = srem i32 %call1, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !8
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @3, i32 2, ptr nonnull @.omp_outlined..1, ptr nonnull %random_number, ptr nonnull %shared_number) !27 !carts !9
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %0, i32 noundef %1)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %random_number) #6
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %shared_number) #6
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #2

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #5 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !12
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1
  %.val = load ptr, ptr %2, align 8, !tbaa !17
  %.val.val = load i32, ptr %.val, align 4, !tbaa !8
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  %4 = load i32, ptr %3, align 4, !tbaa !8, !alias.scope !19
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %.val.val, i32 noundef %4), !noalias !19
  ret i32 0
}

; Function Attrs: nounwind
declare ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #6

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined..1(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, ptr noundef nonnull align 4 dereferenceable(4) %shared_number) #7 !carts !27{
entry:
  %.global_tid..val = load i32, ptr %.global_tid., align 4, !tbaa !8
  %0 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %.global_tid..val, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.) 
  %1 = load ptr, ptr %0, align 8, !tbaa !12
  store ptr %shared_number, ptr %1, align 8, !tbaa.struct !22
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %0, i64 0, i32 1
  %3 = load i32, ptr %random_number, align 4, !tbaa !8
  store i32 %3, ptr %2, align 8, !tbaa !24
  %4 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %.global_tid..val, ptr nonnull %0)
  ret void
}

; Function Attrs: nounwind
declare !callback !25 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata, metadata, metadata) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #8

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #3 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!8 = {carts.value}
; parallel, firsprivate, lastprivates, shared
!9  = {parallel, !10, !11, !12}
!10 = {!8}
!11 = {}
!12 = {}


!0 = !{i32 7, !"Dwarf Version", i32 5}
!1 = !{i32 2, !"Debug Info Version", i32 3}
!2 = !{i32 1, !"wchar_size", i32 4}
!3 = !{i32 7, !"openmp", i32 50}
!4 = !{i32 8, !"PIC Level", i32 2}
!5 = !{i32 7, !"PIE Level", i32 2}
!6 = !{i32 7, !"uwtable", i32 2}
!7 = !{!"clang version 16.0.6"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!13, !15, i64 0}
!13 = !{!"_ZTS24kmp_task_t_with_privates", !14, i64 0, !16, i64 40}
!14 = !{!"_ZTS10kmp_task_t", !15, i64 0, !15, i64 8, !9, i64 16, !10, i64 24, !10, i64 32}
!15 = !{!"any pointer", !10, i64 0}
!16 = !{!"_ZTS15.kmp_privates.t", !9, i64 0}
!17 = !{!18, !15, i64 0}
!18 = !{!"_ZTSZ4mainE3$_0", !15, i64 0}
!19 = !{!20}
!20 = distinct !{!20, !21, !".omp_outlined.: %.privates."}
!21 = distinct !{!21, !".omp_outlined."}
!22 = !{i64 0, i64 8, !23}
!23 = !{!15, !15, i64 0}
!24 = !{!13, !9, i64 40}
!25 = !{!26}
!26 = !{i64 2, i64 -1, i64 -1, i1 true}
!27 = {fp, 1}