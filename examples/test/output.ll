clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
noelle-norm test.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
# noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc

 ---------------------------------------- 
[carts] Running CARTS on Module: 
test_with_metadata.bc

 ---------------------------------------- 
[edt-graph] Building the EDT Graph
[edt-graph] Processing function: main

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  %call = tail call i32 @rand() #6, !noelle.pdg.inst.id !8

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  %call1 = tail call i32 @rand() #6, !noelle.pdg.inst.id !15

- - - - - - - - - - - - - - - -
[edt-graph] Parallel Region Found:
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18
[arts]  - Setting data environment for ParallelEDT

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5), !noelle.pdg.inst.id !30
[edt-graph] Processing function: .omp_outlined.

- - - - - - - - - - - - - - - -
[edt-graph] Task Region Found:
  %1 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !61
[arts]  - Setting data environment for TaskEDT

- - - - - - - - - - - - - - - -
[edt-graph] Task Region Found:
  %11 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !22
[arts]  - Setting data environment for TaskEDT
[edt-graph] Processing function: .omp_task_entry.

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !294), !noelle.pdg.inst.id !239

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %6, i32 noundef %7, i32 noundef %8, i32 noundef %9) #6, !noalias !79, !noelle.pdg.inst.id !37
[edt-graph] Processing function: .omp_task_entry..5

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !318), !noelle.pdg.inst.id !307

- - - - - - - - - - - - - - - -
[edt-graph] Other Function Found:
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %3) #6, !noalias !26, !noelle.pdg.inst.id !16

- - - - - - - - - - - - - - - -

[edt-graph] Printing the EDT Graph
EDT NODE:
- EDT for.omp_task_entry..5
Data environment for EDT: 
Number of ParamV: 1
  -   %14 = load i32, i32* %number, align 4, !tbaa !139, !noelle.pdg.inst.id !25
Number of DepV: 0

EDT NODE:
- EDT for.omp_task_entry.
Data environment for EDT: 
Number of ParamV: 2
  -   %6 = load i32, i32* %random_number, align 4, !tbaa !139, !noelle.pdg.inst.id !87
  -   %9 = load i32, i32* %NewRandom, align 4, !tbaa !139, !noelle.pdg.inst.id !31
Number of DepV: 2
  - i32* %number
  - i32* %shared_number

EDT NODE:
- EDT for.omp_outlined.
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
  -   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  -   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66


 ---------------------------------------- 
[carts] Process has finished

; ModuleID = 'test_with_metadata.bc'
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

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@.str.3 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !noelle.pdg.edges !5 {
entry:
  %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
  %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
  %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
  %0 = bitcast i32* %number to i8*, !noelle.pdg.inst.id !69
  store i32 1, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !7
  %1 = bitcast i32* %shared_number to i8*, !noelle.pdg.inst.id !74
  store i32 10000, i32* %shared_number, align 4, !tbaa !70, !noelle.pdg.inst.id !23
  %2 = bitcast i32* %random_number to i8*, !noelle.pdg.inst.id !75
  %call = tail call i32 @rand() #6, !noelle.pdg.inst.id !8
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !76
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !77
  store i32 %add, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !33
  %3 = bitcast i32* %NewRandom to i8*, !noelle.pdg.inst.id !78
  %call1 = tail call i32 @rand() #6, !noelle.pdg.inst.id !15
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !39
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18
  %4 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !21
  %5 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !45
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5), !noelle.pdg.inst.id !30
  ret i32 0, !noelle.pdg.inst.id !79
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture noundef readonly %.global_tid., i32* noalias nocapture noundef readnone %.bound_tid., i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noundef nonnull align 4 dereferenceable(4) %number, i32* noundef nonnull align 4 dereferenceable(4) %shared_number) #3 !noelle.pdg.args.id !80 !noelle.pdg.edges !87 {
entry:
  %0 = load i32, i32* %.global_tid., align 4, !tbaa !70, !noelle.pdg.inst.id !125
  %1 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !131
  %2 = bitcast i8* %1 to i8**, !noelle.pdg.inst.id !209
  %3 = load i8*, i8** %2, align 8, !tbaa !210, !noelle.pdg.inst.id !151
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %3 to i32**, !noelle.pdg.inst.id !215
  store i32* %number, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !216, !noelle.pdg.inst.id !133
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %3, i64 8, !noelle.pdg.inst.id !218
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**, !noelle.pdg.inst.id !219
  store i32* %shared_number, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !220, !noelle.pdg.inst.id !135
  %4 = getelementptr inbounds i8, i8* %1, i64 40, !noelle.pdg.inst.id !221
  %5 = bitcast i8* %4 to i32*, !noelle.pdg.inst.id !222
  %6 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !157
  store i32 %6, i32* %5, align 8, !tbaa !223, !noelle.pdg.inst.id !137
  %7 = getelementptr inbounds i8, i8* %1, i64 44, !noelle.pdg.inst.id !224
  %8 = bitcast i8* %7 to i32*, !noelle.pdg.inst.id !225
  %9 = load i32, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !101
  store i32 %9, i32* %8, align 4, !tbaa !226, !noelle.pdg.inst.id !104
  %10 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 %0, i8* %1), !noelle.pdg.inst.id !89
  %11 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 %0, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !93
  %12 = getelementptr inbounds i8, i8* %11, i64 40, !noelle.pdg.inst.id !227
  %13 = bitcast i8* %12 to i32*, !noelle.pdg.inst.id !228
  %14 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !96
  store i32 %14, i32* %13, align 8, !tbaa !229, !noelle.pdg.inst.id !98
  %15 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 %0, i8* %11), !noelle.pdg.inst.id !90
  ret void, !noelle.pdg.inst.id !232
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias nocapture noundef readonly %1) #5 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !233 !noelle.pdg.edges !236 {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates* %1 to %struct.anon**, !noelle.pdg.inst.id !288
  %3 = load %struct.anon*, %struct.anon** %2, align 8, !tbaa !210, !noelle.pdg.inst.id !238
  %.idx = getelementptr %struct.anon, %struct.anon* %3, i64 0, i32 0, !noelle.pdg.inst.id !289
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !290, !noelle.pdg.inst.id !245
  %.idx2 = getelementptr %struct.anon, %struct.anon* %3, i64 0, i32 1, !noelle.pdg.inst.id !292
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !293, !noelle.pdg.inst.id !249
  tail call void @llvm.experimental.noalias.scope.decl(metadata !294), !noelle.pdg.inst.id !239
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !297
  %5 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i64 0, i32 1, i32 1, !noelle.pdg.inst.id !298
  %6 = load i32, i32* %.idx.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !253
  %7 = load i32, i32* %.idx2.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !255
  %8 = load i32, i32* %4, align 4, !tbaa !70, !alias.scope !294, !noelle.pdg.inst.id !257
  %9 = load i32, i32* %5, align 4, !tbaa !70, !alias.scope !294, !noelle.pdg.inst.id !259
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %6, i32 noundef %7, i32 noundef %8, i32 noundef %9) #6, !noalias !294, !noelle.pdg.inst.id !261
  %10 = load i32, i32* %.idx.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !264
  %inc.i = add nsw i32 %10, 1, !noelle.pdg.inst.id !299
  store i32 %inc.i, i32* %.idx.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !241
  %11 = load i32, i32* %.idx2.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !268
  %dec.i = add nsw i32 %11, -1, !noelle.pdg.inst.id !300
  store i32 %dec.i, i32* %.idx2.val, align 4, !tbaa !70, !noalias !294, !noelle.pdg.inst.id !243
  ret i32 0, !noelle.pdg.inst.id !301
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #6

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias nocapture noundef %1) #5 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !302 !noelle.pdg.edges !305 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !318), !noelle.pdg.inst.id !307
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !321
  %3 = load i32, i32* %2, align 4, !tbaa !70, !alias.scope !318, !noelle.pdg.inst.id !308
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %3) #6, !noalias !318, !noelle.pdg.inst.id !310
  %inc.i = add nsw i32 %3, 1, !noelle.pdg.inst.id !322
  store i32 %inc.i, i32* %2, align 4, !tbaa !70, !alias.scope !318, !noelle.pdg.inst.id !313
  ret i32 0, !noelle.pdg.inst.id !323
}

; Function Attrs: nounwind
declare !callback !324 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #6

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
!noelle.module.pdg = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.6"}
!4 = !{!"true"}
!5 = !{!6, !12, !14, !16, !17, !19, !20, !22, !24, !25, !26, !27, !28, !29, !31, !32, !35, !36, !37, !38, !40, !41, !42, !43, !44, !46, !47, !48, !49, !50, !51, !52, !53, !54, !55, !56, !57, !58, !59, !60, !61, !62, !63, !64}
!6 = !{!7, !8, !4, !9, !10, !9, !9, !11}
!7 = !{i64 6}
!8 = !{i64 12}
!9 = !{!"false"}
!10 = !{!"RAW"}
!11 = !{}
!12 = !{!7, !8, !4, !9, !13, !9, !9, !11}
!13 = !{!"WAW"}
!14 = !{!7, !15, !4, !9, !13, !9, !9, !11}
!15 = !{i64 18}
!16 = !{!7, !15, !4, !9, !10, !9, !9, !11}
!17 = !{!7, !18, !4, !9, !13, !9, !9, !11}
!18 = !{i64 20}
!19 = !{!7, !18, !4, !9, !10, !9, !9, !11}
!20 = !{!7, !21, !4, !4, !10, !9, !9, !11}
!21 = !{i64 21}
!22 = !{!23, !8, !4, !9, !10, !9, !9, !11}
!23 = !{i64 9}
!24 = !{!23, !8, !4, !9, !13, !9, !9, !11}
!25 = !{!23, !15, !4, !9, !10, !9, !9, !11}
!26 = !{!23, !15, !4, !9, !13, !9, !9, !11}
!27 = !{!23, !18, !4, !9, !10, !9, !9, !11}
!28 = !{!23, !18, !4, !9, !13, !9, !9, !11}
!29 = !{!8, !30, !4, !9, !13, !9, !9, !11}
!30 = !{i64 23}
!31 = !{!8, !30, !4, !9, !10, !9, !9, !11}
!32 = !{!8, !33, !4, !9, !34, !9, !9, !11}
!33 = !{i64 15}
!34 = !{!"WAR"}
!35 = !{!8, !33, !4, !9, !13, !9, !9, !11}
!36 = !{!8, !15, !4, !9, !10, !9, !9, !11}
!37 = !{!8, !15, !4, !9, !13, !9, !9, !11}
!38 = !{!8, !39, !4, !9, !34, !9, !9, !11}
!39 = !{i64 19}
!40 = !{!8, !39, !4, !9, !13, !9, !9, !11}
!41 = !{!8, !18, !4, !9, !10, !9, !9, !11}
!42 = !{!8, !18, !4, !9, !13, !9, !9, !11}
!43 = !{!8, !21, !4, !9, !10, !9, !9, !11}
!44 = !{!8, !45, !4, !9, !10, !9, !9, !11}
!45 = !{i64 22}
!46 = !{!33, !15, !4, !9, !10, !9, !9, !11}
!47 = !{!33, !15, !4, !9, !13, !9, !9, !11}
!48 = !{!33, !18, !4, !9, !10, !9, !9, !11}
!49 = !{!33, !18, !4, !9, !13, !9, !9, !11}
!50 = !{!33, !45, !4, !4, !10, !9, !9, !11}
!51 = !{!15, !30, !4, !9, !10, !9, !9, !11}
!52 = !{!15, !30, !4, !9, !13, !9, !9, !11}
!53 = !{!15, !39, !4, !9, !34, !9, !9, !11}
!54 = !{!15, !39, !4, !9, !13, !9, !9, !11}
!55 = !{!15, !18, !4, !9, !10, !9, !9, !11}
!56 = !{!15, !18, !4, !9, !13, !9, !9, !11}
!57 = !{!15, !21, !4, !9, !10, !9, !9, !11}
!58 = !{!15, !45, !4, !9, !10, !9, !9, !11}
!59 = !{!39, !18, !4, !9, !10, !9, !9, !11}
!60 = !{!39, !18, !4, !9, !13, !9, !9, !11}
!61 = !{!18, !30, !4, !9, !13, !9, !9, !11}
!62 = !{!18, !30, !4, !9, !10, !9, !9, !11}
!63 = !{!18, !21, !4, !9, !10, !9, !9, !11}
!64 = !{!18, !45, !4, !9, !10, !9, !9, !11}
!65 = !{i64 0}
!66 = !{i64 1}
!67 = !{i64 2}
!68 = !{i64 3}
!69 = !{i64 4}
!70 = !{!71, !71, i64 0}
!71 = !{!"int", !72, i64 0}
!72 = !{!"omnipotent char", !73, i64 0}
!73 = !{!"Simple C++ TBAA"}
!74 = !{i64 7}
!75 = !{i64 10}
!76 = !{i64 13}
!77 = !{i64 14}
!78 = !{i64 16}
!79 = !{i64 28}
!80 = !{!81, !82, !83, !84, !85, !86}
!81 = !{i64 29}
!82 = !{i64 30}
!83 = !{i64 31}
!84 = !{i64 32}
!85 = !{i64 33}
!86 = !{i64 34}
!87 = !{!88, !91, !92, !94, !95, !97, !99, !100, !102, !103, !105, !106, !107, !108, !109, !110, !111, !112, !113, !114, !115, !116, !117, !118, !119, !120, !121, !122, !123, !124, !126, !127, !128, !129, !130, !132, !134, !136, !138, !139, !140, !141, !142, !143, !144, !145, !146, !147, !148, !149, !150, !152, !153, !154, !155, !156, !158, !159, !160, !161, !162, !163, !164, !165, !166, !167, !168, !169, !170, !171, !172, !173, !174, !175, !176, !177, !178, !179, !180, !181, !182, !183, !184, !185, !186, !187, !188, !189, !190, !191, !192, !193, !194, !195, !196, !197, !198, !199, !200, !201, !202, !203, !204, !205, !206, !207, !208}
!88 = !{!89, !90, !4, !9, !13, !9, !9, !11}
!89 = !{i64 52}
!90 = !{i64 58}
!91 = !{!89, !90, !4, !9, !10, !9, !9, !11}
!92 = !{!89, !93, !4, !9, !13, !9, !9, !11}
!93 = !{i64 53}
!94 = !{!89, !93, !4, !9, !10, !9, !9, !11}
!95 = !{!89, !96, !4, !9, !10, !9, !9, !11}
!96 = !{i64 56}
!97 = !{!89, !98, !4, !9, !13, !9, !9, !11}
!98 = !{i64 57}
!99 = !{!89, !98, !4, !9, !34, !9, !9, !11}
!100 = !{!101, !90, !4, !9, !34, !9, !9, !11}
!101 = !{i64 50}
!102 = !{!101, !89, !4, !9, !34, !9, !9, !11}
!103 = !{!101, !104, !4, !9, !34, !9, !9, !11}
!104 = !{i64 51}
!105 = !{!101, !93, !4, !9, !34, !9, !9, !11}
!106 = !{!101, !98, !4, !9, !34, !9, !9, !11}
!107 = !{!104, !90, !4, !9, !10, !9, !9, !11}
!108 = !{!104, !90, !4, !9, !13, !9, !9, !11}
!109 = !{!104, !89, !4, !9, !10, !9, !9, !11}
!110 = !{!104, !89, !4, !9, !13, !9, !9, !11}
!111 = !{!104, !93, !4, !9, !10, !9, !9, !11}
!112 = !{!104, !93, !4, !9, !13, !9, !9, !11}
!113 = !{!104, !96, !4, !9, !10, !9, !9, !11}
!114 = !{!104, !98, !4, !9, !13, !9, !9, !11}
!115 = !{!93, !90, !4, !9, !10, !9, !9, !11}
!116 = !{!93, !90, !4, !9, !13, !9, !9, !11}
!117 = !{!93, !96, !4, !9, !10, !9, !9, !11}
!118 = !{!93, !98, !4, !9, !13, !9, !9, !11}
!119 = !{!93, !98, !4, !9, !34, !9, !9, !11}
!120 = !{!96, !90, !4, !9, !34, !9, !9, !11}
!121 = !{!96, !98, !4, !9, !34, !9, !9, !11}
!122 = !{!98, !90, !4, !9, !13, !9, !9, !11}
!123 = !{!98, !90, !4, !9, !10, !9, !9, !11}
!124 = !{!125, !90, !4, !9, !34, !9, !9, !11}
!125 = !{i64 35}
!126 = !{!125, !89, !4, !9, !34, !9, !9, !11}
!127 = !{!125, !104, !4, !9, !34, !9, !9, !11}
!128 = !{!125, !93, !4, !9, !34, !9, !9, !11}
!129 = !{!125, !98, !4, !9, !34, !9, !9, !11}
!130 = !{!125, !131, !4, !9, !34, !9, !9, !11}
!131 = !{i64 36}
!132 = !{!125, !133, !4, !9, !34, !9, !9, !11}
!133 = !{i64 40}
!134 = !{!125, !135, !4, !9, !34, !9, !9, !11}
!135 = !{i64 43}
!136 = !{!125, !137, !4, !9, !34, !9, !9, !11}
!137 = !{i64 47}
!138 = !{!131, !90, !4, !9, !13, !9, !9, !11}
!139 = !{!131, !90, !4, !9, !10, !9, !9, !11}
!140 = !{!131, !89, !4, !9, !13, !9, !9, !11}
!141 = !{!131, !89, !4, !9, !10, !9, !9, !11}
!142 = !{!131, !101, !4, !9, !10, !9, !9, !11}
!143 = !{!131, !104, !4, !9, !34, !9, !9, !11}
!144 = !{!131, !104, !4, !9, !13, !9, !9, !11}
!145 = !{!131, !93, !4, !9, !13, !9, !9, !11}
!146 = !{!131, !93, !4, !9, !10, !9, !9, !11}
!147 = !{!131, !96, !4, !9, !10, !9, !9, !11}
!148 = !{!131, !98, !4, !9, !13, !9, !9, !11}
!149 = !{!131, !98, !4, !9, !34, !9, !9, !11}
!150 = !{!131, !151, !4, !9, !10, !9, !9, !11}
!151 = !{i64 38}
!152 = !{!131, !133, !4, !9, !13, !9, !9, !11}
!153 = !{!131, !133, !4, !9, !34, !9, !9, !11}
!154 = !{!131, !135, !4, !9, !34, !9, !9, !11}
!155 = !{!131, !135, !4, !9, !13, !9, !9, !11}
!156 = !{!131, !157, !4, !9, !10, !9, !9, !11}
!157 = !{i64 46}
!158 = !{!131, !137, !4, !9, !34, !9, !9, !11}
!159 = !{!131, !137, !4, !9, !13, !9, !9, !11}
!160 = !{!151, !90, !4, !9, !34, !9, !9, !11}
!161 = !{!151, !89, !4, !9, !34, !9, !9, !11}
!162 = !{!151, !104, !4, !9, !34, !9, !9, !11}
!163 = !{!151, !93, !4, !9, !34, !9, !9, !11}
!164 = !{!151, !98, !4, !9, !34, !9, !9, !11}
!165 = !{!151, !133, !4, !9, !34, !9, !9, !11}
!166 = !{!151, !135, !4, !9, !34, !9, !9, !11}
!167 = !{!151, !137, !4, !9, !34, !9, !9, !11}
!168 = !{!133, !90, !4, !9, !13, !9, !9, !11}
!169 = !{!133, !90, !4, !9, !10, !9, !9, !11}
!170 = !{!133, !89, !4, !9, !10, !9, !9, !11}
!171 = !{!133, !89, !4, !9, !13, !9, !9, !11}
!172 = !{!133, !101, !4, !9, !10, !9, !9, !11}
!173 = !{!133, !104, !4, !9, !13, !9, !9, !11}
!174 = !{!133, !93, !4, !9, !10, !9, !9, !11}
!175 = !{!133, !93, !4, !9, !13, !9, !9, !11}
!176 = !{!133, !96, !4, !9, !10, !9, !9, !11}
!177 = !{!133, !98, !4, !9, !13, !9, !9, !11}
!178 = !{!133, !135, !4, !9, !13, !9, !9, !11}
!179 = !{!133, !157, !4, !9, !10, !9, !9, !11}
!180 = !{!133, !137, !4, !9, !13, !9, !9, !11}
!181 = !{!135, !90, !4, !9, !13, !9, !9, !11}
!182 = !{!135, !90, !4, !9, !10, !9, !9, !11}
!183 = !{!135, !89, !4, !9, !10, !9, !9, !11}
!184 = !{!135, !89, !4, !9, !13, !9, !9, !11}
!185 = !{!135, !101, !4, !9, !10, !9, !9, !11}
!186 = !{!135, !104, !4, !9, !13, !9, !9, !11}
!187 = !{!135, !93, !4, !9, !13, !9, !9, !11}
!188 = !{!135, !93, !4, !9, !10, !9, !9, !11}
!189 = !{!135, !96, !4, !9, !10, !9, !9, !11}
!190 = !{!135, !98, !4, !9, !13, !9, !9, !11}
!191 = !{!135, !157, !4, !9, !10, !9, !9, !11}
!192 = !{!135, !137, !4, !9, !13, !9, !9, !11}
!193 = !{!157, !90, !4, !9, !34, !9, !9, !11}
!194 = !{!157, !89, !4, !9, !34, !9, !9, !11}
!195 = !{!157, !104, !4, !9, !34, !9, !9, !11}
!196 = !{!157, !93, !4, !9, !34, !9, !9, !11}
!197 = !{!157, !98, !4, !9, !34, !9, !9, !11}
!198 = !{!157, !137, !4, !9, !34, !9, !9, !11}
!199 = !{!137, !90, !4, !9, !10, !9, !9, !11}
!200 = !{!137, !90, !4, !9, !13, !9, !9, !11}
!201 = !{!137, !89, !4, !9, !10, !9, !9, !11}
!202 = !{!137, !89, !4, !9, !13, !9, !9, !11}
!203 = !{!137, !101, !4, !9, !10, !9, !9, !11}
!204 = !{!137, !104, !4, !9, !13, !9, !9, !11}
!205 = !{!137, !93, !4, !9, !10, !9, !9, !11}
!206 = !{!137, !93, !4, !9, !13, !9, !9, !11}
!207 = !{!137, !96, !4, !9, !10, !9, !9, !11}
!208 = !{!137, !98, !4, !9, !13, !9, !9, !11}
!209 = !{i64 37}
!210 = !{!211, !213, i64 0}
!211 = !{!"_ZTS24kmp_task_t_with_privates", !212, i64 0, !214, i64 40}
!212 = !{!"_ZTS10kmp_task_t", !213, i64 0, !213, i64 8, !71, i64 16, !72, i64 24, !72, i64 32}
!213 = !{!"any pointer", !72, i64 0}
!214 = !{!"_ZTS15.kmp_privates.t", !71, i64 0, !71, i64 4}
!215 = !{i64 39}
!216 = !{i64 0, i64 8, !217, i64 8, i64 8, !217}
!217 = !{!213, !213, i64 0}
!218 = !{i64 41}
!219 = !{i64 42}
!220 = !{i64 0, i64 8, !217}
!221 = !{i64 44}
!222 = !{i64 45}
!223 = !{!211, !71, i64 40}
!224 = !{i64 48}
!225 = !{i64 49}
!226 = !{!211, !71, i64 44}
!227 = !{i64 54}
!228 = !{i64 55}
!229 = !{!230, !71, i64 40}
!230 = !{!"_ZTS24kmp_task_t_with_privates", !212, i64 0, !231, i64 40}
!231 = !{!"_ZTS15.kmp_privates.t", !71, i64 0}
!232 = !{i64 59}
!233 = !{!234, !235}
!234 = !{i64 60}
!235 = !{i64 61}
!236 = !{!237, !240, !242, !244, !246, !247, !248, !250, !251, !252, !254, !256, !258, !260, !262, !263, !265, !266, !267, !269, !270, !271, !272, !273, !274, !275, !276, !277, !278, !279, !280, !281, !282, !283, !284, !285, !286, !287}
!237 = !{!238, !239, !4, !9, !34, !9, !9, !11}
!238 = !{i64 63}
!239 = !{i64 68}
!240 = !{!238, !241, !4, !9, !34, !9, !9, !11}
!241 = !{i64 78}
!242 = !{!238, !243, !4, !9, !34, !9, !9, !11}
!243 = !{i64 81}
!244 = !{!245, !239, !4, !9, !34, !9, !9, !11}
!245 = !{i64 65}
!246 = !{!245, !241, !4, !9, !34, !9, !9, !11}
!247 = !{!245, !243, !4, !9, !34, !9, !9, !11}
!248 = !{!249, !239, !4, !9, !34, !9, !9, !11}
!249 = !{i64 67}
!250 = !{!249, !241, !4, !9, !34, !9, !9, !11}
!251 = !{!249, !243, !4, !9, !34, !9, !9, !11}
!252 = !{!239, !253, !4, !9, !10, !9, !9, !11}
!253 = !{i64 71}
!254 = !{!239, !255, !4, !9, !10, !9, !9, !11}
!255 = !{i64 72}
!256 = !{!239, !257, !4, !9, !10, !9, !9, !11}
!257 = !{i64 73}
!258 = !{!239, !259, !4, !9, !10, !9, !9, !11}
!259 = !{i64 74}
!260 = !{!239, !261, !4, !9, !10, !9, !9, !11}
!261 = !{i64 75}
!262 = !{!239, !261, !4, !9, !13, !9, !9, !11}
!263 = !{!239, !264, !4, !9, !10, !9, !9, !11}
!264 = !{i64 76}
!265 = !{!239, !241, !4, !9, !34, !9, !9, !11}
!266 = !{!239, !241, !4, !9, !13, !9, !9, !11}
!267 = !{!239, !268, !4, !9, !10, !9, !9, !11}
!268 = !{i64 79}
!269 = !{!239, !243, !4, !9, !34, !9, !9, !11}
!270 = !{!239, !243, !4, !9, !13, !9, !9, !11}
!271 = !{!253, !241, !4, !4, !34, !9, !9, !11}
!272 = !{!253, !243, !4, !9, !34, !9, !9, !11}
!273 = !{!255, !241, !4, !9, !34, !9, !9, !11}
!274 = !{!255, !243, !4, !4, !34, !9, !9, !11}
!275 = !{!257, !241, !4, !9, !34, !9, !9, !11}
!276 = !{!257, !243, !4, !9, !34, !9, !9, !11}
!277 = !{!259, !241, !4, !9, !34, !9, !9, !11}
!278 = !{!259, !243, !4, !9, !34, !9, !9, !11}
!279 = !{!261, !241, !4, !9, !13, !9, !9, !11}
!280 = !{!261, !241, !4, !9, !34, !9, !9, !11}
!281 = !{!261, !243, !4, !9, !34, !9, !9, !11}
!282 = !{!261, !243, !4, !9, !13, !9, !9, !11}
!283 = !{!264, !241, !4, !4, !34, !9, !9, !11}
!284 = !{!264, !243, !4, !9, !34, !9, !9, !11}
!285 = !{!241, !268, !4, !9, !10, !9, !9, !11}
!286 = !{!241, !243, !4, !9, !13, !9, !9, !11}
!287 = !{!268, !243, !4, !4, !34, !9, !9, !11}
!288 = !{i64 62}
!289 = !{i64 64}
!290 = !{!291, !213, i64 0}
!291 = !{!"_ZTSZ4mainE3$_0", !213, i64 0, !213, i64 8}
!292 = !{i64 66}
!293 = !{!291, !213, i64 8}
!294 = !{!295}
!295 = distinct !{!295, !296, !".omp_outlined..1: %.privates."}
!296 = distinct !{!296, !".omp_outlined..1"}
!297 = !{i64 69}
!298 = !{i64 70}
!299 = !{i64 77}
!300 = !{i64 80}
!301 = !{i64 82}
!302 = !{!303, !304}
!303 = !{i64 83}
!304 = !{i64 84}
!305 = !{!306, !309, !311, !312, !314, !315, !316, !317}
!306 = !{!307, !308, !4, !9, !10, !9, !9, !11}
!307 = !{i64 85}
!308 = !{i64 87}
!309 = !{!307, !310, !4, !9, !13, !9, !9, !11}
!310 = !{i64 88}
!311 = !{!307, !310, !4, !9, !10, !9, !9, !11}
!312 = !{!307, !313, !4, !9, !34, !9, !9, !11}
!313 = !{i64 90}
!314 = !{!307, !313, !4, !9, !13, !9, !9, !11}
!315 = !{!308, !313, !4, !4, !34, !9, !9, !11}
!316 = !{!310, !313, !4, !9, !13, !9, !9, !11}
!317 = !{!310, !313, !4, !9, !34, !9, !9, !11}
!318 = !{!319}
!319 = distinct !{!319, !320, !".omp_outlined..2: %.privates."}
!320 = distinct !{!320, !".omp_outlined..2"}
!321 = !{i64 86}
!322 = !{i64 89}
!323 = !{i64 91}
!324 = !{!325}
!325 = !{i64 2, i64 -1, i64 -1, i1 true}


 ---------------------------------------- 
# noelle-load -debug-only=arts,carts,arts-analyzer -load-pass-plugin /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -passes=CARTS test_with_metadata.bc -o test_opt.bc
llvm-dis test_opt.bc
clang++ -fopenmp test_opt.bc -O3 -march=native -o test_opt -lstdc++
