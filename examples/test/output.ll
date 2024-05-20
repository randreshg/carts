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

 ---------------------------------------- 
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: main
[arts-analyzer] Other Function Found: 
    %call = tail call i32 @rand() #6, !noelle.pdg.inst.id !8
[arts-analyzer] Other Function Found: 
    %call1 = tail call i32 @rand() #6, !noelle.pdg.inst.id !15
- - - - - - - - - - - - - - - -
[arts-analyzer] Parallel Region Found:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18

[arts-analyzer] Handling parallel region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Created ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Created ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

Identifying EDTs in the Parallel Body
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_0
- - - - - - - - - - - - - - - -
[arts-analyzer] Task Region Found:   %4 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !5

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

Identifying EDTs in the Task Body
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_1
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !293), !noelle.pdg.inst.id !239
[arts-analyzer] Other Function Found: 
    %15 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %11, i32 noundef %12, i32 noundef %1, i32 noundef %3) #6, !noalias !21, !noelle.pdg.inst.id !30
[arts] Deleting dead instructions in EDT: arts_edt_1
[arts] - Removing:   %6 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !5, !noelle.pdg.inst.id !13
[arts] - Removing:   %8 = load i32*, i32** %7, align 8, !tbaa !15, !noelle.pdg.inst.id !17
[arts] - Removing:   %10 = load i32*, i32** %9, align 8, !tbaa !19, !noelle.pdg.inst.id !20
[arts] - Removing:   %13 = load i32, i32* undef, align 4, !tbaa !25, !alias.scope !21, !noelle.pdg.inst.id !28
[arts] - Removing:   %14 = load i32, i32* undef, align 4, !tbaa !25, !alias.scope !21, !noelle.pdg.inst.id !29
- - - - - - - - - - - - - - - - - - - - - - -
TaskEDT CallInst:   %22 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %20, i32 %16, i64* %edt_1_paramv, i32 %21)

TaskEDT 
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
  tail call void @llvm.experimental.noalias.scope.decl(metadata !257), !noelle.pdg.inst.id !260
  %6 = load i32, i32* %4, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !261
  %7 = load i32, i32* %5, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !262
  %8 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %6, i32 noundef %7, i32 noundef %1, i32 noundef %3) #5, !noalias !257, !noelle.pdg.inst.id !263
  %9 = load i32, i32* %4, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !264
  %10 = add nsw i32 %9, 1, !noelle.pdg.inst.id !265
  store i32 %10, i32* %4, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !266
  %11 = load i32, i32* %5, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !267
  %12 = add nsw i32 %11, -1, !noelle.pdg.inst.id !268
  store i32 %12, i32* %5, align 4, !tbaa !70, !noalias !257, !noelle.pdg.inst.id !269
  br label %edt.exit
}
- - - - - -- - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[arts-analyzer] Task Region Found:   %23 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !34

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

Identifying EDTs in the Task Body
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_2
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !249), !noelle.pdg.inst.id !238
[arts-analyzer] Other Function Found: 
    %3 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #5, !noalias !5, !noelle.pdg.inst.id !14
[arts] Deleting dead instructions in EDT: arts_edt_2
[arts] - Removing:   %2 = load i32, i32* undef, align 4, !tbaa !9, !alias.scope !5, !noelle.pdg.inst.id !13
[arts] - Removing:   store i32 %4, i32* undef, align 4, !tbaa !9, !alias.scope !5, !noelle.pdg.inst.id !16
- - - - - - - - - - - - - - - - - - - - - - -
TaskEDT CallInst:   %32 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_2, i32* %30, i32 %27, i64* %edt_2_paramv, i32 %31)

TaskEDT 
define void @arts_edt_2(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %paramv.0 = getelementptr inbounds i64, i64* %paramv, i64 0
  %0 = load i64, i64* %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !248), !noelle.pdg.inst.id !251
  %2 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #5, !noalias !248, !noelle.pdg.inst.id !252
  %3 = add nsw i32 %1, 1, !noelle.pdg.inst.id !253
  br label %edt.exit
}
- - - - - -- - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -

[arts-analyzer] Handling done region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
- - - - - - - - - - - - - - - - - - - - - - - - -
DoneEDT CallInst:   %15 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_3, i32* %13, i32 %11, i64* %edt_3_paramv, i32 %14)

DoneEDT:
define void @arts_edt_3(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %0 = bitcast i8** %depv.number to i32*
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %1 = bitcast i8** %depv.random_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %2 = load i32, i32* %0, align 4, !tbaa !70, !noelle.pdg.inst.id !21
  %3 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !45
  %4 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %2, i32 noundef %3), !noelle.pdg.inst.id !30
  br label %edt.exit
}
- - - - - - - - - - - - - - - - - - - - - - - - -
[arts] Deleting dead instructions in EDT: arts_edt_0
[arts] - Removing:   %4 = bitcast i8* undef to i8**, !noelle.pdg.inst.id !5
[arts] - Removing:   %5 = load i8*, i8** undef, align 8, !tbaa !6, !noelle.pdg.inst.id !14
[arts] - Removing:   %9 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !23
[arts] - Removing:   %10 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !24
[arts] - Removing:   %12 = getelementptr inbounds i8, i8* undef, i64 44, !noelle.pdg.inst.id !29
[arts] - Removing:   %13 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !30
[arts] - Removing:   %23 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !34
[arts] - Removing:   %24 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !35
- - - - - -- - - - - - - - - - - - - - - - - - -
ParallelEDT CallInst:   %9 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_0, i32* %7, i32 %5, i64* %edt_0_paramv, i32 %8)

ParallelEDT: 
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
  store i32* %2, i32** undef, align 8, !tbaa.struct !81, !noelle.pdg.inst.id !84
  %4 = bitcast i8* undef to i32**, !noelle.pdg.inst.id !85
  store i32* %3, i32** %4, align 8, !tbaa.struct !86, !noelle.pdg.inst.id !87
  %5 = load i32, i32* %0, align 4, !tbaa !70, !noelle.pdg.inst.id !88
  %6 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !89
  %7 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1_guid.addr = alloca i32*, align 8
  store i32* %7, i32** %edt_1_guid.addr, align 8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, i32* %edt_1_paramc, align 4
  %8 = load i32, i32* %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %8, align 8
  %edt_1_paramv.0 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 0
  %9 = sext i32 %5 to i64
  store i64 %9, i64* %edt_1_paramv.0, align 8
  %edt_1_paramv.1 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 1
  %10 = sext i32 %6 to i64
  store i64 %10, i64* %edt_1_paramv.1, align 8
  %11 = alloca i32, align 4
  store i32 2, i32* %11, align 4
  %12 = bitcast i32** %edt_1_guid.addr to i32*
  %13 = load i32, i32* %11, align 4
  %14 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %12, i32 %8, i64* %edt_1_paramv, i32 %13)
  %15 = load i32, i32* %2, align 4, !tbaa !70, !noelle.pdg.inst.id !90
  %16 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2_guid.addr = alloca i32*, align 8
  store i32* %16, i32** %edt_2_guid.addr, align 8
  %edt_2_paramc = alloca i32, align 4
  store i32 1, i32* %edt_2_paramc, align 4
  %17 = load i32, i32* %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %17, align 8
  %edt_2_paramv.0 = getelementptr inbounds i64, i64* %edt_2_paramv, i64 0
  %18 = sext i32 %15 to i64
  store i64 %18, i64* %edt_2_paramv.0, align 8
  %19 = alloca i32, align 4
  store i32 0, i32* %19, align 4
  %20 = bitcast i32** %edt_2_guid.addr to i32*
  %21 = load i32, i32* %19, align 4
  %22 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_2, i32* %20, i32 %17, i64* %edt_2_paramv, i32 %21)
  br label %edt.exit
}
- - - - - -- - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - - - - - - - -
EDTs: 4
- EDT #0
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
  -   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  -   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
Function: arts_edt_0
Live in instructions: 
  -   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
    -   %3 = bitcast i8** %depv.shared_number to i32*
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
    -   %2 = bitcast i8** %depv.number to i32*
  -   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
    -   %1 = bitcast i8** %depv.NewRandom to i32*
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
    -   %0 = bitcast i8** %depv.random_number to i32*
Live out instructions: 
  -   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
    -   %3 = bitcast i8** %depv.shared_number to i32*
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
    -   %2 = bitcast i8** %depv.number to i32*
  -   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
    -   %1 = bitcast i8** %depv.NewRandom to i32*
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
    -   %0 = bitcast i8** %depv.random_number to i32*

- EDT #1
Data environment for EDT: 
Number of ParamV: 2
  -   %5 = load i32, i32* %0, align 4, !tbaa !14, !noelle.pdg.inst.id !16
  -   %6 = load i32, i32* %1, align 4, !tbaa !14, !noelle.pdg.inst.id !17
Number of DepV: 2
  -   %2 = bitcast i8** %depv.number to i32*
  -   %3 = bitcast i8** %depv.shared_number to i32*
Function: arts_edt_1
Live in instructions: 
  -   %2 = bitcast i8** %depv.number to i32*
    -   %4 = bitcast i8** %depv.0 to i32*
  -   %6 = load i32, i32* %1, align 4, !tbaa !14, !noelle.pdg.inst.id !17
    -   %3 = trunc i64 %2 to i32
  -   %3 = bitcast i8** %depv.shared_number to i32*
    -   %5 = bitcast i8** %depv.1 to i32*
  -   %5 = load i32, i32* %0, align 4, !tbaa !14, !noelle.pdg.inst.id !16
    -   %1 = trunc i64 %0 to i32
Live out instructions: 
  -   %3 = bitcast i8** %depv.shared_number to i32*
    -   %5 = bitcast i8** %depv.1 to i32*
  -   %2 = bitcast i8** %depv.number to i32*
    -   %4 = bitcast i8** %depv.0 to i32*

- EDT #2
Data environment for EDT: 
Number of ParamV: 1
  -   %15 = load i32, i32* %2, align 4, !tbaa !14, !noelle.pdg.inst.id !18
Number of DepV: 0
Function: arts_edt_2
Live in instructions: 
  -   %15 = load i32, i32* %2, align 4, !tbaa !14, !noelle.pdg.inst.id !18
    -   %1 = trunc i64 %0 to i32
Live out instructions: 

- EDT #3
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
Function: arts_edt_3
Live in instructions: 
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
    -   %1 = bitcast i8** %depv.random_number to i32*
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
    -   %0 = bitcast i8** %depv.number to i32*
Live out instructions: 
  -   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
    -   %1 = bitcast i8** %depv.random_number to i32*
  -   %number = alloca i32, align 4, !noelle.pdg.inst.id !65
    -   %0 = bitcast i8** %depv.number to i32*

- - - - - - - - - - - - - - - - - - - - - -

 ---------------------------------------- 
[carts] Process has finished

; ModuleID = 'test_with_metadata.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.artsEdtDep_t = type { i32*, i32, i8* }

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
  %call = tail call i32 @rand() #4, !noelle.pdg.inst.id !8
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !76
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !77
  store i32 %add, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !33
  %3 = bitcast i32* %NewRandom to i8*, !noelle.pdg.inst.id !78
  %call1 = tail call i32 @rand() #4, !noelle.pdg.inst.id !15
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !39
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
  store i32 2, i32* %12, align 4
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
declare !callback !79 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

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
  store i32* %2, i32** undef, align 8, !tbaa.struct !81, !noelle.pdg.inst.id !84
  %4 = bitcast i8* undef to i32**, !noelle.pdg.inst.id !85
  store i32* %3, i32** %4, align 8, !tbaa.struct !86, !noelle.pdg.inst.id !87
  %5 = load i32, i32* %0, align 4, !tbaa !70, !noelle.pdg.inst.id !88
  %6 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !89
  %7 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1_guid.addr = alloca i32*, align 8
  store i32* %7, i32** %edt_1_guid.addr, align 8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, i32* %edt_1_paramc, align 4
  %8 = load i32, i32* %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %8, align 8
  %edt_1_paramv.0 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 0
  %9 = sext i32 %5 to i64
  store i64 %9, i64* %edt_1_paramv.0, align 8
  %edt_1_paramv.1 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 1
  %10 = sext i32 %6 to i64
  store i64 %10, i64* %edt_1_paramv.1, align 8
  %11 = alloca i32, align 4
  store i32 2, i32* %11, align 4
  %12 = bitcast i32** %edt_1_guid.addr to i32*
  %13 = load i32, i32* %11, align 4
  %14 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %12, i32 %8, i64* %edt_1_paramv, i32 %13)
  %15 = load i32, i32* %2, align 4, !tbaa !70, !noelle.pdg.inst.id !90
  %16 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2_guid.addr = alloca i32*, align 8
  store i32* %16, i32** %edt_2_guid.addr, align 8
  %edt_2_paramc = alloca i32, align 4
  store i32 1, i32* %edt_2_paramc, align 4
  %17 = load i32, i32* %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %17, align 8
  %edt_2_paramv.0 = getelementptr inbounds i64, i64* %edt_2_paramv, i64 0
  %18 = sext i32 %15 to i64
  store i64 %18, i64* %edt_2_paramv.0, align 8
  %19 = alloca i32, align 4
  store i32 0, i32* %19, align 4
  %20 = bitcast i32** %edt_2_guid.addr to i32*
  %21 = load i32, i32* %19, align 4
  %22 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_2, i32* %20, i32 %17, i64* %edt_2_paramv, i32 %21)
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
  tail call void @llvm.experimental.noalias.scope.decl(metadata !91), !noelle.pdg.inst.id !94
  %6 = load i32, i32* %4, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !95
  %7 = load i32, i32* %5, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !96
  %8 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %6, i32 noundef %7, i32 noundef %1, i32 noundef %3) #4, !noalias !91, !noelle.pdg.inst.id !97
  %9 = load i32, i32* %4, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !98
  %10 = add nsw i32 %9, 1, !noelle.pdg.inst.id !99
  store i32 %10, i32* %4, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !100
  %11 = load i32, i32* %5, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !101
  %12 = add nsw i32 %11, -1, !noelle.pdg.inst.id !102
  store i32 %12, i32* %5, align 4, !tbaa !70, !noalias !91, !noelle.pdg.inst.id !103
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
  tail call void @llvm.experimental.noalias.scope.decl(metadata !104), !noelle.pdg.inst.id !107
  %2 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #4, !noalias !104, !noelle.pdg.inst.id !108
  %3 = add nsw i32 %1, 1, !noelle.pdg.inst.id !109
  br label %edt.exit
}

define void @arts_edt_3(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %0 = bitcast i8** %depv.number to i32*
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %1 = bitcast i8** %depv.random_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %2 = load i32, i32* %0, align 4, !tbaa !70, !noelle.pdg.inst.id !21
  %3 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !45
  %4 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %2, i32 noundef %3), !noelle.pdg.inst.id !30
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
!79 = !{!80}
!80 = !{i64 2, i64 -1, i64 -1, i1 true}
!81 = !{i64 0, i64 8, !82, i64 8, i64 8, !82}
!82 = !{!83, !83, i64 0}
!83 = !{!"any pointer", !72, i64 0}
!84 = !{i64 40}
!85 = !{i64 42}
!86 = !{i64 0, i64 8, !82}
!87 = !{i64 43}
!88 = !{i64 46}
!89 = !{i64 50}
!90 = !{i64 56}
!91 = !{!92}
!92 = distinct !{!92, !93, !".omp_outlined..1: %.privates."}
!93 = distinct !{!93, !".omp_outlined..1"}
!94 = !{i64 68}
!95 = !{i64 71}
!96 = !{i64 72}
!97 = !{i64 75}
!98 = !{i64 76}
!99 = !{i64 77}
!100 = !{i64 78}
!101 = !{i64 79}
!102 = !{i64 80}
!103 = !{i64 81}
!104 = !{!105}
!105 = distinct !{!105, !106, !".omp_outlined..2: %.privates."}
!106 = distinct !{!106, !".omp_outlined..2"}
!107 = !{i64 85}
!108 = !{i64 88}
!109 = !{i64 89}


 ---------------------------------------- 
# noelle-load -debug-only=arts,carts,arts-analyzer -load-pass-plugin /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -passes=CARTS test_with_metadata.bc -o test_opt.bc
llvm-dis test_opt.bc
clang++ -fopenmp test_opt.bc -O3 -march=native -o test_opt -lstdc++
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: /tmp/test_opt-aae354.o: in function `main':
test.cpp:(.text+0x17): undefined reference to `artsReserveGuidRoute'
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: test.cpp:(.text+0x3d): undefined reference to `artsEdtCreateWithGuid'
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: test.cpp:(.text+0x49): undefined reference to `artsReserveGuidRoute'
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: test.cpp:(.text+0x6a): undefined reference to `artsEdtCreateWithGuid'
clang-14: error: linker command failed with exit code 1 (use -v to see invocation)
make: *** [Makefile:32: test_opt] Error 1
