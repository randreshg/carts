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
[omp-transform] Processing Task with Deps
  %9 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 undef, ptr nonnull %4, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null)
[omp-transform] NumDeps: 1
[omp-transform] FieldSize: 8
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
[omp-transform] ValueToOffsetTD: i64 4 -> 8
[omp-transform] ValueToOffsetTD:   %6 = ptrtoint ptr %0 to i64 -> 0
[omp-transform] ValueToOffsetTD: i8 3 -> 16
opt: /home/randres/projects/carts/carts/src/transform/OMPTransform.cpp:358: Instruction *arts::OMPTransform::handleTaskRegion(CallBase &): Assertion `ValueToOffsetTD.size() == OffsetToValueOF.size() && "ValueToOffsetTD and ValueToOffsetOF have different sizes"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so -debug-only=omp-transform,arts,carts,arts-ir-builder -passes=omp-transform taskwithdeps.bc -o taskwithdeps_arts_ir.bc
 #0 0x00007ff63d7ba6b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x1926b7)
 #1 0x00007ff63d7b821e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x19021e)
 #2 0x00007ff63d7bad8a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007ff63d105520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007ff63d1599fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007ff63d105476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007ff63d0eb7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007ff63d0eb71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007ff63d0fce96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00007ff63be77ff0 arts::OMPTransform::handleTaskWait(llvm::CallBase&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0x11ff0)
#10 0x00007ff63be76833 arts::OMPTransform::identifyEDTs(llvm::Function&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0x10833)
#11 0x00007ff63be76df0 arts::OMPTransform::handleParallelRegion(llvm::CallBase&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0x10df0)
#12 0x00007ff63be767e9 arts::OMPTransform::identifyEDTs(llvm::Function&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0x107e9)
#13 0x00007ff63be755e9 arts::OMPTransform::run(llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0xf5e9)
#14 0x00007ff63be6f179 OMPTransformPass::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0x9179)
#15 0x00007ff63be72969 llvm::detail::PassModel<llvm::Module, OMPTransformPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/carts/lib/OMPTransform.so+0xc969)
#16 0x00007ff63dbc0576 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/llvm/lib/libLLVMCore.so.18.1+0x2c9576)
#17 0x0000559c30a8eacb llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/randres/projects/carts/.install/llvm/bin/opt+0x19acb)
#18 0x0000559c30a9c900 main (/home/randres/projects/carts/.install/llvm/bin/opt+0x27900)
#19 0x00007ff63d0ecd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#20 0x00007ff63d0ece40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#21 0x0000559c30a87ef5 _start (/home/randres/projects/carts/.install/llvm/bin/opt+0x12ef5)
Aborted
make: *** [Makefile:16: taskwithdeps_arts_ir.bc] Error 134
