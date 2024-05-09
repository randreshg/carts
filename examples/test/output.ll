clang++ -fopenmp -O3 -g0 -Xclang -disable-llvm-passes -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
noelle-norm test.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc

 ---------------------------------------- 
[carts] Running CARTS on Module: 
test_with_metadata.bc
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized

[arts-analyzer] Identifying EDTs for: main
[arts-analyzer] Other Function Found: 
    %call = call i32 @rand() #8, !noelle.pdg.inst.id !20
[arts-analyzer] Other Function Found: 
    %call1 = call i32 @rand() #8, !noelle.pdg.inst.id !8
[arts-analyzer] Parallel Region Found: 
    call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number), !noelle.pdg.inst.id !15
[arts-analyzer] Outlined function: .omp_outlined.
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -enable-new-pm=0 -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libSCAFUtilities.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libMemoryAnalysisModules.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/Noelle.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/MetadataCleaner.so --disable-basic-aa -globals-aa -cfl-steens-aa -tbaa -scev-aa -cfl-anders-aa --objc-arc-aa -scalar-evolution -loops -domtree -postdomtree -basic-loop-aa -scev-loop-aa -auto-restrict-aa -intrinsic-aa -global-malloc-aa -pure-fun-aa -semi-local-fun-aa -phi-maze-aa -no-capture-global-aa -no-capture-src-aa -type-aa -no-escape-fields-aa -acyclic-aa -disjoint-fields-aa -field-malloc-aa -loop-variant-allocation-aa -std-in-out-err-aa -array-of-structures-aa -kill-flow-aa -callsite-depth-combinator-aa -unique-access-paths-aa -llvm-aa-results -noelle-scaf -debug-only=arts,carts,arts-analyzer,arts-ir-builder -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
1.	Running pass 'Compiler for ARTS' on module 'test_with_metadata.bc'.
 #0 0x00007f9f58625e13 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a6e13)
 #1 0x00007f9f58623c0e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a4c0e)
 #2 0x00007f9f586262df SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f9f5ae4d910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007f9f56347430 arts::ARTSIRBuilder::insertEDTEntry(arts::EDT&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so+0x8430)
 #5 0x00007f9f56345014 arts::ARTSAnalyzer::handleParallelRegion(llvm::CallBase*) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so+0x6014)
 #6 0x00007f9f56344a5c arts::ARTSAnalyzer::identifyEDTs(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so+0x5a5c)
 #7 0x00007f9f56343c82 (anonymous namespace)::CARTS::runOnModule(llvm::Module&) CARTS.cpp:0:0
 #8 0x00007f9f5891568c llvm::legacy::PassManagerImpl::run(llvm::Module&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x23968c)
 #9 0x0000000000434c23 main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x434c23)
#10 0x00007f9f57f0824d __libc_start_main (/lib64/libc.so.6+0x3524d)
#11 0x000000000041827a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/bin/n-eval: line 7: 44465 Segmentation fault      $@
error: noelle-load: line 5
make: *** [Makefile:22: test_opt.bc] Error 1
