clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
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
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 4, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt(ptr, ptr, ptr, ptr)
Rewiring new function arguments:
  - Rewiring: ptr %shared_number -> ptr %3
  - Rewiring: ptr %number -> ptr %2
  - Rewiring: ptr %NewRandom -> ptr %1
  - Rewiring: ptr %random_number -> ptr %0
New callsite:   call void @carts.edt(ptr %random_number, ptr %NewRandom, ptr %number, ptr %shared_number)
[arts-utils]   - Replacing uses of:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 4, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[arts-utils]    - Removing instruction:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 4, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[arts-utils]   - Replacing uses of: ptr %.global_tid.
[arts-utils]    - Replacing:   %4 = load i32, ptr undef, align 4, !tbaa !6
[arts-utils]   - Replacing uses of: ptr %.bound_tid.
[arts-utils]   - Replacing uses of: ptr %random_number
[arts-utils]   - Replacing uses of: ptr %NewRandom
[arts-utils]   - Replacing uses of: ptr %number
[arts-utils]   - Replacing uses of: ptr %shared_number
Current Function after undefining OldCB:
; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, ptr %number, align 4, !tbaa !6
  store i32 10000, ptr %shared_number, align 4, !tbaa !6
  %call = tail call i32 @rand() #5
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, ptr %random_number, align 4, !tbaa !6
  %call1 = tail call i32 @rand() #5
  store i32 %call1, ptr %NewRandom, align 4, !tbaa !6
  call void @carts.edt(ptr %random_number, ptr %NewRandom, ptr %number, ptr %shared_number)
  %0 = load i32, ptr %number, align 4, !tbaa !6
  %1 = load i32, ptr %random_number, align 4, !tbaa !6
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1)
  ret i32 0
}

[arts-ir-builder] New function:
define internal void @carts.edt(ptr %0, ptr %1, ptr %2, ptr %3) {
entry:
  %4 = load i32, ptr undef, align 4, !tbaa !6
  %5 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 16, ptr nonnull @.omp_task_entry.)
  %6 = load ptr, ptr %5, align 8, !tbaa !10
  store ptr %2, ptr %6, align 8, !tbaa.struct !15
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, ptr %6, i64 8
  store ptr %3, ptr %agg.captured.sroa.2.0..sroa_idx, align 8, !tbaa.struct !17
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %5, i64 0, i32 1
  %8 = load i32, ptr %0, align 4, !tbaa !6
  store i32 %8, ptr %7, align 8, !tbaa !18
  %9 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %5, i64 0, i32 1, i32 1
  %10 = load i32, ptr %1, align 4, !tbaa !6
  store i32 %10, ptr %9, align 4, !tbaa !19
  %11 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %5)
  %12 = load i32, ptr %3, align 4, !tbaa !6
  %inc = add nsw i32 %12, 1
  store i32 %inc, ptr %3, align 4, !tbaa !6
  %13 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..4)
  %14 = load ptr, ptr %13, align 8, !tbaa !20
  store ptr %3, ptr %14, align 8, !tbaa.struct !17
  %15 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %13, i64 0, i32 1
  %16 = load i32, ptr %2, align 4, !tbaa !6
  store i32 %16, ptr %15, align 8, !tbaa !23
  %17 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 undef, ptr nonnull %13)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %5 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 16, ptr nonnull @.omp_task_entry.)
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/carts/src/transform/OMPTransform.cpp:267: Instruction *arts::OMPTransform::handleTaskRegion(CallBase &): Assertion `ValueToOffsetTD.size() == OffsetToValueOF.size() && "ValueToOffsetTD and ValueToOffsetOF have different sizes"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so -passes=omp-transform test.bc -o test_arts_ir.bc
 #0 0x00007f6dbed12ef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f6dbed10b7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f6dbed135ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f6dc1a54910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f6dbe61dd2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f6dbe61f3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f6dbe615c6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007f6dbe615cf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007f6dbd2bcb89 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xeb89)
 #9 0x00007f6dbd2bbd61 arts::OMPTransform::identifyEDTs(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xdd61)
#10 0x00007f6dbd2bc25e arts::OMPTransform::handleParallelRegion(llvm::CallBase&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xe25e)
#11 0x00007f6dbd2bbd30 arts::OMPTransform::identifyEDTs(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xdd30)
#12 0x00007f6dbd2bb40c arts::OMPTransform::run(llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xd40c)
#13 0x00007f6dbd2b4f59 OMPTransformPass::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x6f59)
#14 0x00007f6dbd2b87c9 llvm::detail::PassModel<llvm::Module, OMPTransformPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0xa7c9)
#15 0x00007f6dbf1062a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#16 0x0000558d150e4293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#17 0x0000558d150f1aaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#18 0x00007f6dbe60824d __libc_start_main (/lib64/libc.so.6+0x3524d)
#19 0x0000558d150dda3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:16: test_arts_ir.bc] Aborted
