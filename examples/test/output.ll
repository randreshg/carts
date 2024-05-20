clang++ -fopenmp -O3 -g0 -emit-llvm -c test.cpp -o test.bc -lstdc++
clang-14: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis test.bc
noelle-norm test.bc -o test_norm.bc
llvm-dis test_norm.bc
noelle-meta-pdg-embed test_norm.bc -o test_with_metadata.bc
PDGGenerator: Construct PDG from Analysis
Embed PDG as metadata
llvm-dis test_with_metadata.bc
noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc

 ---------------------------------------- 
[carts] Running CARTS on Module: 
test_with_metadata.bc
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: main
[arts-analyzer] Other Function Found: 
    %call = tail call i32 @rand() #6, !noelle.pdg.inst.id !8
[arts-analyzer] Other Function Found: 
    %call1 = tail call i32 @rand() #6, !noelle.pdg.inst.id !15
- - - - - -- - - - - - - - - -
[arts-analyzer] Parallel Region Found:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* nonnull %random_number, i32* nonnull %NewRandom, i32* nonnull %number, i32* nonnull %shared_number), !noelle.pdg.inst.id !18

[arts-analyzer] Handling parallel region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-utils]   - Replacing uses of: i32* %.global_tid.
[arts-utils]    - Replacing:   %0 = load i32, i32* undef, align 4, !tbaa !139, !noelle.pdg.inst.id !55
[arts-utils]    - Removing instruction:   %0 = load i32, i32* undef, align 4, !tbaa !139, !noelle.pdg.inst.id !55
[arts-utils]   - Replacing uses of:   %0 = load i32, i32* undef, align 4, !tbaa !139, !noelle.pdg.inst.id !55
[arts-utils]   - Replacing uses of: i32* %.bound_tid.
[arts-utils]   - Replacing uses of: i32* %random_number
[arts-utils]   - Replacing uses of: i32* %NewRandom
[arts-utils]   - Replacing uses of: i32* %number
[arts-utils]   - Replacing uses of: i32* %shared_number
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Created ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Created ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
[arts-utils]    - Removing instruction:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull undef, i32 undef, void (i32*, i32*, ...)* undef, i32* nonnull undef, i32* nonnull undef, i32* nonnull undef, i32* nonnull undef), !noelle.pdg.inst.id !18
[arts-utils]   - Replacing uses of:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull undef, i32 undef, void (i32*, i32*, ...)* undef, i32* nonnull undef, i32* nonnull undef, i32* nonnull undef, i32* nonnull undef), !noelle.pdg.inst.id !18
[arts-utils]   - Replacing uses of: ; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture noundef readonly %.global_tid., i32* noalias nocapture noundef readnone %.bound_tid., i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noundef nonnull align 4 dereferenceable(4) %number, i32* noundef nonnull align 4 dereferenceable(4) %shared_number) #3 !noelle.pdg.args.id !80 !noelle.pdg.edges !87 {
entry:
  %0 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !131
  %1 = bitcast i8* %0 to i8**, !noelle.pdg.inst.id !209
  %2 = load i8*, i8** %1, align 8, !tbaa !210, !noelle.pdg.inst.id !151
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %2 to i32**, !noelle.pdg.inst.id !215
  store i32* %number, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !216, !noelle.pdg.inst.id !133
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %2, i64 8, !noelle.pdg.inst.id !218
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**, !noelle.pdg.inst.id !219
  store i32* %shared_number, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !220, !noelle.pdg.inst.id !135
  %3 = getelementptr inbounds i8, i8* %0, i64 40, !noelle.pdg.inst.id !221
  %4 = bitcast i8* %3 to i32*, !noelle.pdg.inst.id !222
  %5 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !157
  store i32 %5, i32* %4, align 8, !tbaa !223, !noelle.pdg.inst.id !137
  %6 = getelementptr inbounds i8, i8* %0, i64 44, !noelle.pdg.inst.id !224
  %7 = bitcast i8* %6 to i32*, !noelle.pdg.inst.id !225
  %8 = load i32, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !101
  store i32 %8, i32* %7, align 4, !tbaa !226, !noelle.pdg.inst.id !104
  %9 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %0), !noelle.pdg.inst.id !89
  %10 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !93
  %11 = getelementptr inbounds i8, i8* %10, i64 40, !noelle.pdg.inst.id !227
  %12 = bitcast i8* %11 to i32*, !noelle.pdg.inst.id !228
  %13 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !96
  store i32 %13, i32* %12, align 8, !tbaa !229, !noelle.pdg.inst.id !98
  %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %10), !noelle.pdg.inst.id !90
  ret void, !noelle.pdg.inst.id !232
}

[arts-utils]    - Removing function: ; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture noundef readonly %.global_tid., i32* noalias nocapture noundef readnone %.bound_tid., i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number, i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, i32* noundef nonnull align 4 dereferenceable(4) %number, i32* noundef nonnull align 4 dereferenceable(4) %shared_number) #3 !noelle.pdg.args.id !80 !noelle.pdg.edges !87 {
entry:
  %0 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !131
  %1 = bitcast i8* %0 to i8**, !noelle.pdg.inst.id !209
  %2 = load i8*, i8** %1, align 8, !tbaa !210, !noelle.pdg.inst.id !151
  %agg.captured.sroa.0.0..sroa_cast = bitcast i8* %2 to i32**, !noelle.pdg.inst.id !215
  store i32* %number, i32** %agg.captured.sroa.0.0..sroa_cast, align 8, !tbaa.struct !216, !noelle.pdg.inst.id !133
  %agg.captured.sroa.2.0..sroa_idx = getelementptr inbounds i8, i8* %2, i64 8, !noelle.pdg.inst.id !218
  %agg.captured.sroa.2.0..sroa_cast = bitcast i8* %agg.captured.sroa.2.0..sroa_idx to i32**, !noelle.pdg.inst.id !219
  store i32* %shared_number, i32** %agg.captured.sroa.2.0..sroa_cast, align 8, !tbaa.struct !220, !noelle.pdg.inst.id !135
  %3 = getelementptr inbounds i8, i8* %0, i64 40, !noelle.pdg.inst.id !221
  %4 = bitcast i8* %3 to i32*, !noelle.pdg.inst.id !222
  %5 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !157
  store i32 %5, i32* %4, align 8, !tbaa !223, !noelle.pdg.inst.id !137
  %6 = getelementptr inbounds i8, i8* %0, i64 44, !noelle.pdg.inst.id !224
  %7 = bitcast i8* %6 to i32*, !noelle.pdg.inst.id !225
  %8 = load i32, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !101
  store i32 %8, i32* %7, align 4, !tbaa !226, !noelle.pdg.inst.id !104
  %9 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %0), !noelle.pdg.inst.id !89
  %10 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !93
  %11 = getelementptr inbounds i8, i8* %10, i64 40, !noelle.pdg.inst.id !227
  %12 = bitcast i8* %11 to i32*, !noelle.pdg.inst.id !228
  %13 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !96
  store i32 %13, i32* %12, align 8, !tbaa !229, !noelle.pdg.inst.id !98
  %14 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* %10), !noelle.pdg.inst.id !90
  ret void, !noelle.pdg.inst.id !232
}

- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_0
- - - - - -- - - - - - - - - -
[arts-analyzer] Task Region Found:   %4 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull @1, i32 undef, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !5

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Replacing uses of: %struct.kmp_task_t_with_privates* %1
[arts-utils]    - Replacing:   %2 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**, !noelle.pdg.inst.id !65
[arts-utils]    - Removing instruction:   %2 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**, !noelle.pdg.inst.id !65
[arts-utils]   - Replacing uses of:   %2 = bitcast %struct.kmp_task_t_with_privates* undef to %struct.anon**, !noelle.pdg.inst.id !65
[arts-utils]    - Replacing:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1, !noelle.pdg.inst.id !82
[arts-utils]    - Removing instruction:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1, !noelle.pdg.inst.id !82
[arts-utils]   - Replacing uses of:   %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 1, !noelle.pdg.inst.id !82
[arts-utils]    - Replacing:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !81
[arts-utils]    - Removing instruction:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !81
[arts-utils]   - Replacing uses of:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !81
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
[arts-utils]    - Removing instruction:   %4 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 undef, i64 undef, i64 undef, i32 (i32, i8*)* undef), !noelle.pdg.inst.id !5
[arts-utils]   - Replacing uses of:   %4 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 undef, i64 undef, i64 undef, i32 (i32, i8*)* undef), !noelle.pdg.inst.id !5
[arts-utils]    - Replacing:   %10 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !24
[arts-utils]    - Replacing:   %5 = bitcast i8* undef to i8**, !noelle.pdg.inst.id !6
[arts-utils]    - Replacing:   %24 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull @1, i32 undef, i8* undef), !noelle.pdg.inst.id !35
[arts-utils]    - Replacing:   %13 = getelementptr inbounds i8, i8* undef, i64 44, !noelle.pdg.inst.id !30
[arts-utils]   - Replacing uses of: @1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
[arts-utils]    - Removing global variable: @1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
[arts-utils]    - Removing instruction:   %23 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !34
[arts-utils]   - Replacing uses of:   %23 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !34
[arts-utils]   - Replacing uses of: ; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias nocapture noundef readonly %1) #4 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !80 !noelle.pdg.edges !83 {
entry:
  %2 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !135, !noelle.pdg.inst.id !85
  %.idx = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 0, !noelle.pdg.inst.id !140
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !141, !noelle.pdg.inst.id !92
  %.idx2 = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 1, !noelle.pdg.inst.id !143
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !144, !noelle.pdg.inst.id !96
  tail call void @llvm.experimental.noalias.scope.decl(metadata !145), !noelle.pdg.inst.id !86
  %3 = load i32, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !100
  %4 = load i32, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !102
  %5 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !145, !noelle.pdg.inst.id !104
  %6 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !145, !noelle.pdg.inst.id !106
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %3, i32 noundef %4, i32 noundef %11, i32 noundef %14) #5, !noalias !145, !noelle.pdg.inst.id !108
  %7 = load i32, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !111
  %inc.i = add nsw i32 %7, 1, !noelle.pdg.inst.id !148
  store i32 %inc.i, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !88
  %8 = load i32, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !115
  %dec.i = add nsw i32 %8, -1, !noelle.pdg.inst.id !149
  store i32 %dec.i, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !90
  ret i32 0, !noelle.pdg.inst.id !150
}

[arts-utils]    - Removing function: ; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias nocapture noundef readonly %1) #4 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !80 !noelle.pdg.edges !83 {
entry:
  %2 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !135, !noelle.pdg.inst.id !85
  %.idx = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 0, !noelle.pdg.inst.id !140
  %.idx.val = load i32*, i32** %.idx, align 8, !tbaa !141, !noelle.pdg.inst.id !92
  %.idx2 = getelementptr %struct.anon, %struct.anon* %2, i64 0, i32 1, !noelle.pdg.inst.id !143
  %.idx2.val = load i32*, i32** %.idx2, align 8, !tbaa !144, !noelle.pdg.inst.id !96
  tail call void @llvm.experimental.noalias.scope.decl(metadata !145), !noelle.pdg.inst.id !86
  %3 = load i32, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !100
  %4 = load i32, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !102
  %5 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !145, !noelle.pdg.inst.id !104
  %6 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !145, !noelle.pdg.inst.id !106
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %3, i32 noundef %4, i32 noundef %11, i32 noundef %14) #5, !noalias !145, !noelle.pdg.inst.id !108
  %7 = load i32, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !111
  %inc.i = add nsw i32 %7, 1, !noelle.pdg.inst.id !148
  store i32 %inc.i, i32* %2, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !88
  %8 = load i32, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !115
  %dec.i = add nsw i32 %8, -1, !noelle.pdg.inst.id !149
  store i32 %dec.i, i32* %3, align 4, !tbaa !70, !noalias !145, !noelle.pdg.inst.id !90
  ret i32 0, !noelle.pdg.inst.id !150
}

- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_1
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !146), !noelle.pdg.inst.id !149
[arts-analyzer] Other Function Found: 
    %15 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %11, i32 noundef %12, i32 noundef %1, i32 noundef %3) #4, !noalias !21, !noelle.pdg.inst.id !30
- - - - - -- - - - - - - - - -
[arts-analyzer] Task Region Found:   %23 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !34

[arts-analyzer] Handling task region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Replacing uses of: %struct.kmp_task_t_with_privates.1* %1
[arts-utils]    - Replacing:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !29
[arts-utils]    - Removing instruction:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !29
[arts-utils]   - Replacing uses of:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* undef, i64 0, i32 1, i32 0, !noelle.pdg.inst.id !29
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
[arts-utils]    - Removing instruction:   %23 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 undef, i64 undef, i64 undef, i32 (i32, i8*)* undef), !noelle.pdg.inst.id !34
[arts-utils]   - Replacing uses of:   %23 = tail call i8* @__kmpc_omp_task_alloc(%struct.ident_t* nonnull undef, i32 undef, i32 undef, i64 undef, i64 undef, i32 (i32, i8*)* undef), !noelle.pdg.inst.id !34
[arts-utils]    - Replacing:   %34 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !42
[arts-utils]    - Replacing:   %24 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !35
[arts-utils]    - Removing instruction:   %33 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !41
[arts-utils]   - Replacing uses of:   %33 = tail call i32 @__kmpc_omp_task(%struct.ident_t* nonnull undef, i32 undef, i8* undef), !noelle.pdg.inst.id !41
[arts-utils]   - Replacing uses of: ; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias nocapture noundef %1) #5 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !80 !noelle.pdg.edges !83 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !96), !noelle.pdg.inst.id !85
  %2 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !96, !noelle.pdg.inst.id !86
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %25) #4, !noalias !96, !noelle.pdg.inst.id !88
  %inc.i = add nsw i32 %25, 1, !noelle.pdg.inst.id !99
  store i32 %inc.i, i32* undef, align 4, !tbaa !70, !alias.scope !96, !noelle.pdg.inst.id !91
  ret i32 0, !noelle.pdg.inst.id !100
}

[arts-utils]    - Removing function: ; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias nocapture noundef %1) #5 personality i32 (...)* @__gxx_personality_v0 !noelle.pdg.args.id !80 !noelle.pdg.edges !83 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !96), !noelle.pdg.inst.id !85
  %2 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !96, !noelle.pdg.inst.id !86
  %call.i = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %25) #4, !noalias !96, !noelle.pdg.inst.id !88
  %inc.i = add nsw i32 %25, 1, !noelle.pdg.inst.id !99
  store i32 %inc.i, i32* undef, align 4, !tbaa !70, !alias.scope !96, !noelle.pdg.inst.id !91
  ret i32 0, !noelle.pdg.inst.id !100
}

- - - - - -- - - - - - - - - -
--------------------------------------------------
[arts-analyzer] Identifying EDTs for: arts_edt_2
[arts-analyzer] Other Function Found: 
    tail call void @llvm.experimental.noalias.scope.decl(metadata !137), !noelle.pdg.inst.id !140
[arts-analyzer] Other Function Found: 
    %3 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #4, !noalias !5, !noelle.pdg.inst.id !14
[arts-analyzer] Other Function Found: 
    %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %10, i32 noundef %11), !noelle.pdg.inst.id !30

 ---------------------------------------- 
[carts] Module: ; ModuleID = 'test_with_metadata.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.artsEdtDep_t = type { i32*, i32, i8* }
%struct.anon = type { i32*, i32* }

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@.str.3 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 !noelle.pdg.edges !5 {
entry:
  %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
  %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
  %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
  %0 = bitcast i32* %number to i8*, !noelle.pdg.inst.id !69
  store i32 1, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !7
  %1 = bitcast i32* %shared_number to i8*, !noelle.pdg.inst.id !74
  store i32 10000, i32* %shared_number, align 4, !tbaa !70, !noelle.pdg.inst.id !23
  %2 = bitcast i32* %random_number to i8*, !noelle.pdg.inst.id !75
  %call = tail call i32 @rand() #4, !noelle.pdg.inst.id !8
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !76
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !77
  store i32 %add, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !33
  %3 = bitcast i32* %NewRandom to i8*, !noelle.pdg.inst.id !78
  %call1 = tail call i32 @rand() #4, !noelle.pdg.inst.id !15
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !39
  %4 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0_guid.addr = alloca i32*, align 8
  store i32* %4, i32** %edt_0_guid.addr, align 8
  %edt_0_paramc = alloca i32, align 4
  store i32 0, i32* %edt_0_paramc, align 4
  %5 = load i32, i32* %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %5, align 8
  %6 = alloca i32, align 4
  store i32 4, i32* %6, align 4
  %7 = bitcast i32** %edt_0_guid.addr to i32*
  %8 = load i32, i32* %6, align 4
  %9 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_0, i32* %7, i32 %5, i64* %edt_0_paramv, i32 %8)
  %10 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !21
  %11 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !45
  %call2 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %10, i32 noundef %11), !noelle.pdg.inst.id !30
  ret i32 0, !noelle.pdg.inst.id !79
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare dso_local noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #3

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !80 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: inaccessiblememonly nofree nosync nounwind willreturn
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

define void @arts_edt_0(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %0 = bitcast i8** %depv.random_number to i32*
  %depv.NewRandom = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %1 = bitcast i8** %depv.NewRandom to i32*
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 2, i32 2
  %2 = bitcast i8** %depv.number to i32*
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 3, i32 2
  %3 = bitcast i8** %depv.shared_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %4 = bitcast i8* undef to i8**, !noelle.pdg.inst.id !82
  %5 = load i8*, i8** undef, align 8, !tbaa !83, !noelle.pdg.inst.id !88
  %6 = bitcast i8* %5 to i32**, !noelle.pdg.inst.id !89
  store i32* %2, i32** %6, align 8, !tbaa.struct !90, !noelle.pdg.inst.id !92
  %7 = getelementptr inbounds i8, i8* %5, i64 8, !noelle.pdg.inst.id !93
  %8 = bitcast i8* %7 to i32**, !noelle.pdg.inst.id !94
  store i32* %3, i32** %8, align 8, !tbaa.struct !95, !noelle.pdg.inst.id !96
  %9 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !97
  %10 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !98
  %11 = load i32, i32* %0, align 4, !tbaa !70, !noelle.pdg.inst.id !99
  store i32 %11, i32* %10, align 8, !tbaa !100, !noelle.pdg.inst.id !101
  %12 = getelementptr inbounds i8, i8* undef, i64 44, !noelle.pdg.inst.id !102
  %13 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !103
  %14 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !104
  store i32 %14, i32* %13, align 4, !tbaa !105, !noelle.pdg.inst.id !106
  %15 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1_guid.addr = alloca i32*, align 8
  store i32* %15, i32** %edt_1_guid.addr, align 8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, i32* %edt_1_paramc, align 4
  %16 = load i32, i32* %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %16, align 8
  %edt_1_paramv.0 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 0
  %17 = sext i32 %11 to i64
  store i64 %17, i64* %edt_1_paramv.0, align 8
  %edt_1_paramv.1 = getelementptr inbounds i64, i64* %edt_1_paramv, i64 1
  %18 = sext i32 %14 to i64
  store i64 %18, i64* %edt_1_paramv.1, align 8
  %19 = alloca i32, align 4
  store i32 2, i32* %19, align 4
  %20 = bitcast i32** %edt_1_guid.addr to i32*
  %21 = load i32, i32* %19, align 4
  %22 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %20, i32 %16, i64* %edt_1_paramv, i32 %21)
  %23 = getelementptr inbounds i8, i8* undef, i64 40, !noelle.pdg.inst.id !107
  %24 = bitcast i8* undef to i32*, !noelle.pdg.inst.id !108
  %25 = load i32, i32* %2, align 4, !tbaa !70, !noelle.pdg.inst.id !109
  store i32 %25, i32* %24, align 8, !tbaa !110, !noelle.pdg.inst.id !113
  %26 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2_guid.addr = alloca i32*, align 8
  store i32* %26, i32** %edt_2_guid.addr, align 8
  %edt_2_paramc = alloca i32, align 4
  store i32 1, i32* %edt_2_paramc, align 4
  %27 = load i32, i32* %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %27, align 8
  %edt_2_paramv.0 = getelementptr inbounds i64, i64* %edt_2_paramv, i64 0
  %28 = sext i32 %25 to i64
  store i64 %28, i64* %edt_2_paramv.0, align 8
  %29 = alloca i32, align 4
  store i32 0, i32* %29, align 4
  %30 = bitcast i32** %edt_2_guid.addr to i32*
  %31 = load i32, i32* %29, align 4
  %32 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_2, i32* %30, i32 %27, i64* %edt_2_paramv, i32 %31)
  br label %edt.exit
}

declare i32* @artsReserveGuidRoute(i32, i32)

declare i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

define void @arts_edt_1(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %paramv.0 = getelementptr inbounds i64, i64* %paramv, i64 0
  %0 = load i64, i64* %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  %paramv.1 = getelementptr inbounds i64, i64* %paramv, i64 1
  %2 = load i64, i64* %paramv.1, align 8
  %3 = trunc i64 %2 to i32
  %depv.0 = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %4 = bitcast i8** %depv.0 to i32*
  %depv.1 = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %5 = bitcast i8** %depv.1 to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %6 = load %struct.anon*, %struct.anon** undef, align 8, !tbaa !83, !noelle.pdg.inst.id !114
  %7 = getelementptr %struct.anon, %struct.anon* %6, i64 0, i32 0, !noelle.pdg.inst.id !115
  %8 = load i32*, i32** %7, align 8, !tbaa !116, !noelle.pdg.inst.id !118
  %9 = getelementptr %struct.anon, %struct.anon* %6, i64 0, i32 1, !noelle.pdg.inst.id !119
  %10 = load i32*, i32** %9, align 8, !tbaa !120, !noelle.pdg.inst.id !121
  tail call void @llvm.experimental.noalias.scope.decl(metadata !122), !noelle.pdg.inst.id !125
  %11 = load i32, i32* %4, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !126
  %12 = load i32, i32* %5, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !127
  %13 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !122, !noelle.pdg.inst.id !128
  %14 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !122, !noelle.pdg.inst.id !129
  %15 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %11, i32 noundef %12, i32 noundef %1, i32 noundef %3) #4, !noalias !122, !noelle.pdg.inst.id !130
  %16 = load i32, i32* %4, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !131
  %17 = add nsw i32 %16, 1, !noelle.pdg.inst.id !132
  store i32 %17, i32* %4, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !133
  %18 = load i32, i32* %5, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !134
  %19 = add nsw i32 %18, -1, !noelle.pdg.inst.id !135
  store i32 %19, i32* %5, align 4, !tbaa !70, !noalias !122, !noelle.pdg.inst.id !136
  br label %edt.exit
}

define void @arts_edt_2(i32 %paramc, i64* %paramv, i32 %depc, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %paramv.0 = getelementptr inbounds i64, i64* %paramv, i64 0
  %0 = load i64, i64* %paramv.0, align 8
  %1 = trunc i64 %0 to i32
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !137), !noelle.pdg.inst.id !140
  %2 = load i32, i32* undef, align 4, !tbaa !70, !alias.scope !137, !noelle.pdg.inst.id !141
  %3 = tail call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %1) #4, !noalias !137, !noelle.pdg.inst.id !142
  %4 = add nsw i32 %1, 1, !noelle.pdg.inst.id !143
  store i32 %4, i32* undef, align 4, !tbaa !70, !alias.scope !137, !noelle.pdg.inst.id !144
  br label %edt.exit
}

attributes #0 = { mustprogress norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nofree nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }
attributes #5 = { inaccessiblememonly nofree nosync nounwind willreturn }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}
!nvvm.annotations = !{}
!noelle.module.pdg = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.6"}
!4 = !{!"true"}
!5 = !{!6, !12, !14, !16, !17, !19, !20, !22, !24, !25, !26, !27, !28, !29, !31, !32, !35, !36, !37, !38, !40, !41, !42, !43, !44, !46, !47, !48, !49, !50, !51, !52, !53, !54, !55, !56, !57, !58, !59, !60, !61, !62, !63, !64}
!6 = !{!7, !8, !4, !9, !10, !9, !9, !11}
!7 = !{i64 6}
!8 = !{i64 12}
!9 = !{!"false"}
!10 = !{!"RAW"}
!11 = !{}
!12 = !{!7, !8, !4, !9, !13, !9, !9, !11}
!13 = !{!"WAW"}
!14 = !{!7, !15, !4, !9, !13, !9, !9, !11}
!15 = !{i64 18}
!16 = !{!7, !15, !4, !9, !10, !9, !9, !11}
!17 = !{!7, !18, !4, !9, !13, !9, !9, !11}
!18 = !{i64 20}
!19 = !{!7, !18, !4, !9, !10, !9, !9, !11}
!20 = !{!7, !21, !4, !9, !10, !9, !9, !11}
!21 = !{i64 21}
!22 = !{!23, !8, !4, !9, !10, !9, !9, !11}
!23 = !{i64 9}
!24 = !{!23, !8, !4, !9, !13, !9, !9, !11}
!25 = !{!23, !15, !4, !9, !10, !9, !9, !11}
!26 = !{!23, !15, !4, !9, !13, !9, !9, !11}
!27 = !{!23, !18, !4, !9, !10, !9, !9, !11}
!28 = !{!23, !18, !4, !9, !13, !9, !9, !11}
!29 = !{!8, !30, !4, !9, !13, !9, !9, !11}
!30 = !{i64 23}
!31 = !{!8, !30, !4, !9, !10, !9, !9, !11}
!32 = !{!8, !33, !4, !9, !34, !9, !9, !11}
!33 = !{i64 15}
!34 = !{!"WAR"}
!35 = !{!8, !33, !4, !9, !13, !9, !9, !11}
!36 = !{!8, !15, !4, !9, !10, !9, !9, !11}
!37 = !{!8, !15, !4, !9, !13, !9, !9, !11}
!38 = !{!8, !39, !4, !9, !34, !9, !9, !11}
!39 = !{i64 19}
!40 = !{!8, !39, !4, !9, !13, !9, !9, !11}
!41 = !{!8, !18, !4, !9, !10, !9, !9, !11}
!42 = !{!8, !18, !4, !9, !13, !9, !9, !11}
!43 = !{!8, !21, !4, !9, !10, !9, !9, !11}
!44 = !{!8, !45, !4, !9, !10, !9, !9, !11}
!45 = !{i64 22}
!46 = !{!33, !15, !4, !9, !10, !9, !9, !11}
!47 = !{!33, !15, !4, !9, !13, !9, !9, !11}
!48 = !{!33, !18, !4, !9, !10, !9, !9, !11}
!49 = !{!33, !18, !4, !9, !13, !9, !9, !11}
!50 = !{!33, !45, !4, !9, !10, !9, !9, !11}
!51 = !{!15, !30, !4, !9, !10, !9, !9, !11}
!52 = !{!15, !30, !4, !9, !13, !9, !9, !11}
!53 = !{!15, !39, !4, !9, !34, !9, !9, !11}
!54 = !{!15, !39, !4, !9, !13, !9, !9, !11}
!55 = !{!15, !18, !4, !9, !10, !9, !9, !11}
!56 = !{!15, !18, !4, !9, !13, !9, !9, !11}
!57 = !{!15, !21, !4, !9, !10, !9, !9, !11}
!58 = !{!15, !45, !4, !9, !10, !9, !9, !11}
!59 = !{!39, !18, !4, !9, !10, !9, !9, !11}
!60 = !{!39, !18, !4, !9, !13, !9, !9, !11}
!61 = !{!18, !30, !4, !9, !13, !9, !9, !11}
!62 = !{!18, !30, !4, !9, !10, !9, !9, !11}
!63 = !{!18, !21, !4, !9, !10, !9, !9, !11}
!64 = !{!18, !45, !4, !9, !10, !9, !9, !11}
!65 = !{i64 0}
!66 = !{i64 1}
!67 = !{i64 2}
!68 = !{i64 3}
!69 = !{i64 4}
!70 = !{!71, !71, i64 0}
!71 = !{!"int", !72, i64 0}
!72 = !{!"omnipotent char", !73, i64 0}
!73 = !{!"Simple C++ TBAA"}
!74 = !{i64 7}
!75 = !{i64 10}
!76 = !{i64 13}
!77 = !{i64 14}
!78 = !{i64 16}
!79 = !{i64 28}
!80 = !{!81}
!81 = !{i64 2, i64 -1, i64 -1, i1 true}
!82 = !{i64 37}
!83 = !{!84, !86, i64 0}
!84 = !{!"_ZTS24kmp_task_t_with_privates", !85, i64 0, !87, i64 40}
!85 = !{!"_ZTS10kmp_task_t", !86, i64 0, !86, i64 8, !71, i64 16, !72, i64 24, !72, i64 32}
!86 = !{!"any pointer", !72, i64 0}
!87 = !{!"_ZTS15.kmp_privates.t", !71, i64 0, !71, i64 4}
!88 = !{i64 38}
!89 = !{i64 39}
!90 = !{i64 0, i64 8, !91, i64 8, i64 8, !91}
!91 = !{!86, !86, i64 0}
!92 = !{i64 40}
!93 = !{i64 41}
!94 = !{i64 42}
!95 = !{i64 0, i64 8, !91}
!96 = !{i64 43}
!97 = !{i64 44}
!98 = !{i64 45}
!99 = !{i64 46}
!100 = !{!84, !71, i64 40}
!101 = !{i64 47}
!102 = !{i64 48}
!103 = !{i64 49}
!104 = !{i64 50}
!105 = !{!84, !71, i64 44}
!106 = !{i64 51}
!107 = !{i64 54}
!108 = !{i64 55}
!109 = !{i64 56}
!110 = !{!111, !71, i64 40}
!111 = !{!"_ZTS24kmp_task_t_with_privates", !85, i64 0, !112, i64 40}
!112 = !{!"_ZTS15.kmp_privates.t", !71, i64 0}
!113 = !{i64 57}
!114 = !{i64 63}
!115 = !{i64 64}
!116 = !{!117, !86, i64 0}
!117 = !{!"_ZTSZ4mainE3$_0", !86, i64 0, !86, i64 8}
!118 = !{i64 65}
!119 = !{i64 66}
!120 = !{!117, !86, i64 8}
!121 = !{i64 67}
!122 = !{!123}
!123 = distinct !{!123, !124, !".omp_outlined..1: %.privates."}
!124 = distinct !{!124, !".omp_outlined..1"}
!125 = !{i64 68}
!126 = !{i64 71}
!127 = !{i64 72}
!128 = !{i64 73}
!129 = !{i64 74}
!130 = !{i64 75}
!131 = !{i64 76}
!132 = !{i64 77}
!133 = !{i64 78}
!134 = !{i64 79}
!135 = !{i64 80}
!136 = !{i64 81}
!137 = !{!138}
!138 = distinct !{!138, !139, !".omp_outlined..2: %.privates."}
!139 = distinct !{!139, !".omp_outlined..2"}
!140 = !{i64 85}
!141 = !{i64 87}
!142 = !{i64 88}
!143 = !{i64 89}
!144 = !{i64 90}
# noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
# noelle-load -debug-only=arts,carts,arts-analyzer -load-pass-plugin /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -passes=CARTS test_with_metadata.bc -o test_opt.bc
llvm-dis test_opt.bc
clang++ -fopenmp test_opt.bc -O3 -march=native -o test_opt -lstdc++
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: /tmp/test_opt-6e3cc2.o: in function `main':
test.cpp:(.text+0x3a): undefined reference to `artsReserveGuidRoute'
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: test.cpp:(.text+0x5b): undefined reference to `artsEdtCreateWithGuid'
clang-14: error: linker command failed with exit code 1 (use -v to see invocation)
make: *** [Makefile:32: test_opt] Error 1
