; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str6 = internal constant [56 x i8] c"-----------------\0AMain function DONE\0A-----------------\0A\00"
@str5 = internal constant [24 x i8] c"A[%d] = %d, B[%d] = %d\0A\00"
@str4 = internal constant [15 x i8] c"Final arrays:\0A\00"
@str3 = internal constant [34 x i8] c"Task %d: Initializing A[%d] = %d\0A\00"
@str2 = internal constant [42 x i8] c"Initializing arrays A and B with size %d\0A\00"
@str1 = internal constant [51 x i8] c"-----------------\0AMain function\0A-----------------\0A\00"
@str0 = internal constant [13 x i8] c"Usage: %s N\0A\00"
@stderr = external global ptr

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsGetCurrentEpochGuid()

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
  %5 = getelementptr i64, ptr %1, i32 1
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
  %23 = alloca i64, i64 1, align 8
  br label %24

24:                                               ; preds = %27, %22
  %25 = phi i64 [ %35, %27 ], [ 0, %22 ]
  %26 = icmp slt i64 %25, %6
  br i1 %26, label %27, label %36

27:                                               ; preds = %24
  %28 = trunc i64 %25 to i32
  %29 = call i64 @artsGetCurrentEpochGuid()
  %30 = call i32 @artsGetCurrentNode()
  %31 = sext i32 %28 to i64
  store i64 %31, ptr %23, align 8
  %32 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %30, i32 1, ptr %23, i32 1, i64 %29)
  %33 = getelementptr i64, ptr %8, i64 %25
  %34 = load i64, ptr %33, align 8
  call void @artsSignalEdt(i64 %32, i32 0, i64 %34)
  %35 = add i64 %25, 1
  br label %24

36:                                               ; preds = %24
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %8 = load ptr, ptr %7, align 8
  %9 = call i32 @rand()
  %10 = srem i32 %9, 100
  store i32 %10, ptr %8, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @str3, i32 %6, i32 %6, i32 %10)
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
  br i1 %4, label %12, label %66

12:                                               ; preds = %10
  %13 = getelementptr ptr, ptr %1, i32 1
  %14 = load ptr, ptr %13, align 8
  %15 = call i32 @atoi(ptr %14)
  %16 = sext i32 %15 to i64
  %17 = alloca i32, i64 %16, align 4
  %18 = call i64 @time(ptr null)
  %19 = trunc i64 %18 to i32
  call void @srand(i32 %19)
  %20 = call i32 (ptr, ...) @printf(ptr @str1)
  %21 = call i32 (ptr, ...) @printf(ptr @str2, i32 %15)
  %22 = call i32 @artsGetCurrentNode()
  %23 = alloca i64, i64 %16, align 8
  %24 = alloca ptr, i64 %16, align 8
  br label %25

25:                                               ; preds = %28, %12
  %26 = phi i64 [ %33, %28 ], [ 0, %12 ]
  %27 = icmp slt i64 %26, %16
  br i1 %27, label %28, label %34

28:                                               ; preds = %25
  %29 = call i64 @artsReserveGuidRoute(i32 9, i32 %22)
  %30 = call ptr @artsDbCreateWithGuid(i64 %29, i64 4)
  %31 = getelementptr i64, ptr %23, i64 %26
  store i64 %29, ptr %31, align 8
  %32 = getelementptr ptr, ptr %24, i64 %26
  store ptr %30, ptr %32, align 8
  %33 = add i64 %26, 1
  br label %25

34:                                               ; preds = %25
  %35 = call i32 @artsGetCurrentNode()
  %36 = alloca i64, i64 0, align 8
  %37 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %35, i32 0, ptr %36, i32 1)
  %38 = call i64 @artsInitializeAndStartEpoch(i64 %37, i32 0)
  %39 = call i32 @artsGetCurrentNode()
  %40 = alloca i64, i64 2, align 8
  store i64 %16, ptr %40, align 8
  %41 = getelementptr i64, ptr %40, i32 1
  store i64 %16, ptr %41, align 8
  %42 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %39, i32 2, ptr %40, i32 %15, i64 %38)
  br label %43

43:                                               ; preds = %46, %34
  %44 = phi i64 [ %49, %46 ], [ 0, %34 ]
  %45 = icmp slt i64 %44, %16
  br i1 %45, label %46, label %50

46:                                               ; preds = %43
  %47 = getelementptr i64, ptr %23, i64 %44
  %48 = load i64, ptr %47, align 8
  call void @artsSignalEdt(i64 %42, i32 0, i64 %48)
  %49 = add i64 %44, 1
  br label %43

50:                                               ; preds = %43
  %51 = call i1 @artsWaitOnHandle(i64 %38)
  %52 = call i32 (ptr, ...) @printf(ptr @str4)
  br label %53

53:                                               ; preds = %56, %50
  %54 = phi i64 [ %63, %56 ], [ 0, %50 ]
  %55 = icmp slt i64 %54, %16
  br i1 %55, label %56, label %64

56:                                               ; preds = %53
  %57 = trunc i64 %54 to i32
  %58 = getelementptr i32, ptr %24, i64 %54
  %59 = load i32, ptr %58, align 4
  %60 = getelementptr i32, ptr %17, i64 %54
  %61 = load i32, ptr %60, align 4
  %62 = call i32 (ptr, ...) @printf(ptr @str5, i32 %57, i32 %59, i32 %57, i32 %61)
  %63 = add i64 %54, 1
  br label %53

64:                                               ; preds = %53
  %65 = call i32 (ptr, ...) @printf(ptr @str6)
  br label %66

66:                                               ; preds = %64, %10
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
