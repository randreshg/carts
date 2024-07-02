clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata\
		-passes="arts-analysis" test_arts_ir.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
[carts-metadata] Function: main has CARTS Metadata
[carts-metadata] Creating Main EDT Metadata
[arts] Creating EDT #0
[arts] Creating Main EDT for function: main
[carts-metadata] Function: carts.edt has CARTS Metadata
[carts-metadata] Creating Parallel EDT Metadata
[arts] Creating EDT #1
[arts] Creating Parallel EDT for function: carts.edt
[carts-metadata] Function: carts.edt.1 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #2
[arts] Creating Task EDT for function: carts.edt.1
[carts-metadata] Function: carts.edt.2 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #3
[arts] Creating Task EDT for function: carts.edt.2
[carts-metadata] Function: carts.edt.3 has CARTS Metadata
[carts-metadata] Creating Task EDT Metadata
[arts] Creating EDT #4
[arts] Creating Task EDT for function: carts.edt.3


[arts-analysis] Initializing AAEDTInfo: 
AAEDTInfoFunction::manifest: <invalid>
- EDT #PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007f4477b4eef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007f4477b4cb7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007f4477b4f5ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f447a890910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f44760ee2f0 arts::EDT::getID() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x142f0)
 #5 0x00007f44760e9e98 arts::operator<<(llvm::raw_ostream&, arts::EDT&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0xfe98)
 #6 0x00007f44760e97d3 AAEDTInfoFunction::manifest(llvm::Attributor&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0xf7d3)
 #7 0x00007f447a2cd3fb llvm::Attributor::manifestAttributes() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x933fb)
 #8 0x00007f447a2d21e6 llvm::Attributor::run() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x981e6)
 #9 0x00007f44760eba55 llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#10 0x00007f4477f422a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#11 0x000055cb2115f293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#12 0x000055cb2116caaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#13 0x00007f447744424d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x000055cb21158a3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Segmentation fault
