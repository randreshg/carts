; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str9 = internal constant [56 x i8] c"-----------------\0AMain function DONE\0A-----------------\0A\00"
@str8 = internal constant [24 x i8] c"A[%d] = %x, B[%d] = %x\0A\00"
@str7 = internal constant [15 x i8] c"Final arrays:\0A\00"
@str6 = internal constant [31 x i8] c"Task %d - 3: Final B[%d] = %d\0A\00"
@str5 = internal constant [35 x i8] c"Task %d - 2: Computing B[%d] = %d\0A\00"
@str4 = internal constant [35 x i8] c"Task %d - 1: Computing B[%d] = %d\0A\00"
@str3 = internal constant [38 x i8] c"Task %d - 0: Initializing A[%d] = %d\0A\00"
@str2 = internal constant [42 x i8] c"Initializing arrays A and B with size %d\0A\00"
@str1 = internal constant [51 x i8] c"-----------------\0AMain function\0A-----------------\0A\00"
@str0 = internal constant [13 x i8] c"Usage: %s N\0A\00"
@stderr = external global ptr

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare void @artsAddDependence(i64, i64, i32)

declare void @artsEventSatisfySlot(i64, i64, i32)

declare i64 @artsGetCurrentEpochGuid()

declare i64 @artsEventCreate(i32, i32)

declare i1 @artsWaitOnHandle(i64)

declare void @artsSignalEdt(i64, i32, i64)

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32)

declare i64 @artsEdtCreate(ptr, i32, i32, ptr, i32)

declare ptr @artsDbCreateWithGuid(i64, i64)

declare i64 @artsReserveGuidRoute(i32, i32)

declare i32 @artsGetCurrentNode()

declare i32 @printf(ptr, ...)

declare i32 @fprintf(ptr, ptr, ...)

declare i32 @atoi(ptr)

declare void @srand(i32)

declare i64 @time(ptr)

declare i32 @rand()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr i64, ptr %1, i32 2
  %6 = load i64, ptr %5, align 8
  %7 = alloca i64, i64 1, align 8
  store i64 0, ptr %7, align 8
  %8 = alloca i64, i64 %6, align 8
  br label %9

9:                                                ; preds = %12, %4
  %10 = phi i64 [ %21, %12 ], [ 0, %4 ]
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
  %23 = alloca i64, i64 %6, align 8
  br label %24

24:                                               ; preds = %27, %22
  %25 = phi i64 [ %36, %27 ], [ 0, %22 ]
  %26 = icmp slt i64 %25, %6
  br i1 %26, label %27, label %37

27:                                               ; preds = %24
  %28 = load i64, ptr %7, align 8
  %29 = mul i64 %28, 24
  %30 = trunc i64 %29 to i32
  %31 = getelementptr i8, ptr %3, i32 %30
  %32 = getelementptr { i64, i32, ptr }, ptr %31, i32 0, i32 0
  %33 = load i64, ptr %32, align 8
  %34 = getelementptr i64, ptr %23, i64 %25
  store i64 %33, ptr %34, align 8
  %35 = add i64 %28, 1
  store i64 %35, ptr %7, align 8
  %36 = add i64 %25, 1
  br label %24

37:                                               ; preds = %24
  %38 = call i32 @artsGetCurrentNode()
  %39 = alloca i64, i64 %6, align 8
  br label %40

40:                                               ; preds = %43, %37
  %41 = phi i64 [ %46, %43 ], [ 0, %37 ]
  %42 = icmp slt i64 %41, %6
  br i1 %42, label %43, label %47

43:                                               ; preds = %40
  %44 = call i64 @artsEventCreate(i32 %38, i32 1)
  %45 = getelementptr i64, ptr %39, i64 %41
  store i64 %44, ptr %45, align 8
  %46 = add i64 %41, 1
  br label %40

47:                                               ; preds = %40
  %48 = alloca i64, i64 2, align 8
  %49 = alloca i64, i64 1, align 8
  %50 = alloca i64, i64 1, align 8
  %51 = alloca i64, i64 1, align 8
  br label %52

52:                                               ; preds = %84, %47
  %53 = phi i64 [ %90, %84 ], [ 0, %47 ]
  %54 = icmp slt i64 %53, %6
  br i1 %54, label %55, label %91

55:                                               ; preds = %52
  %56 = trunc i64 %53 to i32
  %57 = call i64 @artsGetCurrentEpochGuid()
  %58 = call i32 @artsGetCurrentNode()
  %59 = sext i32 %56 to i64
  store i64 %59, ptr %48, align 8
  %60 = getelementptr i64, ptr %39, i64 %53
  %61 = load i64, ptr %60, align 8
  %62 = getelementptr i64, ptr %48, i32 1
  store i64 %61, ptr %62, align 8
  %63 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %58, i32 2, ptr %48, i32 1, i64 %57)
  %64 = getelementptr i64, ptr %23, i64 %53
  %65 = load i64, ptr %64, align 8
  call void @artsSignalEdt(i64 %63, i32 0, i64 %65)
  %66 = icmp eq i64 %53, 0
  br i1 %66, label %67, label %74

67:                                               ; preds = %55
  %68 = call i64 @artsGetCurrentEpochGuid()
  %69 = call i32 @artsGetCurrentNode()
  store i64 %59, ptr %49, align 8
  %70 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_4, i32 %69, i32 1, ptr %49, i32 2, i64 %68)
  %71 = load i64, ptr %60, align 8
  call void @artsAddDependence(i64 %71, i64 %70, i32 1)
  %72 = getelementptr i64, ptr %8, i64 %53
  %73 = load i64, ptr %72, align 8
  call void @artsSignalEdt(i64 %70, i32 0, i64 %73)
  br label %84

74:                                               ; preds = %55
  %75 = call i64 @artsGetCurrentEpochGuid()
  %76 = call i32 @artsGetCurrentNode()
  store i64 %59, ptr %50, align 8
  %77 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_5, i32 %76, i32 1, ptr %50, i32 3, i64 %75)
  %78 = load i64, ptr %60, align 8
  call void @artsAddDependence(i64 %78, i64 %77, i32 2)
  %79 = add i64 %53, -1
  %80 = getelementptr i64, ptr %8, i64 %79
  %81 = load i64, ptr %80, align 8
  call void @artsSignalEdt(i64 %77, i32 0, i64 %81)
  %82 = getelementptr i64, ptr %8, i64 %53
  %83 = load i64, ptr %82, align 8
  call void @artsSignalEdt(i64 %77, i32 1, i64 %83)
  br label %84

84:                                               ; preds = %67, %74
  %85 = call i64 @artsGetCurrentEpochGuid()
  %86 = call i32 @artsGetCurrentNode()
  store i64 %59, ptr %51, align 8
  %87 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_6, i32 %86, i32 1, ptr %51, i32 1, i64 %85)
  %88 = getelementptr i64, ptr %8, i64 %53
  %89 = load i64, ptr %88, align 8
  call void @artsSignalEdt(i64 %87, i32 0, i64 %89)
  %90 = add i64 %53, 1
  br label %52

91:                                               ; preds = %52
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 0
  %8 = load i64, ptr %7, align 8
  %9 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %10 = load ptr, ptr %9, align 8
  %11 = call i32 @rand()
  %12 = srem i32 %11, 100
  store i32 %12, ptr %10, align 4
  %13 = call i32 (ptr, ...) @printf(ptr @str3, i32 %6, i32 %6, i32 %12)
  %14 = getelementptr i64, ptr %1, i32 1
  %15 = load i64, ptr %14, align 8
  call void @artsEventSatisfySlot(i64 %15, i64 %8, i32 0)
  ret void
}

define void @__arts_edt_4(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %8 = load ptr, ptr %7, align 8
  %9 = getelementptr i8, ptr %3, i32 24
  %10 = getelementptr { i64, i32, ptr }, ptr %9, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = load i32, ptr %11, align 4
  %13 = add i32 %12, 5
  store i32 %13, ptr %8, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @str4, i32 %6, i32 %6, i32 %13)
  ret void
}

define void @__arts_edt_5(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %8 = load ptr, ptr %7, align 8
  %9 = getelementptr i8, ptr %3, i32 24
  %10 = getelementptr { i64, i32, ptr }, ptr %9, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = getelementptr i8, ptr %3, i32 48
  %13 = getelementptr { i64, i32, ptr }, ptr %12, i32 0, i32 2
  %14 = load ptr, ptr %13, align 8
  %15 = load i32, ptr %14, align 4
  %16 = load i32, ptr %8, align 4
  %17 = add i32 %15, %16
  %18 = add i32 %17, 5
  store i32 %18, ptr %11, align 4
  %19 = call i32 (ptr, ...) @printf(ptr @str5, i32 %6, i32 %6, i32 %18)
  ret void
}

define void @__arts_edt_6(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %8 = load ptr, ptr %7, align 8
  %9 = load i32, ptr %8, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @str6, i32 %6, i32 %6, i32 %9)
  ret void
}

define i32 @mainBody(i32 %0, ptr %1) {
  %3 = icmp slt i32 %0, 2
  %4 = icmp sge i32 %0, 2
  %5 = select i1 %3, i32 1, i32 undef
  br i1 %3, label %6, label %10

6:                                                ; preds = %2
  %7 = load ptr, ptr @stderr, align 8
  %8 = load ptr, ptr %1, align 8
  %9 = call i32 (ptr, ptr, ...) @fprintf(ptr %7, ptr @str0, ptr %8)
  br label %10

10:                                               ; preds = %6, %2
  %11 = select i1 %4, i32 0, i32 %5
  br i1 %4, label %12, label %97

12:                                               ; preds = %10
  %13 = getelementptr ptr, ptr %1, i32 1
  %14 = load ptr, ptr %13, align 8
  %15 = call i32 @atoi(ptr %14)
  %16 = sext i32 %15 to i64
  %17 = call i64 @time(ptr null)
  %18 = trunc i64 %17 to i32
  call void @srand(i32 %18)
  %19 = call i32 (ptr, ...) @printf(ptr @str1)
  %20 = call i32 (ptr, ...) @printf(ptr @str2, i32 %15)
  %21 = call i32 @artsGetCurrentNode()
  %22 = alloca i64, i64 %16, align 8
  %23 = alloca ptr, i64 %16, align 8
  br label %24

24:                                               ; preds = %27, %12
  %25 = phi i64 [ %32, %27 ], [ 0, %12 ]
  %26 = icmp slt i64 %25, %16
  br i1 %26, label %27, label %33

27:                                               ; preds = %24
  %28 = call i64 @artsReserveGuidRoute(i32 9, i32 %21)
  %29 = call ptr @artsDbCreateWithGuid(i64 %28, i64 4)
  %30 = getelementptr i64, ptr %22, i64 %25
  store i64 %28, ptr %30, align 8
  %31 = getelementptr ptr, ptr %23, i64 %25
  store ptr %29, ptr %31, align 8
  %32 = add i64 %25, 1
  br label %24

33:                                               ; preds = %24
  %34 = call i32 @artsGetCurrentNode()
  %35 = alloca i64, i64 %16, align 8
  %36 = alloca ptr, i64 %16, align 8
  br label %37

37:                                               ; preds = %40, %33
  %38 = phi i64 [ %45, %40 ], [ 0, %33 ]
  %39 = icmp slt i64 %38, %16
  br i1 %39, label %40, label %46

40:                                               ; preds = %37
  %41 = call i64 @artsReserveGuidRoute(i32 9, i32 %34)
  %42 = call ptr @artsDbCreateWithGuid(i64 %41, i64 4)
  %43 = getelementptr i64, ptr %35, i64 %38
  store i64 %41, ptr %43, align 8
  %44 = getelementptr ptr, ptr %36, i64 %38
  store ptr %42, ptr %44, align 8
  %45 = add i64 %38, 1
  br label %37

46:                                               ; preds = %37
  %47 = call i32 @artsGetCurrentNode()
  %48 = alloca i64, i64 0, align 8
  %49 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %47, i32 0, ptr %48, i32 1)
  %50 = call i64 @artsInitializeAndStartEpoch(i64 %49, i32 0)
  %51 = call i32 @artsGetCurrentNode()
  %52 = add i64 %16, %16
  %53 = trunc i64 %52 to i32
  %54 = alloca i64, i64 3, align 8
  store i64 %16, ptr %54, align 8
  %55 = getelementptr i64, ptr %54, i32 1
  store i64 %16, ptr %55, align 8
  %56 = getelementptr i64, ptr %54, i32 2
  store i64 %16, ptr %56, align 8
  %57 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %51, i32 3, ptr %54, i32 %53, i64 %50)
  %58 = call ptr @malloc(i64 8)
  store i64 0, ptr %58, align 8
  br label %59

59:                                               ; preds = %62, %46
  %60 = phi i64 [ %68, %62 ], [ 0, %46 ]
  %61 = icmp slt i64 %60, %16
  br i1 %61, label %62, label %69

62:                                               ; preds = %59
  %63 = getelementptr i64, ptr %22, i64 %60
  %64 = load i64, ptr %63, align 8
  %65 = load i64, ptr %58, align 8
  %66 = trunc i64 %65 to i32
  call void @artsSignalEdt(i64 %57, i32 %66, i64 %64)
  %67 = add i64 %65, 1
  store i64 %67, ptr %58, align 8
  %68 = add i64 %60, 1
  br label %59

69:                                               ; preds = %59
  %70 = call ptr @malloc(i64 8)
  store i64 %16, ptr %70, align 8
  br label %71

71:                                               ; preds = %74, %69
  %72 = phi i64 [ %80, %74 ], [ 0, %69 ]
  %73 = icmp slt i64 %72, %16
  br i1 %73, label %74, label %81

74:                                               ; preds = %71
  %75 = getelementptr i64, ptr %35, i64 %72
  %76 = load i64, ptr %75, align 8
  %77 = load i64, ptr %70, align 8
  %78 = trunc i64 %77 to i32
  call void @artsSignalEdt(i64 %57, i32 %78, i64 %76)
  %79 = add i64 %77, 1
  store i64 %79, ptr %70, align 8
  %80 = add i64 %72, 1
  br label %71

81:                                               ; preds = %71
  %82 = call i1 @artsWaitOnHandle(i64 %50)
  %83 = call i32 (ptr, ...) @printf(ptr @str7)
  br label %84

84:                                               ; preds = %87, %81
  %85 = phi i64 [ %94, %87 ], [ 0, %81 ]
  %86 = icmp slt i64 %85, %16
  br i1 %86, label %87, label %95

87:                                               ; preds = %84
  %88 = trunc i64 %85 to i32
  %89 = getelementptr i32, ptr %36, i64 %85
  %90 = load i32, ptr %89, align 4
  %91 = getelementptr i32, ptr %23, i64 %85
  %92 = load i32, ptr %91, align 4
  %93 = call i32 (ptr, ...) @printf(ptr @str8, i32 %88, i32 %90, i32 %88, i32 %92)
  %94 = add i64 %85, 1
  br label %84

95:                                               ; preds = %84
  %96 = call i32 (ptr, ...) @printf(ptr @str9)
  br label %97

97:                                               ; preds = %95, %10
  ret i32 %11
}

define void @initPerWorker(i32 %0, i32 %1, ptr %2) {
  ret void
}

define void @initPerNode(i32 %0, i32 %1, ptr %2) {
  %4 = sext i32 %0 to i64
  %5 = icmp uge i64 %4, 1
  br i1 %5, label %6, label %7

6:                                                ; preds = %3
  ret void

7:                                                ; preds = %3
  %8 = call i32 @mainBody(i32 %1, ptr %2)
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
