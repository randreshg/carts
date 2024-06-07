clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph,omp-transform \
	-load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts.bc

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
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number)

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %0 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt.1
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %9 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt.2

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.anon = type { i32*, i32* }
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
  store i32 1, i32* %number, align 4, !tbaa !4
  %1 = bitcast i32* %shared_number to i8*
  store i32 10000, i32* %shared_number, align 4, !tbaa !4
  %2 = bitcast i32* %random_number to i8*
  %call = tail call i32 @rand() #4
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
  %3 = bitcast i32* %NewRandom to i8*
  %call1 = tail call i32 @rand() #4
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !4
  call void @edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @edt.3(i32* %number, i32* %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

define internal void @edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number) !carts.metadata !8 {
entry:
  call void @edt.1(i32* %number, i32* %shared_number, i32 %4, i32 %7)
  %0 = bitcast i8* undef to i8**
  %1 = load i8*, i8** undef, align 8, !tbaa !10
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %1 to i32**
  store i32* %number, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !15
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %1, i64 8
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**
  store i32* %shared_number, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !17
  %2 = getelementptr inbounds i8, i8* undef, i64 40
  %3 = bitcast i8* undef to i32*
  %4 = load i32, i32* %random_number, align 4, !tbaa !4
  store i32 %4, i32* %3, align 8, !tbaa !18
  %5 = getelementptr inbounds i8, i8* undef, i64 44
  %6 = bitcast i8* undef to i32*
  %7 = load i32, i32* %NewRandom, align 4, !tbaa !4
  store i32 %7, i32* %6, align 4, !tbaa !19
  %8 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
  call void @edt.2(i32* %shared_number, i32 %13)
  %9 = bitcast i8* undef to i32***
  %10 = load i32**, i32*** undef, align 8, !tbaa !20
  store i32* %shared_number, i32** %10, align 8, !tbaa.struct !17
  %11 = getelementptr inbounds i8, i8* undef, i64 40
  %12 = bitcast i8* undef to i32*
  %13 = load i32, i32* %number, align 4, !tbaa !4
  store i32 %13, i32* %12, align 8, !tbaa !23
  %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

define internal void @edt.1(i32* %.idx.val, i32* %.idx2.val, i32 %0, i32 %1) !carts.metadata !24 {
entry:
  %2 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !10
  %.idx = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 0
  %3 = load i32*, i32** %.idx, align 8, !tbaa !26
  %.idx2 = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 1
  %4 = load i32*, i32** %.idx2, align 8, !tbaa !28
  tail call void @llvm.experimental.noalias.scope.decl(metadata !29)
  %5 = load i32, i32* %.idx.val, align 4, !tbaa !4, !noalias !29
  %6 = load i32, i32* %.idx2.val, align 4, !tbaa !4, !noalias !29
  %7 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !29
  %8 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !29
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %5, i32 noundef %6, i32 noundef %0, i32 noundef %1) #4, !noalias !29
  %9 = load i32, i32* %.idx.val, align 4, !tbaa !4, !noalias !29
  %inc.i = add nsw i32 %9, 1
  store i32 %inc.i, i32* %.idx.val, align 4, !tbaa !4, !noalias !29
  %10 = load i32, i32* %.idx2.val, align 4, !tbaa !4, !noalias !29
  %dec.i = add nsw i32 %10, -1
  store i32 %dec.i, i32* %.idx2.val, align 4, !tbaa !4, !noalias !29
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

define internal void @edt.2(i32* %.idx.val, i32 %0) !carts.metadata !32 {
entry:
  %1 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !20
  %.idx = getelementptr %struct.anon.0, %struct.anon.0* %1, i64 0, i32 0
  %2 = load i32*, i32** %.idx, align 8, !tbaa !34
  %.idx.val.val = load i32, i32* %.idx.val, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !36)
  %3 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !36
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %0, i32 noundef %.idx.val.val) #4, !noalias !36
  %inc.i = add nsw i32 %0, 1
  store i32 %inc.i, i32* undef, align 4, !tbaa !4, !alias.scope !36
  ret void
}

; Function Attrs: nounwind
declare !callback !39 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @edt.3(i32* %number, i32* %random_number) #0 !carts.metadata !41 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, i32* %number, align 4, !tbaa !4
  %1 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %0, i32 noundef %1)
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nofree nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }
attributes #5 = { inaccessiblememonly nofree nosync nounwind willreturn }

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
!8 = !{!"edt.parallel", !9}
!9 = !{!"edt.arg.dep", !"edt.arg.dep", !"edt.arg.dep", !"edt.arg.dep"}
!10 = !{!11, !13, i64 0}
!11 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !14, i64 40}
!12 = !{!"_ZTS10kmp_task_t", !13, i64 0, !13, i64 8, !5, i64 16, !6, i64 24, !6, i64 32}
!13 = !{!"any pointer", !6, i64 0}
!14 = !{!"_ZTS15.kmp_privates.t", !5, i64 0, !5, i64 4}
!15 = !{i64 0, i64 8, !16, i64 8, i64 8, !16}
!16 = !{!13, !13, i64 0}
!17 = !{i64 0, i64 8, !16}
!18 = !{!11, !5, i64 40}
!19 = !{!11, !5, i64 44}
!20 = !{!21, !13, i64 0}
!21 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !22, i64 40}
!22 = !{!"_ZTS15.kmp_privates.t", !5, i64 0}
!23 = !{!21, !5, i64 40}
!24 = !{!"edt.task", !25}
!25 = !{!"edt.arg.dep", !"edt.arg.dep", !"edt.arg.param", !"edt.arg.param"}
!26 = !{!27, !13, i64 0}
!27 = !{!"_ZTSZ4mainE3$_0", !13, i64 0, !13, i64 8}
!28 = !{!27, !13, i64 8}
!29 = !{!30}
!30 = distinct !{!30, !31, !".omp_outlined..1: %.privates."}
!31 = distinct !{!31, !".omp_outlined..1"}
!32 = !{!"edt.task", !33}
!33 = !{!"edt.arg.dep", !"edt.arg.param"}
!34 = !{!35, !13, i64 0}
!35 = !{!"_ZTSZ4mainE3$_1", !13, i64 0}
!36 = !{!37}
!37 = distinct !{!37, !38, !".omp_outlined..2: %.privates."}
!38 = distinct !{!38, !".omp_outlined..2"}
!39 = !{!40}
!40 = !{i64 2, i64 -1, i64 -1, i1 true}
!41 = !{!"edt.task", !42}
!42 = !{!"edt.arg.dep", !"edt.arg.dep"}


-------------------------------------------------
llvm-dis test_arts.bc
clang++ -fopenmp test_arts.bc -O3 -march=native -o test_opt -lstdc++
