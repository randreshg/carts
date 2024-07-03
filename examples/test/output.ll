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
AAEDTInfoFunction::initialize-> EDT #0 for function "main"
AAEDTInfoFunction::initialize-> EDT #1 for function "carts.edt"
AAEDTInfoFunction::initialize-> EDT #2 for function "carts.edt.1"
AAEDTInfoFunction::initialize-> EDT #3 for function "carts.edt.2"
AAEDTInfoFunction::initialize-> EDT #4 for function "carts.edt.3"
AAEDTInfoFunction::updateImpl-> EDT #0
AAEDTInfoChild::initialize: EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is a child of EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is reached from EDT #0
AAEDTInfoChild::initialize: EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is a child of EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is reached from EDT #0
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoChild::initialize: EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoChild::initialize: EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::updateImpl-> EDT #2
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
AAEDTInfoFunction::updateImpl-> EDT #3
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
AAEDTInfoFunction::updateImpl-> EDT #4
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTDataBlockInfoValue::initialize: CallArg#0 from:
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #5
AAEDTDataBlockInfoValue::initialize: CallArg#1 from:
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #5
AAEDTDataBlockInfoValue::initialize: CallArg#2 from:
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #5
AAEDTDataBlockInfoValue::initialize: CallArg#3 from:
  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #5
AAEDTDataBlockInfoValue::initialize: CallArg#0 from:
  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #5
AAEDTDataBlockInfoValue::initialize: CallArg#1 from:
  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #5
AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::manifest: EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 1{0}
- EDT #0: main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0

AAEDTInfoFunction::manifest: EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}
- EDT #1: carts.edt
Ty: parallel
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 4
  - ptr %0
  - ptr %1
  - ptr %2
  - ptr %3

AAEDTInfoFunction::manifest: EDT #2 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #2: carts.edt.1
Ty: task
Data environment for EDT: 
Number of ParamV: 2
  - i32 %2
  - i32 %3
Number of DepV: 2
  - ptr %0
  - ptr %1

AAEDTInfoFunction::manifest: EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #3: carts.edt.2
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0

AAEDTInfoFunction::manifest: EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #4: carts.edt.3
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %number
  - ptr %random_number

[Attributor] Done with 5 functions, result: unchanged.

-------------------------------------------------
[arts-analysis] Process has finished

-------------------------------------------------
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
