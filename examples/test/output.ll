clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen,arts-utils\
		-passes="arts-analysis" test_arts_ir.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized


[Attributor] Initializing AAEDTInfo: 
[arts] Function: main doesn't have CARTS Metadata
[arts] Function: carts.edt.1 has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt.1
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.1
[arts] Creating Sync EDT for function: carts.edt.1
[arts] Creating Parallel EDT for function: carts.edt.1

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt.1"
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt
[arts] Creating Task EDT for function: carts.edt
[arts] Creating Main EDT for function: carts.edt
[arts] Function: carts.edt.3 has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.3
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.3
   - DoneEDT: EDT #2
[arts] Function: carts.edt.2 has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.2
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.2

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.2"

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt"
[arts] Function: main doesn't have CARTS Metadata

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.3"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #3 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #3"

[AAEDTInfoFunction::updateImpl] EDT #3
   - All ReachedEDTs are fixed for EDT #3
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #0
opt: /home/rherreraguaitero/ME/ARTS-env/CARTS/carts/src/analysis/graph/ARTSGraph.cpp:223: bool arts::ARTSGraph::insertCreationEdgeGuid(EDT *, EDT *, EDT *): Assertion `Edge != nullptr && "The edge doesn't exist"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen,arts-utils -passes=arts-analysis test_arts_ir.bc -o test_arts_analysis.bc
 #0 0x00007fa299447ef8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x191ef8)
 #1 0x00007fa299445b7e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.18.1+0x18fb7e)
 #2 0x00007fa2994485ad SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fa29c189910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007fa298d52d2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007fa298d543e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007fa298d4ac6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007fa298d4acf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007fa2979dae24 arts::ARTSGraph::insertCreationEdgeGuid(arts::EDT*, arts::EDT*, arts::EDT*) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x36e24)
 #9 0x00007fa2979dabff arts::ARTSGraph::insertCreationEdgeGuid(arts::EDT*, arts::EDT*, arts::EDT*) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x36bff)
#10 0x00007fa2979b8491 AAEDTInfoCallsite::initialize(llvm::Attributor&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x14491)
#11 0x00007fa2979b8b42 AAEDTInfo const* llvm::Attributor::getOrCreateAAFor<AAEDTInfo>(llvm::IRPosition, llvm::AbstractAttribute const*, llvm::DepClassTy, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x14b42)
#12 0x00007fa2979b9d7f AAEDTInfoFunction::updateImpl(llvm::Attributor&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so+0x15d7f)
#13 0x00007fa29bbba342 llvm::AbstractAttribute::update(llvm::Attributor&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x87342)
#14 0x00007fa29bbc5c31 llvm::Attributor::updateAA(llvm::AbstractAttribute&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x92c31)
#15 0x00007fa29bbc4e06 llvm::Attributor::runTillFixpoint() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x91e06)
#16 0x00007fa29bbcb1dd llvm::Attributor::run() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMipo.so.18.1+0x981dd)
#17 0x00007fa2979c2a84 llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#18 0x00007fa29983b2a6 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.18.1+0x2c02a6)
#19 0x0000557e6e6ec293 llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x19293)
#20 0x0000557e6e6f9aaa main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x26aaa)
#21 0x00007fa298d3d24d __libc_start_main (/lib64/libc.so.6+0x3524d)
#22 0x0000557e6e6e5a3a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
make: *** [Makefile:23: test_arts_analysis.bc] Aborted
