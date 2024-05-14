clang++ -fopenmp -O3 -g0 -Xclang -disable-llvm-passes -emit-llvm -c test.cpp -o test.bc -lstdc++
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

[arts-analyzer] Identifying EDTs for: main
[arts-analyzer] Other Function Found: 
    %call = call i32 @rand() #8, !noelle.pdg.inst.id !20
[arts-analyzer] Other Function Found: 
    %call1 = call i32 @rand() #8, !noelle.pdg.inst.id !8
[arts-analyzer] Parallel Region Found: 
    call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number), !noelle.pdg.inst.id !15

[arts-analyzer] Handling parallel region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Created ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Created ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
[arts-utils]   - Replacing uses of: @1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
[arts-utils]    - Removing global variable: @1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
[arts-utils]   - Replacing uses of: void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*)
[arts-utils]    - Removing instruction:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* undef, i32 4, void (i32*, i32*, ...)* undef, i32* undef, i32* undef, i32* undef, i32* undef), !noelle.pdg.inst.id !15
[arts-utils]   - Replacing uses of:   call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* undef, i32 4, void (i32*, i32*, ...)* undef, i32* undef, i32* undef, i32* undef, i32* undef), !noelle.pdg.inst.id !15

[arts-analyzer] Handling done region
[arts-analyzer] Creating EDT with signature: void (i32, i64*, i32, %struct.artsEdtDep_t*)
[arts-ir-builder] Initializing EDT
[arts-ir-builder] Inserting EDT Entry
[arts-ir-builder] Inserting EDT Call
[arts-ir-builder] Found ARTS runtime function artsReserveGuidRoute with type i32* (i32, i32)
[arts-ir-builder] Found ARTS runtime function artsEdtCreateWithGuid with type i32* (void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)
 - NEW MODULE: ; ModuleID = 'test_with_metadata.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.anon = type { i32*, i32* }
%struct..kmp_privates.t = type { i32, i32 }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { i8*, i32 (i32, i8*)*, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { i32 (i32, i8*)* }
%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.anon.0 = type { i8 }
%struct..kmp_privates.t.2 = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct.artsEdtDep_t = type { i32*, i32, i8* }

$__clang_call_terminate = comdat any

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@.str.3 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() #0 !noelle.pdg.edges !5 {
entry:
  %number = alloca i32, align 4, !noelle.pdg.inst.id !65
  %shared_number = alloca i32, align 4, !noelle.pdg.inst.id !66
  %random_number = alloca i32, align 4, !noelle.pdg.inst.id !67
  %NewRandom = alloca i32, align 4, !noelle.pdg.inst.id !68
  %0 = bitcast i32* %number to i8*, !noelle.pdg.inst.id !69
  store i32 1, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !7
  %1 = bitcast i32* %shared_number to i8*, !noelle.pdg.inst.id !74
  store i32 10000, i32* %shared_number, align 4, !tbaa !70, !noelle.pdg.inst.id !47
  %2 = bitcast i32* %random_number to i8*, !noelle.pdg.inst.id !75
  %call = call i32 @rand() #8, !noelle.pdg.inst.id !20
  %rem = srem i32 %call, 10, !noelle.pdg.inst.id !76
  %add = add nsw i32 %rem, 10, !noelle.pdg.inst.id !77
  store i32 %add, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !23
  %3 = bitcast i32* %NewRandom to i8*, !noelle.pdg.inst.id !78
  %call1 = call i32 @rand() #8, !noelle.pdg.inst.id !8
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !70, !noelle.pdg.inst.id !33
  %4 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0_guid.addr = alloca i32*, align 8
  store i32* %4, i32** %edt_0_guid.addr, align 8
  %edt_0_paramc = alloca i32, align 4
  store i32 0, i32* %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64*, align 8
  %depc = alloca i32, align 4
  store i32 4, i32* %depc, align 4
  %5 = bitcast i32** %edt_0_guid.addr to i32*
  %6 = load i32, i32* %edt_0_paramc, align 4
  %7 = bitcast i64** %edt_0_paramv to i64*
  %8 = load i32, i32* %depc, align 4
  %9 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_0, i32* %5, i32 %6, i64* %7, i32 %8)
  %10 = call i32* @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1_guid.addr = alloca i32*, align 8
  store i32* %10, i32** %edt_1_guid.addr, align 8
  %edt_1_paramc = alloca i32, align 4
  store i32 0, i32* %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64*, align 8
  %depc1 = alloca i32, align 4
  store i32 4, i32* %depc1, align 4
  %11 = bitcast i32** %edt_1_guid.addr to i32*
  %12 = load i32, i32* %edt_1_paramc, align 4
  %13 = bitcast i64** %edt_1_paramv to i64*
  %14 = load i32, i32* %depc1, align 4
  %15 = call i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)* @arts_edt_1, i32* %11, i32 %12, i64* %13, i32 %14)
  ret void
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %.global_tid., i32* noalias nocapture noundef readnone %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias nocapture noundef readonly %.copy_fn., i8* nocapture noundef readnone %.task_t., %struct.anon* noalias nocapture noundef readonly %__context) #3 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !noelle.pdg.args.id !79 !noelle.pdg.edges !86 {
entry:
  %.firstpriv.ptr.addr = alloca i32*, align 8, !noelle.pdg.inst.id !163
  %.firstpriv.ptr.addr1 = alloca i32*, align 8, !noelle.pdg.inst.id !164
  %0 = bitcast void (i8*, ...)* %.copy_fn. to void (i8*, i32**, i32**)*, !noelle.pdg.inst.id !165
  call void %0(i8* %.privates., i32** %.firstpriv.ptr.addr, i32** %.firstpriv.ptr.addr1), !noelle.pdg.inst.id !88
  %1 = load i32*, i32** %.firstpriv.ptr.addr, align 8, !noelle.pdg.inst.id !92
  %2 = load i32*, i32** %.firstpriv.ptr.addr1, align 8, !noelle.pdg.inst.id !94
  %3 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 0, !noelle.pdg.inst.id !166
  %4 = load i32*, i32** %3, align 8, !tbaa !167, !noelle.pdg.inst.id !96
  %5 = load i32, i32* %4, align 4, !tbaa !70, !noelle.pdg.inst.id !98
  %6 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 1, !noelle.pdg.inst.id !170
  %7 = load i32*, i32** %6, align 8, !tbaa !171, !noelle.pdg.inst.id !100
  %8 = load i32, i32* %7, align 4, !tbaa !70, !noelle.pdg.inst.id !102
  %9 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !104
  %10 = load i32, i32* %2, align 4, !tbaa !70, !noelle.pdg.inst.id !106
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %5, i32 noundef %8, i32 noundef %9, i32 noundef %10)
          to label %invoke.cont unwind label %lpad, !noelle.pdg.inst.id !108

invoke.cont:                                      ; preds = %entry
  %11 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 0, !noelle.pdg.inst.id !172
  %12 = load i32*, i32** %11, align 8, !tbaa !167, !noelle.pdg.inst.id !111
  %13 = load i32, i32* %12, align 4, !tbaa !70, !noelle.pdg.inst.id !113
  %inc = add nsw i32 %13, 1, !noelle.pdg.inst.id !173
  store i32 %inc, i32* %12, align 4, !tbaa !70, !noelle.pdg.inst.id !115
  %14 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 1, !noelle.pdg.inst.id !174
  %15 = load i32*, i32** %14, align 8, !tbaa !171, !noelle.pdg.inst.id !118
  %16 = load i32, i32* %15, align 4, !tbaa !70, !noelle.pdg.inst.id !120
  %dec = add nsw i32 %16, -1, !noelle.pdg.inst.id !175
  store i32 %dec, i32* %15, align 4, !tbaa !70, !noelle.pdg.inst.id !122
  ret void, !noelle.pdg.inst.id !176

lpad:                                             ; preds = %entry
  %17 = landingpad { i8*, i32 }
          catch i8* null, !noelle.pdg.inst.id !177
  %18 = extractvalue { i8*, i32 } %17, 0, !noelle.pdg.inst.id !178
  %19 = extractvalue { i8*, i32 } %17, 1, !noelle.pdg.inst.id !179
  br label %terminate.handler, !noelle.pdg.inst.id !180

terminate.handler:                                ; preds = %lpad
  call void @__clang_call_terminate(i8* %18) #10, !noelle.pdg.inst.id !89
  unreachable, !noelle.pdg.inst.id !181
}

declare dso_local i32 @printf(i8* noundef, ...) #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) #5 comdat !noelle.pdg.args.id !182 !noelle.pdg.edges !184 {
  %2 = call i8* @__cxa_begin_catch(i8* %0) #8, !noelle.pdg.inst.id !186
  call void @_ZSt9terminatev() #10, !noelle.pdg.inst.id !187
  unreachable, !noelle.pdg.inst.id !189
}

declare dso_local i8* @__cxa_begin_catch(i8*)

declare dso_local void @_ZSt9terminatev()

; Function Attrs: alwaysinline mustprogress nofree norecurse nosync nounwind uwtable willreturn writeonly
define internal void @.omp_task_privates_map.(%struct..kmp_privates.t* noalias noundef %0, i32** noalias nocapture noundef writeonly %1, i32** noalias nocapture noundef writeonly %2) #6 !noelle.pdg.args.id !190 !noelle.pdg.edges !194 {
entry:
  %3 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %0, i32 0, i32 0, !noelle.pdg.inst.id !198
  store i32* %3, i32** %1, align 8, !tbaa !199, !noelle.pdg.inst.id !196
  %4 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %0, i32 0, i32 1, !noelle.pdg.inst.id !200
  store i32* %4, i32** %2, align 8, !tbaa !199, !noelle.pdg.inst.id !197
  ret void, !noelle.pdg.inst.id !201
}

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias noundef %1) #7 !noelle.pdg.args.id !202 !noelle.pdg.edges !205 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i32 0, i32 0, !noelle.pdg.inst.id !209
  %3 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 2, !noelle.pdg.inst.id !210
  %4 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 0, !noelle.pdg.inst.id !211
  %5 = load i8*, i8** %4, align 8, !tbaa !212, !noelle.pdg.inst.id !207
  %6 = bitcast i8* %5 to %struct.anon*, !noelle.pdg.inst.id !216
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i32 0, i32 1, !noelle.pdg.inst.id !217
  %8 = bitcast %struct..kmp_privates.t* %7 to i8*, !noelle.pdg.inst.id !218
  %9 = bitcast %struct.kmp_task_t_with_privates* %1 to i8*, !noelle.pdg.inst.id !219
  call void @.omp_outlined..1(i32 %0, i32* %3, i8* %8, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t*, i32**, i32**)* @.omp_task_privates_map. to void (i8*, ...)*), i8* %9, %struct.anon* %6) #8, !noelle.pdg.inst.id !208
  ret i32 0, !noelle.pdg.inst.id !220
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) #8

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #9

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) #8

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined..2(i32 noundef %.global_tid., i32* noalias nocapture noundef readnone %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias nocapture noundef readonly %.copy_fn., i8* nocapture noundef readnone %.task_t., %struct.anon.0* noalias nocapture noundef readnone %__context) #3 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !noelle.pdg.args.id !221 !noelle.pdg.edges !228 {
entry:
  %.firstpriv.ptr.addr = alloca i32*, align 8, !noelle.pdg.inst.id !254
  %0 = bitcast void (i8*, ...)* %.copy_fn. to void (i8*, i32**)*, !noelle.pdg.inst.id !255
  call void %0(i8* %.privates., i32** %.firstpriv.ptr.addr), !noelle.pdg.inst.id !230
  %1 = load i32*, i32** %.firstpriv.ptr.addr, align 8, !noelle.pdg.inst.id !231
  %2 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !233
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %2)
          to label %invoke.cont unwind label %lpad, !noelle.pdg.inst.id !235

invoke.cont:                                      ; preds = %entry
  %3 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !238
  %inc = add nsw i32 %3, 1, !noelle.pdg.inst.id !256
  store i32 %inc, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !240
  ret void, !noelle.pdg.inst.id !257

lpad:                                             ; preds = %entry
  %4 = landingpad { i8*, i32 }
          catch i8* null, !noelle.pdg.inst.id !258
  %5 = extractvalue { i8*, i32 } %4, 0, !noelle.pdg.inst.id !259
  %6 = extractvalue { i8*, i32 } %4, 1, !noelle.pdg.inst.id !260
  br label %terminate.handler, !noelle.pdg.inst.id !261

terminate.handler:                                ; preds = %lpad
  call void @__clang_call_terminate(i8* %5) #10, !noelle.pdg.inst.id !243
  unreachable, !noelle.pdg.inst.id !262
}

; Function Attrs: alwaysinline mustprogress nofree norecurse nosync nounwind uwtable willreturn writeonly
define internal void @.omp_task_privates_map..4(%struct..kmp_privates.t.2* noalias noundef %0, i32** noalias nocapture noundef writeonly %1) #6 !noelle.pdg.args.id !263 {
entry:
  %2 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %0, i32 0, i32 0, !noelle.pdg.inst.id !266
  store i32* %2, i32** %1, align 8, !tbaa !199, !noelle.pdg.inst.id !267
  ret void, !noelle.pdg.inst.id !268
}

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias noundef %1) #7 !noelle.pdg.args.id !269 !noelle.pdg.edges !272 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i32 0, i32 0, !noelle.pdg.inst.id !276
  %3 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 2, !noelle.pdg.inst.id !277
  %4 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 0, !noelle.pdg.inst.id !278
  %5 = load i8*, i8** %4, align 8, !tbaa !279, !noelle.pdg.inst.id !274
  %6 = bitcast i8* %5 to %struct.anon.0*, !noelle.pdg.inst.id !282
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i32 0, i32 1, !noelle.pdg.inst.id !283
  %8 = bitcast %struct..kmp_privates.t.2* %7 to i8*, !noelle.pdg.inst.id !284
  %9 = bitcast %struct.kmp_task_t_with_privates.1* %1 to i8*, !noelle.pdg.inst.id !285
  call void @.omp_outlined..2(i32 %0, i32* %3, i8* %8, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t.2*, i32**)* @.omp_task_privates_map..4 to void (i8*, ...)*), i8* %9, %struct.anon.0* %6) #8, !noelle.pdg.inst.id !275
  ret i32 0, !noelle.pdg.inst.id !286
}

; Function Attrs: nounwind
declare !callback !287 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) #8

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

define void @arts_edt_0(i32 %paramc, i64* %depc, i32 %paramv, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %depv.random_number.casted = bitcast i8** %depv.random_number to i32*
  %depv.NewRandom = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %depv.NewRandom.casted = bitcast i8** %depv.NewRandom to i32*
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 2, i32 2
  %depv.number.casted = bitcast i8** %depv.number to i32*
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 3, i32 2
  %depv.shared_number.casted = bitcast i8** %depv.shared_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %0 = alloca %struct.anon, align 8, !noelle.pdg.inst.id !289
  %1 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 0, !noelle.pdg.inst.id !290
  store i32* %depv.number.casted, i32** %1, align 8, !tbaa !199, !noelle.pdg.inst.id !291
  %2 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 1, !noelle.pdg.inst.id !292
  store i32* %depv.shared_number.casted, i32** %2, align 8, !tbaa !199, !noelle.pdg.inst.id !293
  %3 = load i32, i32* undef, align 4, !tbaa !70, !noelle.pdg.inst.id !294
  %4 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* undef, i32 %3, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !295
  %5 = bitcast i8* %4 to %struct.kmp_task_t_with_privates*, !noelle.pdg.inst.id !296
  %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %5, i32 0, i32 0, !noelle.pdg.inst.id !297
  %7 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %6, i32 0, i32 0, !noelle.pdg.inst.id !298
  %8 = load i8*, i8** %7, align 8, !tbaa !212, !noelle.pdg.inst.id !299
  %9 = bitcast %struct.anon* %0 to i8*, !noelle.pdg.inst.id !300
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %8, i8* align 8 %9, i64 16, i1 false), !tbaa.struct !301, !noelle.pdg.inst.id !302
  %10 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %5, i32 0, i32 1, !noelle.pdg.inst.id !303
  %11 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %10, i32 0, i32 0, !noelle.pdg.inst.id !304
  %12 = load i32, i32* %depv.random_number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !305
  store i32 %12, i32* %11, align 8, !tbaa !306, !noelle.pdg.inst.id !307
  %13 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %10, i32 0, i32 1, !noelle.pdg.inst.id !308
  %14 = load i32, i32* %depv.NewRandom.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !309
  store i32 %14, i32* %13, align 4, !tbaa !310, !noelle.pdg.inst.id !311
  %15 = call i32 @__kmpc_omp_task(%struct.ident_t* undef, i32 %3, i8* %4), !noelle.pdg.inst.id !312
  %16 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* undef, i32 %3, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !313
  %17 = bitcast i8* %16 to %struct.kmp_task_t_with_privates.1*, !noelle.pdg.inst.id !314
  %18 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %17, i32 0, i32 0, !noelle.pdg.inst.id !315
  %19 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %17, i32 0, i32 1, !noelle.pdg.inst.id !316
  %20 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %19, i32 0, i32 0, !noelle.pdg.inst.id !317
  %21 = load i32, i32* %depv.number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !318
  store i32 %21, i32* %20, align 8, !tbaa !319, !noelle.pdg.inst.id !320
  %22 = call i32 @__kmpc_omp_task(%struct.ident_t* undef, i32 %3, i8* %16), !noelle.pdg.inst.id !321
  br label %edt.exit
}

declare i32* @artsReserveGuidRoute(i32, i32)

declare i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

define void @arts_edt_1(i32 %paramc, i64* %depc, i32 %paramv, %struct.artsEdtDep_t* %depv) {
edt.entry:
  %depv.number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 0, i32 2
  %depv.number.casted = bitcast i8** %depv.number to i32*
  %depv.random_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 1, i32 2
  %depv.random_number.casted = bitcast i8** %depv.random_number to i32*
  %depv.NewRandom = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 2, i32 2
  %depv.NewRandom.casted = bitcast i8** %depv.NewRandom to i32*
  %depv.shared_number = getelementptr inbounds %struct.artsEdtDep_t, %struct.artsEdtDep_t* %depv, i32 3, i32 2
  %depv.shared_number.casted = bitcast i8** %depv.shared_number to i32*
  br label %edt.body

edt.exit:                                         ; preds = %edt.body
  ret void

edt.body:                                         ; preds = %edt.entry
  %0 = load i32, i32* %depv.number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !18
  %1 = load i32, i32* %depv.random_number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !28
  %2 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %0, i32 noundef %1), !noelle.pdg.inst.id !30
  %3 = bitcast i32* %depv.NewRandom.casted to i8*, !noelle.pdg.inst.id !322
  %4 = bitcast i32* %depv.random_number.casted to i8*, !noelle.pdg.inst.id !323
  %5 = bitcast i32* %depv.shared_number.casted to i8*, !noelle.pdg.inst.id !324
  %6 = bitcast i32* %depv.number.casted to i8*, !noelle.pdg.inst.id !325
  br label %edt.exit
}

attributes #0 = { mustprogress norecurse uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { alwaysinline norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { noinline noreturn nounwind }
attributes #6 = { alwaysinline mustprogress nofree norecurse nosync nounwind uwtable willreturn writeonly "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nounwind }
attributes #9 = { argmemonly nofree nounwind willreturn }
attributes #10 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}
!noelle.module.pdg = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.6"}
!4 = !{!"true"}
!5 = !{!6, !12, !14, !16, !17, !19, !21, !22, !24, !25, !26, !27, !29, !31, !32, !35, !36, !37, !38, !39, !40, !41, !42, !43, !44, !45, !46, !48, !49, !50, !51, !52, !53, !54, !55, !56, !57, !58, !59, !60, !61, !62, !63, !64}
!6 = !{!7, !8, !4, !9, !10, !9, !9, !11}
!7 = !{i64 6}
!8 = !{i64 18}
!9 = !{!"false"}
!10 = !{!"RAW"}
!11 = !{}
!12 = !{!7, !8, !4, !9, !13, !9, !9, !11}
!13 = !{!"WAW"}
!14 = !{!7, !15, !4, !9, !10, !9, !9, !11}
!15 = !{i64 20}
!16 = !{!7, !15, !4, !9, !13, !9, !9, !11}
!17 = !{!7, !18, !4, !9, !10, !9, !9, !11}
!18 = !{i64 21}
!19 = !{!7, !20, !4, !9, !10, !9, !9, !11}
!20 = !{i64 12}
!21 = !{!7, !20, !4, !9, !13, !9, !9, !11}
!22 = !{!23, !8, !4, !9, !10, !9, !9, !11}
!23 = !{i64 15}
!24 = !{!23, !8, !4, !9, !13, !9, !9, !11}
!25 = !{!23, !15, !4, !9, !10, !9, !9, !11}
!26 = !{!23, !15, !4, !9, !13, !9, !9, !11}
!27 = !{!23, !28, !4, !9, !10, !9, !9, !11}
!28 = !{i64 22}
!29 = !{!8, !30, !4, !9, !10, !9, !9, !11}
!30 = !{i64 23}
!31 = !{!8, !30, !4, !9, !13, !9, !9, !11}
!32 = !{!8, !33, !4, !9, !34, !9, !9, !11}
!33 = !{i64 19}
!34 = !{!"WAR"}
!35 = !{!8, !33, !4, !9, !13, !9, !9, !11}
!36 = !{!8, !15, !4, !9, !13, !9, !9, !11}
!37 = !{!8, !15, !4, !9, !10, !9, !9, !11}
!38 = !{!8, !18, !4, !9, !10, !9, !9, !11}
!39 = !{!8, !28, !4, !9, !10, !9, !9, !11}
!40 = !{!33, !15, !4, !9, !13, !9, !9, !11}
!41 = !{!33, !15, !4, !9, !10, !9, !9, !11}
!42 = !{!15, !30, !4, !9, !10, !9, !9, !11}
!43 = !{!15, !30, !4, !9, !13, !9, !9, !11}
!44 = !{!15, !18, !4, !9, !10, !9, !9, !11}
!45 = !{!15, !28, !4, !9, !10, !9, !9, !11}
!46 = !{!47, !8, !4, !9, !10, !9, !9, !11}
!47 = !{i64 9}
!48 = !{!47, !8, !4, !9, !13, !9, !9, !11}
!49 = !{!47, !15, !4, !9, !10, !9, !9, !11}
!50 = !{!47, !15, !4, !9, !13, !9, !9, !11}
!51 = !{!47, !20, !4, !9, !13, !9, !9, !11}
!52 = !{!47, !20, !4, !9, !10, !9, !9, !11}
!53 = !{!20, !30, !4, !9, !10, !9, !9, !11}
!54 = !{!20, !30, !4, !9, !13, !9, !9, !11}
!55 = !{!20, !23, !4, !9, !34, !9, !9, !11}
!56 = !{!20, !23, !4, !9, !13, !9, !9, !11}
!57 = !{!20, !8, !4, !9, !10, !9, !9, !11}
!58 = !{!20, !8, !4, !9, !13, !9, !9, !11}
!59 = !{!20, !33, !4, !9, !34, !9, !9, !11}
!60 = !{!20, !33, !4, !9, !13, !9, !9, !11}
!61 = !{!20, !15, !4, !9, !10, !9, !9, !11}
!62 = !{!20, !15, !4, !9, !13, !9, !9, !11}
!63 = !{!20, !18, !4, !9, !10, !9, !9, !11}
!64 = !{!20, !28, !4, !9, !10, !9, !9, !11}
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
!79 = !{!80, !81, !82, !83, !84, !85}
!80 = !{i64 69}
!81 = !{i64 70}
!82 = !{i64 71}
!83 = !{i64 72}
!84 = !{i64 73}
!85 = !{i64 74}
!86 = !{!87, !90, !91, !93, !95, !97, !99, !101, !103, !105, !107, !109, !110, !112, !114, !116, !117, !119, !121, !123, !124, !125, !126, !127, !128, !129, !130, !131, !132, !133, !134, !135, !136, !137, !138, !139, !140, !141, !142, !143, !144, !145, !146, !147, !148, !149, !150, !151, !152, !153, !154, !155, !156, !157, !158, !159, !160, !161, !162}
!87 = !{!88, !89, !4, !9, !10, !9, !9, !11}
!88 = !{i64 78}
!89 = !{i64 105}
!90 = !{!88, !89, !4, !9, !13, !9, !9, !11}
!91 = !{!88, !92, !4, !9, !10, !9, !9, !11}
!92 = !{i64 79}
!93 = !{!88, !94, !4, !9, !10, !9, !9, !11}
!94 = !{i64 80}
!95 = !{!88, !96, !4, !9, !10, !9, !9, !11}
!96 = !{i64 82}
!97 = !{!88, !98, !4, !9, !10, !9, !9, !11}
!98 = !{i64 83}
!99 = !{!88, !100, !4, !9, !10, !9, !9, !11}
!100 = !{i64 85}
!101 = !{!88, !102, !4, !9, !10, !9, !9, !11}
!102 = !{i64 86}
!103 = !{!88, !104, !4, !9, !10, !9, !9, !11}
!104 = !{i64 87}
!105 = !{!88, !106, !4, !9, !10, !9, !9, !11}
!106 = !{i64 88}
!107 = !{!88, !108, !4, !9, !10, !9, !9, !11}
!108 = !{i64 89}
!109 = !{!88, !108, !4, !9, !13, !9, !9, !11}
!110 = !{!88, !111, !4, !9, !10, !9, !9, !11}
!111 = !{i64 91}
!112 = !{!88, !113, !4, !9, !10, !9, !9, !11}
!113 = !{i64 92}
!114 = !{!88, !115, !4, !9, !34, !9, !9, !11}
!115 = !{i64 94}
!116 = !{!88, !115, !4, !9, !13, !9, !9, !11}
!117 = !{!88, !118, !4, !9, !10, !9, !9, !11}
!118 = !{i64 96}
!119 = !{!88, !120, !4, !9, !10, !9, !9, !11}
!120 = !{i64 97}
!121 = !{!88, !122, !4, !9, !34, !9, !9, !11}
!122 = !{i64 99}
!123 = !{!88, !122, !4, !9, !13, !9, !9, !11}
!124 = !{!92, !89, !4, !9, !34, !9, !9, !11}
!125 = !{!92, !115, !4, !9, !34, !9, !9, !11}
!126 = !{!92, !122, !4, !9, !34, !9, !9, !11}
!127 = !{!94, !89, !4, !9, !34, !9, !9, !11}
!128 = !{!94, !115, !4, !9, !34, !9, !9, !11}
!129 = !{!94, !122, !4, !9, !34, !9, !9, !11}
!130 = !{!96, !89, !4, !9, !34, !9, !9, !11}
!131 = !{!96, !115, !4, !9, !34, !9, !9, !11}
!132 = !{!96, !122, !4, !9, !34, !9, !9, !11}
!133 = !{!98, !89, !4, !9, !34, !9, !9, !11}
!134 = !{!98, !115, !4, !9, !34, !9, !9, !11}
!135 = !{!98, !122, !4, !9, !34, !9, !9, !11}
!136 = !{!100, !89, !4, !9, !34, !9, !9, !11}
!137 = !{!100, !115, !4, !9, !34, !9, !9, !11}
!138 = !{!100, !122, !4, !9, !34, !9, !9, !11}
!139 = !{!102, !89, !4, !9, !34, !9, !9, !11}
!140 = !{!102, !115, !4, !9, !34, !9, !9, !11}
!141 = !{!102, !122, !4, !9, !34, !9, !9, !11}
!142 = !{!104, !89, !4, !9, !34, !9, !9, !11}
!143 = !{!104, !115, !4, !9, !34, !9, !9, !11}
!144 = !{!104, !122, !4, !9, !34, !9, !9, !11}
!145 = !{!106, !89, !4, !9, !34, !9, !9, !11}
!146 = !{!106, !115, !4, !9, !34, !9, !9, !11}
!147 = !{!106, !122, !4, !9, !34, !9, !9, !11}
!148 = !{!108, !89, !4, !9, !13, !9, !9, !11}
!149 = !{!108, !89, !4, !9, !10, !9, !9, !11}
!150 = !{!108, !115, !4, !9, !13, !9, !9, !11}
!151 = !{!108, !115, !4, !9, !34, !9, !9, !11}
!152 = !{!108, !122, !4, !9, !34, !9, !9, !11}
!153 = !{!108, !122, !4, !9, !13, !9, !9, !11}
!154 = !{!111, !115, !4, !9, !34, !9, !9, !11}
!155 = !{!111, !122, !4, !9, !34, !9, !9, !11}
!156 = !{!113, !115, !4, !9, !34, !9, !9, !11}
!157 = !{!113, !122, !4, !9, !34, !9, !9, !11}
!158 = !{!115, !118, !4, !9, !10, !9, !9, !11}
!159 = !{!115, !120, !4, !9, !10, !9, !9, !11}
!160 = !{!115, !122, !4, !9, !13, !9, !9, !11}
!161 = !{!118, !122, !4, !9, !34, !9, !9, !11}
!162 = !{!120, !122, !4, !9, !34, !9, !9, !11}
!163 = !{i64 75}
!164 = !{i64 76}
!165 = !{i64 77}
!166 = !{i64 81}
!167 = !{!168, !169, i64 0}
!168 = !{!"_ZTSZ4mainE3$_0", !169, i64 0, !169, i64 8}
!169 = !{!"any pointer", !72, i64 0}
!170 = !{i64 84}
!171 = !{!168, !169, i64 8}
!172 = !{i64 90}
!173 = !{i64 93}
!174 = !{i64 95}
!175 = !{i64 98}
!176 = !{i64 100}
!177 = !{i64 101}
!178 = !{i64 102}
!179 = !{i64 103}
!180 = !{i64 104}
!181 = !{i64 106}
!182 = !{!183}
!183 = !{i64 107}
!184 = !{!185, !188}
!185 = !{!186, !187, !4, !9, !10, !9, !9, !11}
!186 = !{i64 108}
!187 = !{i64 109}
!188 = !{!186, !187, !4, !9, !13, !9, !9, !11}
!189 = !{i64 110}
!190 = !{!191, !192, !193}
!191 = !{i64 111}
!192 = !{i64 112}
!193 = !{i64 113}
!194 = !{!195}
!195 = !{!196, !197, !4, !9, !13, !9, !9, !11}
!196 = !{i64 115}
!197 = !{i64 117}
!198 = !{i64 114}
!199 = !{!169, !169, i64 0}
!200 = !{i64 116}
!201 = !{i64 118}
!202 = !{!203, !204}
!203 = !{i64 119}
!204 = !{i64 120}
!205 = !{!206}
!206 = !{!207, !208, !4, !9, !34, !9, !9, !11}
!207 = !{i64 124}
!208 = !{i64 129}
!209 = !{i64 121}
!210 = !{i64 122}
!211 = !{i64 123}
!212 = !{!213, !169, i64 0}
!213 = !{!"_ZTS24kmp_task_t_with_privates", !214, i64 0, !215, i64 40}
!214 = !{!"_ZTS10kmp_task_t", !169, i64 0, !169, i64 8, !71, i64 16, !72, i64 24, !72, i64 32}
!215 = !{!"_ZTS15.kmp_privates.t", !71, i64 0, !71, i64 4}
!216 = !{i64 125}
!217 = !{i64 126}
!218 = !{i64 127}
!219 = !{i64 128}
!220 = !{i64 130}
!221 = !{!222, !223, !224, !225, !226, !227}
!222 = !{i64 131}
!223 = !{i64 132}
!224 = !{i64 133}
!225 = !{i64 134}
!226 = !{i64 135}
!227 = !{i64 136}
!228 = !{!229, !232, !234, !236, !237, !239, !241, !242, !244, !245, !246, !247, !248, !249, !250, !251, !252, !253}
!229 = !{!230, !231, !4, !9, !10, !9, !9, !11}
!230 = !{i64 139}
!231 = !{i64 140}
!232 = !{!230, !233, !4, !9, !10, !9, !9, !11}
!233 = !{i64 141}
!234 = !{!230, !235, !4, !9, !10, !9, !9, !11}
!235 = !{i64 142}
!236 = !{!230, !235, !4, !9, !13, !9, !9, !11}
!237 = !{!230, !238, !4, !9, !10, !9, !9, !11}
!238 = !{i64 143}
!239 = !{!230, !240, !4, !9, !34, !9, !9, !11}
!240 = !{i64 145}
!241 = !{!230, !240, !4, !9, !13, !9, !9, !11}
!242 = !{!230, !243, !4, !9, !10, !9, !9, !11}
!243 = !{i64 151}
!244 = !{!230, !243, !4, !9, !13, !9, !9, !11}
!245 = !{!231, !240, !4, !9, !34, !9, !9, !11}
!246 = !{!231, !243, !4, !9, !34, !9, !9, !11}
!247 = !{!233, !240, !4, !9, !34, !9, !9, !11}
!248 = !{!233, !243, !4, !9, !34, !9, !9, !11}
!249 = !{!235, !240, !4, !9, !13, !9, !9, !11}
!250 = !{!235, !240, !4, !9, !34, !9, !9, !11}
!251 = !{!235, !243, !4, !9, !13, !9, !9, !11}
!252 = !{!235, !243, !4, !9, !10, !9, !9, !11}
!253 = !{!238, !240, !4, !9, !34, !9, !9, !11}
!254 = !{i64 137}
!255 = !{i64 138}
!256 = !{i64 144}
!257 = !{i64 146}
!258 = !{i64 147}
!259 = !{i64 148}
!260 = !{i64 149}
!261 = !{i64 150}
!262 = !{i64 152}
!263 = !{!264, !265}
!264 = !{i64 153}
!265 = !{i64 154}
!266 = !{i64 155}
!267 = !{i64 156}
!268 = !{i64 157}
!269 = !{!270, !271}
!270 = !{i64 158}
!271 = !{i64 159}
!272 = !{!273}
!273 = !{!274, !275, !4, !9, !34, !9, !9, !11}
!274 = !{i64 163}
!275 = !{i64 168}
!276 = !{i64 160}
!277 = !{i64 161}
!278 = !{i64 162}
!279 = !{!280, !169, i64 0}
!280 = !{!"_ZTS24kmp_task_t_with_privates", !214, i64 0, !281, i64 40}
!281 = !{!"_ZTS15.kmp_privates.t", !71, i64 0}
!282 = !{i64 164}
!283 = !{i64 165}
!284 = !{i64 166}
!285 = !{i64 167}
!286 = !{i64 169}
!287 = !{!288}
!288 = !{i64 2, i64 -1, i64 -1, i1 true}
!289 = !{i64 39}
!290 = !{i64 40}
!291 = !{i64 41}
!292 = !{i64 42}
!293 = !{i64 43}
!294 = !{i64 44}
!295 = !{i64 45}
!296 = !{i64 46}
!297 = !{i64 47}
!298 = !{i64 48}
!299 = !{i64 49}
!300 = !{i64 50}
!301 = !{i64 0, i64 8, !199, i64 8, i64 8, !199}
!302 = !{i64 51}
!303 = !{i64 52}
!304 = !{i64 53}
!305 = !{i64 54}
!306 = !{!213, !71, i64 40}
!307 = !{i64 55}
!308 = !{i64 56}
!309 = !{i64 57}
!310 = !{!213, !71, i64 44}
!311 = !{i64 58}
!312 = !{i64 59}
!313 = !{i64 60}
!314 = !{i64 61}
!315 = !{i64 62}
!316 = !{i64 63}
!317 = !{i64 64}
!318 = !{i64 65}
!319 = !{!280, !71, i64 40}
!320 = !{i64 66}
!321 = !{i64 67}
!322 = !{i64 24}
!323 = !{i64 26}
!324 = !{i64 28}
!325 = !{i64 30}


 ---------------------------------------- 
[carts] Module: 
Function return type does not match operand type of return inst!
  ret void
 i32in function main
LLVM ERROR: Broken function found, compilation aborted!
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -enable-new-pm=0 -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libSCAFUtilities.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/libMemoryAnalysisModules.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/Noelle.so -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/lib/MetadataCleaner.so --disable-basic-aa -globals-aa -cfl-steens-aa -tbaa -scev-aa -cfl-anders-aa --objc-arc-aa -scalar-evolution -loops -domtree -postdomtree -basic-loop-aa -scev-loop-aa -auto-restrict-aa -intrinsic-aa -global-malloc-aa -pure-fun-aa -semi-local-fun-aa -phi-maze-aa -no-capture-global-aa -no-capture-src-aa -type-aa -no-escape-fields-aa -acyclic-aa -disjoint-fields-aa -field-malloc-aa -loop-variant-allocation-aa -std-in-out-err-aa -array-of-structures-aa -kill-flow-aa -callsite-depth-combinator-aa -unique-access-paths-aa -llvm-aa-results -noelle-scaf -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS test_with_metadata.bc -o test_opt.bc
1.	Running pass 'Function Pass Manager' on module 'test_with_metadata.bc'.
2.	Running pass 'Module Verifier' on function '@main'
 #0 0x00007fe9db792e13 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a6e13)
 #1 0x00007fe9db790c0e llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0x1a4c0e)
 #2 0x00007fe9db7932df SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fe9ddfba910 __restore_rt (/lib64/libpthread.so.0+0x16910)
 #4 0x00007fe9db08ad2b raise (/lib64/libc.so.6+0x4ad2b)
 #5 0x00007fe9db08c3e5 abort (/lib64/libc.so.6+0x4c3e5)
 #6 0x00007fe9db6bdd78 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0xd1d78)
 #7 0x00007fe9db6bdb96 (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMSupport.so.14+0xd1b96)
 #8 0x00007fe9dbb1c87f (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x2d387f)
 #9 0x00007fe9dba81c78 llvm::FPPassManager::runOnFunction(llvm::Function&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x238c78)
#10 0x00007fe9dba89bb1 llvm::FPPassManager::runOnModule(llvm::Module&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x240bb1)
#11 0x00007fe9dba8268c llvm::legacy::PassManagerImpl::run(llvm::Module&) (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/lib/libLLVMCore.so.14+0x23968c)
#12 0x0000000000434c23 main (/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/llvm/bin/opt+0x434c23)
#13 0x00007fe9db07524d __libc_start_main (/lib64/libc.so.6+0x3524d)
#14 0x000000000041827a _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/noelle/bin/n-eval: line 7: 16057 Aborted                 $@
error: noelle-load: line 5
make: *** [Makefile:22: test_opt.bc] Error 1
