clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	-load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc

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
Created new function: declare internal void @carts.edt(i32*, i32*, i32*, i32*)
Rewiring new function arguments:
  - Rewiring: i32* %random_number -> i32* %0
  - Rewiring: i32* %shared_number -> i32* %3
  - Rewiring: i32* %NewRandom -> i32* %1
  - Rewiring: i32* %number -> i32* %2
New callsite:   call void @carts.edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
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
  call void @carts.edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
  %4 = load i32, i32* %number, align 4, !tbaa !4
  %5 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5)
  ret i32 0
}

[arts-ir-builder] New function:
define internal void @carts.edt(i32* %0, i32* %1, i32* %2, i32* %3) {
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
[omp-transform] Processing function: carts.edt
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %5 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.1(i32*, i32*, i32, i32)
Rewiring new function arguments:
  - Rewiring:   %11 = load i32, i32* %7, align 4, !tbaa !18, !alias.scope !15 -> i32 %3
  - Rewiring:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !12 -> i32* %0
  - Rewiring:   %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !14 -> i32* %1
  - Rewiring:   %10 = load i32, i32* %6, align 4, !tbaa !18, !alias.scope !15 -> i32 %2
New callsite:   call void @carts.edt.1(i32* %2, i32* %3, i32 %10, i32 %13)
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
define internal void @carts.edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = load i32, i32* %0, align 4, !tbaa !4
  %6 = load i32, i32* %1, align 4, !tbaa !4
  call void @carts.edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
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
define internal void @carts.edt.1(i32* %0, i32* %1, i32 %2, i32 %3) {
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
[omp-transform] Processing function: carts.edt.1
[omp-transform] Processing function: carts.edt.1 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %8 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 8, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.2(i32*, i32)
Rewiring new function arguments:
  - Rewiring:   %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !12 -> i32* %0
  - Rewiring:   %5 = load i32, i32* %4, align 4, !tbaa !14, !alias.scope !15 -> i32 %1
New callsite:   call void @carts.edt.2(i32* %3, i32 %13)
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
define internal void @carts.edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !8 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !4
  %5 = load i32, i32* %0, align 4, !tbaa !4
  %6 = load i32, i32* %1, align 4, !tbaa !4
  call void @carts.edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
  %7 = load i32, i32* %3, align 4, !tbaa !4
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %3, align 4, !tbaa !4
  %8 = load i32, i32* %2, align 4, !tbaa !4
  call void @carts.edt.2(i32* %3, i32 %8)
  ret void
}

[arts-ir-builder] New function:
define internal void @carts.edt.2(i32* %0, i32 %1) {
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
[omp-transform] Processing function: carts.edt.2
[omp-transform] Processing function: carts.edt.2 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt - Finished
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
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts.metadata !4 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  %0 = bitcast i32* %number to i8*
  store i32 1, i32* %number, align 4, !tbaa !5
  %1 = bitcast i32* %shared_number to i8*
  store i32 10000, i32* %shared_number, align 4, !tbaa !5
  %2 = bitcast i32* %random_number to i8*
  %call = tail call i32 @rand() #4
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !5
  %3 = bitcast i32* %NewRandom to i8*
  %call1 = tail call i32 @rand() #4
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !5
  call void @carts.edt(i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @edt(i32* %number, i32* %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

define internal void @carts.edt(i32* %0, i32* %1, i32* %2, i32* %3) !carts.metadata !9 {
entry:
  %4 = load i32, i32* undef, align 4, !tbaa !5
  %5 = load i32, i32* %0, align 4, !tbaa !5
  %6 = load i32, i32* %1, align 4, !tbaa !5
  call void @carts.edt.1(i32* %2, i32* %3, i32 %5, i32 %6)
  %7 = load i32, i32* %3, align 4, !tbaa !5
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %3, align 4, !tbaa !5
  %8 = load i32, i32* %2, align 4, !tbaa !5
  call void @carts.edt.2(i32* %3, i32 %8)
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

define internal void @carts.edt.1(i32* %0, i32* %1, i32 %2, i32 %3) !carts.metadata !11 {
entry:
  %4 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**
  %5 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !13
  %.idx = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !18
  %.idx2 = getelementptr %struct.anon, %struct.anon* %5, i64 0, i32 1
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !20
  tail call void @llvm.experimental.noalias.scope.decl(metadata !21)
  %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1
  %8 = load i32, i32* %0, align 4, !tbaa !5, !noalias !21
  %9 = load i32, i32* %1, align 4, !tbaa !5, !noalias !21
  %10 = load i32, i32* undef, align 4, !tbaa !5, !alias.scope !21
  %11 = load i32, i32* undef, align 4, !tbaa !5, !alias.scope !21
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %8, i32 noundef %9, i32 noundef %2, i32 noundef %3) #4, !noalias !21
  %12 = load i32, i32* %0, align 4, !tbaa !5, !noalias !21
  %inc.i = add nsw i32 %12, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !5, !noalias !21
  %13 = load i32, i32* %1, align 4, !tbaa !5, !noalias !21
  %dec.i = add nsw i32 %13, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !5, !noalias !21
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

define internal void @carts.edt.2(i32* %0, i32 %1) !carts.metadata !24 {
entry:
  %2 = bitcast %struct.kmp_task_t_with_privates.1* undef to %struct.anon.0**
  %3 = load %struct.anon.0*, %struct.anon.0** undef, align 8, !tbaa !26
  %.idx = getelementptr %struct.anon.0, %struct.anon.0* %3, i64 0, i32 0
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !29
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !5
  tail call void @llvm.experimental.noalias.scope.decl(metadata !31)
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0
  %5 = load i32, i32* undef, align 4, !tbaa !5, !alias.scope !31
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #4, !noalias !31
  %inc.i = add nsw i32 %1, 1
  store i32 %inc.i, i32* undef, align 4, !tbaa !5, !alias.scope !31
  ret void
}

; Function Attrs: nounwind
declare !callback !34 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal void @edt(i32* %number, i32* %random_number) #0 !carts.metadata !36 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, i32* %number, align 4, !tbaa !5
  %1 = load i32, i32* %random_number, align 4, !tbaa !5
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
!4 = !{!"main"}
!5 = !{!6, !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = !{!"parallel", !10}
!10 = !{!"dep", !"dep", !"dep", !"dep"}
!11 = !{!"task", !12}
!12 = !{!"dep", !"dep", !"param", !"param"}
!13 = !{!14, !16, i64 0}
!14 = !{!"_ZTS24kmp_task_t_with_privates", !15, i64 0, !17, i64 40}
!15 = !{!"_ZTS10kmp_task_t", !16, i64 0, !16, i64 8, !6, i64 16, !7, i64 24, !7, i64 32}
!16 = !{!"any pointer", !7, i64 0}
!17 = !{!"_ZTS15.kmp_privates.t", !6, i64 0, !6, i64 4}
!18 = !{!19, !16, i64 0}
!19 = !{!"_ZTSZ4mainE3$_0", !16, i64 0, !16, i64 8}
!20 = !{!19, !16, i64 8}
!21 = !{!22}
!22 = distinct !{!22, !23, !".omp_outlined..1: %.privates."}
!23 = distinct !{!23, !".omp_outlined..1"}
!24 = !{!"task", !25}
!25 = !{!"dep", !"param"}
!26 = !{!27, !16, i64 0}
!27 = !{!"_ZTS24kmp_task_t_with_privates", !15, i64 0, !28, i64 40}
!28 = !{!"_ZTS15.kmp_privates.t", !6, i64 0}
!29 = !{!30, !16, i64 0}
!30 = !{!"_ZTSZ4mainE3$_1", !16, i64 0}
!31 = !{!32}
!32 = distinct !{!32, !33, !".omp_outlined..2: %.privates."}
!33 = distinct !{!33, !".omp_outlined..2"}
!34 = !{!35}
!35 = !{i64 2, i64 -1, i64 -1, i1 true}
!36 = !{!"task", !37}
!37 = !{!"dep", !"dep"}


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

[arts-utils] Removing dead instructions from: carts.edt
[arts-utils] - Removing:   %4 = load i32, i32* undef, align 4, !tbaa !6
[arts-utils] Removing 1 dead instructions
[arts-utils]   - Replacing uses of:   %4 = load i32, i32* undef, align 4, !tbaa !6
[arts-utils]    - Removing instruction:   %4 = load i32, i32* undef, align 4, !tbaa !6

[arts-utils] Removing dead instructions from: carts.edt.1
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

[arts-utils] Removing dead instructions from: carts.edt.2
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

[arts-utils] Removing dead instructions from: edt
[arts-utils] Removing 0 dead instructions

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
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts.metadata !4 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, i32* %number, align 4, !tbaa !5
  store i32 10000, i32* %shared_number, align 4, !tbaa !5
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !5
  %call1 = tail call i32 @rand() #5
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !5
  call void @carts.edt(i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %random_number, i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %shared_number)
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @edt(i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %1, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %3) #3 !carts.metadata !9 {
entry:
  %4 = load i32, i32* %0, align 4, !tbaa !5
  %5 = load i32, i32* %1, align 4, !tbaa !5
  call void @carts.edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %3, i32 %4, i32 %5) #8
  %6 = load i32, i32* %3, align 4, !tbaa !5
  %inc = add nsw i32 %6, 1
  store i32 %inc, i32* %3, align 4, !tbaa !5
  %7 = load i32, i32* %2, align 4, !tbaa !5
  call void @carts.edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %3, i32 %7) #8
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %0, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %1, i32 noundef %2, i32 noundef %3) #3 !carts.metadata !11 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !13) #9
  %4 = load i32, i32* %0, align 4, !tbaa !5, !noalias !13
  %5 = load i32, i32* %1, align 4, !tbaa !5, !noalias !13
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !13
  %6 = load i32, i32* %0, align 4, !tbaa !5, !noalias !13
  %inc.i = add nsw i32 %6, 1
  store i32 %inc.i, i32* %0, align 4, !tbaa !5, !noalias !13
  %7 = load i32, i32* %1, align 4, !tbaa !5, !noalias !13
  %dec.i = add nsw i32 %7, -1
  store i32 %dec.i, i32* %1, align 4, !tbaa !5, !noalias !13
  ret void
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #5

; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32 noundef %1) #3 !carts.metadata !16 {
entry:
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !5
  tail call void @llvm.experimental.noalias.scope.decl(metadata !18) #9
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #5, !noalias !18
  ret void
}

; Function Attrs: nounwind
declare !callback !21 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #5

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #6

; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define internal void @edt(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %random_number) #7 !carts.metadata !23 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, i32* %number, align 4, !tbaa !5
  %1 = load i32, i32* %random_number, align 4, !tbaa !5
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
!4 = !{!"main"}
!5 = !{!6, !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = !{!"parallel", !10}
!10 = !{!"dep", !"dep", !"dep", !"dep"}
!11 = !{!"task", !12}
!12 = !{!"dep", !"dep", !"param", !"param"}
!13 = !{!14}
!14 = distinct !{!14, !15, !".omp_outlined..1: %.privates."}
!15 = distinct !{!15, !".omp_outlined..1"}
!16 = !{!"task", !17}
!17 = !{!"dep", !"param"}
!18 = !{!19}
!19 = distinct !{!19, !20, !".omp_outlined..2: %.privates."}
!20 = distinct !{!20, !".omp_outlined..2"}
!21 = !{!22}
!22 = !{i64 2, i64 -1, i64 -1, i1 true}
!23 = !{!"task", !24}
!24 = !{!"dep", !"dep"}


-------------------------------------------------
llvm-dis test_arts_ir.bc
noelle-norm test_arts_ir.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
noelle-load -debug-only=arts-analyzer,arts,carts,arts-ir-builder,edt-graph,carts-metadata \
	-load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -ARTSAnalysisPass test_with_metadata.bc -o test_arts_analysis.bc
-------------------------------------------------
[edt-graph] Building the EDT Graph
-------------------------------------------------
-------------------------------------------------
[edt-graph] Creating the EDT Nodes

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph]  Processing function: main
[carts-metadata] Function: main has CARTS Metadata
[carts-metadata] Creating Main EDT Metadata
[arts] Creating EDT #0
[arts] Creating Main EDT for function: main

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph]  Processing function: carts.edt
[carts-metadata] Function: carts.edt has CARTS Metadata
[carts-metadata] Creating Parallel EDT Metadata
[arts] Creating EDT #1
[arts] Creating Parallel EDT for function: carts.edt

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph]  Processing function: carts.edt.1
[carts-metadata] Function: carts.edt.1 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #2
[arts] Creating Task EDT for function: carts.edt.1

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph]  Processing function: carts.edt.2
[carts-metadata] Function: carts.edt.2 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #3
[arts] Creating Task EDT for function: carts.edt.2

- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph]  Processing function: edt
[carts-metadata] Function: edt has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #4
[arts] Creating Task EDT for function: edt

-------------------------------------------------
[edt-graph] 5 EDT Nodes were created
-------------------------------------------------
The EDT #3 creates the following EDTs:
The EDT #2 creates the following EDTs:
The EDT #1 creates the following EDTs:
   [must] EDT #2
     [must] "  call void @carts.edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %3, i32 %4, i32 %5) #8, !noelle.pdg.inst.id !15"
        - Creating edge from "EDT #1" to "EDT #2"
          Control Edge
   [must] EDT #3
     [must] "  call void @carts.edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %3, i32 %7) #8, !noelle.pdg.inst.id !22"
        - Creating edge from "EDT #1" to "EDT #3"
          Control Edge
The EDT #0 creates the following EDTs:
   [must] EDT #1
     [must] "  call void @carts.edt(i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %random_number, i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %shared_number), !noelle.pdg.inst.id !9"
        - Creating edge from "EDT #0" to "EDT #1"
          Control Edge
   [must] EDT #4
     [must] "  call void @edt(i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number), !noelle.pdg.inst.id !22"
        - Creating edge from "EDT #0" to "EDT #4"
          Control Edge
The EDT #4 creates the following EDTs:
-------------------------------------------------
[edt-graph] Printing the Cache
  Value:   %7 = load i32, i32* %2, align 4, !tbaa !43, !noelle.pdg.inst.id !34 in EDT #1
    - EDT #3
  Value:   %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !67 in EDT #0
    - EDT #1
  Value:   %4 = load i32, i32* %0, align 4, !tbaa !43, !noelle.pdg.inst.id !14 in EDT #1
    - EDT #2
  Value:   %5 = load i32, i32* %1, align 4, !tbaa !43, !noelle.pdg.inst.id !24 in EDT #1
    - EDT #2
  Value:   %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !65 in EDT #0
    - EDT #1
  Value:   %random_number = alloca i32, align 4, !noelle.pdg.inst.id !66 in EDT #0
    - EDT #1
    - EDT #4
  Value: i32* %3 in EDT #1
    - EDT #2
    - EDT #3
  Value: i32* %2 in EDT #1
    - EDT #2
  Value:   %number = alloca i32, align 4, !noelle.pdg.inst.id !64 in EDT #0
    - EDT #1
    - EDT #4
-------------------------------------------------
-------------------------------------------------
[edt-graph] Analyzing dependencies
 - EDT #0
; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !carts.metadata !5 !noelle.pdg.edges !6 {
entry:
  %number = alloca i32, align 4, !noelle.pdg.inst.id !64
  %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !65
  %random_number = alloca i32, align 4, !noelle.pdg.inst.id !66
  %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !67
; 1 = MemoryDef(liveOnEntry)
  store i32 1, i32* %number, align 4, !tbaa !68, !noelle.pdg.inst.id !8
; 2 = MemoryDef(1)
  store i32 10000, i32* %shared_number, align 4, !tbaa !68, !noelle.pdg.inst.id !25
; 3 = MemoryDef(2)
  %call = tail call i32 @rand() #5, !noelle.pdg.inst.id !16
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !72
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !73
; 4 = MemoryDef(3)
  store i32 %add, i32* %random_number, align 4, !tbaa !68, !noelle.pdg.inst.id !38
; 5 = MemoryDef(4)
  %call1 = tail call i32 @rand() #5, !noelle.pdg.inst.id !19
; 6 = MemoryDef(5)
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !68, !noelle.pdg.inst.id !44
; 7 = MemoryDef(6)
  call void @carts.edt(i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %random_number, i32* noalias nocapture nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull align 4 dereferenceable(4) %shared_number), !noelle.pdg.inst.id !9
  br label %codeRepl, !noelle.pdg.inst.id !74

codeRepl:                                         ; preds = %entry
; 8 = MemoryDef(7)
  call void @edt(i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number), !noelle.pdg.inst.id !22
  br label %entry.split.ret, !noelle.pdg.inst.id !75

entry.split.ret:                                  ; preds = %codeRepl
  ret i32 0, !noelle.pdg.inst.id !76
}
   - EDTChild #1
     - Its call is : carts.edt
     Checking clobbering of: carts.edt
        - Data dependency with Parent EDT
   - EDTChild #4
     - Its call is : edt
     Checking clobbering of: edt
        - Data dependency with EDT #1
        - Data dependency with Parent EDT


-------------------------------------------------
 - EDT #4
; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define internal void @edt(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %number, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %random_number) #7 !carts.metadata !5 !noelle.pdg.args.id !7 {
newFuncRoot:
  br label %entry.split, !noelle.pdg.inst.id !10

entry.split:                                      ; preds = %newFuncRoot
; MemoryUse(liveOnEntry) MayAlias
  %0 = load i32, i32* %number, align 4, !tbaa !11, !noelle.pdg.inst.id !15
; MemoryUse(liveOnEntry) MayAlias
  %1 = load i32, i32* %random_number, align 4, !tbaa !11, !noelle.pdg.inst.id !16
; 1 = MemoryDef(liveOnEntry)
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %0, i32 noundef %1) #5, !noelle.pdg.inst.id !17
  br label %entry.split.ret.exitStub, !noelle.pdg.inst.id !18

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void, !noelle.pdg.inst.id !19
}


-------------------------------------------------
 - EDT #1
; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %1, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %3) #3 !carts.metadata !5 !noelle.pdg.args.id !7 !noelle.pdg.edges !12 {
entry:
; MemoryUse(liveOnEntry) MayAlias
  %4 = load i32, i32* %0, align 4, !tbaa !43, !noelle.pdg.inst.id !14
; MemoryUse(liveOnEntry) MayAlias
  %5 = load i32, i32* %1, align 4, !tbaa !43, !noelle.pdg.inst.id !24
; 1 = MemoryDef(liveOnEntry)
  call void @carts.edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %3, i32 %4, i32 %5) #8, !noelle.pdg.inst.id !15
; MemoryUse(1) MayAlias
  %6 = load i32, i32* %3, align 4, !tbaa !43, !noelle.pdg.inst.id !28
  %inc = add nsw i32 %6, 1, !noelle.pdg.inst.id !47
; 2 = MemoryDef(1)
  store i32 %inc, i32* %3, align 4, !tbaa !43, !noelle.pdg.inst.id !20
; MemoryUse(2) MayAlias
  %7 = load i32, i32* %2, align 4, !tbaa !43, !noelle.pdg.inst.id !34
; 3 = MemoryDef(2)
  call void @carts.edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %3, i32 %7) #8, !noelle.pdg.inst.id !22
  ret void, !noelle.pdg.inst.id !48
}
   - EDTChild #2
     - Its call is : carts.edt.1
     Checking clobbering of: carts.edt.1
        - Data dependency with Parent EDT
   - EDTChild #3
     - Its call is : carts.edt.2
     Checking clobbering of: carts.edt.2
        - Data dependency with EDT #2
        - Data dependency with Parent EDT


-------------------------------------------------
 - EDT #2
; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt.1(i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %0, i32* nocapture nofree noundef nonnull align 4 dereferenceable(4) %1, i32 noundef %2, i32 noundef %3) #3 !carts.metadata !5 !noelle.pdg.args.id !7 !noelle.pdg.edges !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !49) #8, !noelle.pdg.inst.id !14
; MemoryUse(liveOnEntry) MayAlias
  %4 = load i32, i32* %0, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !15
; MemoryUse(liveOnEntry) MayAlias
  %5 = load i32, i32* %1, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !20
; 1 = MemoryDef(liveOnEntry)
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !49, !noelle.pdg.inst.id !22
; MemoryUse(1) MayAlias
  %6 = load i32, i32* %0, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !26
  %inc.i = add nsw i32 %6, 1, !noelle.pdg.inst.id !56
; 2 = MemoryDef(1)
  store i32 %inc.i, i32* %0, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !28
; MemoryUse(2) MayAlias
  %7 = load i32, i32* %1, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !32
  %dec.i = add nsw i32 %7, -1, !noelle.pdg.inst.id !57
; 3 = MemoryDef(2)
  store i32 %dec.i, i32* %1, align 4, !tbaa !52, !noalias !49, !noelle.pdg.inst.id !34
  ret void, !noelle.pdg.inst.id !58
}


-------------------------------------------------
 - EDT #3
; Function Attrs: nofree norecurse nounwind
define internal void @carts.edt.2(i32* noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %0, i32 noundef %1) #3 !carts.metadata !5 !noelle.pdg.args.id !7 !noelle.pdg.edges !10 {
entry:
; MemoryUse(liveOnEntry) MayAlias
  %.idx.val.val = load i32, i32* %0, align 4, !tbaa !22, !noelle.pdg.inst.id !12
  tail call void @llvm.experimental.noalias.scope.decl(metadata !26) #8, !noelle.pdg.inst.id !13
; 1 = MemoryDef(liveOnEntry)
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([32 x i8], [32 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1, i32 noundef %.idx.val.val) #5, !noalias !26, !noelle.pdg.inst.id !18
  ret void, !noelle.pdg.inst.id !29
}


-------------------------------------------------
-------------------------------------------------
- - - - - - - - - - - - - - - - - - - - - - - -
[edt-graph] Printing the EDT Graph
- EDT #0 - "main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Edges:
    - The EDT has no incoming edges
  - Outgoing Edges:
    - [control/ creation] "EDT #1"
    - [control/ creation] "EDT #4"

- EDT #4 - "edt"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - i32* %number
      - i32* %random_number
  - Incoming Edges:
    - [control/ creation] "main"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #1 - "carts.edt"
  - Type: parallel
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 4
      - i32* %0
      - i32* %1
      - i32* %2
      - i32* %3
  - Incoming Edges:
    - [control/ creation] "main"
  - Outgoing Edges:
    - [control/ creation] "EDT #2"
    - [control/ creation] "EDT #3"

- EDT #2 - "carts.edt.1"
  - Type: task
  - Data Environment:
    - Number of ParamV = 2
      - i32 %2
      - i32 %3
    - Number of DepV = 2
      - i32* %0
      - i32* %1
  - Incoming Edges:
    - [control/ creation] "carts.edt"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- EDT #3 - "carts.edt.2"
  - Type: task
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - i32* %0
  - Incoming Edges:
    - [control/ creation] "carts.edt"
  - Outgoing Edges:
    - The EDT has no outgoing edges

- - - - - - - - - - - - - - - - - - - - - - - -

[edt-graph] Destroying the EDT Graph
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
