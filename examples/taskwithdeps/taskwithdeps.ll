; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str5 = internal constant [56 x i8] c"-----------------\0AMain function DONE\0A-----------------\0A\00"
@str4 = internal constant [14 x i8] c"A: %d, B: %d\0A\00"
@str3 = internal constant [37 x i8] c"Task 2: Reading a=%d and updating b\0A\00"
@str2 = internal constant [38 x i8] c"Task 1: Initializing a with value %d\0A\00"
@str1 = internal constant [29 x i8] c"Main thread: Creating tasks\0A\00"
@str0 = internal constant [51 x i8] c"-----------------\0AMain function\0A-----------------\0A\00"

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

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 24
  %8 = getelementptr { i64, i32, ptr }, ptr %7, i32 0, i32 0
  %9 = load i64, ptr %8, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @str1)
  %11 = call i32 @artsGetCurrentNode()
  %12 = call i64 @artsEventCreate(i32 %11, i32 1)
  %13 = call i64 @artsGetCurrentEpochGuid()
  %14 = call i32 @artsGetCurrentNode()
  %15 = alloca i64, i64 1, align 8
  store i64 %12, ptr %15, align 8
  %16 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %14, i32 1, ptr %15, i32 1, i64 %13)
  call void @artsSignalEdt(i64 %16, i32 0, i64 %9)
  %17 = call i64 @artsGetCurrentEpochGuid()
  %18 = call i32 @artsGetCurrentNode()
  %19 = alloca i64, i64 0, align 8
  %20 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_4, i32 %18, i32 0, ptr %19, i32 2, i64 %17)
  call void @artsAddDependence(i64 %12, i64 %20, i32 1)
  call void @artsSignalEdt(i64 %20, i32 0, i64 %6)
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %7 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  store i32 10, ptr %7, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @str2, i32 10)
  %9 = load i64, ptr %1, align 8
  call void @artsEventSatisfySlot(i64 %9, i64 %6, i32 0)
  ret void
}

define void @__arts_edt_4(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %6 = getelementptr { i64, i32, ptr }, ptr %3, i32 24
  %7 = getelementptr { i64, i32, ptr }, ptr %6, i32 0, i32 2
  %8 = load i32, ptr %7, align 4
  %9 = call i32 (ptr, ...) @printf(ptr @str3, i32 %8)
  %10 = add i32 %8, 5
  store i32 %10, ptr %5, align 4
  ret void
}

define i32 @mainBody() {
  %1 = call i32 (ptr, ...) @printf(ptr @str0)
  %2 = call i32 @artsGetCurrentNode()
  %3 = call i64 @artsReserveGuidRoute(i32 9, i32 %2)
  %4 = call ptr @artsDbCreateWithGuid(i64 %3, i64 4)
  %5 = call i32 @artsGetCurrentNode()
  %6 = call i64 @artsReserveGuidRoute(i32 9, i32 %5)
  %7 = call ptr @artsDbCreateWithGuid(i64 %6, i64 4)
  %8 = call i32 @artsGetCurrentNode()
  %9 = alloca i64, i64 0, align 8
  %10 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %8, i32 0, ptr %9, i32 1)
  %11 = call i64 @artsInitializeAndStartEpoch(i64 %10, i32 0)
  %12 = call i32 @artsGetCurrentNode()
  %13 = alloca i64, i64 0, align 8
  %14 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %12, i32 0, ptr %13, i32 2, i64 %11)
  call void @artsSignalEdt(i64 %14, i32 0, i64 %3)
  call void @artsSignalEdt(i64 %14, i32 1, i64 %6)
  %15 = call i1 @artsWaitOnHandle(i64 %11)
  %16 = load i32, ptr %7, align 4
  %17 = load i32, ptr %4, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @str4, i32 %16, i32 %17)
  %19 = call i32 (ptr, ...) @printf(ptr @str5)
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
