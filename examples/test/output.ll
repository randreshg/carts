clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
llvm-dis test_arts_ir.bc
noelle-norm test_arts_ir.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
noelle-load -debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata \
	-load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -ARTSAnalysisPass test_with_metadata.bc -o test_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
test_with_metadata.bc
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
AAEDTInfoFunction::initialize-> EDT #0 for function "main"
AAEDTInfoFunction::initialize-> EDT #1 for function "carts.edt"
AAEDTInfoFunction::initialize-> EDT #2 for function "carts.edt.1"
AAEDTInfoFunction::initialize-> EDT #3 for function "carts.edt.2"
AAEDTInfoFunction::initialize-> EDT #4 for function "carts.edt.3"
AAEDTInfoChild::initialize: EDT #1
AAEDTInfoChild::initialize: EDT #4
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoChild::initialize: EDT #2
AAEDTInfoChild::initialize: EDT #3
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::updateImpl: MemorySSA Analysis
AAEDTInfoFunction::manifest: EDT #0 -> #Reached EDTs: 4{1, 4, 2, 3}, #Child EDTs: 2{1, 4}
- EDT #0: main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0

AAEDTInfoFunction::manifest: EDT #1 -> #Reached EDTs: 2{2, 3}, #Child EDTs: 2{2, 3}
- EDT #1: carts.edt
Ty: parallel
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  - i32* %0
  - i32* %1
  - i32* %2
  - i32* %3

AAEDTInfoFunction::manifest: EDT #2 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #2: carts.edt.1
Ty: task
Data environment for EDT: 
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - i32* %0
  - i32* %1

AAEDTInfoFunction::manifest: EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #3: carts.edt.2
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - i32* %0

AAEDTInfoFunction::manifest: EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #4: carts.edt.3
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - i32* %number
  - i32* %random_number

[Attributor] Done with 5 functions, result: changed.

-------------------------------------------------
[arts-analysis] Process has finished

-------------------------------------------------
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
