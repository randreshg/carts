; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@stdout = external global ptr
@str4 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str3 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str2 = internal constant [43 x i8] c"Parallel addition finished in %f seconds.\0A\00"
@str1 = internal constant [38 x i8] c"Error: N must be a positive integer.\0A\00"
@str0 = internal constant [25 x i8] c"Usage: %s <num_threads>\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare void @artsPersistentEventIncrementLatch(i64, i64)

declare void @artsPersistentEventDecrementLatch(i64, i64)

declare i64 @artsGetCurrentEpochGuid()

declare void @artsAddDependenceToPersistentEvent(i64, i64, i32, i64)

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i64 @artsPersistentEventCreate(i32, i32, i64)

declare i1 @artsWaitOnHandle(i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32)

declare i64 @artsEdtCreate(ptr, i32, i32, ptr, i32)

declare ptr @artsDbCreateWithGuid(i64, i64)

declare i64 @artsReserveGuidRoute(i32, i32)

declare i32 @artsGetCurrentNode()

declare i32 @printf(ptr, ...)

define i32 @mainBody(i32 %0, ptr %1) {
  %3 = icmp slt i32 %0, 2
  %4 = icmp sge i32 %0, 2
  %5 = select i1 %3, i32 1, i32 undef
  br i1 %3, label %6, label %9

6:                                                ; preds = %2
  %7 = load ptr, ptr %1, align 8
  %8 = call i32 (ptr, ...) @printf(ptr @str0, ptr %7)
  br label %9

9:                                                ; preds = %6, %2
  br i1 %4, label %10, label %20

10:                                               ; preds = %9
  %11 = getelementptr ptr, ptr %1, i32 1
  %12 = load ptr, ptr %11, align 8
  %13 = call i32 @atoi(ptr %12)
  %14 = icmp sgt i32 %13, 0
  %15 = icmp sle i32 %13, 0
  %16 = select i1 %15, i32 1, i32 %5
  br i1 %15, label %17, label %19

17:                                               ; preds = %10
  %18 = call i32 (ptr, ...) @printf(ptr @str1)
  br label %19

19:                                               ; preds = %17, %10
  br label %21

20:                                               ; preds = %9
  br label %21

21:                                               ; preds = %19, %20
  %22 = phi i32 [ undef, %20 ], [ %13, %19 ]
  %23 = phi i1 [ false, %20 ], [ %14, %19 ]
  %24 = phi i32 [ %5, %20 ], [ %16, %19 ]
  %25 = phi i1 [ undef, %20 ], [ %14, %19 ]
  br label %26

26:                                               ; preds = %21
  %27 = select i1 %23, i32 0, i32 %24
  br i1 %23, label %28, label %113

28:                                               ; preds = %26
  %29 = sext i32 %22 to i64
  %30 = call i32 @artsGetCurrentNode()
  %31 = alloca i64, i64 %29, align 8
  br label %32

32:                                               ; preds = %35, %28
  %33 = phi i64 [ 0, %28 ], [ %38, %35 ]
  %34 = icmp slt i64 %33, %29
  br i1 %34, label %35, label %39

35:                                               ; preds = %32
  %36 = call i64 @artsPersistentEventCreate(i32 %30, i32 0, i64 0)
  %37 = getelementptr i64, ptr %31, i64 %33
  store i64 %36, ptr %37, align 8
  %38 = add i64 %33, 1
  br label %32

39:                                               ; preds = %32
  %40 = call i32 @artsGetCurrentNode()
  %41 = alloca i64, i64 %29, align 8
  %42 = alloca ptr, i64 %29, align 8
  br label %43

43:                                               ; preds = %46, %39
  %44 = phi i64 [ 0, %39 ], [ %51, %46 ]
  %45 = icmp slt i64 %44, %29
  br i1 %45, label %46, label %52

46:                                               ; preds = %43
  %47 = call i64 @artsReserveGuidRoute(i32 10, i32 %40)
  %48 = call ptr @artsDbCreateWithGuid(i64 %47, i64 4)
  %49 = getelementptr i64, ptr %41, i64 %44
  store i64 %47, ptr %49, align 8
  %50 = getelementptr ptr, ptr %42, i64 %44
  store ptr %48, ptr %50, align 8
  %51 = add i64 %44, 1
  br label %43

52:                                               ; preds = %43
  %53 = call double @omp_get_wtime()
  %54 = call i32 @artsGetCurrentNode()
  %55 = alloca i64, i64 0, align 8
  %56 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %54, i32 0, ptr %55, i32 1)
  %57 = call i64 @artsInitializeAndStartEpoch(i64 %56, i32 0)
  %58 = call i32 @artsGetCurrentNode()
  %59 = alloca i64, i64 1, align 8
  store i64 %29, ptr %59, align 8
  %60 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %58, i32 1, ptr %59, i32 %22, i64 %57)
  %61 = call ptr @malloc(i64 8)
  store i64 0, ptr %61, align 8
  br label %62

62:                                               ; preds = %65, %52
  %63 = phi i64 [ 0, %52 ], [ %73, %65 ]
  %64 = icmp slt i64 %63, %29
  br i1 %64, label %65, label %74

65:                                               ; preds = %62
  %66 = getelementptr i64, ptr %31, i64 %63
  %67 = load i64, ptr %66, align 8
  %68 = load i64, ptr %61, align 8
  %69 = getelementptr i64, ptr %41, i64 %63
  %70 = load i64, ptr %69, align 8
  %71 = trunc i64 %68 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %67, i64 %60, i32 %71, i64 %70)
  %72 = add i64 %68, 1
  store i64 %72, ptr %61, align 8
  %73 = add i64 %63, 1
  br label %62

74:                                               ; preds = %62
  %75 = call i1 @artsWaitOnHandle(i64 %57)
  %76 = call double @omp_get_wtime()
  %77 = fsub double %76, %53
  %78 = call i32 (ptr, ...) @printf(ptr @str2, double %77)
  br i1 %25, label %79, label %102

79:                                               ; preds = %74
  %80 = icmp sgt i64 %29, 0
  br i1 %80, label %81, label %98

81:                                               ; preds = %84, %79
  %82 = phi i64 [ %95, %84 ], [ 0, %79 ]
  %83 = phi i8 [ %93, %84 ], [ 1, %79 ]
  br label %84

84:                                               ; preds = %81
  %85 = trunc i64 %82 to i32
  %86 = mul i64 %82, 8
  %87 = getelementptr i8, ptr %42, i64 %86
  %88 = load ptr, ptr %87, align 8
  %89 = load i32, ptr %88, align 4
  %90 = mul i32 %85, 2
  %91 = add i32 %85, %90
  %92 = icmp ne i32 %89, %91
  %93 = select i1 %92, i8 0, i8 %83
  %94 = icmp eq i32 %89, %91
  %95 = add i64 %82, 1
  %96 = icmp slt i64 %95, %29
  %97 = and i1 %96, %94
  br i1 %97, label %81, label %98

98:                                               ; preds = %84, %79
  %99 = phi i8 [ %93, %84 ], [ 1, %79 ]
  br label %100

100:                                              ; preds = %98
  %101 = icmp ne i8 %99, 0
  br label %103

102:                                              ; preds = %74
  br label %103

103:                                              ; preds = %100, %102
  %104 = phi i1 [ true, %102 ], [ %101, %100 ]
  br label %105

105:                                              ; preds = %103
  br i1 %104, label %106, label %108

106:                                              ; preds = %105
  %107 = call i32 (ptr, ...) @printf(ptr @str3)
  br label %110

108:                                              ; preds = %105
  %109 = call i32 (ptr, ...) @printf(ptr @str4)
  br label %110

110:                                              ; preds = %106, %108
  %111 = load ptr, ptr @stdout, align 8
  %112 = call i32 @fflush(ptr %111)
  br label %113

113:                                              ; preds = %110, %26
  ret i32 %27
}

declare i32 @atoi(ptr)

declare double @omp_get_wtime()

declare i32 @fflush(ptr)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = alloca i64, i64 1, align 8
  store i64 0, ptr %6, align 8
  %7 = alloca i64, i64 %5, align 8
  br label %8

8:                                                ; preds = %11, %4
  %9 = phi i64 [ 0, %4 ], [ %20, %11 ]
  %10 = icmp slt i64 %9, %5
  br i1 %10, label %11, label %21

11:                                               ; preds = %8
  %12 = load i64, ptr %6, align 8
  %13 = mul i64 %12, 24
  %14 = trunc i64 %13 to i32
  %15 = getelementptr i8, ptr %3, i32 %14
  %16 = getelementptr { i64, i32, ptr }, ptr %15, i32 0, i32 0
  %17 = load i64, ptr %16, align 8
  %18 = getelementptr i64, ptr %7, i64 %9
  store i64 %17, ptr %18, align 8
  %19 = add i64 %12, 1
  store i64 %19, ptr %6, align 8
  %20 = add i64 %9, 1
  br label %8

21:                                               ; preds = %8
  %22 = call i32 @artsGetCurrentNode()
  %23 = alloca i64, i64 %5, align 8
  br label %24

24:                                               ; preds = %27, %21
  %25 = phi i64 [ 0, %21 ], [ %30, %27 ]
  %26 = icmp slt i64 %25, %5
  br i1 %26, label %27, label %31

27:                                               ; preds = %24
  %28 = call i64 @artsPersistentEventCreate(i32 %22, i32 0, i64 0)
  %29 = getelementptr i64, ptr %23, i64 %25
  store i64 %28, ptr %29, align 8
  %30 = add i64 %25, 1
  br label %24

31:                                               ; preds = %24
  %32 = alloca i64, i64 1, align 8
  %33 = alloca i64, i64 1, align 8
  %34 = alloca i64, i64 1, align 8
  %35 = alloca i64, i64 1, align 8
  br label %36

36:                                               ; preds = %39, %31
  %37 = phi i64 [ 0, %31 ], [ %70, %39 ]
  %38 = icmp slt i64 %37, %5
  br i1 %38, label %39, label %71

39:                                               ; preds = %36
  %40 = trunc i64 %37 to i32
  %41 = mul i32 %40, 2
  %42 = add i32 %40, %41
  %43 = call i64 @artsGetCurrentEpochGuid()
  %44 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %32, align 8
  store i64 0, ptr %33, align 8
  %45 = load i64, ptr %33, align 8
  %46 = add i64 %45, 1
  store i64 %46, ptr %33, align 8
  %47 = load i64, ptr %32, align 8
  %48 = add i64 %47, 1
  store i64 %48, ptr %32, align 8
  %49 = load i64, ptr %33, align 8
  %50 = trunc i64 %49 to i32
  store i64 1, ptr %34, align 8
  %51 = load i64, ptr %34, align 8
  %52 = load i64, ptr %32, align 8
  %53 = add i64 %51, %52
  store i64 %53, ptr %34, align 8
  %54 = load i64, ptr %34, align 8
  %55 = trunc i64 %54 to i32
  %56 = alloca i64, i64 %54, align 8
  %57 = sext i32 %42 to i64
  store i64 %57, ptr %56, align 8
  store i64 1, ptr %35, align 8
  %58 = getelementptr i64, ptr %23, i64 %37
  %59 = load i64, ptr %58, align 8
  %60 = load i64, ptr %35, align 8
  %61 = getelementptr i64, ptr %56, i64 %60
  store i64 %59, ptr %61, align 8
  %62 = add i64 %60, 1
  store i64 %62, ptr %35, align 8
  %63 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %44, i32 %55, ptr %56, i32 %50, i64 %43)
  %64 = load i64, ptr %58, align 8
  %65 = getelementptr i64, ptr %7, i64 %37
  %66 = load i64, ptr %65, align 8
  %67 = trunc i64 %45 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %64, i64 %63, i32 %67, i64 %66)
  %68 = load i64, ptr %58, align 8
  %69 = load i64, ptr %65, align 8
  call void @artsPersistentEventIncrementLatch(i64 %68, i64 %69)
  %70 = add i64 %37, 1
  br label %36

71:                                               ; preds = %36
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 0
  %8 = load i64, ptr %7, align 8
  %9 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %10 = load ptr, ptr %9, align 8
  store i32 %6, ptr %10, align 4
  %11 = getelementptr i64, ptr %1, i32 1
  %12 = load i64, ptr %11, align 8
  call void @artsPersistentEventDecrementLatch(i64 %12, i64 %8)
  ret void
}

define void @artsMain(i32 %0, ptr %1) {
  %3 = call i32 @mainBody(i32 %0, ptr %1)
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
