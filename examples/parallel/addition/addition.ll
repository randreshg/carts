; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@stdout = external global ptr
@str4 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str3 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str2 = internal constant [24 x i8] c"Finished in %f seconds\0A\00"
@str1 = internal constant [38 x i8] c"Error: N must be a positive integer.\0A\00"
@str0 = internal constant [18 x i8] c"Usage: %s <size>\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsGetCurrentEpochGuid()

declare void @artsDbIncrementLatch(i64)

declare void @artsDbAddDependence(i64, i64, i32)

declare i64 @artsGetCurrentGuid()

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

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
  br i1 %23, label %28, label %112

28:                                               ; preds = %26
  %29 = sext i32 %22 to i64
  %30 = call i32 @artsGetCurrentNode()
  %31 = alloca i64, i64 %29, align 8
  %32 = alloca ptr, i64 %29, align 8
  br label %33

33:                                               ; preds = %36, %28
  %34 = phi i64 [ 0, %28 ], [ %41, %36 ]
  %35 = icmp slt i64 %34, %29
  br i1 %35, label %36, label %42

36:                                               ; preds = %33
  %37 = call i64 @artsReserveGuidRoute(i32 10, i32 %30)
  %38 = call ptr @artsDbCreateWithGuid(i64 %37, i64 4)
  %39 = getelementptr i64, ptr %31, i64 %34
  store i64 %37, ptr %39, align 8
  %40 = getelementptr ptr, ptr %32, i64 %34
  store ptr %38, ptr %40, align 8
  %41 = add i64 %34, 1
  br label %33

42:                                               ; preds = %33
  %43 = call double @omp_get_wtime()
  %44 = call i32 @artsGetCurrentNode()
  %45 = alloca i64, i64 0, align 8
  %46 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %44, i32 0, ptr %45, i32 1)
  %47 = call i64 @artsInitializeAndStartEpoch(i64 %46, i32 0)
  %48 = call i32 @artsGetCurrentNode()
  %49 = alloca i64, i64 1, align 8
  store i64 %29, ptr %49, align 8
  %50 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %48, i32 1, ptr %49, i32 %22, i64 %47)
  %51 = call ptr @malloc(i64 8)
  store i64 0, ptr %51, align 8
  br label %52

52:                                               ; preds = %55, %42
  %53 = phi i64 [ 0, %42 ], [ %61, %55 ]
  %54 = icmp slt i64 %53, %29
  br i1 %54, label %55, label %62

55:                                               ; preds = %52
  %56 = load i64, ptr %51, align 8
  %57 = getelementptr i64, ptr %31, i64 %53
  %58 = load i64, ptr %57, align 8
  %59 = trunc i64 %56 to i32
  call void @artsDbAddDependence(i64 %58, i64 %50, i32 %59)
  %60 = add i64 %56, 1
  store i64 %60, ptr %51, align 8
  %61 = add i64 %53, 1
  br label %52

62:                                               ; preds = %52
  %63 = call ptr @malloc(i64 8)
  store i64 0, ptr %63, align 8
  br label %64

64:                                               ; preds = %67, %62
  %65 = phi i64 [ 0, %62 ], [ %72, %67 ]
  %66 = icmp slt i64 %65, %29
  br i1 %66, label %67, label %73

67:                                               ; preds = %64
  %68 = getelementptr i64, ptr %31, i64 %65
  %69 = load i64, ptr %68, align 8
  call void @artsDbIncrementLatch(i64 %69)
  %70 = load i64, ptr %63, align 8
  %71 = add i64 %70, 1
  store i64 %71, ptr %63, align 8
  %72 = add i64 %65, 1
  br label %64

73:                                               ; preds = %64
  %74 = call i1 @artsWaitOnHandle(i64 %47)
  %75 = call double @omp_get_wtime()
  %76 = fsub double %75, %43
  %77 = call i32 (ptr, ...) @printf(ptr @str2, double %76)
  br i1 %25, label %78, label %101

78:                                               ; preds = %73
  %79 = icmp sgt i64 %29, 0
  br i1 %79, label %80, label %97

80:                                               ; preds = %83, %78
  %81 = phi i64 [ %94, %83 ], [ 0, %78 ]
  %82 = phi i8 [ %92, %83 ], [ 1, %78 ]
  br label %83

83:                                               ; preds = %80
  %84 = trunc i64 %81 to i32
  %85 = mul i64 %81, 8
  %86 = getelementptr i8, ptr %32, i64 %85
  %87 = load ptr, ptr %86, align 8
  %88 = load i32, ptr %87, align 4
  %89 = mul i32 %84, 2
  %90 = add i32 %84, %89
  %91 = icmp ne i32 %88, %90
  %92 = select i1 %91, i8 0, i8 %82
  %93 = icmp eq i32 %88, %90
  %94 = add i64 %81, 1
  %95 = icmp slt i64 %94, %29
  %96 = and i1 %95, %93
  br i1 %96, label %80, label %97

97:                                               ; preds = %83, %78
  %98 = phi i8 [ %92, %83 ], [ 1, %78 ]
  br label %99

99:                                               ; preds = %97
  %100 = icmp ne i8 %98, 0
  br label %102

101:                                              ; preds = %73
  br label %102

102:                                              ; preds = %99, %101
  %103 = phi i1 [ true, %101 ], [ %100, %99 ]
  br label %104

104:                                              ; preds = %102
  br i1 %103, label %105, label %107

105:                                              ; preds = %104
  %106 = call i32 (ptr, ...) @printf(ptr @str3)
  br label %109

107:                                              ; preds = %104
  %108 = call i32 (ptr, ...) @printf(ptr @str4)
  br label %109

109:                                              ; preds = %105, %107
  %110 = load ptr, ptr @stdout, align 8
  %111 = call i32 @fflush(ptr %110)
  br label %112

112:                                              ; preds = %109, %26
  ret i32 %27
}

declare i32 @atoi(ptr)

declare double @omp_get_wtime()

declare i32 @fflush(ptr)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = alloca i64, i64 1, align 8
  store i64 0, ptr %7, align 8
  %8 = alloca i64, i64 %6, align 8
  br label %9

9:                                                ; preds = %12, %4
  %10 = phi i64 [ 0, %4 ], [ %21, %12 ]
  %11 = icmp slt i64 %10, %6
  br i1 %11, label %12, label %22

12:                                               ; preds = %9
  %13 = load i64, ptr %7, align 8
  %14 = mul i64 %13, 24
  %15 = trunc i64 %14 to i32
  %16 = getelementptr i8, ptr %3, i32 %15
  %17 = getelementptr { i64, i32, ptr }, ptr %16, i32 0, i32 0
  %18 = load i64, ptr %17, align 8
  %19 = getelementptr i64, ptr %8, i64 %10
  store i64 %18, ptr %19, align 8
  %20 = add i64 %13, 1
  store i64 %20, ptr %7, align 8
  %21 = add i64 %10, 1
  br label %9

22:                                               ; preds = %9
  %23 = alloca i64, i64 1, align 8
  %24 = alloca i64, i64 1, align 8
  br label %25

25:                                               ; preds = %28, %22
  %26 = phi i64 [ 0, %22 ], [ %47, %28 ]
  %27 = icmp slt i64 %26, %6
  br i1 %27, label %28, label %48

28:                                               ; preds = %25
  %29 = trunc i64 %26 to i32
  %30 = mul i32 %29, 2
  %31 = add i32 %29, %30
  %32 = call i64 @artsGetCurrentEpochGuid()
  %33 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %23, align 8
  %34 = load i64, ptr %23, align 8
  %35 = add i64 %34, 1
  store i64 %35, ptr %23, align 8
  %36 = load i64, ptr %23, align 8
  %37 = trunc i64 %36 to i32
  store i64 1, ptr %24, align 8
  %38 = load i64, ptr %24, align 8
  %39 = trunc i64 %38 to i32
  %40 = alloca i64, i64 %38, align 8
  %41 = sext i32 %31 to i64
  store i64 %41, ptr %40, align 8
  %42 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %33, i32 %39, ptr %40, i32 %37, i64 %32)
  %43 = getelementptr i64, ptr %8, i64 %26
  %44 = load i64, ptr %43, align 8
  %45 = trunc i64 %34 to i32
  call void @artsDbAddDependence(i64 %44, i64 %42, i32 %45)
  %46 = load i64, ptr %43, align 8
  call void @artsDbIncrementLatch(i64 %46)
  %47 = add i64 %26, 1
  br label %25

48:                                               ; preds = %25
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = trunc i64 %6 to i32
  %8 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %9 = load ptr, ptr %8, align 8
  store i32 %7, ptr %9, align 4
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
