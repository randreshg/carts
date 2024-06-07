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
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/include/llvm/Support/Casting.h:104: static bool llvm::isa_impl_cl<llvm::PointerType, const llvm::Type *>::doit(const From *) [To = llvm::PointerType, From = const llvm::Type *]: Assertion `Val && "isa<> used on a null pointer"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph,omp-transform -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so -passes=omp-transform test.bc -o test_arts.bc
 #0 0x00007f3dc843ce13 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a6e13)
 #1 0x00007f3dc843ac0e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a4c0e)
 #2 0x00007f3dc843d2df SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f3dcac64910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f3dc7d34d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007f3dc7d363e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007f3dc7d2cc6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007f3dc7d2ccf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007f3dc6f2bfc6 arts::utils::removeValue(llvm::Value*, llvm::Instruction*, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x13fc6)
 #9 0x00007f3dc6f2c9d5 arts::utils::removeDeadInstructions(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x149d5)
#10 0x00007f3dc6f2ab28 arts::EDTIRBuilder::buildEDT(llvm::CallBase*, llvm::Function*, std::function<void (arts::EDTIRBuilder*, llvm::Function*, llvm::Function*)>) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x12b28)
#11 0x00007f3dc6f1f732 arts::OMPTransform::handleParallelRegion(llvm::CallBase&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x7732)
#12 0x00007f3dc6f1f24b arts::OMPTransform::identifyEDTs(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x724b)
#13 0x00007f3dc6f1ec93 arts::OMPTransform::run(llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x6c93)
#14 0x00007f3dc6f209f4 arts::OMPTransformPass::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x89f4)
#15 0x00007f3dc6f21b79 llvm::detail::PassModel<llvm::Module, arts::OMPTransformPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module> >::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so+0x9b79)
#16 0x00007f3dc8768f94 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module> >::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x275f94)
#17 0x000000000042099f llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::StringRef>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x42099f)
#18 0x0000000000435007 main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x435007)
#19 0x00007f3dc7d1f24d __libc_start_main (/lib64/libc.so.6+0x3524d)
#20 0x000000000041827a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:14: test_arts.bc] Aborted
