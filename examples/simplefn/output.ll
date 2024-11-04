clang++ -fopenmp -O3 -g0 -emit-llvm -c simplefn.cpp -o simplefn.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis simplefn.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" simplefn.bc -o simplefn_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis simplefn_arts_ir.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" simplefn_arts_ir.bc -o simplefn_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
simplefn_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Initializing ARTS Cache
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
 - Analyzing Functions
-- Function: carts.edt
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt

-- Function: carts.edt.task
[arts] Function: carts.edt.task has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt.task
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.task

-- Function: main
[arts] Function: main doesn't have CARTS Metadata

-- Function: carts.edt.parallel
[arts] Function: carts.edt.parallel has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.parallel
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.parallel
[arts] Creating Sync EDT for function: carts.edt.parallel

-- Function: carts.edt.main
[arts] Function: carts.edt.main has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.main
[arts] Creating Task EDT for function: carts.edt.main
[arts] Creating Main EDT for function: carts.edt.main

-- Function: carts.edt.sync.done
[arts] Function: carts.edt.sync.done has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.sync.done
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.sync.done

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt"
   - Call to EDTFunction:
      tail call void @carts.edt(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %1)

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt.task"
   - Call to EDTFunction:
      call void @carts.edt.task(ptr nocapture %shared_number, i32 %1) #11

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.parallel"
   - Call to EDTFunction:
      call void @carts.edt.parallel(ptr nocapture %shared_number, ptr nocapture %random_number) #11
   - DoneEDT: EDT #4

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.main"
   - Call to EDTFunction:
      call void @carts.edt.main()
[arts] Function: main doesn't have CARTS Metadata
   - The ContextEDT is the MainEDT

[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.sync.done"
   - Call to EDTFunction:
      call void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture %random_number) #3

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #1"

[AAEDTInfoFunction::updateImpl] EDT #1
   - All ReachedEDTs are fixed for EDT #1
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #2

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
        - Inserting CreationEdge from "EDT #2" to "EDT #0"

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3
        - Inserting CreationEdge from "EDT #3" to "EDT #2"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #2

[AAEDTInfoCallsite::initialize] EDT #0

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3
   - All ReachedEDTs are fixed for EDT #3

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
   - Creating DepSlot #0 for value: ptr %shared_number
        - Inserting DataBlockEdge from "EDT #0" to "EDT #1" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
   - Creating DepSlot #1 for value: ptr %1
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #1

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #3" to "EDT #2" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #3" to "EDT #2" in Slot #1

[AAEDTInfoCallsite::updateImpl] EDT #3
   - All DataBlocks were fixed for EDT #3

[AAEDTInfoCallsiteArg::updateImpl] ptr %shared_number from EDT #1

[AADataBlockInfoCtxAndVal::initialize] ptr %shared_number from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #0

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #0

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl] ptr %shared_number from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::updateImpl] ptr %shared_number from EDT #1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #0
   - Analyzing DependentChildEDTs on ptr %0 (ptr %shared_number)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #0
   - Analyzing DependentChildEDTs on ptr %1 (ptr %random_number)
        - Number of DependentChildEDTs: 0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 1

[AAEDTInfoFunction::updateImpl] EDT #4
   - All ReachedEDTs are fixed for EDT #4

[AAEDTInfoCallsite::initialize] EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsite::updateImpl] EDT #4

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
        - Inserting DataBlockEdge from "EDT #1" to "EDT #4" in Slot #0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #2
     -ParentSyncEDT: EDT #2
     #Child EDTs: 1{1}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #2
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #2 -> 
     -ParentEDT: EDT #3
     -ParentSyncEDT: <null>
     #Child EDTs: 1{0}
     #Reached DescendantEDTs: 1{1}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 1{2}
     #Reached DescendantEDTs: 2{1, 0}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #4 -> 
     -ParentEDT: EDT #3
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

opt: /home/randres/projects/carts/carts/src/analysis/graph/ARTSGraph.cpp:240: bool arts::ARTSGraph::insertCreationEdgeGuid(EDT *, EDT *, EDT *): Assertion `FromParent && "The parent of the EDT must exist"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis simplefn_arts_ir.bc -o simplefn_arts_analysis.bc
 #0 0x00007f63c045a6b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x1926b7)
 #1 0x00007f63c045821e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x19021e)
 #2 0x00007f63c045ad8a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f63bfda5520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f63bfdf99fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f63bfda5476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f63bfd8b7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f63bfd8b71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f63bfd9ce96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00007f63beb0cbe3 (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x3ebe3)
#10 0x00007f63beb0c99f arts::ARTSGraph::insertCreationEdgeGuid(arts::EDT*, arts::EDT*, arts::EDT*) (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x3e99f)
#11 0x00007f63beb0c99f arts::ARTSGraph::insertCreationEdgeGuid(arts::EDT*, arts::EDT*, arts::EDT*) (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x3e99f)
#12 0x00007f63beae4ac0 AAEDTInfoCallsite::manifest(llvm::Attributor&) (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x16ac0)
#13 0x00007f63c2c8622e llvm::Attributor::manifestAttributes() (/home/randres/projects/carts/.install/llvm/lib/libLLVMipo.so.18.1+0x9422e)
#14 0x00007f63c2c8af48 llvm::Attributor::run() (/home/randres/projects/carts/.install/llvm/lib/libLLVMipo.so.18.1+0x98f48)
#15 0x00007f63beaf0959 llvm::detail::PassModel<llvm::Module, (anonymous namespace)::ARTSAnalysisPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) ARTSAnalysisPass.cpp:0:0
#16 0x00007f63c0860576 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/llvm/lib/libLLVMCore.so.18.1+0x2c9576)
#17 0x0000564e1d5e8acb llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/randres/projects/carts/.install/llvm/bin/opt+0x19acb)
#18 0x0000564e1d5f6900 main (/home/randres/projects/carts/.install/llvm/bin/opt+0x27900)
#19 0x00007f63bfd8cd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#20 0x00007f63bfd8ce40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#21 0x0000564e1d5e1ef5 _start (/home/randres/projects/carts/.install/llvm/bin/opt+0x12ef5)
Aborted
make: *** [Makefile:23: simplefn_arts_analysis.bc] Error 134
