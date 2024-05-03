; ModuleID = 'test.bc'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%struct.anon = type { i32*, i32* }
%struct.anon.0 = type { i8 }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { i8*, i32 (i32, i8*)*, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { i32 (i32, i8*)* }
%struct..kmp_privates.t = type { i32, i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

$__clang_call_terminate = comdat any

@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@.str.3 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.6 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast i32* %number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #9
  store i32 1, i32* %number, align 4, !tbaa !4
  %1 = bitcast i32* %shared_number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %1) #9
  store i32 10000, i32* %shared_number, align 4, !tbaa !4
  %2 = bitcast i32* %random_number to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %2) #9
  %call = call i32 @rand() #9
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  store i32 %add, i32* %random_number, align 4, !tbaa !4
  %3 = bitcast i32* %NewRandom to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %3) #9
  %call1 = call i32 @rand() #9
  store i32 %call1, i32* %NewRandom, align 4, !tbaa !4
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* %random_number, i32* %NewRandom, i32* %number, i32* %shared_number)
  %4 = load i32, i32* %number, align 4, !tbaa !4
  %5 = load i32, i32* %random_number, align 4, !tbaa !4
  %call2 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %4, i32 noundef %5)
  %6 = bitcast i32* %NewRandom to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %6) #9
  %7 = bitcast i32* %random_number to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %7) #9
  %8 = bitcast i32* %shared_number to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %8) #9
  %9 = bitcast i32* %number to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %9) #9
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias noundef %.global_tid., i32* noalias noundef %.bound_tid., i32* noundef nonnull align 4 dereferenceable(4) %random_number, i32* noundef nonnull align 4 dereferenceable(4) %NewRandom, i32* noundef nonnull align 4 dereferenceable(4) %number, i32* noundef nonnull align 4 dereferenceable(4) %shared_number) #3 {
entry:
  %.global_tid..addr = alloca i32*, align 8
  %.bound_tid..addr = alloca i32*, align 8
  %random_number.addr = alloca i32*, align 8
  %NewRandom.addr = alloca i32*, align 8
  %number.addr = alloca i32*, align 8
  %shared_number.addr = alloca i32*, align 8
  %agg.captured = alloca %struct.anon, align 8
  %agg.captured1 = alloca %struct.anon.0, align 1
  store i32* %.global_tid., i32** %.global_tid..addr, align 8, !tbaa !8
  store i32* %.bound_tid., i32** %.bound_tid..addr, align 8, !tbaa !8
  store i32* %random_number, i32** %random_number.addr, align 8, !tbaa !8
  store i32* %NewRandom, i32** %NewRandom.addr, align 8, !tbaa !8
  store i32* %number, i32** %number.addr, align 8, !tbaa !8
  store i32* %shared_number, i32** %shared_number.addr, align 8, !tbaa !8
  %0 = load i32*, i32** %random_number.addr, align 8, !tbaa !8
  %1 = load i32*, i32** %NewRandom.addr, align 8, !tbaa !8
  %2 = load i32*, i32** %number.addr, align 8, !tbaa !8
  %3 = load i32*, i32** %shared_number.addr, align 8, !tbaa !8
  %4 = getelementptr inbounds %struct.anon, %struct.anon* %agg.captured, i32 0, i32 0
  store i32* %2, i32** %4, align 8, !tbaa !8
  %5 = getelementptr inbounds %struct.anon, %struct.anon* %agg.captured, i32 0, i32 1
  store i32* %3, i32** %5, align 8, !tbaa !8
  %6 = load i32*, i32** %.global_tid..addr, align 8
  %7 = load i32, i32* %6, align 4, !tbaa !4
  %8 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* @1, i32 %7, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
  %9 = bitcast i8* %8 to %struct.kmp_task_t_with_privates*
  %10 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %9, i32 0, i32 0
  %11 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %10, i32 0, i32 0
  %12 = load i8*, i8** %11, align 8, !tbaa !10
  %13 = bitcast %struct.anon* %agg.captured to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %12, i8* align 8 %13, i64 16, i1 false), !tbaa.struct !14
  %14 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %9, i32 0, i32 1
  %15 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %14, i32 0, i32 0
  %16 = load i32, i32* %0, align 4, !tbaa !4
  store i32 %16, i32* %15, align 8, !tbaa !15
  %17 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %14, i32 0, i32 1
  %18 = load i32, i32* %1, align 4, !tbaa !4
  store i32 %18, i32* %17, align 4, !tbaa !16
  %19 = call i32 @__kmpc_omp_task(%struct.ident_t* @1, i32 %7, i8* %8)
  %20 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* @1, i32 %7, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
  %21 = bitcast i8* %20 to %struct.kmp_task_t_with_privates.1*
  %22 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %21, i32 0, i32 0
  %23 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %21, i32 0, i32 1
  %24 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %23, i32 0, i32 0
  %25 = load i32, i32* %2, align 4, !tbaa !4
  store i32 %25, i32* %24, align 8, !tbaa !17
  %26 = call i32 @__kmpc_omp_task(%struct.ident_t* @1, i32 %7, i8* %20)
  ret void
}

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %.global_tid., i32* noalias noundef %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias noundef %.copy_fn., i8* noundef %.task_t., %struct.anon* noalias noundef %__context) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %.global_tid..addr = alloca i32, align 4
  %.part_id..addr = alloca i32*, align 8
  %.privates..addr = alloca i8*, align 8
  %.copy_fn..addr = alloca void (i8*, ...)*, align 8
  %.task_t..addr = alloca i8*, align 8
  %__context.addr = alloca %struct.anon*, align 8
  %.firstpriv.ptr.addr = alloca i32*, align 8
  %.firstpriv.ptr.addr1 = alloca i32*, align 8
  %exn.slot = alloca i8*, align 8
  %ehselector.slot = alloca i32, align 4
  store i32 %.global_tid., i32* %.global_tid..addr, align 4, !tbaa !4
  store i32* %.part_id., i32** %.part_id..addr, align 8, !tbaa !8
  store i8* %.privates., i8** %.privates..addr, align 8, !tbaa !8
  store void (i8*, ...)* %.copy_fn., void (i8*, ...)** %.copy_fn..addr, align 8, !tbaa !8
  store i8* %.task_t., i8** %.task_t..addr, align 8, !tbaa !8
  store %struct.anon* %__context, %struct.anon** %__context.addr, align 8, !tbaa !8
  %0 = load %struct.anon*, %struct.anon** %__context.addr, align 8
  %1 = load void (i8*, ...)*, void (i8*, ...)** %.copy_fn..addr, align 8
  %2 = load i8*, i8** %.privates..addr, align 8
  %3 = bitcast void (i8*, ...)* %1 to void (i8*, i32**, i32**)*
  call void %3(i8* %2, i32** %.firstpriv.ptr.addr, i32** %.firstpriv.ptr.addr1)
  %4 = load i32*, i32** %.firstpriv.ptr.addr, align 8
  %5 = load i32*, i32** %.firstpriv.ptr.addr1, align 8
  %6 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 0
  %7 = load i32*, i32** %6, align 8, !tbaa !20
  %8 = load i32, i32* %7, align 4, !tbaa !4
  %9 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 1
  %10 = load i32*, i32** %9, align 8, !tbaa !22
  %11 = load i32, i32* %10, align 4, !tbaa !4
  %12 = load i32, i32* %4, align 4, !tbaa !4
  %13 = load i32, i32* %5, align 4, !tbaa !4
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %8, i32 noundef %11, i32 noundef %12, i32 noundef %13)
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %entry
  %14 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 0
  %15 = load i32*, i32** %14, align 8, !tbaa !20
  %16 = load i32, i32* %15, align 4, !tbaa !4
  %inc = add nsw i32 %16, 1
  store i32 %inc, i32* %15, align 4, !tbaa !4
  %17 = getelementptr inbounds %struct.anon, %struct.anon* %0, i32 0, i32 1
  %18 = load i32*, i32** %17, align 8, !tbaa !22
  %19 = load i32, i32* %18, align 4, !tbaa !4
  %dec = add nsw i32 %19, -1
  store i32 %dec, i32* %18, align 4, !tbaa !4
  ret void

lpad:                                             ; preds = %entry
  %20 = landingpad { i8*, i32 }
          catch i8* null
  %21 = extractvalue { i8*, i32 } %20, 0
  store i8* %21, i8** %exn.slot, align 8
  %22 = extractvalue { i8*, i32 } %20, 1
  store i32 %22, i32* %ehselector.slot, align 4
  br label %terminate.handler

terminate.handler:                                ; preds = %lpad
  %exn = load i8*, i8** %exn.slot, align 8
  call void @__clang_call_terminate(i8* %exn) #11
  unreachable
}

declare dso_local i32 @printf(i8* noundef, ...) #5

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) #6 comdat {
  %2 = call i8* @__cxa_begin_catch(i8* %0) #9
  call void @_ZSt9terminatev() #11
  unreachable
}

declare dso_local i8* @__cxa_begin_catch(i8*)

declare dso_local void @_ZSt9terminatev()

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map.(%struct..kmp_privates.t* noalias noundef %0, i32** noalias noundef %1, i32** noalias noundef %2) #7 {
entry:
  %.addr = alloca %struct..kmp_privates.t*, align 8
  %.addr1 = alloca i32**, align 8
  %.addr2 = alloca i32**, align 8
  store %struct..kmp_privates.t* %0, %struct..kmp_privates.t** %.addr, align 8, !tbaa !8
  store i32** %1, i32*** %.addr1, align 8, !tbaa !8
  store i32** %2, i32*** %.addr2, align 8, !tbaa !8
  %3 = load %struct..kmp_privates.t*, %struct..kmp_privates.t** %.addr, align 8
  %4 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %3, i32 0, i32 0
  %5 = load i32**, i32*** %.addr1, align 8
  store i32* %4, i32** %5, align 8, !tbaa !8
  %6 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %3, i32 0, i32 1
  %7 = load i32**, i32*** %.addr2, align 8
  store i32* %6, i32** %7, align 8, !tbaa !8
  ret void
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias noundef %1) #8 {
entry:
  %.addr = alloca i32, align 4
  %.addr1 = alloca %struct.kmp_task_t_with_privates*, align 8
  store i32 %0, i32* %.addr, align 4, !tbaa !4
  store %struct.kmp_task_t_with_privates* %1, %struct.kmp_task_t_with_privates** %.addr1, align 8, !tbaa !8
  %2 = load i32, i32* %.addr, align 4, !tbaa !4
  %3 = load %struct.kmp_task_t_with_privates*, %struct.kmp_task_t_with_privates** %.addr1, align 8
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %3, i32 0, i32 0
  %5 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %4, i32 0, i32 2
  %6 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %4, i32 0, i32 0
  %7 = load i8*, i8** %6, align 8, !tbaa !10
  %8 = bitcast i8* %7 to %struct.anon*
  %9 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %3, i32 0, i32 1
  %10 = bitcast %struct..kmp_privates.t* %9 to i8*
  %11 = bitcast %struct.kmp_task_t_with_privates* %3 to i8*
  call void @.omp_outlined..1(i32 %2, i32* %5, i8* %10, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t*, i32**, i32**)* @.omp_task_privates_map. to void (i8*, ...)*), i8* %11, %struct.anon* %8) #9
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) #9

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #10

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) #9

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..2(i32 noundef %.global_tid., i32* noalias noundef %.part_id., i8* noalias noundef %.privates., void (i8*, ...)* noalias noundef %.copy_fn., i8* noundef %.task_t., %struct.anon.0* noalias noundef %__context) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %.global_tid..addr = alloca i32, align 4
  %.part_id..addr = alloca i32*, align 8
  %.privates..addr = alloca i8*, align 8
  %.copy_fn..addr = alloca void (i8*, ...)*, align 8
  %.task_t..addr = alloca i8*, align 8
  %__context.addr = alloca %struct.anon.0*, align 8
  %.firstpriv.ptr.addr = alloca i32*, align 8
  %exn.slot = alloca i8*, align 8
  %ehselector.slot = alloca i32, align 4
  store i32 %.global_tid., i32* %.global_tid..addr, align 4, !tbaa !4
  store i32* %.part_id., i32** %.part_id..addr, align 8, !tbaa !8
  store i8* %.privates., i8** %.privates..addr, align 8, !tbaa !8
  store void (i8*, ...)* %.copy_fn., void (i8*, ...)** %.copy_fn..addr, align 8, !tbaa !8
  store i8* %.task_t., i8** %.task_t..addr, align 8, !tbaa !8
  store %struct.anon.0* %__context, %struct.anon.0** %__context.addr, align 8, !tbaa !8
  %0 = load %struct.anon.0*, %struct.anon.0** %__context.addr, align 8
  %1 = load void (i8*, ...)*, void (i8*, ...)** %.copy_fn..addr, align 8
  %2 = load i8*, i8** %.privates..addr, align 8
  %3 = bitcast void (i8*, ...)* %1 to void (i8*, i32**)*
  call void %3(i8* %2, i32** %.firstpriv.ptr.addr)
  %4 = load i32*, i32** %.firstpriv.ptr.addr, align 8
  %5 = load i32, i32* %4, align 4, !tbaa !4
  %call = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %5)
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %entry
  %6 = load i32, i32* %4, align 4, !tbaa !4
  %inc = add nsw i32 %6, 1
  store i32 %inc, i32* %4, align 4, !tbaa !4
  ret void

lpad:                                             ; preds = %entry
  %7 = landingpad { i8*, i32 }
          catch i8* null
  %8 = extractvalue { i8*, i32 } %7, 0
  store i8* %8, i8** %exn.slot, align 8
  %9 = extractvalue { i8*, i32 } %7, 1
  store i32 %9, i32* %ehselector.slot, align 4
  br label %terminate.handler

terminate.handler:                                ; preds = %lpad
  %exn = load i8*, i8** %exn.slot, align 8
  call void @__clang_call_terminate(i8* %exn) #11
  unreachable
}

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map..4(%struct..kmp_privates.t.2* noalias noundef %0, i32** noalias noundef %1) #7 {
entry:
  %.addr = alloca %struct..kmp_privates.t.2*, align 8
  %.addr1 = alloca i32**, align 8
  store %struct..kmp_privates.t.2* %0, %struct..kmp_privates.t.2** %.addr, align 8, !tbaa !8
  store i32** %1, i32*** %.addr1, align 8, !tbaa !8
  %2 = load %struct..kmp_privates.t.2*, %struct..kmp_privates.t.2** %.addr, align 8
  %3 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %2, i32 0, i32 0
  %4 = load i32**, i32*** %.addr1, align 8
  store i32* %3, i32** %4, align 8, !tbaa !8
  ret void
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias noundef %1) #8 {
entry:
  %.addr = alloca i32, align 4
  %.addr1 = alloca %struct.kmp_task_t_with_privates.1*, align 8
  store i32 %0, i32* %.addr, align 4, !tbaa !4
  store %struct.kmp_task_t_with_privates.1* %1, %struct.kmp_task_t_with_privates.1** %.addr1, align 8, !tbaa !8
  %2 = load i32, i32* %.addr, align 4, !tbaa !4
  %3 = load %struct.kmp_task_t_with_privates.1*, %struct.kmp_task_t_with_privates.1** %.addr1, align 8
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %3, i32 0, i32 0
  %5 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %4, i32 0, i32 2
  %6 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %4, i32 0, i32 0
  %7 = load i8*, i8** %6, align 8, !tbaa !23
  %8 = bitcast i8* %7 to %struct.anon.0*
  %9 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %3, i32 0, i32 1
  %10 = bitcast %struct..kmp_privates.t.2* %9 to i8*
  %11 = bitcast %struct.kmp_task_t_with_privates.1* %3 to i8*
  call void @.omp_outlined..2(i32 %2, i32* %5, i8* %10, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t.2*, i32**)* @.omp_task_privates_map..4 to void (i8*, ...)*), i8* %11, %struct.anon.0* %8) #9
  ret i32 0
}

; Function Attrs: nounwind
declare !callback !24 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) #9

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

attributes #0 = { mustprogress norecurse uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { alwaysinline norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { alwaysinline nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { noinline noreturn nounwind }
attributes #7 = { alwaysinline uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { norecurse uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { nounwind }
attributes #10 = { argmemonly nofree nounwind willreturn }
attributes #11 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.6"}
!4 = !{!5, !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = !{!9, !9, i64 0}
!9 = !{!"any pointer", !6, i64 0}
!10 = !{!11, !9, i64 0}
!11 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !13, i64 40}
!12 = !{!"_ZTS10kmp_task_t", !9, i64 0, !9, i64 8, !5, i64 16, !6, i64 24, !6, i64 32}
!13 = !{!"_ZTS15.kmp_privates.t", !5, i64 0, !5, i64 4}
!14 = !{i64 0, i64 8, !8, i64 8, i64 8, !8}
!15 = !{!11, !5, i64 40}
!16 = !{!11, !5, i64 44}
!17 = !{!18, !5, i64 40}
!18 = !{!"_ZTS24kmp_task_t_with_privates", !12, i64 0, !19, i64 40}
!19 = !{!"_ZTS15.kmp_privates.t", !5, i64 0}
!20 = !{!21, !9, i64 0}
!21 = !{!"_ZTSZ4mainE3$_0", !9, i64 0, !9, i64 8}
!22 = !{!21, !9, i64 8}
!23 = !{!18, !9, i64 0}
!24 = !{!25}
!25 = !{i64 2, i64 -1, i64 -1, i1 true}
