; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx15.0.0"

@str_5 = internal constant [34 x i8] c"DEBUG: artsMain function called!\0A\00"
@str4 = internal constant [37 x i8] c"EDT 3: The final number is %d - %d.\0A\00"
@str3 = internal constant [28 x i8] c"EDT 2: The number is %d/%d\0A\00"
@str2 = internal constant [28 x i8] c"EDT 1: The number is %d/%d\0A\00"
@str1 = internal constant [36 x i8] c"EDT 0: The initial number is %d/%d\0A\00"
@str0 = internal constant [23 x i8] c"Main EDT: Starting...\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32 signext, ptr nocapture nofree readonly)

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

declare void @artsShutdown()

define void @artsMain(i32 %0, ptr %1) {
  %3 = call i32 (ptr, ...) @printf(ptr @str_5)
  %4 = call i32 @mainBody()
  call void @artsShutdown()
  ret void
}

declare ptr @artsDbCreateWithGuid(i64, i64)

declare i64 @artsReserveGuidRoute(i32 signext, i32 zeroext)

declare void @artsDbIncrementLatch(i64)

declare void @artsDbAddDependence(i64, i64, i32 zeroext)

declare i64 @artsEdtCreateWithEpoch(ptr nocapture nofree readonly, i32 zeroext, i32 zeroext, ptr nocapture nofree readonly, i32 zeroext, i64)

declare i1 @artsWaitOnHandle(i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32 zeroext)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %6 = load ptr, ptr %5, align 8
  %7 = load i32, ptr %6, align 4
  %8 = add i32 %7, 1
  store i32 %8, ptr %6, align 4
  %9 = getelementptr { i64, i32, ptr }, ptr %3, i32 1
  %10 = getelementptr { i64, i32, ptr }, ptr %9, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = load i32, ptr %11, align 4
  %13 = add i32 %12, 1
  store i32 %13, ptr %11, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @str3, i32 %8, i32 %13)
  ret void
}

declare i32 @printf(ptr, ...)

define i32 @mainBody() {
  %1 = call ptr @malloc(i64 8)
  %2 = call i64 @artsReserveGuidRoute(i32 10, i32 0)
  %3 = call ptr @artsDbCreateWithGuid(i64 %2, i64 4)
  store i64 %2, ptr %1, align 8
  store i32 undef, ptr %3, align 4
  %4 = call i64 @time(ptr null)
  %5 = trunc i64 %4 to i32
  call void @srand(i32 %5)
  %6 = call i32 (ptr, ...) @printf(ptr @str0)
  %7 = call i32 @rand()
  %8 = srem i32 %7, 100
  %9 = add i32 %8, 1
  store i32 %9, ptr %3, align 4
  %10 = call i32 @rand()
  %11 = srem i32 %10, 10
  %12 = add i32 %11, 1
  %13 = call i32 (ptr, ...) @printf(ptr @str1, i32 %9, i32 %12)
  %14 = call i64 @artsInitializeAndStartEpoch(i64 0, i32 0)
  %15 = call ptr @malloc(i64 8)
  %16 = call i64 @artsReserveGuidRoute(i32 10, i32 0)
  %17 = call ptr @artsDbCreateWithGuid(i64 %16, i64 4)
  store i64 %16, ptr %15, align 8
  store i32 undef, ptr %17, align 4
  %18 = load i32, ptr %3, align 4
  %19 = call i32 (ptr, ...) @printf(ptr @str2, i32 %18, i32 %12)
  store i32 %12, ptr %17, align 4
  %20 = call ptr @malloc(i64 0)
  %21 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_1, i32 0, i32 0, ptr %20, i32 2, i64 %14)
  %22 = load i64, ptr %1, align 8
  call void @artsDbAddDependence(i64 %22, i64 %21, i32 0)
  %23 = load i64, ptr %15, align 8
  call void @artsDbAddDependence(i64 %23, i64 %21, i32 1)
  %24 = load i64, ptr %1, align 8
  call void @artsDbIncrementLatch(i64 %24)
  %25 = load i64, ptr %15, align 8
  call void @artsDbIncrementLatch(i64 %25)
  %26 = call i1 @artsWaitOnHandle(i64 %14)
  %27 = load i32, ptr %3, align 4
  %28 = add i32 %27, 1
  store i32 %28, ptr %3, align 4
  %29 = call i32 (ptr, ...) @printf(ptr @str4, i32 %28, i32 %12)
  ret i32 0
}

declare void @srand(i32)

declare i64 @time(ptr)

declare i32 @rand()

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
