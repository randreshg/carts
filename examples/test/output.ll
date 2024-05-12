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
  %10 = load i32, i32* %number, align 4, !tbaa !70, !noelle.pdg.inst.id !18
  %11 = load i32, i32* %random_number, align 4, !tbaa !70, !noelle.pdg.inst.id !28
  %call2 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %10, i32 noundef %11), !noelle.pdg.inst.id !30
  %12 = bitcast i32* %NewRandom to i8*, !noelle.pdg.inst.id !79
  %13 = bitcast i32* %random_number to i8*, !noelle.pdg.inst.id !80
  %14 = bitcast i32* %shared_number to i8*, !noelle.pdg.inst.id !81
  %15 = bitcast i32* %number to i8*, !noelle.pdg.inst.id !82
  ret i32 0, !noelle.pdg.inst.id !83
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %.global_tid., i32* noalias nocapture noundef readnone %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias nocapture noundef readonly %.copy_fn., i8* nocapture noundef readnone %.task_t., %struct.anon* noalias nocapture noundef readonly %__context) #3 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !noelle.pdg.args.id !84 !noelle.pdg.edges !91 {
entry:
  %.firstpriv.ptr.addr = alloca i32*, align 8, !noelle.pdg.inst.id !168
  %.firstpriv.ptr.addr1 = alloca i32*, align 8, !noelle.pdg.inst.id !169
  %0 = bitcast void (i8*, ...)* %.copy_fn. to void (i8*, i32**, i32**)*, !noelle.pdg.inst.id !170
  call void %0(i8* %.privates., i32** %.firstpriv.ptr.addr, i32** %.firstpriv.ptr.addr1), !noelle.pdg.inst.id !93
  %1 = load i32*, i32** %.firstpriv.ptr.addr, align 8, !noelle.pdg.inst.id !94
  %2 = load i32*, i32** %.firstpriv.ptr.addr1, align 8, !noelle.pdg.inst.id !96
  %3 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 0, !noelle.pdg.inst.id !171
  %4 = load i32*, i32** %3, align 8, !tbaa !172, !noelle.pdg.inst.id !98
  %5 = load i32, i32* %4, align 4, !tbaa !70, !noelle.pdg.inst.id !100
  %6 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 1, !noelle.pdg.inst.id !175
  %7 = load i32*, i32** %6, align 8, !tbaa !176, !noelle.pdg.inst.id !102
  %8 = load i32, i32* %7, align 4, !tbaa !70, !noelle.pdg.inst.id !104
  %9 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !106
  %10 = load i32, i32* %2, align 4, !tbaa !70, !noelle.pdg.inst.id !108
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %5, i32 noundef %8, i32 noundef %9, i32 noundef %10)
          to label %invoke.cont unwind label %lpad, !noelle.pdg.inst.id !110

invoke.cont:                                      ; preds = %entry
  %11 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 0, !noelle.pdg.inst.id !177
  %12 = load i32*, i32** %11, align 8, !tbaa !172, !noelle.pdg.inst.id !113
  %13 = load i32, i32* %12, align 4, !tbaa !70, !noelle.pdg.inst.id !115
  %inc = add nsw i32 %13, 1, !noelle.pdg.inst.id !178
  store i32 %inc, i32* %12, align 4, !tbaa !70, !noelle.pdg.inst.id !117
  %14 = getelementptr inbounds %struct.anon, %struct.anon* %__context, i32 0, i32 1, !noelle.pdg.inst.id !179
  %15 = load i32*, i32** %14, align 8, !tbaa !176, !noelle.pdg.inst.id !120
  %16 = load i32, i32* %15, align 4, !tbaa !70, !noelle.pdg.inst.id !122
  %dec = add nsw i32 %16, -1, !noelle.pdg.inst.id !180
  store i32 %dec, i32* %15, align 4, !tbaa !70, !noelle.pdg.inst.id !124
  ret void, !noelle.pdg.inst.id !181

lpad:                                             ; preds = %entry
  %17 = landingpad { i8*, i32 }
          catch i8* null, !noelle.pdg.inst.id !182
  %18 = extractvalue { i8*, i32 } %17, 0, !noelle.pdg.inst.id !183
  %19 = extractvalue { i8*, i32 } %17, 1, !noelle.pdg.inst.id !184
  br label %terminate.handler, !noelle.pdg.inst.id !185

terminate.handler:                                ; preds = %lpad
  call void @__clang_call_terminate(i8* %18) #10, !noelle.pdg.inst.id !127
  unreachable, !noelle.pdg.inst.id !186
}

declare dso_local i32 @printf(i8* noundef, ...) #4

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) #5 comdat !noelle.pdg.args.id !187 !noelle.pdg.edges !189 {
  %2 = call i8* @__cxa_begin_catch(i8* %0) #8, !noelle.pdg.inst.id !191
  call void @_ZSt9terminatev() #10, !noelle.pdg.inst.id !192
  unreachable, !noelle.pdg.inst.id !194
}

declare dso_local i8* @__cxa_begin_catch(i8*)

declare dso_local void @_ZSt9terminatev()

; Function Attrs: alwaysinline mustprogress nofree norecurse nosync nounwind uwtable willreturn writeonly
define internal void @.omp_task_privates_map.(%struct..kmp_privates.t* noalias noundef %0, i32** noalias nocapture noundef writeonly %1, i32** noalias nocapture noundef writeonly %2) #6 !noelle.pdg.args.id !195 !noelle.pdg.edges !199 {
entry:
  %3 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %0, i32 0, i32 0, !noelle.pdg.inst.id !203
  store i32* %3, i32** %1, align 8, !tbaa !204, !noelle.pdg.inst.id !201
  %4 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %0, i32 0, i32 1, !noelle.pdg.inst.id !205
  store i32* %4, i32** %2, align 8, !tbaa !204, !noelle.pdg.inst.id !202
  ret void, !noelle.pdg.inst.id !206
}

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias noundef %1) #7 !noelle.pdg.args.id !207 !noelle.pdg.edges !210 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i32 0, i32 0, !noelle.pdg.inst.id !214
  %3 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 2, !noelle.pdg.inst.id !215
  %4 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 0, !noelle.pdg.inst.id !216
  %5 = load i8*, i8** %4, align 8, !tbaa !217, !noelle.pdg.inst.id !212
  %6 = bitcast i8* %5 to %struct.anon*, !noelle.pdg.inst.id !221
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %1, i32 0, i32 1, !noelle.pdg.inst.id !222
  %8 = bitcast %struct..kmp_privates.t* %7 to i8*, !noelle.pdg.inst.id !223
  %9 = bitcast %struct.kmp_task_t_with_privates* %1 to i8*, !noelle.pdg.inst.id !224
  call void @.omp_outlined..1(i32 %0, i32* %3, i8* %8, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t*, i32**, i32**)* @.omp_task_privates_map. to void (i8*, ...)*), i8* %9, %struct.anon* %6) #8, !noelle.pdg.inst.id !213
  ret i32 0, !noelle.pdg.inst.id !225
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) #8

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #9

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) #8

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined..2(i32 noundef %.global_tid., i32* noalias nocapture noundef readnone %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias nocapture noundef readonly %.copy_fn., i8* nocapture noundef readnone %.task_t., %struct.anon.0* noalias nocapture noundef readnone %__context) #3 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !noelle.pdg.args.id !226 !noelle.pdg.edges !233 {
entry:
  %.firstpriv.ptr.addr = alloca i32*, align 8, !noelle.pdg.inst.id !259
  %0 = bitcast void (i8*, ...)* %.copy_fn. to void (i8*, i32**)*, !noelle.pdg.inst.id !260
  call void %0(i8* %.privates., i32** %.firstpriv.ptr.addr), !noelle.pdg.inst.id !235
  %1 = load i32*, i32** %.firstpriv.ptr.addr, align 8, !noelle.pdg.inst.id !236
  %2 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !238
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %2)
          to label %invoke.cont unwind label %lpad, !noelle.pdg.inst.id !240

invoke.cont:                                      ; preds = %entry
  %3 = load i32, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !243
  %inc = add nsw i32 %3, 1, !noelle.pdg.inst.id !261
  store i32 %inc, i32* %1, align 4, !tbaa !70, !noelle.pdg.inst.id !245
  ret void, !noelle.pdg.inst.id !262

lpad:                                             ; preds = %entry
  %4 = landingpad { i8*, i32 }
          catch i8* null, !noelle.pdg.inst.id !263
  %5 = extractvalue { i8*, i32 } %4, 0, !noelle.pdg.inst.id !264
  %6 = extractvalue { i8*, i32 } %4, 1, !noelle.pdg.inst.id !265
  br label %terminate.handler, !noelle.pdg.inst.id !266

terminate.handler:                                ; preds = %lpad
  call void @__clang_call_terminate(i8* %5) #10, !noelle.pdg.inst.id !248
  unreachable, !noelle.pdg.inst.id !267
}

; Function Attrs: alwaysinline mustprogress nofree norecurse nosync nounwind uwtable willreturn writeonly
define internal void @.omp_task_privates_map..4(%struct..kmp_privates.t.2* noalias noundef %0, i32** noalias nocapture noundef writeonly %1) #6 !noelle.pdg.args.id !268 {
entry:
  %2 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %0, i32 0, i32 0, !noelle.pdg.inst.id !271
  store i32* %2, i32** %1, align 8, !tbaa !204, !noelle.pdg.inst.id !272
  ret void, !noelle.pdg.inst.id !273
}

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias noundef %1) #7 !noelle.pdg.args.id !274 !noelle.pdg.edges !277 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i32 0, i32 0, !noelle.pdg.inst.id !281
  %3 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 2, !noelle.pdg.inst.id !282
  %4 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %2, i32 0, i32 0, !noelle.pdg.inst.id !283
  %5 = load i8*, i8** %4, align 8, !tbaa !284, !noelle.pdg.inst.id !279
  %6 = bitcast i8* %5 to %struct.anon.0*, !noelle.pdg.inst.id !287
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %1, i32 0, i32 1, !noelle.pdg.inst.id !288
  %8 = bitcast %struct..kmp_privates.t.2* %7 to i8*, !noelle.pdg.inst.id !289
  %9 = bitcast %struct.kmp_task_t_with_privates.1* %1 to i8*, !noelle.pdg.inst.id !290
  call void @.omp_outlined..2(i32 %0, i32* %3, i8* %8, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t.2*, i32**)* @.omp_task_privates_map..4 to void (i8*, ...)*), i8* %9, %struct.anon.0* %6) #8, !noelle.pdg.inst.id !280
  ret i32 0, !noelle.pdg.inst.id !291
}

; Function Attrs: nounwind
declare !callback !292 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) #8

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
  %0 = alloca %struct.anon, align 8, !noelle.pdg.inst.id !294
  %1 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 0, !noelle.pdg.inst.id !295
  store i32* %depv.number.casted, i32** %1, align 8, !tbaa !204, !noelle.pdg.inst.id !296
  %2 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 1, !noelle.pdg.inst.id !297
  store i32* %depv.shared_number.casted, i32** %2, align 8, !tbaa !204, !noelle.pdg.inst.id !298
  %3 = load i32, i32* undef, align 4, !tbaa !70, !noelle.pdg.inst.id !299
  %4 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* undef, i32 %3, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*)), !noelle.pdg.inst.id !300
  %5 = bitcast i8* %4 to %struct.kmp_task_t_with_privates*, !noelle.pdg.inst.id !301
  %6 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %5, i32 0, i32 0, !noelle.pdg.inst.id !302
  %7 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %6, i32 0, i32 0, !noelle.pdg.inst.id !303
  %8 = load i8*, i8** %7, align 8, !tbaa !217, !noelle.pdg.inst.id !304
  %9 = bitcast %struct.anon* %0 to i8*, !noelle.pdg.inst.id !305
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %8, i8* align 8 %9, i64 16, i1 false), !tbaa.struct !306, !noelle.pdg.inst.id !307
  %10 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %5, i32 0, i32 1, !noelle.pdg.inst.id !308
  %11 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %10, i32 0, i32 0, !noelle.pdg.inst.id !309
  %12 = load i32, i32* %depv.random_number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !310
  store i32 %12, i32* %11, align 8, !tbaa !311, !noelle.pdg.inst.id !312
  %13 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %10, i32 0, i32 1, !noelle.pdg.inst.id !313
  %14 = load i32, i32* %depv.NewRandom.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !314
  store i32 %14, i32* %13, align 4, !tbaa !315, !noelle.pdg.inst.id !316
  %15 = call i32 @__kmpc_omp_task(%struct.ident_t* undef, i32 %3, i8* %4), !noelle.pdg.inst.id !317
  %16 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* undef, i32 %3, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*)), !noelle.pdg.inst.id !318
  %17 = bitcast i8* %16 to %struct.kmp_task_t_with_privates.1*, !noelle.pdg.inst.id !319
  %18 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %17, i32 0, i32 0, !noelle.pdg.inst.id !320
  %19 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %17, i32 0, i32 1, !noelle.pdg.inst.id !321
  %20 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %19, i32 0, i32 0, !noelle.pdg.inst.id !322
  %21 = load i32, i32* %depv.number.casted, align 4, !tbaa !70, !noelle.pdg.inst.id !323
  store i32 %21, i32* %20, align 8, !tbaa !324, !noelle.pdg.inst.id !325
  %22 = call i32 @__kmpc_omp_task(%struct.ident_t* undef, i32 %3, i8* %16), !noelle.pdg.inst.id !326
  br label %edt.exit
}

declare i32* @artsReserveGuidRoute(i32, i32)

declare i32* @artsEdtCreateWithGuid(void (i32, i64*, i32, %struct.artsEdtDep_t*)*, i32*, i32, i64*, i32)

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
!5 = !{!6, !12, !14, !16, !17, !19, !21, !22, !24, !25, !26, !27, !29, !31, !32, !34, !36, !37, !38, !39, !40, !41, !42, !43, !44, !45, !46, !48, !49, !50, !51, !52, !53, !54, !55, !56, !57, !58, !59, !60, !61, !62, !63, !64}
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
!32 = !{!8, !33, !4, !9, !13, !9, !9, !11}
!33 = !{i64 19}
!34 = !{!8, !33, !4, !9, !35, !9, !9, !11}
!35 = !{!"WAR"}
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
!51 = !{!47, !20, !4, !9, !10, !9, !9, !11}
!52 = !{!47, !20, !4, !9, !13, !9, !9, !11}
!53 = !{!20, !30, !4, !9, !10, !9, !9, !11}
!54 = !{!20, !30, !4, !9, !13, !9, !9, !11}
!55 = !{!20, !23, !4, !9, !35, !9, !9, !11}
!56 = !{!20, !23, !4, !9, !13, !9, !9, !11}
!57 = !{!20, !8, !4, !9, !10, !9, !9, !11}
!58 = !{!20, !8, !4, !9, !13, !9, !9, !11}
!59 = !{!20, !33, !4, !9, !13, !9, !9, !11}
!60 = !{!20, !33, !4, !9, !35, !9, !9, !11}
!61 = !{!20, !15, !4, !9, !13, !9, !9, !11}
!62 = !{!20, !15, !4, !9, !10, !9, !9, !11}
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
!79 = !{i64 24}
!80 = !{i64 26}
!81 = !{i64 28}
!82 = !{i64 30}
!83 = !{i64 32}
!84 = !{!85, !86, !87, !88, !89, !90}
!85 = !{i64 69}
!86 = !{i64 70}
!87 = !{i64 71}
!88 = !{i64 72}
!89 = !{i64 73}
!90 = !{i64 74}
!91 = !{!92, !95, !97, !99, !101, !103, !105, !107, !109, !111, !112, !114, !116, !118, !119, !121, !123, !125, !126, !128, !129, !130, !131, !132, !133, !134, !135, !136, !137, !138, !139, !140, !141, !142, !143, !144, !145, !146, !147, !148, !149, !150, !151, !152, !153, !154, !155, !156, !157, !158, !159, !160, !161, !162, !163, !164, !165, !166, !167}
!92 = !{!93, !94, !4, !9, !10, !9, !9, !11}
!93 = !{i64 78}
!94 = !{i64 79}
!95 = !{!93, !96, !4, !9, !10, !9, !9, !11}
!96 = !{i64 80}
!97 = !{!93, !98, !4, !9, !10, !9, !9, !11}
!98 = !{i64 82}
!99 = !{!93, !100, !4, !9, !10, !9, !9, !11}
!100 = !{i64 83}
!101 = !{!93, !102, !4, !9, !10, !9, !9, !11}
!102 = !{i64 85}
!103 = !{!93, !104, !4, !9, !10, !9, !9, !11}
!104 = !{i64 86}
!105 = !{!93, !106, !4, !9, !10, !9, !9, !11}
!106 = !{i64 87}
!107 = !{!93, !108, !4, !9, !10, !9, !9, !11}
!108 = !{i64 88}
!109 = !{!93, !110, !4, !9, !13, !9, !9, !11}
!110 = !{i64 89}
!111 = !{!93, !110, !4, !9, !10, !9, !9, !11}
!112 = !{!93, !113, !4, !9, !10, !9, !9, !11}
!113 = !{i64 91}
!114 = !{!93, !115, !4, !9, !10, !9, !9, !11}
!115 = !{i64 92}
!116 = !{!93, !117, !4, !9, !13, !9, !9, !11}
!117 = !{i64 94}
!118 = !{!93, !117, !4, !9, !35, !9, !9, !11}
!119 = !{!93, !120, !4, !9, !10, !9, !9, !11}
!120 = !{i64 96}
!121 = !{!93, !122, !4, !9, !10, !9, !9, !11}
!122 = !{i64 97}
!123 = !{!93, !124, !4, !9, !35, !9, !9, !11}
!124 = !{i64 99}
!125 = !{!93, !124, !4, !9, !13, !9, !9, !11}
!126 = !{!93, !127, !4, !9, !10, !9, !9, !11}
!127 = !{i64 105}
!128 = !{!93, !127, !4, !9, !13, !9, !9, !11}
!129 = !{!94, !117, !4, !9, !35, !9, !9, !11}
!130 = !{!94, !124, !4, !9, !35, !9, !9, !11}
!131 = !{!94, !127, !4, !9, !35, !9, !9, !11}
!132 = !{!96, !117, !4, !9, !35, !9, !9, !11}
!133 = !{!96, !124, !4, !9, !35, !9, !9, !11}
!134 = !{!96, !127, !4, !9, !35, !9, !9, !11}
!135 = !{!98, !117, !4, !9, !35, !9, !9, !11}
!136 = !{!98, !124, !4, !9, !35, !9, !9, !11}
!137 = !{!98, !127, !4, !9, !35, !9, !9, !11}
!138 = !{!100, !117, !4, !9, !35, !9, !9, !11}
!139 = !{!100, !124, !4, !9, !35, !9, !9, !11}
!140 = !{!100, !127, !4, !9, !35, !9, !9, !11}
!141 = !{!102, !117, !4, !9, !35, !9, !9, !11}
!142 = !{!102, !124, !4, !9, !35, !9, !9, !11}
!143 = !{!102, !127, !4, !9, !35, !9, !9, !11}
!144 = !{!104, !117, !4, !9, !35, !9, !9, !11}
!145 = !{!104, !124, !4, !9, !35, !9, !9, !11}
!146 = !{!104, !127, !4, !9, !35, !9, !9, !11}
!147 = !{!106, !117, !4, !9, !35, !9, !9, !11}
!148 = !{!106, !124, !4, !9, !35, !9, !9, !11}
!149 = !{!106, !127, !4, !9, !35, !9, !9, !11}
!150 = !{!108, !117, !4, !9, !35, !9, !9, !11}
!151 = !{!108, !124, !4, !9, !35, !9, !9, !11}
!152 = !{!108, !127, !4, !9, !35, !9, !9, !11}
!153 = !{!110, !117, !4, !9, !35, !9, !9, !11}
!154 = !{!110, !117, !4, !9, !13, !9, !9, !11}
!155 = !{!110, !124, !4, !9, !13, !9, !9, !11}
!156 = !{!110, !124, !4, !9, !35, !9, !9, !11}
!157 = !{!110, !127, !4, !9, !13, !9, !9, !11}
!158 = !{!110, !127, !4, !9, !10, !9, !9, !11}
!159 = !{!113, !117, !4, !9, !35, !9, !9, !11}
!160 = !{!113, !124, !4, !9, !35, !9, !9, !11}
!161 = !{!115, !117, !4, !9, !35, !9, !9, !11}
!162 = !{!115, !124, !4, !9, !35, !9, !9, !11}
!163 = !{!117, !120, !4, !9, !10, !9, !9, !11}
!164 = !{!117, !122, !4, !9, !10, !9, !9, !11}
!165 = !{!117, !124, !4, !9, !13, !9, !9, !11}
!166 = !{!120, !124, !4, !9, !35, !9, !9, !11}
!167 = !{!122, !124, !4, !9, !35, !9, !9, !11}
!168 = !{i64 75}
!169 = !{i64 76}
!170 = !{i64 77}
!171 = !{i64 81}
!172 = !{!173, !174, i64 0}
!173 = !{!"_ZTSZ4mainE3$_0", !174, i64 0, !174, i64 8}
!174 = !{!"any pointer", !72, i64 0}
!175 = !{i64 84}
!176 = !{!173, !174, i64 8}
!177 = !{i64 90}
!178 = !{i64 93}
!179 = !{i64 95}
!180 = !{i64 98}
!181 = !{i64 100}
!182 = !{i64 101}
!183 = !{i64 102}
!184 = !{i64 103}
!185 = !{i64 104}
!186 = !{i64 106}
!187 = !{!188}
!188 = !{i64 107}
!189 = !{!190, !193}
!190 = !{!191, !192, !4, !9, !13, !9, !9, !11}
!191 = !{i64 108}
!192 = !{i64 109}
!193 = !{!191, !192, !4, !9, !10, !9, !9, !11}
!194 = !{i64 110}
!195 = !{!196, !197, !198}
!196 = !{i64 111}
!197 = !{i64 112}
!198 = !{i64 113}
!199 = !{!200}
!200 = !{!201, !202, !4, !9, !13, !9, !9, !11}
!201 = !{i64 115}
!202 = !{i64 117}
!203 = !{i64 114}
!204 = !{!174, !174, i64 0}
!205 = !{i64 116}
!206 = !{i64 118}
!207 = !{!208, !209}
!208 = !{i64 119}
!209 = !{i64 120}
!210 = !{!211}
!211 = !{!212, !213, !4, !9, !35, !9, !9, !11}
!212 = !{i64 124}
!213 = !{i64 129}
!214 = !{i64 121}
!215 = !{i64 122}
!216 = !{i64 123}
!217 = !{!218, !174, i64 0}
!218 = !{!"_ZTS24kmp_task_t_with_privates", !219, i64 0, !220, i64 40}
!219 = !{!"_ZTS10kmp_task_t", !174, i64 0, !174, i64 8, !71, i64 16, !72, i64 24, !72, i64 32}
!220 = !{!"_ZTS15.kmp_privates.t", !71, i64 0, !71, i64 4}
!221 = !{i64 125}
!222 = !{i64 126}
!223 = !{i64 127}
!224 = !{i64 128}
!225 = !{i64 130}
!226 = !{!227, !228, !229, !230, !231, !232}
!227 = !{i64 131}
!228 = !{i64 132}
!229 = !{i64 133}
!230 = !{i64 134}
!231 = !{i64 135}
!232 = !{i64 136}
!233 = !{!234, !237, !239, !241, !242, !244, !246, !247, !249, !250, !251, !252, !253, !254, !255, !256, !257, !258}
!234 = !{!235, !236, !4, !9, !10, !9, !9, !11}
!235 = !{i64 139}
!236 = !{i64 140}
!237 = !{!235, !238, !4, !9, !10, !9, !9, !11}
!238 = !{i64 141}
!239 = !{!235, !240, !4, !9, !10, !9, !9, !11}
!240 = !{i64 142}
!241 = !{!235, !240, !4, !9, !13, !9, !9, !11}
!242 = !{!235, !243, !4, !9, !10, !9, !9, !11}
!243 = !{i64 143}
!244 = !{!235, !245, !4, !9, !35, !9, !9, !11}
!245 = !{i64 145}
!246 = !{!235, !245, !4, !9, !13, !9, !9, !11}
!247 = !{!235, !248, !4, !9, !10, !9, !9, !11}
!248 = !{i64 151}
!249 = !{!235, !248, !4, !9, !13, !9, !9, !11}
!250 = !{!236, !245, !4, !9, !35, !9, !9, !11}
!251 = !{!236, !248, !4, !9, !35, !9, !9, !11}
!252 = !{!238, !245, !4, !9, !35, !9, !9, !11}
!253 = !{!238, !248, !4, !9, !35, !9, !9, !11}
!254 = !{!240, !245, !4, !9, !13, !9, !9, !11}
!255 = !{!240, !245, !4, !9, !35, !9, !9, !11}
!256 = !{!240, !248, !4, !9, !10, !9, !9, !11}
!257 = !{!240, !248, !4, !9, !13, !9, !9, !11}
!258 = !{!243, !245, !4, !9, !35, !9, !9, !11}
!259 = !{i64 137}
!260 = !{i64 138}
!261 = !{i64 144}
!262 = !{i64 146}
!263 = !{i64 147}
!264 = !{i64 148}
!265 = !{i64 149}
!266 = !{i64 150}
!267 = !{i64 152}
!268 = !{!269, !270}
!269 = !{i64 153}
!270 = !{i64 154}
!271 = !{i64 155}
!272 = !{i64 156}
!273 = !{i64 157}
!274 = !{!275, !276}
!275 = !{i64 158}
!276 = !{i64 159}
!277 = !{!278}
!278 = !{!279, !280, !4, !9, !35, !9, !9, !11}
!279 = !{i64 163}
!280 = !{i64 168}
!281 = !{i64 160}
!282 = !{i64 161}
!283 = !{i64 162}
!284 = !{!285, !174, i64 0}
!285 = !{!"_ZTS24kmp_task_t_with_privates", !219, i64 0, !286, i64 40}
!286 = !{!"_ZTS15.kmp_privates.t", !71, i64 0}
!287 = !{i64 164}
!288 = !{i64 165}
!289 = !{i64 166}
!290 = !{i64 167}
!291 = !{i64 169}
!292 = !{!293}
!293 = !{i64 2, i64 -1, i64 -1, i1 true}
!294 = !{i64 39}
!295 = !{i64 40}
!296 = !{i64 41}
!297 = !{i64 42}
!298 = !{i64 43}
!299 = !{i64 44}
!300 = !{i64 45}
!301 = !{i64 46}
!302 = !{i64 47}
!303 = !{i64 48}
!304 = !{i64 49}
!305 = !{i64 50}
!306 = !{i64 0, i64 8, !204, i64 8, i64 8, !204}
!307 = !{i64 51}
!308 = !{i64 52}
!309 = !{i64 53}
!310 = !{i64 54}
!311 = !{!218, !71, i64 40}
!312 = !{i64 55}
!313 = !{i64 56}
!314 = !{i64 57}
!315 = !{!218, !71, i64 44}
!316 = !{i64 58}
!317 = !{i64 59}
!318 = !{i64 60}
!319 = !{i64 61}
!320 = !{i64 62}
!321 = !{i64 63}
!322 = !{i64 64}
!323 = !{i64 65}
!324 = !{!285, !71, i64 40}
!325 = !{i64 66}
!326 = !{i64 67}

[arts-analyzer] Other Function Found: 
    %call2 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %10, i32 noundef %11), !noelle.pdg.inst.id !30

 ---------------------------------------- 
[carts] Module: 
llvm-dis test_opt.bc
clang++ -fopenmp test_opt.bc -O3 -march=native -o test_opt -lstdc++
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: /tmp/test_opt-96d190.o: in function `main':
test.cpp:(.text+0x3a): undefined reference to `artsReserveGuidRoute'
/usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/bin/ld: test.cpp:(.text+0x5a): undefined reference to `artsEdtCreateWithGuid'
clang-14: error: linker command failed with exit code 1 (use -v to see invocation)
make: *** [Makefile:30: test_opt] Error 1
