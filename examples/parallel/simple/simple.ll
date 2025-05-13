; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str3 = internal constant [27 x i8] c"Parallel region finished.\0A\00"
@str2 = internal constant [38 x i8] c"Thread %d is working on iteration %d\0A\00"
@str1 = internal constant [28 x i8] c"Hello from thread %d of %d\0A\00"
@str0 = internal constant [48 x i8] c"Demonstrating basic parallel region execution.\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i1 @artsWaitOnHandle(i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32)

declare i64 @artsEdtCreate(ptr, i32, i32, ptr, i32)

declare i32 @artsGetCurrentNode()

declare i32 @artsGetCurrentWorker()

declare i32 @artsGetTotalWorkers()

declare i32 @printf(ptr, ...)

define i32 @mainBody() {
  %1 = call i32 (ptr, ...) @printf(ptr @str0)
  %2 = call i32 @artsGetCurrentNode()
  %3 = alloca i64, i64 0, align 8
  %4 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %2, i32 0, ptr %3, i32 1)
  %5 = call i64 @artsInitializeAndStartEpoch(i64 %4, i32 0)
  %6 = call i32 @artsGetTotalWorkers()
  %7 = sext i32 %6 to i64
  %8 = alloca i64, i64 1, align 8
  %9 = alloca i64, i64 1, align 8
  br label %10

10:                                               ; preds = %13, %0
  %11 = phi i64 [ 0, %0 ], [ %21, %13 ]
  %12 = icmp slt i64 %11, %7
  br i1 %12, label %13, label %22

13:                                               ; preds = %10
  %14 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %8, align 8
  %15 = load i64, ptr %8, align 8
  %16 = trunc i64 %15 to i32
  store i64 0, ptr %9, align 8
  %17 = load i64, ptr %9, align 8
  %18 = trunc i64 %17 to i32
  %19 = alloca i64, i64 %17, align 8
  %20 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %14, i32 %18, ptr %19, i32 %16, i64 %5)
  %21 = add i64 %11, 1
  br label %10

22:                                               ; preds = %10
  %23 = call i1 @artsWaitOnHandle(i64 %5)
  %24 = call i32 (ptr, ...) @printf(ptr @str3)
  ret i32 0
}

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i32 @artsGetCurrentWorker()
  %6 = call i32 @artsGetTotalWorkers()
  %7 = call i32 (ptr, ...) @printf(ptr @str1, i32 %5, i32 %6)
  br label %8

8:                                                ; preds = %17, %4
  %9 = phi i64 [ 0, %4 ], [ %18, %17 ]
  %10 = icmp slt i64 %9, 100000000
  br i1 %10, label %11, label %19

11:                                               ; preds = %8
  %12 = trunc i64 %9 to i32
  %13 = srem i32 %12, 100000000
  %14 = icmp eq i32 %13, 0
  br i1 %14, label %15, label %17

15:                                               ; preds = %11
  %16 = call i32 (ptr, ...) @printf(ptr @str2, i32 %5, i32 %12)
  br label %17

17:                                               ; preds = %15, %11
  %18 = add i64 %9, 1
  br label %8

19:                                               ; preds = %8
  ret void
}

define void @artsMain(i32 %0, ptr %1) {
  %3 = call i32 @mainBody()
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
