; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { i8*, i32 (i32, i8*)*, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { i32 (i32, i8*)* }
%struct..kmp_privates.t = type { i32, i32 }
%struct.anon = type { i32*, i32* }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }
%struct.anon.0 = type { i32* }

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@.str.3 = private unnamed_addr constant [32 x i8] c"I think the number is %d - %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  %0 = bitcast i32* %number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #6
  store i32 1, i32* %number, align 4, !tbaa !4
  %1 = bitcast i32* %shared_number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #6
  store i32 10000, i32* %shared_number, align 4, !tbaa !4
  %2 = bitcast i32* %random_number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #6
  %call = tail call i32 @rand() #6
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
  %3 = bitcast i32* %NewRandom to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #6
  %call1 = tail call i32 @rand() #6
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !4
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number)
  %4 = load i32, i32* %number, align 4, !tbaa !4
  %5 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #6
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture noundef readonly %.global_tid., i32* noalias nocapture noundef readnone %.bound_tid., i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noundef nonnull align 4 dereferenceable(4) %number, i32* noundef nonnull align 4 dereferenceable(4) %shared_number) #3 {
entry:
  %0 = load i32, i32* %.global_tid., align 4, !tbaa !4
  %1 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
  %2 = bitcast i8* %1 to i8**
  %3 = load i8*, i8** %2, align 8, !tbaa !8
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %3 to i32**
  store i32* %number, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !13
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %3, i64 8
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**
  store i32* %shared_number, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !15
  %4 = getelementptr inbounds i8, i8* %1, i64 40
  %5 = bitcast i8* %4 to i32*
  %6 = load i32, i32* %random_number, align 4, !tbaa !4
  store i32 %6, i32* %5, align 8, !tbaa !16
  %7 = getelementptr inbounds i8, i8* %1, i64 44
  %8 = bitcast i8* %7 to i32*
  %9 = load i32, i32* %NewRandom, align 4, !tbaa !4
  store i32 %9, i32* %8, align 4, !tbaa !17
  %10 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 %0, i8* %1)
  %11 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
  %12 = bitcast i8* %11 to i32***
  %13 = load i32**, i32*** %12, align 8, !tbaa !18
  store i32* %shared_number, i32** %13, align 8, !tbaa.struct !15
  %14 = getelementptr inbounds i8, i8* %11, i64 40
  %15 = bitcast i8* %14 to i32*
  %16 = load i32, i32* %number, align 4, !tbaa !4
  store i32 %16, i32* %15, align 8, !tbaa !21
  %17 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 %0, i8* %11)
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias nocapture noundef readonly %1) #5 personality i32 (...)* @__gxx_personality_v0 {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates* %1 to %struct.anon**
  %3 = load %struct.anon*, %struct.anon** %2, align 8, !tbaa !8
  %.idx = getelementptr %struct.anon, %struct.anon* %3, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !22
  %.idx2 = getelementptr %struct.anon, %struct.anon* %3, i64 0, i32 1
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !24
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i64 0, i32 1, i32 0
  %5 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i64 0, i32 1, i32 1
  %6 = load i32, i32* %.idx.val, align 4, !tbaa !4, !noalias !25
  %7 = load i32, i32* %.idx2.val, align 4, !tbaa !4, !noalias !25
  %8 = load i32, i32* %4, align 4, !tbaa !4, !alias.scope !25
  %9 = load i32, i32* %5, align 4, !tbaa !4, !alias.scope !25
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %6, i32 noundef %7, i32 noundef %8, i32 noundef %9) #6, !noalias !25
  %10 = load i32, i32* %.idx.val, align 4, !tbaa !4, !noalias !25
  %inc.i = add nsw i32 %10, 1
  store i32 %inc.i, i32* %.idx.val, align 4, !tbaa !4, !noalias !25
  %11 = load i32, i32* %.idx2.val, align 4, !tbaa !4, !noalias !25
  %dec.i = add nsw i32 %11, -1
  store i32 %dec.i, i32* %.idx2.val, align 4, !tbaa !4, !noalias !25
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #6

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias nocapture noundef %1) #5 personality i32 (...)* @__gxx_personality_v0 {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates.1* %1 to %struct.anon.0**
  %3 = load %struct.anon.0*, %struct.anon.0** %2, align 8, !tbaa !18
  %.idx = getelementptr %struct.anon.0, %struct.anon.0* %3, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !28
  %.idx.val.val = load i32, i32* %.idx.val, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !30)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i64 0, i32 1, i32 0
  %5 = load i32, i32* %4, align 4, !tbaa !4, !alias.scope !30
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %5, i32 noundef %.idx.val.val) #6, !noalias !30
  %inc.i = add nsw i32 %5, 1
  store i32 %inc.i, i32* %4, align 4, !tbaa !4, !alias.scope !30
  ret i32 0
}

; Function Attrs: nounwind
declare !callback !33 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #6

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

attributes #0 = { mustprogress norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { alwaysinline norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nofree norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { inaccessiblememonly nofree nosync nounwind willreturn }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}
!nvvm.annotations = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.6"}
!4 = !{!5, !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = !{!9, !11, i64 0}
!9 = !{!"_ZTS24kmp_task_t_with_privates", !10, i64 0, !12, i64 40}
!10 = !{!"_ZTS10kmp_task_t", !11, i64 0, !11, i64 8, !5, i64 16, !6, i64 24, !6, i64 32}
!11 = !{!"any pointer", !6, i64 0}
!12 = !{!"_ZTS15.kmp_privates.t", !5, i64 0, !5, i64 4}
!13 = !{i64 0, i64 8, !14, i64 8, i64 8, !14}
!14 = !{!11, !11, i64 0}
!15 = !{i64 0, i64 8, !14}
!16 = !{!9, !5, i64 40}
!17 = !{!9, !5, i64 44}
!18 = !{!19, !11, i64 0}
!19 = !{!"_ZTS24kmp_task_t_with_privates", !10, i64 0, !20, i64 40}
!20 = !{!"_ZTS15.kmp_privates.t", !5, i64 0}
!21 = !{!19, !5, i64 40}
!22 = !{!23, !11, i64 0}
!23 = !{!"_ZTSZ4mainE3$_0", !11, i64 0, !11, i64 8}
!24 = !{!23, !11, i64 8}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..1: %.privates."}
!27 = distinct !{!27, !".omp_outlined..1"}
!28 = !{!29, !11, i64 0}
!29 = !{!"_ZTSZ4mainE3$_1", !11, i64 0}
!30 = !{!31}
!31 = distinct !{!31, !32, !".omp_outlined..2: %.privates."}
!32 = distinct !{!32, !".omp_outlined..2"}
!33 = !{!34}
!34 = !{i64 2, i64 -1, i64 -1, i1 true}
