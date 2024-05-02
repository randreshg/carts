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
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %6 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %6) #9
  store i32 1, i32* %2, align 4, !tbaa !4
  %7 = bitcast i32* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %7) #9
  store i32 10000, i32* %3, align 4, !tbaa !4
  %8 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %8) #9
  %9 = call i32 @rand() #9
  %10 = srem i32 %9, 10
  %11 = add nsw i32 %10, 10
  store i32 %11, i32* %4, align 4, !tbaa !4
  %12 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %12) #9
  %13 = call i32 @rand() #9
  store i32 %13, i32* %5, align 4, !tbaa !4
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* @1, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, i32*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32* %4, i32* %5, i32* %2, i32* %3)
  %14 = load i32, i32* %2, align 4, !tbaa !4
  %15 = load i32, i32* %4, align 4, !tbaa !4
  %16 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([31 x i8], [31 x i8]* @.str.6, i64 0, i64 0), i32 noundef %14, i32 noundef %15)
  %17 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %17) #9
  %18 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %18) #9
  %19 = bitcast i32* %3 to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %19) #9
  %20 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %20) #9
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() #2

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias noundef %0, i32* noalias noundef %1, i32* noundef nonnull align 4 dereferenceable(4) %2, i32* noundef nonnull align 4 dereferenceable(4) %3, i32* noundef nonnull align 4 dereferenceable(4) %4, i32* noundef nonnull align 4 dereferenceable(4) %5) #3 {
  %7 = alloca i32*, align 8
  %8 = alloca i32*, align 8
  %9 = alloca i32*, align 8
  %10 = alloca i32*, align 8
  %11 = alloca i32*, align 8
  %12 = alloca i32*, align 8
  %13 = alloca %struct.anon, align 8
  %14 = alloca %struct.anon.0, align 1
  store i32* %0, i32** %7, align 8, !tbaa !8
  store i32* %1, i32** %8, align 8, !tbaa !8
  store i32* %2, i32** %9, align 8, !tbaa !8
  store i32* %3, i32** %10, align 8, !tbaa !8
  store i32* %4, i32** %11, align 8, !tbaa !8
  store i32* %5, i32** %12, align 8, !tbaa !8
  %15 = load i32*, i32** %9, align 8, !tbaa !8
  %16 = load i32*, i32** %10, align 8, !tbaa !8
  %17 = load i32*, i32** %11, align 8, !tbaa !8
  %18 = load i32*, i32** %12, align 8, !tbaa !8
  %19 = getelementptr inbounds %struct.anon, %struct.anon* %13, i32 0, i32 0
  store i32* %17, i32** %19, align 8, !tbaa !8
  %20 = getelementptr inbounds %struct.anon, %struct.anon* %13, i32 0, i32 1
  store i32* %18, i32** %20, align 8, !tbaa !8
  %21 = load i32*, i32** %7, align 8
  %22 = load i32, i32* %21, align 4, !tbaa !4
  %23 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* @1, i32 %22, i32 1, i64 48, i64 16, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates*)* @.omp_task_entry. to i32 (i32, i8*)*))
  %24 = bitcast i8* %23 to %struct.kmp_task_t_with_privates*
  %25 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %24, i32 0, i32 0
  %26 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %25, i32 0, i32 0
  %27 = load i8*, i8** %26, align 8, !tbaa !10
  %28 = bitcast %struct.anon* %13 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %27, i8* align 8 %28, i64 16, i1 false), !tbaa.struct !14
  %29 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %24, i32 0, i32 1
  %30 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %29, i32 0, i32 0
  %31 = load i32, i32* %15, align 4, !tbaa !4
  store i32 %31, i32* %30, align 8, !tbaa !15
  %32 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %29, i32 0, i32 1
  %33 = load i32, i32* %16, align 4, !tbaa !4
  store i32 %33, i32* %32, align 4, !tbaa !16
  %34 = call i32 @__kmpc_omp_task(%struct.ident_t* @1, i32 %22, i8* %23)
  %35 = call i8* @__kmpc_omp_task_alloc(%struct.ident_t* @1, i32 %22, i32 1, i64 48, i64 1, i32 (i32, i8*)* bitcast (i32 (i32, %struct.kmp_task_t_with_privates.1*)* @.omp_task_entry..5 to i32 (i32, i8*)*))
  %36 = bitcast i8* %35 to %struct.kmp_task_t_with_privates.1*
  %37 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %36, i32 0, i32 0
  %38 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %36, i32 0, i32 1
  %39 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %38, i32 0, i32 0
  %40 = load i32, i32* %17, align 4, !tbaa !4
  store i32 %40, i32* %39, align 8, !tbaa !17
  %41 = call i32 @__kmpc_omp_task(%struct.ident_t* @1, i32 %22, i8* %35)
  ret void
}

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %0, i32* noalias noundef %1, i8* noalias noundef %2, void (i8*, ...)* noalias noundef %3, i8* noundef %4, %struct.anon* noalias noundef %5) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %7 = alloca i32, align 4
  %8 = alloca i32*, align 8
  %9 = alloca i8*, align 8
  %10 = alloca void (i8*, ...)*, align 8
  %11 = alloca i8*, align 8
  %12 = alloca %struct.anon*, align 8
  %13 = alloca i32*, align 8
  %14 = alloca i32*, align 8
  %15 = alloca i8*, align 8
  %16 = alloca i32, align 4
  store i32 %0, i32* %7, align 4, !tbaa !4
  store i32* %1, i32** %8, align 8, !tbaa !8
  store i8* %2, i8** %9, align 8, !tbaa !8
  store void (i8*, ...)* %3, void (i8*, ...)** %10, align 8, !tbaa !8
  store i8* %4, i8** %11, align 8, !tbaa !8
  store %struct.anon* %5, %struct.anon** %12, align 8, !tbaa !8
  %17 = load %struct.anon*, %struct.anon** %12, align 8
  %18 = load void (i8*, ...)*, void (i8*, ...)** %10, align 8
  %19 = load i8*, i8** %9, align 8
  %20 = bitcast void (i8*, ...)* %18 to void (i8*, i32**, i32**)*
  call void %20(i8* %19, i32** %13, i32** %14)
  %21 = load i32*, i32** %13, align 8
  %22 = load i32*, i32** %14, align 8
  %23 = getelementptr inbounds %struct.anon, %struct.anon* %17, i32 0, i32 0
  %24 = load i32*, i32** %23, align 8, !tbaa !20
  %25 = load i32, i32* %24, align 4, !tbaa !4
  %26 = getelementptr inbounds %struct.anon, %struct.anon* %17, i32 0, i32 1
  %27 = load i32*, i32** %26, align 8, !tbaa !22
  %28 = load i32, i32* %27, align 4, !tbaa !4
  %29 = load i32, i32* %21, align 4, !tbaa !4
  %30 = load i32, i32* %22, align 4, !tbaa !4
  %31 = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i64 0, i64 0), i32 noundef %25, i32 noundef %28, i32 noundef %29, i32 noundef %30)
          to label %32 unwind label %41

32:                                               ; preds = %6
  %33 = getelementptr inbounds %struct.anon, %struct.anon* %17, i32 0, i32 0
  %34 = load i32*, i32** %33, align 8, !tbaa !20
  %35 = load i32, i32* %34, align 4, !tbaa !4
  %36 = add nsw i32 %35, 1
  store i32 %36, i32* %34, align 4, !tbaa !4
  %37 = getelementptr inbounds %struct.anon, %struct.anon* %17, i32 0, i32 1
  %38 = load i32*, i32** %37, align 8, !tbaa !22
  %39 = load i32, i32* %38, align 4, !tbaa !4
  %40 = add nsw i32 %39, -1
  store i32 %40, i32* %38, align 4, !tbaa !4
  ret void

41:                                               ; preds = %6
  %42 = landingpad { i8*, i32 }
          catch i8* null
  %43 = extractvalue { i8*, i32 } %42, 0
  store i8* %43, i8** %15, align 8
  %44 = extractvalue { i8*, i32 } %42, 1
  store i32 %44, i32* %16, align 4
  br label %45

45:                                               ; preds = %41
  %46 = load i8*, i8** %15, align 8
  call void @__clang_call_terminate(i8* %46) #11
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
  %4 = alloca %struct..kmp_privates.t*, align 8
  %5 = alloca i32**, align 8
  %6 = alloca i32**, align 8
  store %struct..kmp_privates.t* %0, %struct..kmp_privates.t** %4, align 8, !tbaa !8
  store i32** %1, i32*** %5, align 8, !tbaa !8
  store i32** %2, i32*** %6, align 8, !tbaa !8
  %7 = load %struct..kmp_privates.t*, %struct..kmp_privates.t** %4, align 8
  %8 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %7, i32 0, i32 0
  %9 = load i32**, i32*** %5, align 8
  store i32* %8, i32** %9, align 8, !tbaa !8
  %10 = getelementptr inbounds %struct..kmp_privates.t, %struct..kmp_privates.t* %7, i32 0, i32 1
  %11 = load i32**, i32*** %6, align 8
  store i32* %10, i32** %11, align 8, !tbaa !8
  ret void
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, %struct.kmp_task_t_with_privates* noalias noundef %1) #8 {
  %3 = alloca i32, align 4
  %4 = alloca %struct.kmp_task_t_with_privates*, align 8
  store i32 %0, i32* %3, align 4, !tbaa !4
  store %struct.kmp_task_t_with_privates* %1, %struct.kmp_task_t_with_privates** %4, align 8, !tbaa !8
  %5 = load i32, i32* %3, align 4, !tbaa !4
  %6 = load %struct.kmp_task_t_with_privates*, %struct.kmp_task_t_with_privates** %4, align 8
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %6, i32 0, i32 0
  %8 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %7, i32 0, i32 2
  %9 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %7, i32 0, i32 0
  %10 = load i8*, i8** %9, align 8, !tbaa !10
  %11 = bitcast i8* %10 to %struct.anon*
  %12 = getelementptr inbounds %struct.kmp_task_t_with_privates, %struct.kmp_task_t_with_privates* %6, i32 0, i32 1
  %13 = bitcast %struct..kmp_privates.t* %12 to i8*
  %14 = bitcast %struct.kmp_task_t_with_privates* %6 to i8*
  call void @.omp_outlined..1(i32 %5, i32* %8, i8* %13, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t*, i32**, i32**)* @.omp_task_privates_map. to void (i8*, ...)*), i8* %14, %struct.anon* %11) #9
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @__kmpc_omp_task_alloc(%struct.ident_t*, i32, i32, i64, i64, i32 (i32, i8*)*) #9

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #10

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(%struct.ident_t*, i32, i8*) #9

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..2(i32 noundef %0, i32* noalias noundef %1, i8* noalias noundef %2, void (i8*, ...)* noalias noundef %3, i8* noundef %4, %struct.anon.0* noalias noundef %5) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %7 = alloca i32, align 4
  %8 = alloca i32*, align 8
  %9 = alloca i8*, align 8
  %10 = alloca void (i8*, ...)*, align 8
  %11 = alloca i8*, align 8
  %12 = alloca %struct.anon.0*, align 8
  %13 = alloca i32*, align 8
  %14 = alloca i8*, align 8
  %15 = alloca i32, align 4
  store i32 %0, i32* %7, align 4, !tbaa !4
  store i32* %1, i32** %8, align 8, !tbaa !8
  store i8* %2, i8** %9, align 8, !tbaa !8
  store void (i8*, ...)* %3, void (i8*, ...)** %10, align 8, !tbaa !8
  store i8* %4, i8** %11, align 8, !tbaa !8
  store %struct.anon.0* %5, %struct.anon.0** %12, align 8, !tbaa !8
  %16 = load %struct.anon.0*, %struct.anon.0** %12, align 8
  %17 = load void (i8*, ...)*, void (i8*, ...)** %10, align 8
  %18 = load i8*, i8** %9, align 8
  %19 = bitcast void (i8*, ...)* %17 to void (i8*, i32**)*
  call void %19(i8* %18, i32** %13)
  %20 = load i32*, i32** %13, align 8
  %21 = load i32, i32* %20, align 4, !tbaa !4
  %22 = invoke i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([27 x i8], [27 x i8]* @.str.3, i64 0, i64 0), i32 noundef %21)
          to label %23 unwind label %26

23:                                               ; preds = %6
  %24 = load i32, i32* %20, align 4, !tbaa !4
  %25 = add nsw i32 %24, 1
  store i32 %25, i32* %20, align 4, !tbaa !4
  ret void

26:                                               ; preds = %6
  %27 = landingpad { i8*, i32 }
          catch i8* null
  %28 = extractvalue { i8*, i32 } %27, 0
  store i8* %28, i8** %14, align 8
  %29 = extractvalue { i8*, i32 } %27, 1
  store i32 %29, i32* %15, align 4
  br label %30

30:                                               ; preds = %26
  %31 = load i8*, i8** %14, align 8
  call void @__clang_call_terminate(i8* %31) #11
  unreachable
}

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map..4(%struct..kmp_privates.t.2* noalias noundef %0, i32** noalias noundef %1) #7 {
  %3 = alloca %struct..kmp_privates.t.2*, align 8
  %4 = alloca i32**, align 8
  store %struct..kmp_privates.t.2* %0, %struct..kmp_privates.t.2** %3, align 8, !tbaa !8
  store i32** %1, i32*** %4, align 8, !tbaa !8
  %5 = load %struct..kmp_privates.t.2*, %struct..kmp_privates.t.2** %3, align 8
  %6 = getelementptr inbounds %struct..kmp_privates.t.2, %struct..kmp_privates.t.2* %5, i32 0, i32 0
  %7 = load i32**, i32*** %4, align 8
  store i32* %6, i32** %7, align 8, !tbaa !8
  ret void
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry..5(i32 noundef %0, %struct.kmp_task_t_with_privates.1* noalias noundef %1) #8 {
  %3 = alloca i32, align 4
  %4 = alloca %struct.kmp_task_t_with_privates.1*, align 8
  store i32 %0, i32* %3, align 4, !tbaa !4
  store %struct.kmp_task_t_with_privates.1* %1, %struct.kmp_task_t_with_privates.1** %4, align 8, !tbaa !8
  %5 = load i32, i32* %3, align 4, !tbaa !4
  %6 = load %struct.kmp_task_t_with_privates.1*, %struct.kmp_task_t_with_privates.1** %4, align 8
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %6, i32 0, i32 0
  %8 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %7, i32 0, i32 2
  %9 = getelementptr inbounds %struct.kmp_task_t, %struct.kmp_task_t* %7, i32 0, i32 0
  %10 = load i8*, i8** %9, align 8, !tbaa !23
  %11 = bitcast i8* %10 to %struct.anon.0*
  %12 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, %struct.kmp_task_t_with_privates.1* %6, i32 0, i32 1
  %13 = bitcast %struct..kmp_privates.t.2* %12 to i8*
  %14 = bitcast %struct.kmp_task_t_with_privates.1* %6 to i8*
  call void @.omp_outlined..2(i32 %5, i32* %8, i8* %13, void (i8*, ...)* bitcast (void (%struct..kmp_privates.t.2*, i32**)* @.omp_task_privates_map..4 to void (i8*, ...)*), i8* %14, %struct.anon.0* %11) #9
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
!3 = !{!"clang version 14.0.1"}
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
