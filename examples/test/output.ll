clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
	-passes="omp-transform" test.bc -o test_arts_ir.bc
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis test_arts_ir.bc
opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,attributor\
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
[Attributor] Identified and initialized 5 abstract attributes.


[Attributor] #Iteration: 1, Worklist size: 5
[Attributor] #Iteration: 1, Worklist+Dependent size: 5
[Attributor] Update: [AAIsDead] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state Live[#BB 1/3][#TBEP 1][#KDE 0]

[AAIsDead] Live [1/3] BBs and 1 exploration points and 0 known dead ends
[AAIsDead] Exploration inst:   %number = alloca i32, align 4
[Attributor] Update: [AANoReturn] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %rem = srem i32 %call, 10
[Attributor] Update: [AANoReturn] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7
[Attributor] Update: [AANoReturn] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state noreturn

[Attributor] Call site callback failed for   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state Live[#BB 1/1][#TBEP 1][#KDE 0]

[AAIsDead] Live [1/1] BBs and 1 exploration points and 0 known dead ends
[AAIsDead] Exploration inst:   %4 = load i32, ptr %0, align 4, !tbaa !8
[Attributor] Update: [AANoReturn] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state noreturn

[Attributor] Call site callback failed for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state Live[#BB 1/1][#TBEP 1][#KDE 0]

[AAIsDead] Live [1/1] BBs and 1 exploration points and 0 known dead ends
[AAIsDead] Exploration inst:   tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
[Attributor] Update: [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AANoReturn] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAIsDead] #AliveSuccessors: 0 UsedAssumedInformation: 0
[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state Live[#BB 1/1][#TBEP 0][#KDE 1]

[Attributor] Update: [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state may-return

[Attributor] Update changed [AANoReturn] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %6 = load i32, ptr %3, align 4, !tbaa !8
[Attributor] Update: [AANoReturn] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state noreturn

[Attributor] Call site callback failed for   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state Live[#BB 1/1][#TBEP 1][#KDE 0]

[AAIsDead] Live [1/1] BBs and 1 exploration points and 0 known dead ends
[AAIsDead] Exploration inst:   tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
[Attributor] Update: [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AANoReturn] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   ret void
[AAIsDead] #AliveSuccessors: 0 UsedAssumedInformation: 0
[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state Live[#BB 1/1][#TBEP 0][#KDE 1]

[Attributor] Update: [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state may-return

[Attributor] Update changed [AANoReturn] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   ret void
[AAIsDead] #AliveSuccessors: 0 UsedAssumedInformation: 0
[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state Live[#BB 1/1][#TBEP 0][#KDE 1]

[Attributor] Update: [AANoReturn] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state may-return

[Attributor] Update changed [AANoReturn] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   br label %codeRepl
[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
[Attributor] Update: [AANoReturn] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state noreturn

[Attributor] Call site callback failed for   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
[Attributor] Update: [AAIsDead] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state Live[#BB 1/3][#TBEP 1][#KDE 0]

[AAIsDead] Live [1/3] BBs and 1 exploration points and 0 known dead ends
[AAIsDead] Exploration inst:   br label %entry.split
[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   %0 = load i32, ptr %number, align 4, !tbaa !8
[Attributor] Update: [AANoReturn] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   br label %entry.split.ret.exitStub
[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   ret void
[AAIsDead] #AliveSuccessors: 0 UsedAssumedInformation: 0
[Attributor] Update changed [AAIsDead] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state Live[#BB 3/3][#TBEP 0][#KDE 1]

[Attributor] Update: [AANoReturn] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state noreturn

[Attributor] Update changed [AANoReturn] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state may-return

[Attributor] Update changed [AANoReturn] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state may-return

[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   br label %entry.split.ret
[AAIsDead] #AliveSuccessors: 1 UsedAssumedInformation: 0
[AAIsDead] Exploration inst:   ret i32 0
[AAIsDead] #AliveSuccessors: 0 UsedAssumedInformation: 0
[Attributor] Update changed [AAIsDead] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state Live[#BB 3/3][#TBEP 0][#KDE 1]

[Attributor] Update: [AAEDTInfo] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state EDT #0 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl-> EDT #0
[Attributor] Update: [AAMemoryBehavior] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state all memory

[Attributor] Update: [AAMemoryBehavior] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state all memory

[Attributor] Update: [AAMemoryBehavior] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state readnone

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Update: [AAInstanceInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state <unique [fAa]>

[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %random_number = alloca i32, align 4
[Attributor] Update: [AAPointerInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state PointerInfo #0 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   store i32 %add, ptr %random_number, align 4, !tbaa !7
[Attributor] Update: [AAPotentialValues] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %call = tail call i32 @rand() #5' at position {cs_ret:call [call@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %call = tail call i32 @rand() #5' at position {cs_ret:call [call@-1]} with state set-state(< {full-set} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state set-state(< {  %rem = srem i32 %call, 10[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state set-state(< {  %add = add nsw i32 %rem, 10[3], } >)

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state PointerInfo #0 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 1
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !8
Accesses by bin after update:
[0-4] : 1
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state PointerInfo #0 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %random_number in   %1 = load i32, ptr %random_number, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 1
     - 5 -   %1 = load i32, ptr %random_number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state PointerInfo #1 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %random_number in   %1 = load i32, ptr %random_number, align 4, !tbaa !8
Accesses by bin after update:
[0-4] : 1
     - 5 -   %1 = load i32, ptr %random_number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 3
     - 9 -   store i32 %add, ptr %random_number, align 4, !tbaa !7
       - c:   %add = add nsw i32 %rem, 10
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %1 = load i32, ptr %random_number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   store i32 %add, ptr %random_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %random_number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 3
     - 9 -   store i32 %add, ptr %random_number, align 4, !tbaa !7
       - c:   %add = add nsw i32 %rem, 10
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %1 = load i32, ptr %random_number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[Attributor] Update: [AAMemoryBehavior] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state readnone

[Attributor] Update: [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 1, ptr %number, align 4, !tbaa !7 (only exact: 0)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %number = alloca i32, align 4
[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #0 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #0 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #0 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Update: [AAInstanceInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state <unique [fAa]>

[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %2[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %2[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %2[1], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state set-state(< {ptr %2[1],   %number = alloca i32, align 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state set-state(< {ptr %2[1],   %number = alloca i32, align 4[2], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state set-state(< {ptr %2[1],   %number = alloca i32, align 4[2], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %number = alloca i32, align 4
[Attributor] Update: [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Update: [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {fn_ret:main [main@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {fn_ret:main [main@-1]} with state set-state(< {i32 0[2], i32 0[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {fn_ret:main [main@-1]} with state set-state(< {i32 0[2], i32 0[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {fn_ret:main [main@-1]} with state set-state(< {i32 0[2], i32 0[1], } >)

[Attributor] Update unchanged [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state PointerInfo #0 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %number in   %0 = load i32, ptr %number, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 1
     - 5 -   %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state PointerInfo #1 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %number in   %0 = load i32, ptr %number, align 4, !tbaa !8
Accesses by bin after update:
[0-4] : 1
     - 5 -   %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AANoSync] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state nosync

[Attributor] Update: [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 10000, ptr %shared_number, align 4, !tbaa !7 (only exact: 0)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state set-state(< {  %shared_number = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %shared_number = alloca i32, align 4
[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #0 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #0 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[Attributor] Update: [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[3], } >)

[Attributor] Update: [AAInstanceInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state <unique [fAa]>

[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %3[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %3[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %3[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %shared_number = alloca i32, align 4
[Attributor] Update: [AANoCapture] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state assumed not-captured

[Attributor] Update unchanged [AANoCapture] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state assumed not-captured

[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #0 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state PointerInfo #0 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 1
     - 5 -   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 1
     - 5 -   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[Attributor] Update: [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(0)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(0)

[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[Attributor] Update: [AAInterFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(0)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(0)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(0)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(0)

[Attributor] Update: [AACallEdges] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: rand
[Attributor] Update changed [AACallEdges] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state CallEdges[0,1]

[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(0)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(0)

[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(0)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(0)

[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update unchanged [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update unchanged [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AANoSync] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state nosync

[Attributor] Update changed [AANoSync] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state may-sync

[Attributor] Update changed [AANoSync] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state may-sync

[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update: [AAIntraFnReachability] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state #queries(0)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state #queries(0)

[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update unchanged [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %number[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %number[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %number[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %number = alloca i32, align 4
[Attributor] Update: [AANoSync] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state nosync

[Attributor] Update: [AAIsDead] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %1 = load i32, ptr %random_number, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %random_number[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %random_number[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %random_number[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ToFn]
[Attributor] Update: [AACallEdges] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: rand
[Attributor] Update changed [AACallEdges] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state CallEdges[0,1]

[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update: [AAInstanceInfo] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state <unique [fAa]>

[AAValueConstantRange] We give up:   %1 = load i32, ptr %random_number, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state range(32)<[1,20) / empty-set>

[Attributor] Update: [AAValueConstantRange] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state range(32)<[-9,10) / empty-set>

[Attributor] Update: [AAValueConstantRange] for CtxI '  %call = tail call i32 @rand() #5' at position {cs_ret:call [call@-1]} with state range(32)<full-set / empty-set>

[Attributor] Introducing call base context:  %call = tail call i32 @rand() #5
[Attributor] Update changed [AAValueConstantRange] for CtxI '  %call = tail call i32 @rand() #5' at position {cs_ret:call [call@-1]} with state range(32)<full-set / full-set>

[Attributor] Update changed [AAValueConstantRange] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state range(32)<[-9,10) / [-9,10)>

[Attributor] Update changed [AAValueConstantRange] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state range(32)<[1,20) / [1,20)>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state set-state(< {full-set} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs_arg: [call2@2]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs_arg: [call2@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAMemoryBehavior] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state all memory

[Attributor] Update: [AANoSync] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state nosync

[Attributor] Update changed [AANoSync] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs:call2 [call2@-1]} with state may-sync

[Attributor] Update changed [AANoSync] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state may-sync

[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update unchanged [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state memory

[AAMemoryLocation] Categorize accessed locations for   store i32 1, ptr %number, align 4, !tbaa !7
[AAMemoryLocation] Categorize memory access with pointer:   store i32 1, ptr %number, align 4, !tbaa !7 [  %number = alloca i32, align 4]
[AAMemoryLocation] Categorize pointer locations for   %number = alloca i32, align 4 [no memory]
[AAMemoryLocation] Ptr value can be categorized:   %number = alloca i32, align 4 -> memory:constant,internal global,external global,argument,inaccessible,malloced,unknown
[AAMemoryLocation] Accessed locations with pointer locations: memory:stack
[AAMemoryLocation] Accessed locations for   store i32 1, ptr %number, align 4, !tbaa !7: memory:stack
[AAMemoryLocation] Categorize accessed locations for   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAMemoryLocation] Categorize memory access with pointer:   store i32 10000, ptr %shared_number, align 4, !tbaa !7 [  %shared_number = alloca i32, align 4]
[AAMemoryLocation] Categorize pointer locations for   %shared_number = alloca i32, align 4 [no memory]
[AAMemoryLocation] Ptr value can be categorized:   %shared_number = alloca i32, align 4 -> memory:constant,internal global,external global,argument,inaccessible,malloced,unknown
[AAMemoryLocation] Accessed locations with pointer locations: memory:stack
[AAMemoryLocation] Accessed locations for   store i32 10000, ptr %shared_number, align 4, !tbaa !7: memory:stack
[AAMemoryLocation] Categorize accessed locations for   %call = tail call i32 @rand() #5
[AAMemoryLocation] Categorize call site:   %call = tail call i32 @rand() #5 [0x5615a38e6b10]
[AAMemoryLocation] Accessed locations for   %call = tail call i32 @rand() #5: all memory
[Attributor] Update changed [AAMemoryLocation] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state all memory

[Attributor] Got 3 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  store i32 %add, ptr %random_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %add, ptr %random_number, align 4, !tbaa !7 (only exact: 0)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 2 potential copies.
[AAIsDead] Potential copy   %1 = load i32, ptr %random_number, align 4, !tbaa !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 %add, ptr %random_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update changed [AAIsDead] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state assumed-live

[Attributor] Update unchanged [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AAValueConstantRange] We give up:   %4 = load i32, ptr %0, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {} >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AAValueConstantRange] We give up:   %4 = load i32, ptr %0, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state range(32)<full-set / empty-set>

[Attributor] Clamp call site argument states for [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state range(32)<full-set / empty-set>
 into range-state(32)<full-set / empty-set>
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: range(32)<full-set / full-set> @{cs_arg: [@2]}
[Attributor] AA State: range-state(32)<full-set / full-set>top CSA State: range-state(32)<full-set / full-set>top
[Attributor] Call site callback failed for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update changed [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state range(32)<full-set / full-set>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)

[Attributor] Clamp call site argument states for [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)
 into set-state(< {} >)
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: set-state(< {} >) @{cs_arg: [@2]}
[Attributor] AA State: set-state(< {} >) CSA State: set-state(< {} >)
[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@3]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@3]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[3], } >)

[Attributor] Update: [AAInstanceInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state <unique [fAa]>

[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %NewRandom = alloca i32, align 4
[Attributor] Update: [AAPointerInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state PointerInfo #0 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %NewRandom = alloca i32, align 4 in   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7
[Attributor] Update: [AAPotentialValues] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state set-state(< {full-set} >)

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[AAPointerInfo] Analyze   %NewRandom = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state PointerInfo #0 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state PointerInfo #0 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !8
[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 1
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 1 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !8
Accesses by bin after update:
[0-4] : 1
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state PointerInfo #1 bins

[AAPointerInfo] Inserting access in new offset bins
    key [0, 4]
Accesses by bin after update:
[0-4] : 2
     - 9 -   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7
       - c:   %call1 = tail call i32 @rand() #5
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %NewRandom = alloca i32, align 4 in   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7
[AAPointerInfo] Analyze   %NewRandom = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 2
     - 9 -   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7
       - c:   %call1 = tail call i32 @rand() #5
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[Attributor] Got 2 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  store i32 %call1, ptr %NewRandom, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 (only exact: 0)
[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state UnderlyingObjects inter #0 objs, intra #0 objs

[Attributor] Update: [AAPotentialValues] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state set-state(< {  %NewRandom = alloca i32, align 4[3], } >)

[Attributor] Update changed [AAUnderlyingObjects] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update: [AAUnderlyingObjects] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Update unchanged [AAUnderlyingObjects] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AAIsDead] Store has 1 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %call1, ptr %NewRandom, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state assumed-dead

[Attributor] Update unchanged [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: carts.edt
[Attributor] Update changed [AACallEdges] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update: [AAInstanceInfo] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state <unique [fAa]>

[AAValueConstantRange] We give up:   %5 = load i32, ptr %1, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state range(32)<full-set / empty-set>

[Attributor] Introducing call base context:  %call1 = tail call i32 @rand() #5
[Attributor] Update changed [AAValueConstantRange] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state range(32)<full-set / full-set>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state set-state(< {full-set} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {} >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AAValueConstantRange] We give up:   %5 = load i32, ptr %1, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state range(32)<full-set / empty-set>

[Attributor] Clamp call site argument states for [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state range(32)<full-set / empty-set>
 into range-state(32)<full-set / empty-set>
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: range(32)<full-set / full-set> @{cs_arg: [@3]}
[Attributor] AA State: range-state(32)<full-set / full-set>top CSA State: range-state(32)<full-set / full-set>top
[Attributor] Call site callback failed for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update changed [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state range(32)<full-set / full-set>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)

[Attributor] Clamp call site argument states for [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)
 into set-state(< {} >)
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: set-state(< {} >) @{cs_arg: [@3]}
[Attributor] AA State: set-state(< {} >) CSA State: set-state(< {} >)
[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@4]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@4]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAMemoryBehavior] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state memory

[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state memory

[AAMemoryLocation] Categorize accessed locations for   tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Update unchanged [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[AAMemoryLocation] Categorize call site:   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) [0x5615a3920570]
[AAMemoryLocation] Accessed locations for   tail call void @llvm.experimental.noalias.scope.decl(metadata !15): memory:inaccessible
[Attributor] Update: [AAMemoryBehavior] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state all memory

[AAMemoryLocation] Categorize accessed locations for   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8
[AAMemoryLocation] Categorize call site:   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8 [0x5615a39c5500]
[AAMemoryLocation] Accessed locations for   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8: all memory
[Attributor] Update changed [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state all memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state all memory

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state memory

[Attributor] Update: [AAMemoryLocation] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state memory

[AAMemoryLocation] Categorize accessed locations for   %4 = load i32, ptr %0, align 4, !tbaa !8
[AAMemoryLocation] Categorize memory access with pointer:   %4 = load i32, ptr %0, align 4, !tbaa !8 [ptr %0]
[AAMemoryLocation] Categorize pointer locations for ptr %0 [no memory]
[AAMemoryLocation] Ptr value can be categorized: ptr %0 -> memory:stack,constant,internal global,external global,inaccessible,malloced,unknown
[AAMemoryLocation] Accessed locations with pointer locations: memory:argument
[AAMemoryLocation] Accessed locations for   %4 = load i32, ptr %0, align 4, !tbaa !8: memory:argument
[AAMemoryLocation] Categorize accessed locations for   %5 = load i32, ptr %1, align 4, !tbaa !8
[AAMemoryLocation] Categorize memory access with pointer:   %5 = load i32, ptr %1, align 4, !tbaa !8 [ptr %1]
[AAMemoryLocation] Categorize pointer locations for ptr %1 [no memory]
[AAMemoryLocation] Ptr value can be categorized: ptr %1 -> memory:stack,constant,internal global,external global,inaccessible,malloced,unknown
[AAMemoryLocation] Accessed locations with pointer locations: memory:argument
[AAMemoryLocation] Accessed locations for   %5 = load i32, ptr %1, align 4, !tbaa !8: memory:argument
[AAMemoryLocation] Categorize accessed locations for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAMemoryLocation] Categorize call site:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 [0x5615a3920410]
[AAMemoryLocation] Accessed locations for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7: all memory
[Attributor] Update changed [AAMemoryLocation] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state all memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state memory:stack,constant,argument

AAEDTInfoChild::initialize: EDT #0
[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update unchanged [AAEDTInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl: EDT #0 is a child of EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is reached from EDT #0
[Attributor] Update: [AAMemoryBehavior] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state memory

[Attributor] Update: [AAMemoryLocation] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state memory

[AAMemoryLocation] Categorize accessed locations for   %1 = load i32, ptr %random_number, align 4, !tbaa !8
[AAMemoryLocation] Categorize memory access with pointer:   %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ptr %random_number]
[AAMemoryLocation] Categorize pointer locations for ptr %random_number [no memory]
[AAMemoryLocation] Ptr value can be categorized: ptr %random_number -> memory:stack,constant,internal global,external global,inaccessible,malloced,unknown
[AAMemoryLocation] Accessed locations with pointer locations: memory:argument
[AAMemoryLocation] Accessed locations for   %1 = load i32, ptr %random_number, align 4, !tbaa !8: memory:argument
[AAMemoryLocation] Categorize accessed locations for   %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5
[AAMemoryLocation] Categorize call site:   %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5 [0x5615a3933130]
[AAMemoryLocation] Accessed locations for   %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5: all memory
[Attributor] Update changed [AAMemoryLocation] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state all memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state all memory

AAEDTInfoChild::initialize: EDT #0
[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update changed [AAEDTInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl: EDT #0 is a child of EDT #0
AAEDTInfoFunction::updateImpl: EDT #0 is reached from EDT #0
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
[Attributor] Update changed [AAEDTInfo] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 1{0}

[Attributor] Update: [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoChild::initialize: EDT #1
[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update unchanged [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
[Attributor] Update: [AAMemoryBehavior] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state readnone

[Attributor] Update: [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state may-read/write

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state memory

[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state memory

[AAMemoryLocation] Categorize accessed locations for   tail call void @llvm.experimental.noalias.scope.decl(metadata !20)
[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Update: [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Update unchanged [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[AAMemoryLocation] Categorize call site:   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) [0x5615a39c7b80]
[AAMemoryLocation] Accessed locations for   tail call void @llvm.experimental.noalias.scope.decl(metadata !20): memory:inaccessible
[Attributor] Update: [AAMemoryBehavior] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state readnone

[Attributor] Update changed [AAMemoryBehavior] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state may-read/write

[Attributor] Update: [AAMemoryLocation] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state all memory

[AAMemoryLocation] Categorize accessed locations for   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8
[AAMemoryLocation] Categorize call site:   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8 [0x5615a39c7f60]
[AAMemoryLocation] Accessed locations for   %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8: all memory
[Attributor] Update changed [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state all memory

[Attributor] Update changed [AAMemoryLocation] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state all memory

AAEDTInfoChild::initialize: EDT #1
[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update changed [AAEDTInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
[Attributor] Update changed [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

[Attributor] Update: [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state EDT #2 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl-> EDT #2
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
[Attributor] Update changed [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state EDT #2 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update: [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl-> EDT #3
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
[Attributor] Update changed [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Update: [AAEDTInfo] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::updateImpl-> EDT #4
AAEDTInfoFunction::updateImpl: All EDT children were fixed!
[Attributor] Update changed [AAEDTInfo] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}



[Attributor] #Iteration: 2, Worklist size: 246
[Attributor] #Iteration: 2, Worklist+Dependent size: 246
[Attributor] Update: [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
[Attributor] Update unchanged [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update: [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[Attributor] Got 3 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[Attributor] Update: [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 1, ptr %number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update: [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Got 3 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state assumed-dead

[Attributor] Got 2 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state assumed-dead

[Attributor] Got 3 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(0)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(0)

[Attributor] Update: [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: llvm.experimental.noalias.scope.decl
[Attributor] Update changed [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: printf
[Attributor] Update changed [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[Attributor] Update: [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(0)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(0)

[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: carts.edt.1
[Attributor] Update changed [AACallEdges] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: carts.edt.2
[Attributor] Update changed [AACallEdges] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(0)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(0)

[Attributor] Update: [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: llvm.experimental.noalias.scope.decl
[Attributor] Update changed [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,0]

[AACallEdges] New call edge: printf
[Attributor] Update changed [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Update: [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Update unchanged [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state assumed-dead

[Attributor] Update unchanged [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Update: [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 10000, ptr %shared_number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AANoCapture] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state assumed not-captured

[Attributor] Got 2 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state assumed-dead

[Attributor] Got 4 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc, ptr %3, align 4, !tbaa !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state assumed-live

[Attributor] Update unchanged [AANoCapture] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state assumed not-captured

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(10)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(10)

[Attributor] Update: [AAInterFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(13)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(13)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(9)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(9)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(2)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(2)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state #queries(2)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state #queries(2)

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %1 = load i32, ptr %random_number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state assumed-live

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)

[Attributor] Clamp call site argument states for [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {} >)
 into set-state(< {} >)
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: set-state(< {full-set} >) @{cs_arg: [@2]}
[Attributor] AA State: set-state(< {full-set} >) CSA State: set-state(< {full-set} >)
[Attributor] Call site callback failed for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update: [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[Attributor] Got 2 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %call1, ptr %NewRandom, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AAIsDead] Store has 1 potential copies.
[AAIsDead] Potential copy   %5 = load i32, ptr %1, align 4, !tbaa !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 %call1, ptr %NewRandom, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state assumed-live

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)

[Attributor] Clamp call site argument states for [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {} >)
 into set-state(< {} >)
[Attributor] ACS:   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7 AA: set-state(< {full-set} >) @{cs_arg: [@3]}
[Attributor] AA State: set-state(< {full-set} >) CSA State: set-state(< {full-set} >)
[Attributor] Call site callback failed for   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {full-set} >)

[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#0 from EDT #2
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %2
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#1 from EDT #2
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %3
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#2 from EDT #2
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl:   %4 = load i32, ptr %0, align 4, !tbaa !8
AAEDTDataBlockInfoValue::updateImpl: Failed to get AAPointerInfo!
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#3 from EDT #2
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl:   %5 = load i32, ptr %1, align 4, !tbaa !8
AAEDTDataBlockInfoValue::updateImpl: Failed to get AAPointerInfo!
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update changed [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#0 from EDT #3
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %3
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::initialize: CallArg#1 from EDT #3
[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl:   %7 = load i32, ptr %2, align 4, !tbaa !8
AAEDTDataBlockInfoValue::updateImpl: Failed to get AAPointerInfo!
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update unchanged [AAEDTInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}



[Attributor] #Iteration: 3, Worklist size: 42
[Attributor] #Iteration: 3, Worklist+Dependent size: 59
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

[Attributor] Update: [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

[Attributor] Update unchanged [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(7)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(7)

[Attributor] Update: [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(9)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(9)

[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(3)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(3)

[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc, ptr %3, align 4, !tbaa !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %2
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %3
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(9)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(9)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(10)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(10)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update: [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %1 = load i32, ptr %random_number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %1 = load i32, ptr %random_number, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], i32 %2[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], i32 %3[1], } >)

[Attributor] Update: [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[Attributor] Got 3 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state assumed not-captured

[Attributor] Update: [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[Attributor] Got 2 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state assumed not-captured

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

Generic inst   %inc.i = add nsw i32 %6, 1 assumed simplified to i32 2
[Attributor] Update changed [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {i32 2[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

Generic inst   %inc = add nsw i32 %6, 1 assumed simplified to i32 10001
[Attributor] Update changed [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {i32 10001[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

Generic inst   %dec.i = add nsw i32 %7, -1 assumed simplified to i32 9999
[Attributor] Update changed [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {i32 9999[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

AAEDTInfoFunction::updateImpl-> EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is a child of EDT #1
AAEDTInfoFunction::updateImpl: EDT #1 is reached from EDT #1
[Attributor] Update unchanged [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

[Attributor] Update: [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Got 3 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured



[Attributor] #Iteration: 4, Worklist size: 15
[Attributor] #Iteration: 4, Worklist+Dependent size: 18
[Attributor] Update: [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %1 = load i32, ptr %random_number, align 4, !tbaa !8[1], } >)

Trying to determine the potential copies of   %1 = load i32, ptr %random_number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %1 = load i32, ptr %random_number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %1 = load i32, ptr %random_number, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %random_number = alloca i32, align 4
[AA] Object '  %random_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %add, ptr %random_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], i32 %2[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], i32 %2[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %NewRandom = alloca i32, align 4
[AA] Object '  %NewRandom = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %call1, ptr %NewRandom, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], i32 %3[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], i32 %3[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {i32 2[3], } >)

Generic inst   %inc.i = add nsw i32 %6, 1 assumed simplified to i32 2
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {i32 10001[3], } >)

Generic inst   %inc = add nsw i32 %6, 1 assumed simplified to i32 10001
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {i32 9999[3], } >)

Generic inst   %dec.i = add nsw i32 %7, -1 assumed simplified to i32 9999
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {i32 9999[3], } >)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 5, Worklist size: 4
[Attributor] #Iteration: 5, Worklist+Dependent size: 7
[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 6, Worklist size: 3
[Attributor] #Iteration: 6, Worklist+Dependent size: 8
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %2
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %3
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}



[Attributor] #Iteration: 7, Worklist size: 3
[Attributor] #Iteration: 7, Worklist+Dependent size: 11
[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 10000, ptr %shared_number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc, ptr %3, align 4, !tbaa !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins



[Attributor] #Iteration: 8, Worklist size: 7
[Attributor] #Iteration: 8, Worklist+Dependent size: 15
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {i32 10001[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {  %inc = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {i32 9999[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {  %dec.i = add nsw i32 %7, -1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAIsDead] Store has 1 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update unchanged [AAIsDead] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@2]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs_arg: [call.i@2]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs_arg: [call.i@2]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 9, Worklist size: 15
[Attributor] #Iteration: 9, Worklist+Dependent size: 31
[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: i32 2
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: i32 10001
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAIsDead] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAIsDead] Store has 1 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(13)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(13)

[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(10)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(10)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(12)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(12)

[Attributor] Update: [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(11)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(11)

[Attributor] Update: [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 10000, ptr %shared_number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[AAIsDead] Potential copy   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 10000, ptr %shared_number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc, ptr %3, align 4, !tbaa !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AAIsDead] Store has 4 potential copies.
[AAIsDead] Potential copy   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 %inc, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 1, ptr %number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[Attributor] Update unchanged [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %1 in   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %1 in   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead



[Attributor] #Iteration: 10, Worklist size: 14
[Attributor] #Iteration: 10, Worklist+Dependent size: 23
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], } >)

[Attributor] Update: [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(16)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(16)

[Attributor] Update: [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(14)

[Attributor] Update unchanged [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(14)

[Attributor] Update: [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(13)

[Attributor] Update unchanged [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(13)

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAIsDead] Store has 1 potential copies.
[AAIsDead] Potential copy   %6 = load i32, ptr %3, align 4, !tbaa !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {i32 2[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {  %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state assumed-dead

[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state assumed-dead

[Attributor] Update: [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {} >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {} >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs_arg: [call.i@1]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs_arg: [call.i@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@1]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs_arg: [call.i@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update: [AAIsDead] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs_arg: [call2@1]} with state assumed-dead

[Attributor] Update changed [AAIsDead] for CtxI '  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #5' at position {cs_arg: [call2@1]} with state assumed-live

[Attributor] Update changed [AAIsDead] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 11, Worklist size: 15
[Attributor] #Iteration: 11, Worklist+Dependent size: 25
[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAIsDead] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state assumed-dead

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Update: [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 1, ptr %number, align 4, !tbaa !7 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[AAIsDead] Potential copy   %7 = load i32, ptr %2, align 4, !tbaa !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 1, ptr %number, align 4, !tbaa !7' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead-store

Trying to determine the potential copies of   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 0)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[AAIsDead] Store has 4 potential copies.
[AAIsDead] Potential copy   %7 = load i32, ptr %2, align 4, !tbaa !8 is assumed live!
[Attributor] Update changed [AAIsDead] for CtxI '  store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-live

[Attributor] Update: [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Got 3 initial uses to check
[Attributor] Update unchanged [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state assumed not-captured

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze ptr %0 in   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
[AAPointerInfo] Analyze ptr %0 in   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
Accesses by bin after update:
[0-4] : 3
     - 5 -   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Got 4 initial uses to check
[AAPointerInfo] Analyze ptr %3 in   store i32 %inc, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   %6 = load i32, ptr %3, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
[AAPointerInfo] Analyze ptr %3 in   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %3
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update: [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead

[Attributor] Got 1 initial uses to check
[Attributor] Update unchanged [AAIsDead] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state assumed-dead



[Attributor] #Iteration: 12, Worklist size: 5
[Attributor] #Iteration: 12, Worklist+Dependent size: 12
[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: i32 9999
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update: [AAInstanceInfo] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state <unique [fAa]>

[Attributor] Update: [AAValueConstantRange] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state range(32)<[-2147483647,-2147483648) / empty-set>

[AAValueConstantRange] We give up:   %6 = load i32, ptr %3, align 4, !tbaa !8
[Attributor] Update changed [AAValueConstantRange] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state range(32)<[-2147483647,-2147483648) / [-2147483647,-2147483648)>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAValueConstantRange] We give up:   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAValueConstantRange] We give up:   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[AAValueConstantRange] We give up:   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins



[Attributor] #Iteration: 13, Worklist size: 17
[Attributor] #Iteration: 13, Worklist+Dependent size: 21
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze ptr %2 in   %7 = load i32, ptr %2, align 4, !tbaa !8
[AAPointerInfo] Analyze ptr %2 in   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
Accesses by bin after update:
[0-4] : 4
     - 5 -   %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

AAEDTDataBlockInfoValue::updateImpl: ptr %2
[Attributor] Update unchanged [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Got 2 initial uses to check
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   store i32 10000, ptr %shared_number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %shared_number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
Accesses by bin after update:
[0-4] : 7
     - 9 -   store i32 10000, ptr %shared_number, align 4, !tbaa !7
       - c: i32 10000
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %3, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 14, Worklist size: 6
[Attributor] #Iteration: 14, Worklist+Dependent size: 11
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update: [AAInstanceInfo] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state <unique [fAa]>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, } >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state range(32)<[-2147483648,2147483647) / empty-set>

[Attributor] Update changed [AAValueConstantRange] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state range(32)<[-2147483648,2147483647) / [-2147483648,2147483647)>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update changed [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Update unchanged [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins



[Attributor] #Iteration: 15, Worklist size: 10
[Attributor] #Iteration: 15, Worklist+Dependent size: 15
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update changed [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Update: [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Got 3 initial uses to check
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   store i32 1, ptr %number, align 4, !tbaa !7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
[AAPointerInfo] Analyze   %number = alloca i32, align 4 in   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
Accesses by bin after update:
[0-4] : 6
     - 9 -   store i32 1, ptr %number, align 4, !tbaa !7
       - c: i32 1
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %7 = load i32, ptr %2, align 4, !tbaa !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 9 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7
     -->                           %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
       - c: <unknown>
     - 5 -   call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)
     -->                           %0 = load i32, ptr %number, align 4, !tbaa !8
       - c: <unknown>
[Attributor] Update unchanged [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins



[Attributor] #Iteration: 16, Worklist size: 7
[Attributor] #Iteration: 16, Worklist+Dependent size: 18
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update: [AAInstanceInfo] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state <unique [fAa]>

[Attributor] Update unchanged [AAInstanceInfo] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state <unique [fAa]>

[Attributor] Update: [AAValueConstantRange] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state range(32)<[-2147483647,-2147483648) / empty-set>

[AAValueConstantRange] We give up:   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8
[Attributor] Update changed [AAValueConstantRange] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state range(32)<[-2147483647,-2147483648) / [-2147483647,-2147483648)>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AAValueConstantRange] We give up:   %7 = load i32, ptr %2, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[AAValueConstantRange] We give up:   %0 = load i32, ptr %number, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[AAValueConstantRange] We give up:   %7 = load i32, ptr %2, align 4, !tbaa !8
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {} >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)



[Attributor] #Iteration: 17, Worklist size: 19
[Attributor] #Iteration: 17, Worklist+Dependent size: 27
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], } >)



[Attributor] #Iteration: 18, Worklist size: 11
[Attributor] #Iteration: 18, Worklist+Dependent size: 20
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, 10005, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)



[Attributor] #Iteration: 19, Worklist size: 9
[Attributor] #Iteration: 19, Worklist+Dependent size: 19
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 9999, 10002, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {10001, 10002, 10000, 10003, 10004, 10005, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)



[Attributor] #Iteration: 20, Worklist size: 10
[Attributor] #Iteration: 20, Worklist+Dependent size: 20
[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, } >)



[Attributor] #Iteration: 21, Worklist size: 10
[Attributor] #Iteration: 21, Worklist+Dependent size: 18
[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {9999, 10000, 10001, 10002, 10003, 10004, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {10000, 10001, 10002, 10003, 10004, 10005, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], } >)



[Attributor] #Iteration: 22, Worklist size: 9
[Attributor] #Iteration: 22, Worklist+Dependent size: 18
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2],   %dec.i = add nsw i32 %7, -1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update changed [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, } >)



[Attributor] #Iteration: 23, Worklist size: 11
[Attributor] #Iteration: 23, Worklist+Dependent size: 14
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2],   %dec.i = add nsw i32 %7, -1[2], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %3, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   store i32 %inc, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 cannot reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %6 = load i32, ptr %3, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %6 = load i32, ptr %3, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %6 = load i32, ptr %3, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2],   %dec.i = add nsw i32 %7, -1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

Trying to determine the potential copies of   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[AA] check   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 cannot reach   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8[1], } >)

Trying to determine the potential copies of   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %shared_number = alloca i32, align 4
[AA] Object '  %shared_number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 10000, ptr %shared_number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.2 [FromFn]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %dec.i, ptr %1, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !20) of @carts.edt.2 can potentially reach @  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt cannot reach @carts.edt.2 [FromFn]
[AA][Ret]   %6 = load i32, ptr %3, align 4, !tbaa !8 cannot reach   ret void [Intra]
[AA] No return is reachable, done
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, 7, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)



[Attributor] #Iteration: 24, Worklist size: 4
[Attributor] #Iteration: 24, Worklist+Dependent size: 10
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, 7, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, 7, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)



[Attributor] #Iteration: 25, Worklist size: 7
[Attributor] #Iteration: 25, Worklist+Dependent size: 10
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update unchanged [AAPotentialConstantValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {2, 3, 4, 5, 6, 7, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {1, 2, 3, 4, 5, 6, } >)

[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {full-set} >)



[Attributor] #Iteration: 26, Worklist size: 4
[Attributor] #Iteration: 26, Worklist+Dependent size: 9
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update changed [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %0 = load i32, ptr %number, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3], } >)

[Attributor] Update: [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state range(32)<full-set / empty-set>

[Attributor] Clamp call site argument states for [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state range(32)<full-set / empty-set>
 into range-state(32)<full-set / empty-set>
[Attributor] ACS:   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8 AA: range(32)<full-set / full-set> @{cs_arg: [@1]}
[Attributor] AA State: range-state(32)<full-set / full-set>top CSA State: range-state(32)<full-set / full-set>top
[Attributor] Call site callback failed for   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
[Attributor] Update changed [AAValueConstantRange] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state range(32)<full-set / full-set>

[Attributor] Update: [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {} >)

[Attributor] Clamp call site argument states for [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {} >)
 into set-state(< {} >)
[Attributor] ACS:   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8 AA: set-state(< {full-set} >) @{cs_arg: [@1]}
[Attributor] AA State: set-state(< {full-set} >) CSA State: set-state(< {full-set} >)
[Attributor] Call site callback failed for   call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8
[Attributor] Update changed [AAPotentialConstantValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {full-set} >)

[Attributor] Update changed [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[2], i32 2[2], i32 1[2], i32 3[2], i32 4[2], i32 5[2], i32 6[2], i32 7[2],   %inc.i = add nsw i32 %6, 1[2], i32 %1[1], } >)



[Attributor] #Iteration: 27, Worklist size: 8
[Attributor] #Iteration: 27, Worklist+Dependent size: 8
[Attributor] Update: [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

Trying to determine the potential copies of   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %7 = load i32, ptr %2, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt [FromFn]
[AA] Entry   %4 = load i32, ptr %0, align 4, !tbaa !8 of @carts.edt can potentially reach @  %7 = load i32, ptr %2, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] check   %7 = load i32, ptr %2, align 4, !tbaa !8 from   %6 = load i32, ptr %3, align 4, !tbaa !8 intraprocedurally
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 can potentially reach   %7 = load i32, ptr %2, align 4, !tbaa !8 [Intra]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

Trying to determine the potential copies of   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.1 [FromFn]
[AA] check   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 intraprocedurally
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 cannot reach   %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [Intra]
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.1 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   tail call void @llvm.experimental.noalias.scope.decl(metadata !15) of @carts.edt.1 can potentially reach @  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.1 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %0 = load i32, ptr %number, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

Trying to determine the potential copies of   %0 = load i32, ptr %number, align 4, !tbaa !8 (only exact: 1)
Visit underlying object   %number = alloca i32, align 4
[AA] Object '  %number = alloca i32, align 4' is  thread local; non-captured stack object.
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 1, ptr %number, align 4, !tbaa !7 in @main can potentially reach @carts.edt.3 [FromFn]
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 cannot reach @carts.edt.3 [FromFn]
[AA][Ret]   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 can potentially reach   ret void [Intra]
Stepping backwards to the call sites of @carts.edt.1
[AA] stepped back to call sites from   store i32 %inc.i, ptr %0, align 4, !tbaa !11, !noalias !8 in @carts.edt.1 worklist size is: 1
[AA] Entry   br label %entry.split of @carts.edt.3 can potentially reach @  %0 = load i32, ptr %number, align 4, !tbaa !8 [ToFn]
[AA]   %6 = load i32, ptr %3, align 4, !tbaa !8 in @carts.edt can potentially reach @carts.edt.3 [FromFn]
[Attributor] Update unchanged [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %0 = load i32, ptr %number, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Update: [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[2], i32 2[2], i32 1[2], i32 3[2], i32 4[2], i32 5[2], i32 6[2], i32 7[2],   %inc.i = add nsw i32 %6, 1[2], i32 %1[1], } >)

[Attributor] Update unchanged [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[2], i32 2[2], i32 1[2], i32 3[2], i32 4[2], i32 5[2], i32 6[2], i32 7[2],   %inc.i = add nsw i32 %6, 1[2], i32 %1[1], } >)


[Attributor] Fixpoint iteration done after: 27/32 iterations
AAEDTInfoFunction::manifest: EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 1{0}
- EDT #0: main
Ty: main
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 0

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 1{0}

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

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 1{1}

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

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state EDT #2 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::manifest: EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #3: carts.edt.2
Ty: task
Data environment for EDT: 
Number of ParamV: 1
  - i32 %1
Number of DepV: 1
  - ptr %0

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state EDT #3 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

AAEDTInfoFunction::manifest: EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}
- EDT #4: carts.edt.3
Ty: task
Data environment for EDT: 
Number of ParamV: 0
Number of DepV: 2
  - ptr %number
  - ptr %random_number

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state EDT #4 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 1[3], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state set-state(< {  %add = add nsw i32 %rem, 10[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state set-state(< {  %rem = srem i32 %call, 10[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 10[3], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:random_number [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AANoCapture] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state known not-captured

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state set-state(< {  %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %6 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state set-state(< {ptr %2[1],   %number = alloca i32, align 4[2], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %2[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:number [@2]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AANoCapture] for CtxI '  %number = alloca i32, align 4' at position {flt:number [number@-1]} with state known not-captured

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %number = alloca i32, align 4' at position {fn_ret:main [main@-1]} with state set-state(< {i32 0[2], i32 0[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 0[3], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state set-state(< {  %shared_number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 10000[3], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state set-state(< {  %inc = add nsw i32 %6, 1[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %6 = load i32, ptr %3, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 9999[3], i32 10002[3], i32 10000[1], i32 10001[1], i32 9999[1], i32 10002[1], i32 undef[2], i32 9999[2], i32 10000[2], i32 10001[2], i32 10002[2], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2],   %6 = load i32, ptr %3, align 4, !tbaa !8[1],   %inc = add nsw i32 %6, 1[2],   %dec.i = add nsw i32 %7, -1[2], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %3[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:shared_number [@3]} with state set-state(< {  %shared_number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AANoCapture] for CtxI '  %shared_number = alloca i32, align 4' at position {flt:shared_number [shared_number@-1]} with state known not-captured

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state set-state(< {  %dec.i = add nsw i32 %7, -1[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %7 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@1]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAIntraFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(14)

[Attributor] Manifest unchanged : [AAInterFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(13)

[Attributor] Manifest unchanged : [AAIntraFnReachability] for CtxI '  %number = alloca i32, align 4' at position {fn:main [main@-1]} with state #queries(8)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  %call = tail call i32 @rand() #5' at position {cs:call [call@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(13)

[Attributor] Manifest unchanged : [AAIntraFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(2)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %5 = load i32, ptr %1, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 undef[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 10000[3], i32 10001[3], i32 10000[1], i32 10001[1], i32 undef[2], i32 10001[2], i32 10002[2], i32 10000[2], i32 10002[1], i32 10003[2], i32 10003[1], i32 10004[2], i32 10004[1], i32 10005[2], i32 10005[1],   %inc = add nsw i32 %6, 1[2],   %2 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8[1], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@0]} with state set-state(< {  %shared_number = alloca i32, align 4[2], ptr %0[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state set-state(< {ptr %3[1],   %shared_number = alloca i32, align 4[2], } >)

[Attributor] Manifest unchanged : [AAIntraFnReachability] for CtxI '  br label %entry.split' at position {fn:carts.edt.3 [carts.edt.3@-1]} with state #queries(2)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %7 = load i32, ptr %2, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@2]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !11, !noalias !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 3[3], i32 4[3], i32 5[3], i32 6[3], i32 7[3],   %inc.i = add nsw i32 %6, 1[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %0 = load i32, ptr %number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[3], i32 1[3], i32 2[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 4[1], i32 5[2], i32 5[1], i32 6[2], i32 6[1], i32 7[2],   %0 = load i32, ptr %number, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:number [number@0]} with state set-state(< {  %number = alloca i32, align 4[2], ptr %number[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:number [@0]} with state set-state(< {  %number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %1 = load i32, ptr %random_number, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %1 = load i32, ptr %random_number, align 4, !tbaa !8[1], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  br label %entry.split' at position {arg:random_number [random_number@1]} with state set-state(< {  %random_number = alloca i32, align 4[2], ptr %random_number[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs_arg:random_number [@1]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs:call1 [call1@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI '  %add = add nsw i32 %rem, 10' at position {flt:add [add@-1]} with state range(32)<[1,20) / [1,20)>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI '  %rem = srem i32 %call, 10' at position {flt:rem [rem@-1]} with state range(32)<[-9,10) / [-9,10)>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI <<null inst>> at position {flt: [@-1]} with state range(32)<[10,11) / [10,11)>

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %random_number = alloca i32, align 4' at position {flt:random_number [random_number@-1]} with state set-state(< {  %random_number = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2], i32 %2[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state set-state(< {i32 undef[2],   %add = add nsw i32 %rem, 10[2],   %4 = load i32, ptr %0, align 4, !tbaa !8[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %5 = load i32, ptr %1, align 4, !tbaa !8' at position {flt: [@-1]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[2], ptr %1[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state set-state(< {  %NewRandom = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs_arg:NewRandom [@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AAPointerInfo] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {arg: [@1]} with state PointerInfo #1 bins

[Attributor] Manifest unchanged : [AANoCapture] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state known not-captured

[Attributor] Manifest unchanged : [AAUnderlyingObjects] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state UnderlyingObjects inter #1 objs, intra #1 objs

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  %NewRandom = alloca i32, align 4' at position {flt:NewRandom [NewRandom@-1]} with state set-state(< {  %NewRandom = alloca i32, align 4[3], } >)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %call1 = tail call i32 @rand() #5' at position {cs_ret:call1 [call1@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt:rand [rand@-1]} with state set-state(< {@rand[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2], i32 %3[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state set-state(< {i32 undef[2],   %call1 = tail call i32 @rand() #5[2],   %5 = load i32, ptr %1, align 4, !tbaa !8[1], } >)

[Attributor] Manifest unchanged : [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Manifest unchanged : [AAMemoryLocation] for CtxI <<null inst>> at position {fn:llvm.experimental.noalias.scope.decl [llvm.experimental.noalias.scope.decl@-1]} with state memory:stack,constant,inaccessible

[Attributor] Manifest unchanged : [AAMemoryLocation] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state memory:stack,constant,argument

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  call void @carts.edt(ptr nocapture %random_number, ptr nocapture %NewRandom, ptr nocapture %number, ptr nocapture %shared_number) #7' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 0{}, #Child EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  call void @carts.edt.3(ptr nocapture %number, ptr nocapture %random_number)' at position {cs: [@-1]} with state EDT #0 -> #Reached EDTs: 1{0}, #Child EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

[Attributor] Manifest unchanged : [AAMemoryLocation] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state memory:stack,constant,inaccessible

[Attributor] Manifest unchanged : [AAEDTInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state EDT #1 -> #Reached EDTs: 1{1}, #Child EDTs: 0{}

[Attributor] Manifest unchanged : [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {fn:carts.edt.1 [carts.edt.1@-1]} with state #queries(16)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %2, i32 noundef %3) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAInterFnReachability] for CtxI '  %4 = load i32, ptr %0, align 4, !tbaa !8' at position {fn:carts.edt [carts.edt@-1]} with state #queries(13)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAInterFnReachability] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {fn:carts.edt.2 [carts.edt.2@-1]} with state #queries(3)

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {cs: [@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AACallEdges] for CtxI '  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %1, i32 noundef %2) #5, !noalias !8' at position {cs:call.i [call.i@-1]} with state CallEdges[0,1]

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@2]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.1(ptr nocapture %2, ptr nocapture %3, i32 %4, i32 %5) #7' at position {cs_arg: [@3]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@0]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAEDTDataBlockInfo] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state EDTDataBlock -> #Reached EDTs: 0{}

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 2[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 10001[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 -1[3], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI <<null inst>> at position {flt: [@-1]} with state set-state(< {i32 9999[3], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI <<null inst>> at position {flt: [@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  tail call void @llvm.experimental.noalias.scope.decl(metadata !20)' at position {arg: [@1]} with state set-state(< {i32 undef[2], i32 2[2], i32 1[2], i32 3[2], i32 4[2], i32 5[2], i32 6[2], i32 7[2],   %inc.i = add nsw i32 %6, 1[2], i32 %1[1], } >)

[Attributor] Manifest unchanged : [AAPotentialValues] for CtxI '  call void @carts.edt.2(ptr nocapture readonly %3, i32 %7) #8' at position {cs_arg: [@1]} with state set-state(< {i32 undef[3], i32 2[3], i32 1[3], i32 1[1], i32 2[1], i32 undef[2], i32 2[2], i32 3[2], i32 1[2], i32 3[1], i32 4[2], i32 5[2], i32 4[1], i32 6[2], i32 5[1], i32 7[2], i32 6[1],   %7 = load i32, ptr %2, align 4, !tbaa !8[1],   %inc.i = add nsw i32 %6, 1[2], } >)

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI '  %inc = add nsw i32 %6, 1' at position {flt:inc [inc@-1]} with state range(32)<[-2147483647,-2147483648) / [-2147483647,-2147483648)>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI <<null inst>> at position {flt: [@-1]} with state range(32)<[1,2) / [1,2)>

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI '  %dec.i = add nsw i32 %7, -1' at position {flt:dec.i [dec.i@-1]} with state range(32)<[-2147483648,2147483647) / [-2147483648,2147483647)>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI <<null inst>> at position {flt: [@-1]} with state range(32)<[-1,0) / [-1,0)>

[Attributor] Manifest unchanged : [AAInstanceInfo] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state <unique [fAa]>

[Attributor] Manifest unchanged : [AAValueConstantRange] for CtxI '  %inc.i = add nsw i32 %6, 1' at position {flt:inc.i [inc.i@-1]} with state range(32)<[-2147483647,-2147483648) / [-2147483647,-2147483648)>


[Attributor] Manifested 0 arguments while 154 were in a valid fixpoint state

[Attributor] Delete/replace at least 0 functions and 0 blocks and 0 instructions and 0 values and 0 uses. To insert 0 unreachables.
Preserve manifest added 0 blocks
[Attributor] DeadInsts size: 0
[Attributor] Deleted 0 functions after manifest.
[Attributor] Done with 5 functions, result: unchanged.

-------------------------------------------------
[arts-analysis] Process has finished

-------------------------------------------------
llvm-dis test_arts_analysis.bc
clang++ -fopenmp test_arts_analysis.bc -O3 -march=native -o test_opt -lstdc++
