clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph,omp-transform,arts-utils \
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
[arts-ir-builder] Building EDT:
Created new function: declare internal void @edt(i32*, i32*, i32*, i32*)
Rewiring new function arguments:
  - Rewiring: i32* %random_number -> i32* %0
  - Rewiring: i32* %shared_number -> i32* %3
  - Rewiring: i32* %NewRandom -> i32* %1
  - Rewiring: i32* %number -> i32* %2
New callsite:   call void @edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
[arts-utils]   - Replacing uses of:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number)
[arts-utils]    - Removing instruction:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number)
[arts-utils]   - Replacing uses of: i32* %.global_tid.
[arts-utils]    - Replacing:   %4 = load i32, i32* undef, align 4, !tbaa !4
[arts-utils]   - Replacing uses of: i32* %.bound_tid.
[arts-utils]   - Replacing uses of: i32* %random_number
[arts-utils]   - Replacing uses of: i32* %NewRandom
[arts-utils]   - Replacing uses of: i32* %number
[arts-utils]   - Replacing uses of: i32* %shared_number
Current Function after undefining OldCB:
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
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
  %3 = bitcast i32* %NewRandom to i8*
  %call1 = tail call i32 @rand() #5
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !4
  call void @edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
  %4 = load i32, i32* %number, align 4, !tbaa !4
  %5 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5)
  ret i32 0
}

[arts-ir-builder] New function:
define internal void @edt(i32* %0, i32* %1, i32* %2, i32* %3) {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
  %6 = bitcast i8* %5 to i8**
  %7 = load i8*, i8** %6, align 8, !tbaa !8
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %7 to i32**
  store i32* %2, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !13
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %7, i64 8
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**
  store i32* %3, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !15
  %8 = getelementptr inbounds i8, i8* %5, i64 40
  %9 = bitcast i8* %8 to i32*
  %10 = load i32, i32* %0, align 4, !tbaa !4
  store i32 %10, i32* %9, align 8, !tbaa !16
  %11 = getelementptr inbounds i8, i8* %5, i64 44
  %12 = bitcast i8* %11 to i32*
  %13 = load i32, i32* %1, align 4, !tbaa !4
  store i32 %13, i32* %12, align 4, !tbaa !17
  %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %5)
  %15 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %15, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %16 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
  %17 = bitcast i8* %16 to i32***
  %18 = load i32**, i32*** %17, align 8, !tbaa !18
  store i32* %3, i32** %18, align 8, !tbaa.struct !15
  %19 = getelementptr inbounds i8, i8* %16, i64 40
  %20 = bitcast i8* %19 to i32*
  %21 = load i32, i32* %2, align 4, !tbaa !4
  store i32 %21, i32* %20, align 8, !tbaa !21
  %22 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %16)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %5 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
[arts-ir-builder] Building EDT:
Created new function: declare internal void @edt.1(i32*, i32*, i32, i32)
Rewiring new function arguments:
  - Rewiring:   %10 = load i32, i32* %6, align 4, !tbaa !18, !alias.scope !15 -> i32 %2
  - Rewiring:   %11 = load i32, i32* %7, align 4, !tbaa !18, !alias.scope !15 -> i32 %3
  - Rewiring:   %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !14 -> i32* %1
  - Rewiring:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !12 -> i32* %0
New callsite:   call void @edt.1(i32* %2, i32* %3, i32 %10, i32 %13)
[arts-utils]   - Replacing uses of:   %5 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
[arts-utils]    - Replacing:   %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
[arts-utils]    - Removing instruction:   %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
[arts-utils]    - Replacing:   %6 = bitcast i8* undef to i8**
[arts-utils]    - Removing instruction:   %6 = bitcast i8* undef to i8**
[arts-utils]    - Replacing:   %6 = load i8*, i8** undef, align 8, !tbaa !10
[arts-utils]    - Removing instruction:   %6 = load i8*, i8** undef, align 8, !tbaa !10
[arts-utils]    - Replacing:   %agg.captured.sroa.0.0..sroa_cast = bitcast i8* undef to i32**
[arts-utils]    - Removing instruction:   %agg.captured.sroa.0.0..sroa_cast = bitcast i8* undef to i32**
[arts-utils]    - Replacing:   store i32* %2, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Removing instruction:   store i32* %2, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Replacing:   %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* undef, i64 8
[arts-utils]    - Removing instruction:   %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* undef, i64 8
[arts-utils]    - Replacing:   %agg.captured.sroa.2.0..sroa_cast = bitcast i8* undef to i32**
[arts-utils]    - Removing instruction:   %agg.captured.sroa.2.0..sroa_cast = bitcast i8* undef to i32**
[arts-utils]    - Replacing:   store i32* %3, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Removing instruction:   store i32* %3, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Replacing:   %6 = getelementptr inbounds i8, i8* undef, i64 40
[arts-utils]    - Removing instruction:   %6 = getelementptr inbounds i8, i8* undef, i64 40
[arts-utils]    - Replacing:   %6 = bitcast i8* undef to i32*
[arts-utils]    - Removing instruction:   %6 = bitcast i8* undef to i32*
[arts-utils]    - Replacing:   store i32 %6, i32* undef, align 8, !tbaa !10
[arts-utils]    - Removing instruction:   store i32 %6, i32* undef, align 8, !tbaa !10
[arts-utils]    - Replacing:   %7 = getelementptr inbounds i8, i8* undef, i64 44
[arts-utils]    - Removing instruction:   %7 = getelementptr inbounds i8, i8* undef, i64 44
[arts-utils]    - Replacing:   %7 = bitcast i8* undef to i32*
[arts-utils]    - Removing instruction:   %7 = bitcast i8* undef to i32*
[arts-utils]    - Replacing:   store i32 %7, i32* undef, align 4, !tbaa !10
[arts-utils]    - Removing instruction:   store i32 %7, i32* undef, align 4, !tbaa !10
[arts-utils]    - Removing instruction:   %5 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Replacing uses of: %struct.kmp_task_t_with_privates* %1
[arts-utils]    - Replacing:   %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
[arts-utils]    - Replacing:   %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
[arts-utils]    - Replacing:   %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
Current Function after undefining OldCB:
define internal void @edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = load i32, i32* %0, align 4, !tbaa !4
  %6 = load i32, i32* %1, align 4, !tbaa !4
  call void @edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
  %7 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %8 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
  %9 = bitcast i8* %8 to i32***
  %10 = load i32**, i32*** %9, align 8, !tbaa !10
  store i32* %3, i32** %10, align 8, !tbaa.struct !15
  %11 = getelementptr inbounds i8, i8* %8, i64 40
  %12 = bitcast i8* %11 to i32*
  %13 = load i32, i32* %2, align 4, !tbaa !4
  store i32 %13, i32* %12, align 8, !tbaa !17
  %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %8)
  ret void
}

[arts-ir-builder] New function:
define internal void @edt.1(i32* %0, i32* %1, i32 %2, i32 %3) {
entry:
  %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
  %5 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !18
  %.idx = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !21
  %.idx2 = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 1
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !23
  tail call void @llvm.experimental.noalias.scope.decl(metadata !24)
  %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
  %8 = load i32, i32* %0, align 4, !tbaa !4, !noalias !24
  %9 = load i32, i32* %1, align 4, !tbaa !4, !noalias !24
  %10 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !24
  %11 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !24
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %8, i32 noundef %9, i32 noundef %2, i32 noundef %3) #4, !noalias !24
  %12 = load i32, i32* %0, align 4, !tbaa !4, !noalias !24
  %inc.i = add nsw i32 %12, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !4, !noalias !24
  %13 = load i32, i32* %1, align 4, !tbaa !4, !noalias !24
  %dec.i = add nsw i32 %13, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !4, !noalias !24
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt.1
[omp-transform] Processing function: edt.1 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %8 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
[arts-ir-builder] Building EDT:
Created new function: declare internal void @edt.2(i32*, i32)
Rewiring new function arguments:
  - Rewiring:   %5 = load i32, i32* %4, align 4, !tbaa !14, !alias.scope !15 -> i32 %1
  - Rewiring:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !12 -> i32* %0
New callsite:   call void @edt.2(i32* %3, i32 %13)
[arts-utils]   - Replacing uses of:   %8 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
[arts-utils]    - Replacing:   %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
[arts-utils]    - Removing instruction:   %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef)
[arts-utils]    - Replacing:   %9 = bitcast i8* undef to i32***
[arts-utils]    - Removing instruction:   %9 = bitcast i8* undef to i32***
[arts-utils]    - Replacing:   %9 = load i32**, i32*** undef, align 8, !tbaa !10
[arts-utils]    - Removing instruction:   %9 = load i32**, i32*** undef, align 8, !tbaa !10
[arts-utils]    - Replacing:   store i32* %3, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Removing instruction:   store i32* %3, i32** undef, align 8, !tbaa.struct !10
[arts-utils]    - Replacing:   %9 = getelementptr inbounds i8, i8* undef, i64 40
[arts-utils]    - Removing instruction:   %9 = getelementptr inbounds i8, i8* undef, i64 40
[arts-utils]    - Replacing:   %9 = bitcast i8* undef to i32*
[arts-utils]    - Removing instruction:   %9 = bitcast i8* undef to i32*
[arts-utils]    - Replacing:   store i32 %9, i32* undef, align 8, !tbaa !10
[arts-utils]    - Removing instruction:   store i32 %9, i32* undef, align 8, !tbaa !10
[arts-utils]    - Removing instruction:   %8 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Replacing uses of: %struct.kmp_task_t_with_privates.1* %1
[arts-utils]    - Replacing:   %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
[arts-utils]    - Replacing:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
Current Function after undefining OldCB:
define internal void @edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = load i32, i32* %0, align 4, !tbaa !4
  %6 = load i32, i32* %1, align 4, !tbaa !4
  call void @edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
  %7 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %8 = load i32, i32* %2, align 4, !tbaa !4
  call void @edt.2(i32* %3, i32 %8)
  ret void
}

[arts-ir-builder] New function:
define internal void @edt.2(i32* %0, i32 %1) {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
  %3 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !23
  %.idx = getelementptr %struct.anon.0, %struct.anon.0* %3, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !26
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !28)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
  %5 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !28
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #4, !noalias !28
  %inc.i = add nsw i32 %1, 1
  store i32 %inc.i, i32* undef, align 4, !tbaa !4, !alias.scope !28
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt.2
[omp-transform] Processing function: edt.2 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: edt - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: main - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.anon = type { i32*, i32* }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { i8*, i32 (i32, i8*)*, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { i32 (i32, i8*)* }
%struct..kmp_privates.t = type { i32, i32 }
%struct.anon.0 = type { i32* }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

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

define internal void @edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = load i32, i32* %0, align 4, !tbaa !4
  %6 = load i32, i32* %1, align 4, !tbaa !4
  call void @edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
  %7 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %8 = load i32, i32* %2, align 4, !tbaa !4
  call void @edt.2(i32* %3, i32 %8)
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

define internal void @edt.1(i32* %0, i32* %1, i32 %2, i32 %3) !carts.metadata !10 {
entry:
  %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
  %5 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !12
  %.idx = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !17
  %.idx2 = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 1
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !19
  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
  %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
  %8 = load i32, i32* %0, align 4, !tbaa !4, !noalias !20
  %9 = load i32, i32* %1, align 4, !tbaa !4, !noalias !20
  %10 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !20
  %11 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !20
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %8, i32 noundef %9, i32 noundef %2, i32 noundef %3) #4, !noalias !20
  %12 = load i32, i32* %0, align 4, !tbaa !4, !noalias !20
  %inc.i = add nsw i32 %12, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !4, !noalias !20
  %13 = load i32, i32* %1, align 4, !tbaa !4, !noalias !20
  %dec.i = add nsw i32 %13, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !4, !noalias !20
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

define internal void @edt.2(i32* %0, i32 %1) !carts.metadata !23 {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
  %3 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !25
  %.idx = getelementptr %struct.anon.0, %struct.anon.0* %3, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !28
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !30)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
  %5 = load i32, i32* undef, align 4, !tbaa !4, !alias.scope !30
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #4, !noalias !30
  %inc.i = add nsw i32 %1, 1
  store i32 %inc.i, i32* undef, align 4, !tbaa !4, !alias.scope !30
  ret void
}

; Function Attrs: nounwind
declare !callback !33 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @edt.3(i32* %number, i32* %random_number) #0 !carts.metadata !35 {
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
!10 = !{!"edt.task", !11}
!11 = !{!"edt.arg.dep", !"edt.arg.dep", !"edt.arg.param", !"edt.arg.param"}
!12 = !{!13, !15, i64 0}
!13 = !{!"_ZTS24kmp_task_t_with_privates", !14, i64 0, !16, i64 40}
!14 = !{!"_ZTS10kmp_task_t", !15, i64 0, !15, i64 8, !5, i64 16, !6, i64 24, !6, i64 32}
!15 = !{!"any pointer", !6, i64 0}
!16 = !{!"_ZTS15.kmp_privates.t", !5, i64 0, !5, i64 4}
!17 = !{!18, !15, i64 0}
!18 = !{!"_ZTSZ4mainE3$_0", !15, i64 0, !15, i64 8}
!19 = !{!18, !15, i64 8}
!20 = !{!21}
!21 = distinct !{!21, !22, !".omp_outlined..1: %.privates."}
!22 = distinct !{!22, !".omp_outlined..1"}
!23 = !{!"edt.task", !24}
!24 = !{!"edt.arg.dep", !"edt.arg.param"}
!25 = !{!26, !15, i64 0}
!26 = !{!"_ZTS24kmp_task_t_with_privates", !14, i64 0, !27, i64 40}
!27 = !{!"_ZTS15.kmp_privates.t", !5, i64 0}
!28 = !{!29, !15, i64 0}
!29 = !{!"_ZTSZ4mainE3$_1", !15, i64 0}
!30 = !{!31}
!31 = distinct !{!31, !32, !".omp_outlined..2: %.privates."}
!32 = distinct !{!32, !".omp_outlined..2"}
!33 = !{!34}
!34 = !{i64 2, i64 -1, i64 -1, i1 true}
!35 = !{!"edt.task", !36}
!36 = !{!"edt.arg.dep", !"edt.arg.dep"}


-------------------------------------------------
[arts-utils] Removing dead instructions from: main
[arts-utils] - Removing:   %0 = bitcast i32* %number to i8*
[arts-utils] - Removing:   %1 = bitcast i32* %shared_number to i8*
[arts-utils] - Removing:   %2 = bitcast i32* %random_number to i8*
[arts-utils] - Removing:   %3 = bitcast i32* %NewRandom to i8*
[arts-utils] Removing 4 dead instructions
[arts-utils]   - Replacing uses of:   %0 = bitcast i32* %number to i8*
[arts-utils]    - Removing instruction:   %0 = bitcast i32* %number to i8*
[arts-utils]   - Replacing uses of:   %0 = bitcast i32* %shared_number to i8*
[arts-utils]    - Removing instruction:   %0 = bitcast i32* %shared_number to i8*
[arts-utils]   - Replacing uses of:   %0 = bitcast i32* %random_number to i8*
[arts-utils]    - Removing instruction:   %0 = bitcast i32* %random_number to i8*
[arts-utils]   - Replacing uses of:   %0 = bitcast i32* %NewRandom to i8*
[arts-utils]    - Removing instruction:   %0 = bitcast i32* %NewRandom to i8*

[arts-utils] Removing dead instructions from: edt
[arts-utils] - Removing:   %4 = load i32, i32* undef, align 4, !tbaa !6
[arts-utils] Removing 1 dead instructions
[arts-utils]   - Replacing uses of:   %4 = load i32, i32* undef, align 4, !tbaa !6
[arts-utils]    - Removing instruction:   %4 = load i32, i32* undef, align 4, !tbaa !6

[arts-utils] Removing dead instructions from: edt.1
[arts-utils] - Removing:   %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
[arts-utils] - Removing:   %5 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !6
[arts-utils] - Removing:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !14
[arts-utils] - Removing:   %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !16
[arts-utils] - Removing:   %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
[arts-utils] - Removing:   %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
[arts-utils] - Removing:   %10 = load i32, i32* undef, align 4, !tbaa !20, !alias.scope !17
[arts-utils] - Removing:   %11 = load i32, i32* undef, align 4, !tbaa !20, !alias.scope !17
[arts-utils] Removing 8 dead instructions
[arts-utils]   - Replacing uses of:   %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
[arts-utils]    - Removing instruction:   %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
[arts-utils]   - Replacing uses of:   %4 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !6
[arts-utils]    - Replacing:   %.idx = getelementptr %struct.anon, %struct.anon* undef, i64 0, i32 0
[arts-utils]    - Removing instruction:   %.idx = getelementptr %struct.anon, %struct.anon* undef, i64 0, i32 0
[arts-utils]    - Replacing:   %.idx.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Removing instruction:   %.idx.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Replacing:   %.idx2 = getelementptr %struct.anon, %struct.anon* undef, i64 0, i32 1
[arts-utils]    - Removing instruction:   %.idx2 = getelementptr %struct.anon, %struct.anon* undef, i64 0, i32 1
[arts-utils]    - Replacing:   %.idx2.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Removing instruction:   %.idx2.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Removing instruction:   %4 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !6
[arts-utils]   - Replacing uses of:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
[arts-utils]    - Removing instruction:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
[arts-utils]   - Replacing uses of:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
[arts-utils]    - Removing instruction:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
[arts-utils]   - Replacing uses of:   %6 = load i32, i32* undef, align 4, !tbaa !9, !alias.scope !6
[arts-utils]    - Removing instruction:   %6 = load i32, i32* undef, align 4, !tbaa !9, !alias.scope !6
[arts-utils]   - Replacing uses of:   %6 = load i32, i32* undef, align 4, !tbaa !9, !alias.scope !6
[arts-utils]    - Removing instruction:   %6 = load i32, i32* undef, align 4, !tbaa !9, !alias.scope !6

[arts-utils] Removing dead instructions from: edt.2
[arts-utils] - Removing:   %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
[arts-utils] - Removing:   %3 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !6
[arts-utils] - Removing:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !14
[arts-utils] - Removing:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
[arts-utils] - Removing:   %5 = load i32, i32* undef, align 4, !tbaa !16, !alias.scope !17
[arts-utils] - Removing:   store i32 %inc.i, i32* undef, align 4, !tbaa !16, !alias.scope !17
[arts-utils] Removing 6 dead instructions
[arts-utils]   - Replacing uses of:   %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
[arts-utils]    - Removing instruction:   %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
[arts-utils]   - Replacing uses of:   %2 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !6
[arts-utils]    - Replacing:   %.idx = getelementptr %struct.anon.0, %struct.anon.0* undef, i64 0, i32 0
[arts-utils]    - Removing instruction:   %.idx = getelementptr %struct.anon.0, %struct.anon.0* undef, i64 0, i32 0
[arts-utils]    - Replacing:   %.idx.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Removing instruction:   %.idx.val = load i32*, i32** undef, align 8, !tbaa !14
[arts-utils]    - Removing instruction:   %2 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !6
[arts-utils]   - Replacing uses of:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
[arts-utils]    - Removing instruction:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
[arts-utils]   - Replacing uses of:   %2 = load i32, i32* undef, align 4, !tbaa !6, !alias.scope !10
[arts-utils]    - Removing instruction:   %2 = load i32, i32* undef, align 4, !tbaa !6, !alias.scope !10
[arts-utils]   - Replacing uses of:   store i32 %inc.i, i32* undef, align 4, !tbaa !6, !alias.scope !10
[arts-utils]    - Removing instruction:   store i32 %inc.i, i32* undef, align 4, !tbaa !6, !alias.scope !10

[arts-utils] Removing dead instructions from: edt.3
[arts-utils] Removing 0 dead instructions


-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }

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
  store i32 1, i32* %number, align 4, !tbaa !4
  store i32 10000, i32* %shared_number, align 4, !tbaa !4
  %call = tail call i32 @rand() #4
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
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

define internal void @edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* %0, align 4, !tbaa !4
  %5 = load i32, i32* %1, align 4, !tbaa !4
  call void @edt.1(i32* %2, i32* %3, i32 %4, i32 %5)
  %6 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %6, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %7 = load i32, i32* %2, align 4, !tbaa !4
  call void @edt.2(i32* %3, i32 %7)
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

define internal void @edt.1(i32* %0, i32* %1, i32 %2, i32 %3) !carts.metadata !10 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %4 = load i32, i32* %0, align 4, !tbaa !4, !noalias !12
  %5 = load i32, i32* %1, align 4, !tbaa !4, !noalias !12
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #4, !noalias !12
  %6 = load i32, i32* %0, align 4, !tbaa !4, !noalias !12
  %inc.i = add nsw i32 %6, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !4, !noalias !12
  %7 = load i32, i32* %1, align 4, !tbaa !4, !noalias !12
  %dec.i = add nsw i32 %7, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !4, !noalias !12
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

define internal void @edt.2(i32* %0, i32 %1) !carts.metadata !15 {
entry:
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #4, !noalias !17
  %inc.i = add nsw i32 %1, 1
  ret void
}

; Function Attrs: nounwind
declare !callback !20 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @edt.3(i32* %number, i32* %random_number) #0 !carts.metadata !22 {
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
!10 = !{!"edt.task", !11}
!11 = !{!"edt.arg.dep", !"edt.arg.dep", !"edt.arg.param", !"edt.arg.param"}
!12 = !{!13}
!13 = distinct !{!13, !14, !".omp_outlined..1: %.privates."}
!14 = distinct !{!14, !".omp_outlined..1"}
!15 = !{!"edt.task", !16}
!16 = !{!"edt.arg.dep", !"edt.arg.param"}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..2: %.privates."}
!19 = distinct !{!19, !".omp_outlined..2"}
!20 = !{!21}
!21 = !{i64 2, i64 -1, i64 -1, i1 true}
!22 = !{!"edt.task", !23}
!23 = !{!"edt.arg.dep", !"edt.arg.dep"}


-------------------------------------------------
[omp-transform] [Attributor] Done with 5 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }

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
  store i32 1, i32* %number, align 4, !tbaa !4
  store i32 10000, i32* %shared_number, align 4, !tbaa !4
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
  %call1 = tail call i32 @rand() #5
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !4
  call void @edt(i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %random_number, i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %shared_number)
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @edt.3(i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: nofree norecurse nounwind
define internal void @edt(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %1, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %3) #3 !carts.metadata !8 {
entry:
  %4 = load i32, i32* %0, align 4, !tbaa !4
  %5 = load i32, i32* %1, align 4, !tbaa !4
  call void @edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %3, i32 %4, i32 %5) #8
  %6 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %6, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %7 = load i32, i32* %2, align 4, !tbaa !4
  call void @edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %3, i32 %7) #8
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind
define internal void @edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %0, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %1, i32 noundef %2, i32 noundef %3) #3 !carts.metadata !10 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12) #9
  %4 = load i32, i32* %0, align 4, !tbaa !4, !noalias !12
  %5 = load i32, i32* %1, align 4, !tbaa !4, !noalias !12
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !12
  %6 = load i32, i32* %0, align 4, !tbaa !4, !noalias !12
  %inc.i = add nsw i32 %6, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !4, !noalias !12
  %7 = load i32, i32* %1, align 4, !tbaa !4, !noalias !12
  %dec.i = add nsw i32 %7, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !4, !noalias !12
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #5

; Function Attrs: nofree norecurse nounwind
define internal void @edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32 noundef %1) #3 !carts.metadata !15 {
entry:
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !4
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17) #9
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #5, !noalias !17
  ret void
}

; Function Attrs: nounwind
declare !callback !20 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #5

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define internal void @edt.3(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %random_number) #7 !carts.metadata !22 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, i32* %number, align 4, !tbaa !4
  %1 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %0, i32 noundef %1) #5
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nofree norecurse nounwind }
attributes #4 = { nofree nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { inaccessiblememonly nofree nosync nounwind willreturn }
attributes #7 = { mustprogress nofree norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nofree nounwind }
attributes #9 = { willreturn }

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
!10 = !{!"edt.task", !11}
!11 = !{!"edt.arg.dep", !"edt.arg.dep", !"edt.arg.param", !"edt.arg.param"}
!12 = !{!13}
!13 = distinct !{!13, !14, !".omp_outlined..1: %.privates."}
!14 = distinct !{!14, !".omp_outlined..1"}
!15 = !{!"edt.task", !16}
!16 = !{!"edt.arg.dep", !"edt.arg.param"}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..2: %.privates."}
!19 = distinct !{!19, !".omp_outlined..2"}
!20 = !{!21}
!21 = !{i64 2, i64 -1, i64 -1, i1 true}
!22 = !{!"edt.task", !23}
!23 = !{!"edt.arg.dep", !"edt.arg.dep"}


-------------------------------------------------
llvm-dis test_arts.bc
noelle-norm test_arts.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
clang++ -fopenmp test_with_metadata.bc -O3 -march=native -o test_opt -lstdc++
