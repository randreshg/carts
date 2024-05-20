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
noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc

 ---------------------------------------- 
[carts] Running CARTS on Module: 
test_with_metadata.bc
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: main
[arts-analyzer] Other Function Found: 
    %call = tail call i32 @rand() #6, !noelle.pdg.inst.id !8
[arts-analyzer] Other Function Found: 
    %call1 = tail call i32 @rand() #6, !noelle.pdg.inst.id !15
- - - - - -- - - - - - - - - -
[arts-analyzer] Parallel Region Found:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18

[arts-analyzer] Handling parallel region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Created ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Created ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_0
- - - - - -- - - - - - - - - -
[arts-analyzer] Task Region Found:   %4 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !5

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_1
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !143), !noelle.pdg.inst.id !146
[arts-analyzer] Other Function Found: 
    %15 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %11, i32 noundef %12, i32 noundef %1, i32 noundef %3) #4, !noalias !21, !noelle.pdg.inst.id !30
- - - - - -- - - - - - - - - -
[arts-analyzer] Task Region Found:   %24 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !35

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_2
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !135), !noelle.pdg.inst.id !138
[arts-analyzer] Other Function Found: 
    %3 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #4, !noalias !5, !noelle.pdg.inst.id !14

[arts-analyzer] Handling done region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

 ---------------------------------------- 
[carts] Module: ; ModuleID = 'test_with_metadata.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.artsEdtDep_t = type { i32*, i32, i8* }
%struct.anon = type { i32*, i32* }

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@.str.3 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !noelle.pdg.edges !5 {
entry:
  %0 = bitcast i32* undef to i8*, !noelle.pdg.inst.id !65
  store i32 1, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !7
  %1 = bitcast i32* undef to i8*, !noelle.pdg.inst.id !70
  store i32 10000, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !23
  %2 = bitcast i32* undef to i8*, !noelle.pdg.inst.id !71
  %call = tail call i32 @rand() #4, !noelle.pdg.inst.id !8
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !72
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !73
  store i32 %add, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !33
  %3 = bitcast i32* undef to i8*, !noelle.pdg.inst.id !74
  %call1 = tail call i32 @rand() #4, !noelle.pdg.inst.id !15
  store i32 %call1, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !39
  %4 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0_guid.addr = alloca i32*, align 8
  store i32* %4, i32** %edt_0_guid.addr, align 8
  %edt_0_paramc = alloca i32, align 4
  store i32 0, i32* %edt_0_paramc, align 4
  %5 = load i32, i32* %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %5, align 8
  %6 = alloca i32, align 4
  store i32 4, i32* %6, align 4
  %7 = bitcast i32** %edt_0_guid.addr to i32*
  %8 = load i32, i32* %6, align 4
  %9 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_0, i32* %7, i32 %5, i64* %edt_0_paramv, i32 %8)
  %10 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3_guid.addr = alloca i32*, align 8
  store i32* %10, i32** %edt_3_guid.addr, align 8
  %edt_3_paramc = alloca i32, align 4
  store i32 0, i32* %edt_3_paramc, align 4
  %11 = load i32, i32* %edt_3_paramc, align 4
  %edt_3_paramv = alloca i64, i32 %11, align 8
  %12 = alloca i32, align 4
  store i32 0, i32* %12, align 4
  %13 = bitcast i32** %edt_3_guid.addr to i32*
  %14 = load i32, i32* %12, align 4
  %15 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_3, i32* %13, i32 %11, i64* %edt_3_paramv, i32 %14)
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !75 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

define void @arts_edt_0(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %0 = bitcast i8** %depv.random_number to i32*
  %depv.NewRandom = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %1 = bitcast i8** %depv.NewRandom to i32*
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 2, i32 2
  %2 = bitcast i8** %depv.number to i32*
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 3, i32 2
  %3 = bitcast i8** %depv.shared_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %4 = bitcast i8* undef to i8**, !noelle.pdg.inst.id !77
  %5 = load i8*, i8** undef, align 8, !tbaa !78, !noelle.pdg.inst.id !83
  %6 = bitcast i8* %5 to i32**, !noelle.pdg.inst.id !84
  store i32* %2, i32** %6, align 8, !tbaa.struct !85, !noelle.pdg.inst.id !87
  %7 = getelementptr inbounds i8, i8* %5, i64 8, !noelle.pdg.inst.id !88
  %8 = bitcast i8* %7 to i32**, !noelle.pdg.inst.id !89
  store i32* %3, i32** %8, align 8, !tbaa.struct !90, !noelle.pdg.inst.id !91
  %9 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !92
  %10 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !93
  %11 = load i32, i32* %0, align 4, !tbaa !66, !noelle.pdg.inst.id !94
  store i32 %11, i32* %10, align 8, !tbaa !95, !noelle.pdg.inst.id !96
  %12 = getelementptr inbounds i8, i8* undef, i64 44, !noelle.pdg.inst.id !97
  %13 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !98
  %14 = load i32, i32* %1, align 4, !tbaa !66, !noelle.pdg.inst.id !99
  store i32 %14, i32* %13, align 4, !tbaa !100, !noelle.pdg.inst.id !101
  %15 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1_guid.addr = alloca i32*, align 8
  store i32* %15, i32** %edt_1_guid.addr, align 8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, i32* %edt_1_paramc, align 4
  %16 = load i32, i32* %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %16, align 8
  %edt_1_paramv.0 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 0
  %17 = sext i32 %11 to i64
  store i64 %17, i64* %edt_1_paramv.0, align 8
  %edt_1_paramv.1 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 1
  %18 = sext i32 %14 to i64
  store i64 %18, i64* %edt_1_paramv.1, align 8
  %19 = alloca i32, align 4
  store i32 2, i32* %19, align 4
  %20 = bitcast i32** %edt_1_guid.addr to i32*
  %21 = load i32, i32* %19, align 4
  %22 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %20, i32 %16, i64* %edt_1_paramv, i32 %21)
  %23 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !102
  %24 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !103
  %25 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !104
  %26 = load i32, i32* %2, align 4, !tbaa !66, !noelle.pdg.inst.id !105
  store i32 %26, i32* %25, align 8, !tbaa !106, !noelle.pdg.inst.id !109
  %27 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2_guid.addr = alloca i32*, align 8
  store i32* %27, i32** %edt_2_guid.addr, align 8
  %edt_2_paramc = alloca i32, align 4
  store i32 1, i32* %edt_2_paramc, align 4
  %28 = load i32, i32* %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %28, align 8
  %edt_2_paramv.0 = getelementptr inbounds i64, i64* %edt_2_paramv, i64 0
  %29 = sext i32 %26 to i64
  store i64 %29, i64* %edt_2_paramv.0, align 8
  %30 = alloca i32, align 4
  store i32 0, i32* %30, align 4
  %31 = bitcast i32** %edt_2_guid.addr to i32*
  %32 = load i32, i32* %30, align 4
  %33 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_2, i32* %31, i32 %28, i64* %edt_2_paramv, i32 %32)
  %34 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !110
  br label %edt.exit
}

declare i32* @artsReserveGuidRoute(i32, i32)

declare i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

define void @arts_edt_1(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %paramv.0 = getelementptr inbounds i64, i64* %paramv, i64 0
  %0 = load i64, i64* %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.1 = getelementptr inbounds i64, i64* %paramv, i64 1
  %2 = load i64, i64* %paramv.1, align 8
  %3 = trunc i64 %2 to i32
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %4 = bitcast i8** %depv.0 to i32*
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %5 = bitcast i8** %depv.1 to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %6 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !78, !noelle.pdg.inst.id !111
  %7 = getelementptr %struct.anon, %struct.anon* %6, i64 0, i32 0, !noelle.pdg.inst.id !112
  %8 = load i32*, i32** %7, align 8, !tbaa !113, !noelle.pdg.inst.id !115
  %9 = getelementptr %struct.anon, %struct.anon* %6, i64 0, i32 1, !noelle.pdg.inst.id !116
  %10 = load i32*, i32** %9, align 8, !tbaa !117, !noelle.pdg.inst.id !118
  tail call void @llvm.experimental.noalias.scope.decl(metadata !119), !noelle.pdg.inst.id !122
  %11 = load i32, i32* %4, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !123
  %12 = load i32, i32* %5, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !124
  %13 = load i32, i32* undef, align 4, !tbaa !66, !alias.scope !119, !noelle.pdg.inst.id !125
  %14 = load i32, i32* undef, align 4, !tbaa !66, !alias.scope !119, !noelle.pdg.inst.id !126
  %15 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %11, i32 noundef %12, i32 noundef %1, i32 noundef %3) #4, !noalias !119, !noelle.pdg.inst.id !127
  %16 = load i32, i32* %4, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !128
  %17 = add nsw i32 %16, 1, !noelle.pdg.inst.id !129
  store i32 %17, i32* %4, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !130
  %18 = load i32, i32* %5, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !131
  %19 = add nsw i32 %18, -1, !noelle.pdg.inst.id !132
  store i32 %19, i32* %5, align 4, !tbaa !66, !noalias !119, !noelle.pdg.inst.id !133
  br label %edt.exit
}

define void @arts_edt_2(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %paramv.0 = getelementptr inbounds i64, i64* %paramv, i64 0
  %0 = load i64, i64* %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !134), !noelle.pdg.inst.id !137
  %2 = load i32, i32* undef, align 4, !tbaa !66, !alias.scope !134, !noelle.pdg.inst.id !138
  %3 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #4, !noalias !134, !noelle.pdg.inst.id !139
  %4 = add nsw i32 %1, 1, !noelle.pdg.inst.id !140
  store i32 %4, i32* undef, align 4, !tbaa !66, !alias.scope !134, !noelle.pdg.inst.id !141
  br label %edt.exit
}

define void @arts_edt_3(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %0 = load i32, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !21
  %1 = load i32, i32* undef, align 4, !tbaa !66, !noelle.pdg.inst.id !45
  %2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %0, i32 noundef %1), !noelle.pdg.inst.id !30
  br label %edt.exit
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
!20 = !{!7, !21, !4, !9, !10, !9, !9, !11}
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
!50 = !{!33, !45, !4, !9, !10, !9, !9, !11}
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
!65 = !{i64 4}
!66 = !{!67, !67, i64 0}
!67 = !{!"int", !68, i64 0}
!68 = !{!"omnipotent char", !69, i64 0}
!69 = !{!"Simple C++ TBAA"}
!70 = !{i64 7}
!71 = !{i64 10}
!72 = !{i64 13}
!73 = !{i64 14}
!74 = !{i64 16}
!75 = !{!76}
!76 = !{i64 2, i64 -1, i64 -1, i1 true}
!77 = !{i64 37}
!78 = !{!79, !81, i64 0}
!79 = !{!"_ZTS24kmp_task_t_with_privates", !80, i64 0, !82, i64 40}
!80 = !{!"_ZTS10kmp_task_t", !81, i64 0, !81, i64 8, !67, i64 16, !68, i64 24, !68, i64 32}
!81 = !{!"any pointer", !68, i64 0}
!82 = !{!"_ZTS15.kmp_privates.t", !67, i64 0, !67, i64 4}
!83 = !{i64 38}
!84 = !{i64 39}
!85 = !{i64 0, i64 8, !86, i64 8, i64 8, !86}
!86 = !{!81, !81, i64 0}
!87 = !{i64 40}
!88 = !{i64 41}
!89 = !{i64 42}
!90 = !{i64 0, i64 8, !86}
!91 = !{i64 43}
!92 = !{i64 44}
!93 = !{i64 45}
!94 = !{i64 46}
!95 = !{!79, !67, i64 40}
!96 = !{i64 47}
!97 = !{i64 48}
!98 = !{i64 49}
!99 = !{i64 50}
!100 = !{!79, !67, i64 44}
!101 = !{i64 51}
!102 = !{i64 52}
!103 = !{i64 54}
!104 = !{i64 55}
!105 = !{i64 56}
!106 = !{!107, !67, i64 40}
!107 = !{!"_ZTS24kmp_task_t_with_privates", !80, i64 0, !108, i64 40}
!108 = !{!"_ZTS15.kmp_privates.t", !67, i64 0}
!109 = !{i64 57}
!110 = !{i64 58}
!111 = !{i64 63}
!112 = !{i64 64}
!113 = !{!114, !81, i64 0}
!114 = !{!"_ZTSZ4mainE3$_0", !81, i64 0, !81, i64 8}
!115 = !{i64 65}
!116 = !{i64 66}
!117 = !{!114, !81, i64 8}
!118 = !{i64 67}
!119 = !{!120}
!120 = distinct !{!120, !121, !".omp_outlined..1: %.privates."}
!121 = distinct !{!121, !".omp_outlined..1"}
!122 = !{i64 68}
!123 = !{i64 71}
!124 = !{i64 72}
!125 = !{i64 73}
!126 = !{i64 74}
!127 = !{i64 75}
!128 = !{i64 76}
!129 = !{i64 77}
!130 = !{i64 78}
!131 = !{i64 79}
!132 = !{i64 80}
!133 = !{i64 81}
!134 = !{!135}
!135 = distinct !{!135, !136, !".omp_outlined..2: %.privates."}
!136 = distinct !{!136, !".omp_outlined..2"}
!137 = !{i64 85}
!138 = !{i64 87}
!139 = !{i64 88}
!140 = !{i64 89}
!141 = !{i64 90}
llvm-dis test_opt.bc
clang++ -fopenmp test_opt.bc -O3 -march=native -o test_opt -lstdc++
