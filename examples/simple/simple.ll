; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str3 = internal constant [37 x i8] c"EDT 3: The final number is %d - %d.\0A\00"
@str2 = internal constant [28 x i8] c"EDT 2: The number is %d/%d\0A\00"
@str1 = internal constant [28 x i8] c"EDT 1: The number is %d/%d\0A\00"
@str0 = internal constant [36 x i8] c"EDT 0: The initial number is %d/%d\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsGetCurrentEpochGuid()

declare i1 @artsWaitOnHandle(i64)

declare void @artsAddDependenceToPersistentEvent(i64, i64, i32, i64)

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32)

declare i64 @artsEdtCreate(ptr, i32, i32, ptr, i32)

declare i64 @artsPersistentEventCreate(i32, i32, i64)

declare ptr @artsDbCreateWithGuid(i64, i64)

declare i64 @artsReserveGuidRoute(i32, i32)

declare i32 @artsGetCurrentNode()

declare i32 @printf(ptr, ...)

declare void @srand(i32)

declare i64 @time(ptr)

declare i32 @rand()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr i64, ptr %1, i32 1
  %6 = load i64, ptr %5, align 8
  %7 = trunc i64 %6 to i32
  %8 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 0
  %9 = load i64, ptr %8, align 8
  %10 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = call i32 @artsGetCurrentNode()
  %13 = alloca i64, i64 1, align 8
  %14 = call i64 @artsPersistentEventCreate(i32 %12, i32 0, i64 0)
  store i64 %14, ptr %13, align 8
  %15 = call i32 @artsGetCurrentNode()
  %16 = alloca i64, i64 1, align 8
  %17 = call i64 @artsPersistentEventCreate(i32 %15, i32 0, i64 0)
  store i64 %17, ptr %16, align 8
  %18 = load i32, ptr %11, align 4
  %19 = call i32 (ptr, ...) @printf(ptr @str1, i32 %18, i32 %7)
  %20 = call i32 @artsGetCurrentNode()
  %21 = call i64 @artsReserveGuidRoute(i32 9, i32 %20)
  %22 = call ptr @artsDbCreateWithGuid(i64 %21, i64 4)
  %23 = call i64 @artsGetCurrentEpochGuid()
  %24 = call i32 @artsGetCurrentNode()
  %25 = alloca i64, i64 0, align 8
  %26 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %24, i32 0, ptr %25, i32 2, i64 %23)
  %27 = load i64, ptr %16, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %27, i64 %26, i32 0, i64 %9)
  %28 = load i64, ptr %13, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %28, i64 %26, i32 1, i64 %21)
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr i8, ptr %3, i32 24
  %8 = getelementptr { i64, i32, ptr }, ptr %7, i32 0, i32 2
  %9 = load ptr, ptr %8, align 8
  %10 = load i32, ptr %6, align 4
  %11 = add i32 %10, 1
  store i32 %11, ptr %6, align 4
  %12 = load i32, ptr %9, align 4
  %13 = add i32 %12, 1
  store i32 %13, ptr %9, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @str2, i32 %11, i32 %13)
  ret void
}

define i32 @mainBody() {
  %1 = call i64 @time(ptr null)
  %2 = trunc i64 %1 to i32
  call void @srand(i32 %2)
  %3 = call i32 @rand()
  %4 = srem i32 %3, 100
  %5 = add i32 %4, 1
  %6 = call i32 @rand()
  %7 = srem i32 %6, 10
  %8 = add i32 %7, 1
  %9 = call i32 (ptr, ...) @printf(ptr @str0, i32 %5, i32 %8)
  %10 = call i32 @artsGetCurrentNode()
  %11 = alloca i64, i64 1, align 8
  %12 = call i64 @artsPersistentEventCreate(i32 %10, i32 0, i64 0)
  store i64 %12, ptr %11, align 8
  %13 = call i32 @artsGetCurrentNode()
  %14 = call i64 @artsReserveGuidRoute(i32 9, i32 %13)
  %15 = call ptr @artsDbCreateWithGuid(i64 %14, i64 4)
  %16 = call i32 @artsGetCurrentNode()
  %17 = alloca i64, i64 0, align 8
  %18 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %16, i32 0, ptr %17, i32 1)
  %19 = call i64 @artsInitializeAndStartEpoch(i64 %18, i32 0)
  %20 = call i32 @artsGetCurrentNode()
  %21 = alloca i64, i64 2, align 8
  store i64 undef, ptr %21, align 8
  %22 = sext i32 %8 to i64
  %23 = getelementptr i64, ptr %21, i32 1
  store i64 %22, ptr %23, align 8
  %24 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %20, i32 2, ptr %21, i32 1, i64 %19)
  %25 = load i64, ptr %11, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %25, i64 %24, i32 0, i64 %14)
  %26 = call i1 @artsWaitOnHandle(i64 %19)
  %27 = load i32, ptr %15, align 4
  %28 = add i32 %27, 1
  store i32 %28, ptr %15, align 4
  %29 = call i32 (ptr, ...) @printf(ptr @str3, i32 %28, i32 %8)
  ret i32 0
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
  %8 = call i32 @mainBody()
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
