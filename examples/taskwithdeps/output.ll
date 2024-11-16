clang++ -fopenmp -O3 -g0 -emit-llvm -c taskwithdeps.cpp -o taskwithdeps.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis taskwithdeps.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder\
	-passes="omp-transform" taskwithdeps.bc -o taskwithdeps_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
taskwithdeps.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 
[omp-transform] Processing main function
[omp-transform] Main EDT Function: define internal void @carts.edt.main() !carts !56 {
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
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.main
[omp-transform] Other Function Found:
  %call = tail call i64 @time(ptr noundef null) #7
[omp-transform] Other Function Found:
  tail call void @srand(i32 noundef %conv) #7
[omp-transform] Other Function Found:
  %call1 = tail call i32 @rand() #7
[omp-transform] Other Function Found:
  %call2 = tail call i32 @rand() #7
- - - - - - - - - - - - - - - -
[omp-transform] Parallel Region Found:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 3, ptr nonnull @main.omp_outlined, ptr nonnull %res, ptr nonnull %x, ptr nonnull %y)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.parallel(ptr, ptr, ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.parallel(ptr %res, ptr %x, ptr %y)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1, ptr %2) #1 {
entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  %3 = load i32, ptr undef, align 4, !tbaa !6
  %4 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
  %.not = icmp eq i32 %4, 0
  br i1 %.not, label %omp_if.end, label %omp_if.then

omp_if.then:                                      ; preds = %entry
  %5 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry.)
  %6 = load ptr, ptr %5, align 8, !tbaa !10
  store ptr %0, ptr %6, align 8, !tbaa.struct !14
  %7 = ptrtoint ptr %0 to i64
  store i64 %7, ptr %.dep.arr.addr, align 8, !tbaa !16
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  store i64 4, ptr %8, align 8, !tbaa !19
  %9 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  store i8 3, ptr %9, align 8, !tbaa !20
  %10 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %5, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null)
  %11 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2)
  %12 = load ptr, ptr %11, align 8, !tbaa !10
  store ptr %1, ptr %12, align 8, !tbaa.struct !14
  %13 = ptrtoint ptr %1 to i64
  store i64 %13, ptr %.dep.arr.addr2, align 8, !tbaa !16
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  store i64 4, ptr %14, align 8, !tbaa !19
  %15 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  store i8 3, ptr %15, align 8, !tbaa !20
  %16 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %11, i32 1, ptr nonnull %.dep.arr.addr2, i32 0, ptr null)
  %17 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4)
  %18 = load ptr, ptr %17, align 8, !tbaa !10
  store ptr %2, ptr %18, align 8, !tbaa.struct !14
  %19 = ptrtoint ptr %2 to i64
  store i64 %19, ptr %.dep.arr.addr5, align 8, !tbaa !16
  %20 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  store i64 4, ptr %20, align 8, !tbaa !19
  %21 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  store i8 3, ptr %21, align 8, !tbaa !20
  %22 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %17, i32 1, ptr nonnull %.dep.arr.addr5, i32 0, ptr null)
  %23 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..6)
  %24 = load ptr, ptr %23, align 8, !tbaa !10
  store ptr %0, ptr %24, align 8, !tbaa.struct !21
  %agg.captured7.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %24, i64 8
  store ptr %1, ptr %agg.captured7.sroa.2.0..sroa_idx, align 8, !tbaa.struct !14
  store i64 %13, ptr %.dep.arr.addr8, align 8, !tbaa !16
  %25 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  store i64 4, ptr %25, align 8, !tbaa !19
  %26 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  store i8 1, ptr %26, align 8, !tbaa !20
  %27 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %23, i32 1, ptr nonnull %.dep.arr.addr8, i32 0, ptr null)
  %28 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..8)
  %29 = load ptr, ptr %28, align 8, !tbaa !10
  store ptr %0, ptr %29, align 8, !tbaa.struct !21
  %agg.captured10.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %29, i64 8
  store ptr %2, ptr %agg.captured10.sroa.2.0..sroa_idx, align 8, !tbaa.struct !14
  store i64 %19, ptr %.dep.arr.addr11, align 8, !tbaa !16
  %30 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  store i64 4, ptr %30, align 8, !tbaa !19
  %31 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  store i8 1, ptr %31, align 8, !tbaa !20
  %32 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %28, i32 1, ptr nonnull %.dep.arr.addr11, i32 0, ptr null)
  %33 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..10)
  %34 = load ptr, ptr %33, align 8, !tbaa !10
  store ptr %0, ptr %34, align 8, !tbaa.struct !14
  store i64 %7, ptr %.dep.arr.addr14, align 8, !tbaa !16
  %35 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  store i64 4, ptr %35, align 8, !tbaa !19
  %36 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  store i8 1, ptr %36, align 8, !tbaa !20
  %37 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %33, i32 1, ptr nonnull %.dep.arr.addr14, i32 0, ptr null)
  call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.parallel
- - - - - - - - - - - - - - - -
[omp-transform] Single Region Found:
  %4 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
[omp-transform] Function after: ; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1, ptr %2) #1 !carts !10 {
pre_entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  %3 = load i32, ptr undef, align 4, !tbaa !6
  br label %entry

entry:                                            ; preds = %pre_entry
  %4 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry.)
  %5 = load ptr, ptr %4, align 8, !tbaa !12
  store ptr %0, ptr %5, align 8, !tbaa.struct !16
  %6 = ptrtoint ptr %0 to i64
  store i64 %6, ptr %.dep.arr.addr, align 8, !tbaa !18
  %7 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  store i64 4, ptr %7, align 8, !tbaa !21
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  store i8 3, ptr %8, align 8, !tbaa !22
  %9 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %4, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null)
  %10 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2)
  %11 = load ptr, ptr %10, align 8, !tbaa !12
  store ptr %1, ptr %11, align 8, !tbaa.struct !16
  %12 = ptrtoint ptr %1 to i64
  store i64 %12, ptr %.dep.arr.addr2, align 8, !tbaa !18
  %13 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  store i64 4, ptr %13, align 8, !tbaa !21
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  store i8 3, ptr %14, align 8, !tbaa !22
  %15 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %10, i32 1, ptr nonnull %.dep.arr.addr2, i32 0, ptr null)
  %16 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4)
  %17 = load ptr, ptr %16, align 8, !tbaa !12
  store ptr %2, ptr %17, align 8, !tbaa.struct !16
  %18 = ptrtoint ptr %2 to i64
  store i64 %18, ptr %.dep.arr.addr5, align 8, !tbaa !18
  %19 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  store i64 4, ptr %19, align 8, !tbaa !21
  %20 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  store i8 3, ptr %20, align 8, !tbaa !22
  %21 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %16, i32 1, ptr nonnull %.dep.arr.addr5, i32 0, ptr null)
  %22 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..6)
  %23 = load ptr, ptr %22, align 8, !tbaa !12
  store ptr %0, ptr %23, align 8, !tbaa.struct !23
  %agg.captured7.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %23, i64 8
  store ptr %1, ptr %agg.captured7.sroa.2.0..sroa_idx, align 8, !tbaa.struct !16
  store i64 %12, ptr %.dep.arr.addr8, align 8, !tbaa !18
  %24 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  store i64 4, ptr %24, align 8, !tbaa !21
  %25 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  store i8 1, ptr %25, align 8, !tbaa !22
  %26 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %22, i32 1, ptr nonnull %.dep.arr.addr8, i32 0, ptr null)
  %27 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..8)
  %28 = load ptr, ptr %27, align 8, !tbaa !12
  store ptr %0, ptr %28, align 8, !tbaa.struct !23
  %agg.captured10.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %28, i64 8
  store ptr %2, ptr %agg.captured10.sroa.2.0..sroa_idx, align 8, !tbaa.struct !16
  store i64 %18, ptr %.dep.arr.addr11, align 8, !tbaa !18
  %29 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  store i64 4, ptr %29, align 8, !tbaa !21
  %30 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  store i8 1, ptr %30, align 8, !tbaa !22
  %31 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %27, i32 1, ptr nonnull %.dep.arr.addr11, i32 0, ptr null)
  %32 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..10)
  %33 = load ptr, ptr %32, align 8, !tbaa !12
  store ptr %0, ptr %33, align 8, !tbaa.struct !16
  store i64 %6, ptr %.dep.arr.addr14, align 8, !tbaa !18
  %34 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  store i64 4, ptr %34, align 8, !tbaa !21
  %35 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  store i8 1, ptr %35, align 8, !tbaa !22
  %36 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %32, i32 1, ptr nonnull %.dep.arr.addr14, i32 0, ptr null)
  call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %4 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry.)
 - BasePointer:   %5 = load ptr, ptr %4, align 8, !tbaa !12
 - Offset: 0
[omp-transform] Processing Task with Deps
  %9 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %4, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %0
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In Out 
[omp-transform] Task Outlined Function: ; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #5 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !12
  tail call void @llvm.experimental.noalias.scope.decl(metadata !24)
  %3 = load ptr, ptr %2, align 8, !tbaa !27, !alias.scope !24
  store i32 0, ptr %3, align 4, !tbaa !6, !noalias !24
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %0 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task(ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task(ptr %0)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0) #1 {
entry:
  %1 = load ptr, ptr undef, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !24)
  %2 = load ptr, ptr undef, align 8, !tbaa !27, !alias.scope !24
  store i32 0, ptr %0, align 4, !tbaa !6, !noalias !24
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !26)
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %7 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2)
 - BasePointer:   %8 = load ptr, ptr %7, align 8, !tbaa !17
 - Offset: 0
[omp-transform] Processing Task with Deps
  %12 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %7, i32 1, ptr nonnull %.dep.arr.addr2, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %1
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In Out 
[omp-transform] Task Outlined Function: ; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..2(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !29)
  %3 = load ptr, ptr %2, align 8, !tbaa !32, !alias.scope !29
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !noalias !29
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %1 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task.1(ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task.1(ptr %1)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.1(ptr %0) #1 {
entry:
  %1 = load ptr, ptr undef, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !29)
  %2 = load ptr, ptr undef, align 8, !tbaa !32, !alias.scope !29
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !29
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task.1
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !29)
[omp-transform] Other Function Found:
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !15

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: _Z16long_computationRi
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: _Z16long_computationRi - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task.1 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %10 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4)
 - BasePointer:   %11 = load ptr, ptr %10, align 8, !tbaa !17
 - Offset: 0
[omp-transform] Processing Task with Deps
  %15 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %10, i32 1, ptr nonnull %.dep.arr.addr5, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %2
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In Out 
[omp-transform] Task Outlined Function: ; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..4(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !32)
  %3 = load ptr, ptr %2, align 8, !tbaa !35, !alias.scope !32
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !noalias !32
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %2 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task.2(ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task.2(ptr %2)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.2(ptr %0) #1 {
entry:
  %1 = load ptr, ptr undef, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !32)
  %2 = load ptr, ptr undef, align 8, !tbaa !35, !alias.scope !32
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !32
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task.2
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !32)
[omp-transform] Other Function Found:
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !15

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: _Z17short_computationRi
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: _Z17short_computationRi - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task.2 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %13 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..6)
 - BasePointer:   %14 = load ptr, ptr %13, align 8, !tbaa !17
 - Offset: 0
 - BasePointer:   %14 = load ptr, ptr %13, align 8, !tbaa !17
 - Offset: 8
[omp-transform] Processing Task with Deps
  %17 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %13, i32 1, ptr nonnull %.dep.arr.addr8, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %1
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In 
[omp-transform] Task Outlined Function: ; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..6(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !35)
  %3 = getelementptr inbounds %struct.anon.4, ptr %2, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !38, !alias.scope !35
  %5 = load i32, ptr %4, align 4, !tbaa !6, !noalias !35
  %6 = load ptr, ptr %2, align 8, !tbaa !40, !alias.scope !35
  %7 = load i32, ptr %6, align 4, !tbaa !6, !noalias !35
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %6, align 4, !tbaa !6, !noalias !35
  %8 = load i32, ptr %4, align 4, !tbaa !6, !noalias !35
  %inc.i = add nsw i32 %8, 1
  store i32 %inc.i, ptr %4, align 4, !tbaa !6, !noalias !35
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 8
- - - - - - - - - - - - - - - - - -
BasePointer:   %4 = load ptr, ptr %3, align 8, !tbaa !16, !alias.scope !13
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %6 = load ptr, ptr %2, align 8, !tbaa !19, !alias.scope !13
[omp-transform] OffsetToValueOF: 8 ->   %4 = load ptr, ptr %3, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %1 -> 8
[omp-transform] ValueToOffsetTD: ptr %0 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task.3(ptr, ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task.3(ptr %0, ptr %1)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.3(ptr %0, ptr %1) #1 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !35)
  %3 = getelementptr inbounds %struct.anon.4, ptr undef, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !38, !alias.scope !35
  %5 = load i32, ptr %1, align 4, !tbaa !6, !noalias !35
  %6 = load ptr, ptr undef, align 8, !tbaa !40, !alias.scope !35
  %7 = load i32, ptr %0, align 4, !tbaa !6, !noalias !35
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !35
  %8 = load i32, ptr %1, align 4, !tbaa !6, !noalias !35
  %inc.i = add nsw i32 %8, 1
  store i32 %inc.i, ptr %1, align 4, !tbaa !6, !noalias !35
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task.3
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !37)
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task.3 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %15 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..8)
 - BasePointer:   %16 = load ptr, ptr %15, align 8, !tbaa !17
 - Offset: 0
 - BasePointer:   %16 = load ptr, ptr %15, align 8, !tbaa !17
 - Offset: 8
[omp-transform] Processing Task with Deps
  %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %15, i32 1, ptr nonnull %.dep.arr.addr11, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %2
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In 
[omp-transform] Task Outlined Function: ; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..8(i32 %0, ptr noalias nocapture noundef readonly %1) #6 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !40)
  %3 = getelementptr inbounds %struct.anon.6, ptr %2, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !43, !alias.scope !40
  %5 = load i32, ptr %4, align 4, !tbaa !6, !noalias !40
  %6 = load ptr, ptr %2, align 8, !tbaa !45, !alias.scope !40
  %7 = load i32, ptr %6, align 4, !tbaa !6, !noalias !40
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %6, align 4, !tbaa !6, !noalias !40
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 8
- - - - - - - - - - - - - - - - - -
BasePointer:   %4 = load ptr, ptr %3, align 8, !tbaa !16, !alias.scope !13
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %6 = load ptr, ptr %2, align 8, !tbaa !19, !alias.scope !13
[omp-transform] OffsetToValueOF: 8 ->   %4 = load ptr, ptr %3, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %2 -> 8
[omp-transform] ValueToOffsetTD: ptr %0 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task.4(ptr, ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task.4(ptr %0, ptr %2)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.4(ptr %0, ptr %1) #1 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !39)
  %3 = getelementptr inbounds %struct.anon.6, ptr undef, i64 0, i32 1
  %4 = load ptr, ptr %3, align 8, !tbaa !42, !alias.scope !39
  %5 = load i32, ptr %1, align 4, !tbaa !6, !noalias !39
  %6 = load ptr, ptr undef, align 8, !tbaa !44, !alias.scope !39
  %7 = load i32, ptr %0, align 4, !tbaa !6, !noalias !39
  %add.i = add nsw i32 %7, %5
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !39
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task.4
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !39)
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task.4 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %17 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..10)
 - BasePointer:   %18 = load ptr, ptr %17, align 8, !tbaa !17
 - Offset: 0
[omp-transform] Processing Task with Deps
  %21 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %17, i32 1, ptr nonnull %.dep.arr.addr14, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 0
BaseAddr: ptr %0
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 1
Len: i64 4
- - - - - - -- - - - - - - - - - - 
Dependency #0
Field #: 2
In 
[omp-transform] Task Outlined Function: ; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..10(i32 %0, ptr noalias nocapture noundef readonly %1) #7 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !17
  tail call void @llvm.experimental.noalias.scope.decl(metadata !42)
  %3 = load ptr, ptr %2, align 8, !tbaa !45, !alias.scope !42
  %4 = load i32, ptr %3, align 4, !tbaa !6, !noalias !42
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4), !noalias !42
  ret i32 0
}

- - - - - - - - - - - - - - - - - -
BasePointer: ptr %1
Offset: 0
- - - - - - - - - - - - - - - - - -
BasePointer:   %2 = load ptr, ptr %1, align 8, !tbaa !6
Offset: 0
[omp-transform] OffsetToValueOF: 0 ->   %3 = load ptr, ptr %2, align 8, !tbaa !16, !alias.scope !13
[omp-transform] ValueToOffsetTD: ptr %0 -> 0
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task.5(ptr)
[arts-ir-builder] New callsite:   call void @carts.edt.task.5(ptr %0)
[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.5(ptr %0) #1 {
entry:
  %1 = load ptr, ptr undef, align 8, !tbaa !36
  tail call void @llvm.experimental.noalias.scope.decl(metadata !40)
  %2 = load ptr, ptr undef, align 8, !tbaa !43, !alias.scope !40
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !40
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %3), !noalias !40
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task.5
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !40)
[omp-transform] Other Function Found:
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %3), !noalias !15
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.task.5 - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Single End Region Found:
  call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.parallel - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.sync.done
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.sync.done - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Removing Values and Dead Instructions
[omp-transform] Processing function: carts.edt.main - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'taskwithdeps.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }

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
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1, ptr %2) #1 !carts !10 {
pre_entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  br label %entry

entry:                                            ; preds = %pre_entry
  call void @carts.edt.task(ptr %0) #8
  %3 = ptrtoint ptr %0 to i64
  store i64 %3, ptr %.dep.arr.addr, align 8, !tbaa !12
  %4 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  store i64 4, ptr %4, align 8, !tbaa !15
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  store i8 3, ptr %5, align 8, !tbaa !16
  call void @carts.edt.task.1(ptr %1) #8
  %6 = ptrtoint ptr %1 to i64
  store i64 %6, ptr %.dep.arr.addr2, align 8, !tbaa !12
  %7 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  store i64 4, ptr %7, align 8, !tbaa !15
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  store i8 3, ptr %8, align 8, !tbaa !16
  call void @carts.edt.task.2(ptr %2) #8
  %9 = ptrtoint ptr %2 to i64
  store i64 %9, ptr %.dep.arr.addr5, align 8, !tbaa !12
  %10 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  store i64 4, ptr %10, align 8, !tbaa !15
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  store i8 3, ptr %11, align 8, !tbaa !16
  call void @carts.edt.task.3(ptr %0, ptr %1) #8
  store i64 %6, ptr %.dep.arr.addr8, align 8, !tbaa !12
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  store i64 4, ptr %12, align 8, !tbaa !15
  %13 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  store i8 1, ptr %13, align 8, !tbaa !16
  call void @carts.edt.task.4(ptr %0, ptr %2) #8
  store i64 %9, ptr %.dep.arr.addr11, align 8, !tbaa !12
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  store i64 4, ptr %14, align 8, !tbaa !15
  %15 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  store i8 1, ptr %15, align 8, !tbaa !16
  call void @carts.edt.task.5(ptr %0) #8
  store i64 %3, ptr %.dep.arr.addr14, align 8, !tbaa !12
  %16 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  store i64 4, ptr %16, align 8, !tbaa !15
  %17 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  store i8 1, ptr %17, align 8, !tbaa !16
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0) #1 !carts !17 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !19)
  store i32 0, ptr %0, align 4, !tbaa !6, !noalias !19
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.1(ptr %0) #1 !carts !17 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !22)
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !22
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.2(ptr %0) #1 !carts !17 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %0), !noalias !25
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.3(ptr %0, ptr %1) #1 !carts !28 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !30)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !30
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !30
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !30
  %4 = load i32, ptr %1, align 4, !tbaa !6, !noalias !30
  %inc.i = add nsw i32 %4, 1
  store i32 %inc.i, ptr %1, align 4, !tbaa !6, !noalias !30
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.4(ptr %0, ptr %1) #1 !carts !28 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !33)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !33
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !33
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !33
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #6

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task.5(ptr %0) #1 !carts !17 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !36)
  %1 = load i32, ptr %0, align 4, !tbaa !6, !noalias !36
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %1), !noalias !36
  ret void
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !39 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

define internal void @carts.edt.main() !carts !41 {
entry:
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %res = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #5
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #5
  %call1 = tail call i32 @rand() #5
  store i32 %call1, ptr %x, align 4, !tbaa !6
  %call2 = tail call i32 @rand() #5
  store i32 %call2, ptr %y, align 4, !tbaa !6
  store i32 0, ptr %res, align 4, !tbaa !6
  call void @carts.edt.parallel(ptr %res, ptr %x, ptr %y) #8
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.sync.done()
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

define internal void @carts.edt.sync.done() !carts !43 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { convergent nounwind }
attributes #5 = { nounwind }
attributes #6 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #8 = { memory(argmem: readwrite) }

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
!10 = !{!"sync", !11}
!11 = !{!"dep", !"dep", !"dep"}
!12 = !{!13, !14, i64 0}
!13 = !{!"_ZTS15kmp_depend_info", !14, i64 0, !14, i64 8, !8, i64 16}
!14 = !{!"long", !8, i64 0}
!15 = !{!13, !14, i64 8}
!16 = !{!13, !8, i64 16}
!17 = !{!"task", !18}
!18 = !{!"dep"}
!19 = !{!20}
!20 = distinct !{!20, !21, !".omp_outlined.: %__context"}
!21 = distinct !{!21, !".omp_outlined."}
!22 = !{!23}
!23 = distinct !{!23, !24, !".omp_outlined..1: %__context"}
!24 = distinct !{!24, !".omp_outlined..1"}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..3: %__context"}
!27 = distinct !{!27, !".omp_outlined..3"}
!28 = !{!"task", !29}
!29 = !{!"dep", !"dep"}
!30 = !{!31}
!31 = distinct !{!31, !32, !".omp_outlined..5: %__context"}
!32 = distinct !{!32, !".omp_outlined..5"}
!33 = !{!34}
!34 = distinct !{!34, !35, !".omp_outlined..7: %__context"}
!35 = distinct !{!35, !".omp_outlined..7"}
!36 = !{!37}
!37 = distinct !{!37, !38, !".omp_outlined..9: %__context"}
!38 = distinct !{!38, !".omp_outlined..9"}
!39 = !{!40}
!40 = !{i64 2, i64 -1, i64 -1, i1 true}
!41 = !{!"main", !42}
!42 = !{}
!43 = !{!"task", !42}


-------------------------------------------------
[omp-transform] [Attributor] Done with 12 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'taskwithdeps.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }

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
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #3

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.parallel(ptr nocapture %0, ptr nocapture %1, ptr nocapture %2) #4 !carts !10 {
pre_entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr11 = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr14 = alloca [1 x %struct.kmp_depend_info], align 8
  br label %entry

entry:                                            ; preds = %pre_entry
  call void @carts.edt.task(ptr nocapture writeonly %0) #13
  %3 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  %4 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  call void @carts.edt.task.1(ptr nocapture %1) #13
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  call void @carts.edt.task.2(ptr nocapture %2) #13
  %7 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 1
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5, i64 0, i32 2
  call void @carts.edt.task.3(ptr nocapture %0, ptr nocapture %1) #13
  %9 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 1
  %10 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8, i64 0, i32 2
  call void @carts.edt.task.4(ptr nocapture %0, ptr nocapture readonly %2) #13
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 1
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr11, i64 0, i32 2
  call void @carts.edt.task.5(ptr nocapture %0) #13
  %13 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 1
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr14, i64 0, i32 2
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task(ptr nocapture writeonly %0) #6 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  store i32 0, ptr %0, align 4, !tbaa !6, !noalias !14
  ret void
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.1(ptr nocapture %0) #8 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  tail call void @_Z16long_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0) #13, !noalias !17
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.2(ptr nocapture %0) #8 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
  tail call void @_Z17short_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0) #13, !noalias !20
  ret void
}

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.3(ptr nocapture %0, ptr nocapture %1) #6 !carts !23 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !25
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !25
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !25
  %4 = load i32, ptr %1, align 4, !tbaa !6, !noalias !25
  %inc.i = add nsw i32 %4, 1
  store i32 %inc.i, ptr %1, align 4, !tbaa !6, !noalias !25
  ret void
}

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite)
define internal void @carts.edt.task.4(ptr nocapture %0, ptr nocapture readonly %1) #6 !carts !23 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !28)
  %2 = load i32, ptr %1, align 4, !tbaa !6, !noalias !28
  %3 = load i32, ptr %0, align 4, !tbaa !6, !noalias !28
  %add.i = add nsw i32 %3, %2
  store i32 %add.i, ptr %0, align 4, !tbaa !6, !noalias !28
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #9

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.task.5(ptr nocapture readonly %0) #4 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !31)
  %1 = load i32, ptr %0, align 4, !tbaa !6, !noalias !31
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %1) #7, !noalias !31
  ret void
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare !callback !34 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #10

; Function Attrs: norecurse nounwind
define internal void @carts.edt.main() #11 !carts !36 {
entry:
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %res = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #7
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #7
  %call1 = tail call i32 @rand() #7
  store i32 %call1, ptr %x, align 4, !tbaa !6
  %call2 = tail call i32 @rand() #7
  store i32 %call2, ptr %y, align 4, !tbaa !6
  store i32 0, ptr %res, align 4, !tbaa !6
  call void @carts.edt.parallel(ptr nocapture %res, ptr nocapture %x, ptr nocapture %y) #13
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: nounwind memory(none)
define internal void @carts.edt.sync.done() #12 !carts !38 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #5 = { convergent nounwind }
attributes #6 = { nocallback nofree norecurse nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) }
attributes #7 = { nounwind }
attributes #8 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite, inaccessiblemem: readwrite) }
attributes #9 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #11 = { norecurse nounwind }
attributes #12 = { nounwind memory(none) }
attributes #13 = { nounwind memory(readwrite) }

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
!10 = !{!"sync", !11}
!11 = !{!"dep", !"dep", !"dep"}
!12 = !{!"task", !13}
!13 = !{!"dep"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..1: %__context"}
!19 = distinct !{!19, !".omp_outlined..1"}
!20 = !{!21}
!21 = distinct !{!21, !22, !".omp_outlined..3: %__context"}
!22 = distinct !{!22, !".omp_outlined..3"}
!23 = !{!"task", !24}
!24 = !{!"dep", !"dep"}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..5: %__context"}
!27 = distinct !{!27, !".omp_outlined..5"}
!28 = !{!29}
!29 = distinct !{!29, !30, !".omp_outlined..7: %__context"}
!30 = distinct !{!30, !".omp_outlined..7"}
!31 = !{!32}
!32 = distinct !{!32, !33, !".omp_outlined..9: %__context"}
!33 = distinct !{!33, !".omp_outlined..9"}
!34 = !{!35}
!35 = !{i64 2, i64 -1, i64 -1, i1 true}
!36 = !{!"main", !37}
!37 = !{}
!38 = !{!"task", !37}


-------------------------------------------------
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis taskwithdeps_arts_ir.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" taskwithdeps_arts_ir.bc -o taskwithdeps_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
taskwithdeps_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Initializing ARTS Cache
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
 - Analyzing Functions
-- Function: _Z16long_computationRi
[arts] Function: _Z16long_computationRi doesn't have CARTS Metadata

-- Function: _Z17short_computationRi
[arts] Function: _Z17short_computationRi doesn't have CARTS Metadata

-- Function: main
[arts] Function: main doesn't have CARTS Metadata

-- Function: carts.edt.parallel
[arts] Function: carts.edt.parallel has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt.parallel
   - DepV: ptr %0
   - DepV: ptr %1
   - DepV: ptr %2
[arts] Creating Task EDT for function: carts.edt.parallel
[arts] Creating Sync EDT for function: carts.edt.parallel

-- Function: carts.edt.task
[arts] Function: carts.edt.task has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt.task
   - DepV: ptr %0
[arts] Creating Task EDT for function: carts.edt.task

-- Function: carts.edt.task.1
[arts] Function: carts.edt.task.1 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.task.1
   - DepV: ptr %0
[arts] Creating Task EDT for function: carts.edt.task.1

-- Function: carts.edt.task.2
[arts] Function: carts.edt.task.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.task.2
   - DepV: ptr %0
[arts] Creating Task EDT for function: carts.edt.task.2

-- Function: carts.edt.task.3
[arts] Function: carts.edt.task.3 has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.task.3
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.task.3

-- Function: carts.edt.task.4
[arts] Function: carts.edt.task.4 has CARTS Metadata
[arts] Creating EDT #5 for function: carts.edt.task.4
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.task.4

-- Function: carts.edt.task.5
[arts] Function: carts.edt.task.5 has CARTS Metadata
[arts] Creating EDT #6 for function: carts.edt.task.5
   - DepV: ptr %0
[arts] Creating Task EDT for function: carts.edt.task.5

-- Function: carts.edt.main
[arts] Function: carts.edt.main has CARTS Metadata
[arts] Creating EDT #7 for function: carts.edt.main
[arts] Creating Task EDT for function: carts.edt.main
[arts] Creating Main EDT for function: carts.edt.main

-- Function: carts.edt.sync.done
[arts] Function: carts.edt.sync.done has CARTS Metadata
[arts] Creating EDT #8 for function: carts.edt.sync.done
[arts] Creating Task EDT for function: carts.edt.sync.done

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt.parallel"
   - Call to EDTFunction:
      call void @carts.edt.parallel(ptr nocapture %res, ptr nocapture %x, ptr nocapture %y) #13
opt: /home/randres/projects/carts/carts/src/analysis/ARTSAnalysisPass.cpp:596: virtual void AAEDTInfoFunction::initialize(Attributor &): Assertion `CB && "Next instruction is not a CallBase!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis taskwithdeps_arts_ir.bc -o taskwithdeps_arts_analysis.bc
 #0 0x00007f14fd3ba6b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x1926b7)
 #1 0x00007f14fd3b821e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x19021e)
 #2 0x00007f14fd3bad8a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f14fcd05520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f14fcd599fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f14fcd05476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f14fcceb7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f14fcceb71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f14fccfce96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00007f14fba45fff (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x17fff)
#10 0x00007f14fba45052 AAEDTInfo const* llvm::Attributor::getOrCreateAAFor<AAEDTInfo>(llvm::IRPosition, llvm::AbstractAttribute const*, llvm::DepClassTy, bool, bool) (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x17052)
#11 0x00007f14fba508e3 llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#12 0x00007f14fd7c0576 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/llvm/lib/libLLVMCore.so.18.1+0x2c9576)
#13 0x000055fcdfe6bacb llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/randres/projects/carts/.install/llvm/bin/opt+0x19acb)
#14 0x000055fcdfe79900 main (/home/randres/projects/carts/.install/llvm/bin/opt+0x27900)
#15 0x00007f14fccecd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#16 0x00007f14fccece40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#17 0x000055fcdfe64ef5 _start (/home/randres/projects/carts/.install/llvm/bin/opt+0x12ef5)
Aborted
make: *** [Makefile:24: taskwithdeps_arts_analysis.bc] Error 134
