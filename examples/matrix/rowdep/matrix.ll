; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str2 = internal constant [18 x i8] c"B[%d][%d] = %f   \00"
@str1 = internal constant [2 x i8] c"\0A\00"
@str0 = internal constant [18 x i8] c"A[%d][%d] = %f   \00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

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

declare i32 @rand()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = alloca i64, i64 1, align 8
  %8 = getelementptr i64, ptr %7, i32 0
  store i64 0, ptr %8, align 8
  br label %9

9:                                                ; preds = %12, %4
  %10 = phi i64 [ %15, %12 ], [ 0, %4 ]
  %11 = icmp slt i64 %10, 100
  br i1 %11, label %12, label %16

12:                                               ; preds = %9
  %13 = load i64, ptr %8, align 8
  %14 = add i64 %13, 1
  store i64 %14, ptr %8, align 8
  %15 = add i64 %10, 1
  br label %9

16:                                               ; preds = %9
  br label %17

17:                                               ; preds = %20, %16
  %18 = phi i64 [ %23, %20 ], [ 0, %16 ]
  %19 = icmp slt i64 %18, 100
  br i1 %19, label %20, label %24

20:                                               ; preds = %17
  %21 = load i64, ptr %8, align 8
  %22 = add i64 %21, 1
  store i64 %22, ptr %8, align 8
  %23 = add i64 %18, 1
  br label %17

24:                                               ; preds = %17
  %25 = call i32 @artsGetCurrentNode()
  %26 = alloca [10 x i64], i64 10, align 8
  br label %27

27:                                               ; preds = %30, %24
  %28 = phi i64 [ %43, %30 ], [ 0, %24 ]
  %29 = icmp slt i64 %28, 100
  br i1 %29, label %30, label %44

30:                                               ; preds = %27
  %31 = srem i64 %28, 10
  %32 = icmp slt i64 %31, 0
  %33 = add i64 %31, 10
  %34 = select i1 %32, i64 %33, i64 %31
  %35 = icmp slt i64 %28, 0
  %36 = sub i64 -1, %28
  %37 = select i1 %35, i64 %36, i64 %28
  %38 = sdiv i64 %37, 10
  %39 = sub i64 -1, %38
  %40 = select i1 %35, i64 %39, i64 %38
  %41 = call i64 @artsEventCreate(i32 %25, i32 1)
  %42 = getelementptr [10 x i64], ptr %26, i64 %40, i64 %34
  store i64 %41, ptr %42, align 8
  %43 = add i64 %28, 1
  br label %27

44:                                               ; preds = %27
  %45 = sitofp i32 %6 to double
  %46 = alloca i32, i64 1, align 4
  %47 = alloca i64, i64 12, align 8
  br label %48

48:                                               ; preds = %70, %44
  %49 = phi i64 [ %72, %70 ], [ 0, %44 ]
  %50 = icmp slt i64 %49, 10
  br i1 %50, label %51, label %73

51:                                               ; preds = %48
  %52 = trunc i64 %49 to i32
  %53 = call i64 @artsGetCurrentEpochGuid()
  %54 = call i32 @artsGetCurrentNode()
  %55 = sext i32 %52 to i64
  store i64 %55, ptr %47, align 8
  %56 = fptosi double %45 to i64
  %57 = getelementptr i64, ptr %47, i32 1
  store i64 %56, ptr %57, align 8
  %58 = getelementptr i32, ptr %46, i32 0
  store i32 2, ptr %58, align 4
  br label %59

59:                                               ; preds = %62, %51
  %60 = phi i64 [ %69, %62 ], [ 0, %51 ]
  %61 = icmp slt i64 %60, 10
  br i1 %61, label %62, label %70

62:                                               ; preds = %59
  %63 = getelementptr [10 x i64], ptr %26, i64 %49, i64 %60
  %64 = load i64, ptr %63, align 8
  %65 = load i32, ptr %58, align 4
  %66 = sext i32 %65 to i64
  %67 = getelementptr i64, ptr %47, i64 %66
  store i64 %64, ptr %67, align 8
  %68 = add i32 %65, 1
  store i32 %68, ptr %58, align 4
  %69 = add i64 %60, 1
  br label %59

70:                                               ; preds = %59
  %71 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %54, i32 12, ptr %47, i32 10, i64 %53)
  %72 = add i64 %49, 1
  br label %48

73:                                               ; preds = %48
  %74 = call i64 @artsGetCurrentEpochGuid()
  %75 = call i32 @artsGetCurrentNode()
  %76 = alloca i64, i64 0, align 8
  %77 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_4, i32 %75, i32 0, ptr %76, i32 20, i64 %74)
  %78 = call ptr @malloc(i64 4)
  %79 = getelementptr i32, ptr %78, i32 0
  store i32 10, ptr %79, align 4
  br label %80

80:                                               ; preds = %83, %73
  %81 = phi i64 [ %88, %83 ], [ 0, %73 ]
  %82 = icmp slt i64 %81, 10
  br i1 %82, label %83, label %89

83:                                               ; preds = %80
  %84 = getelementptr [10 x i64], ptr %26, i32 0, i64 %81
  %85 = load i64, ptr %84, align 8
  %86 = load i32, ptr %79, align 4
  call void @artsAddDependence(i64 %85, i64 %77, i32 %86)
  %87 = add i32 %86, 1
  store i32 %87, ptr %79, align 4
  %88 = add i64 %81, 1
  br label %80

89:                                               ; preds = %80
  %90 = alloca i64, i64 0, align 8
  br label %91

91:                                               ; preds = %122, %89
  %92 = phi i64 [ %123, %122 ], [ 1, %89 ]
  %93 = icmp slt i64 %92, 10
  br i1 %93, label %94, label %124

94:                                               ; preds = %91
  %95 = call i64 @artsGetCurrentEpochGuid()
  %96 = call i32 @artsGetCurrentNode()
  %97 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_5, i32 %96, i32 0, ptr %90, i32 30, i64 %95)
  %98 = call ptr @malloc(i64 4)
  %99 = getelementptr i32, ptr %98, i32 0
  store i32 0, ptr %99, align 4
  br label %100

100:                                              ; preds = %103, %94
  %101 = phi i64 [ %108, %103 ], [ 0, %94 ]
  %102 = icmp slt i64 %101, 10
  br i1 %102, label %103, label %109

103:                                              ; preds = %100
  %104 = getelementptr [10 x i64], ptr %26, i64 %92, i64 %101
  %105 = load i64, ptr %104, align 8
  %106 = load i32, ptr %99, align 4
  call void @artsAddDependence(i64 %105, i64 %97, i32 %106)
  %107 = add i32 %106, 1
  store i32 %107, ptr %99, align 4
  %108 = add i64 %101, 1
  br label %100

109:                                              ; preds = %100
  %110 = call ptr @malloc(i64 4)
  %111 = getelementptr i32, ptr %110, i32 0
  store i32 20, ptr %111, align 4
  br label %112

112:                                              ; preds = %115, %109
  %113 = phi i64 [ %121, %115 ], [ 0, %109 ]
  %114 = icmp slt i64 %113, 10
  br i1 %114, label %115, label %122

115:                                              ; preds = %112
  %116 = add i64 %92, -1
  %117 = getelementptr [10 x i64], ptr %26, i64 %116, i64 %113
  %118 = load i64, ptr %117, align 8
  %119 = load i32, ptr %111, align 4
  call void @artsAddDependence(i64 %118, i64 %97, i32 %119)
  %120 = add i32 %119, 1
  store i32 %120, ptr %111, align 4
  %121 = add i64 %113, 1
  br label %112

122:                                              ; preds = %112
  %123 = add i64 %92, 1
  br label %91

124:                                              ; preds = %91
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = sitofp i64 %8 to double
  %10 = alloca i64, i64 1, align 8
  %11 = getelementptr i64, ptr %10, i32 0
  store i64 0, ptr %11, align 8
  %12 = alloca i64, i64 10, align 8
  %13 = alloca ptr, i64 10, align 8
  br label %14

14:                                               ; preds = %17, %4
  %15 = phi i64 [ %28, %17 ], [ 0, %4 ]
  %16 = icmp slt i64 %15, 10
  br i1 %16, label %17, label %29

17:                                               ; preds = %14
  %18 = load i64, ptr %11, align 8
  %19 = mul i64 %18, 24
  %20 = trunc i64 %19 to i32
  %21 = getelementptr { i64, i32, ptr }, ptr %3, i32 %20
  %22 = getelementptr { i64, i32, ptr }, ptr %21, i32 0, i32 0
  %23 = load i64, ptr %22, align 8
  %24 = getelementptr { i64, i32, ptr }, ptr %21, i32 0, i32 2
  %25 = getelementptr i64, ptr %12, i64 %15
  store i64 %23, ptr %25, align 8
  %26 = getelementptr ptr, ptr %13, i64 %15
  store ptr %24, ptr %26, align 8
  %27 = add i64 %18, 1
  store i64 %27, ptr %11, align 8
  %28 = add i64 %15, 1
  br label %14

29:                                               ; preds = %14
  %30 = sitofp i32 %6 to double
  %31 = fadd double %30, %9
  br label %32

32:                                               ; preds = %35, %29
  %33 = phi i64 [ %37, %35 ], [ 0, %29 ]
  %34 = icmp slt i64 %33, 10
  br i1 %34, label %35, label %38

35:                                               ; preds = %32
  %36 = getelementptr double, ptr %13, i64 %33
  store double %31, ptr %36, align 8
  %37 = add i64 %33, 1
  br label %32

38:                                               ; preds = %32
  %39 = alloca i32, i64 1, align 4
  %40 = getelementptr i32, ptr %39, i32 0
  store i32 0, ptr %40, align 4
  br label %41

41:                                               ; preds = %44, %38
  %42 = phi i64 [ %52, %44 ], [ 0, %38 ]
  %43 = icmp slt i64 %42, 10
  br i1 %43, label %44, label %53

44:                                               ; preds = %41
  %45 = load i32, ptr %40, align 4
  %46 = getelementptr i64, ptr %1, i32 %45
  %47 = load i64, ptr %46, align 8
  %48 = trunc i64 %42 to i32
  %49 = getelementptr i64, ptr %12, i32 %48
  %50 = load i64, ptr %49, align 8
  call void @artsEventSatisfySlot(i64 %47, i64 %50, i32 0)
  %51 = add i32 %45, 1
  store i32 %51, ptr %40, align 4
  %52 = add i64 %42, 1
  br label %41

53:                                               ; preds = %41
  ret void
}

define void @__arts_edt_4(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = alloca i64, i64 1, align 8
  %6 = getelementptr i64, ptr %5, i32 0
  store i64 0, ptr %6, align 8
  %7 = alloca ptr, i64 10, align 8
  br label %8

8:                                                ; preds = %11, %4
  %9 = phi i64 [ %19, %11 ], [ 0, %4 ]
  %10 = icmp slt i64 %9, 10
  br i1 %10, label %11, label %20

11:                                               ; preds = %8
  %12 = load i64, ptr %6, align 8
  %13 = mul i64 %12, 24
  %14 = trunc i64 %13 to i32
  %15 = getelementptr { i64, i32, ptr }, ptr %3, i32 %14
  %16 = getelementptr { i64, i32, ptr }, ptr %15, i32 0, i32 2
  %17 = getelementptr ptr, ptr %7, i64 %9
  store ptr %16, ptr %17, align 8
  %18 = add i64 %12, 1
  store i64 %18, ptr %6, align 8
  %19 = add i64 %9, 1
  br label %8

20:                                               ; preds = %8
  %21 = alloca ptr, i64 10, align 8
  br label %22

22:                                               ; preds = %25, %20
  %23 = phi i64 [ %33, %25 ], [ 0, %20 ]
  %24 = icmp slt i64 %23, 10
  br i1 %24, label %25, label %34

25:                                               ; preds = %22
  %26 = load i64, ptr %6, align 8
  %27 = mul i64 %26, 24
  %28 = trunc i64 %27 to i32
  %29 = getelementptr { i64, i32, ptr }, ptr %3, i32 %28
  %30 = getelementptr { i64, i32, ptr }, ptr %29, i32 0, i32 2
  %31 = getelementptr ptr, ptr %21, i64 %23
  store ptr %30, ptr %31, align 8
  %32 = add i64 %26, 1
  store i64 %32, ptr %6, align 8
  %33 = add i64 %23, 1
  br label %22

34:                                               ; preds = %22
  br label %35

35:                                               ; preds = %38, %34
  %36 = phi i64 [ %42, %38 ], [ 0, %34 ]
  %37 = icmp slt i64 %36, 10
  br i1 %37, label %38, label %43

38:                                               ; preds = %35
  %39 = getelementptr double, ptr %21, i64 %36
  %40 = load double, ptr %39, align 8
  %41 = getelementptr double, ptr %7, i64 %36
  store double %40, ptr %41, align 8
  %42 = add i64 %36, 1
  br label %35

43:                                               ; preds = %35
  ret void
}

define void @__arts_edt_5(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = alloca i64, i64 1, align 8
  %6 = getelementptr i64, ptr %5, i32 0
  store i64 0, ptr %6, align 8
  %7 = alloca ptr, i64 10, align 8
  br label %8

8:                                                ; preds = %11, %4
  %9 = phi i64 [ %19, %11 ], [ 0, %4 ]
  %10 = icmp slt i64 %9, 10
  br i1 %10, label %11, label %20

11:                                               ; preds = %8
  %12 = load i64, ptr %6, align 8
  %13 = mul i64 %12, 24
  %14 = trunc i64 %13 to i32
  %15 = getelementptr { i64, i32, ptr }, ptr %3, i32 %14
  %16 = getelementptr { i64, i32, ptr }, ptr %15, i32 0, i32 2
  %17 = getelementptr ptr, ptr %7, i64 %9
  store ptr %16, ptr %17, align 8
  %18 = add i64 %12, 1
  store i64 %18, ptr %6, align 8
  %19 = add i64 %9, 1
  br label %8

20:                                               ; preds = %8
  %21 = alloca ptr, i64 10, align 8
  br label %22

22:                                               ; preds = %25, %20
  %23 = phi i64 [ %33, %25 ], [ 0, %20 ]
  %24 = icmp slt i64 %23, 10
  br i1 %24, label %25, label %34

25:                                               ; preds = %22
  %26 = load i64, ptr %6, align 8
  %27 = mul i64 %26, 24
  %28 = trunc i64 %27 to i32
  %29 = getelementptr { i64, i32, ptr }, ptr %3, i32 %28
  %30 = getelementptr { i64, i32, ptr }, ptr %29, i32 0, i32 2
  %31 = getelementptr ptr, ptr %21, i64 %23
  store ptr %30, ptr %31, align 8
  %32 = add i64 %26, 1
  store i64 %32, ptr %6, align 8
  %33 = add i64 %23, 1
  br label %22

34:                                               ; preds = %22
  %35 = alloca ptr, i64 10, align 8
  br label %36

36:                                               ; preds = %39, %34
  %37 = phi i64 [ %47, %39 ], [ 0, %34 ]
  %38 = icmp slt i64 %37, 10
  br i1 %38, label %39, label %48

39:                                               ; preds = %36
  %40 = load i64, ptr %6, align 8
  %41 = mul i64 %40, 24
  %42 = trunc i64 %41 to i32
  %43 = getelementptr { i64, i32, ptr }, ptr %3, i32 %42
  %44 = getelementptr { i64, i32, ptr }, ptr %43, i32 0, i32 2
  %45 = getelementptr ptr, ptr %35, i64 %37
  store ptr %44, ptr %45, align 8
  %46 = add i64 %40, 1
  store i64 %46, ptr %6, align 8
  %47 = add i64 %37, 1
  br label %36

48:                                               ; preds = %36
  br label %49

49:                                               ; preds = %52, %48
  %50 = phi i64 [ %59, %52 ], [ 0, %48 ]
  %51 = icmp slt i64 %50, 10
  br i1 %51, label %52, label %60

52:                                               ; preds = %49
  %53 = getelementptr double, ptr %7, i64 %50
  %54 = load double, ptr %53, align 8
  %55 = getelementptr double, ptr %35, i64 %50
  %56 = load double, ptr %55, align 8
  %57 = fadd double %54, %56
  %58 = getelementptr double, ptr %21, i64 %50
  store double %57, ptr %58, align 8
  %59 = add i64 %50, 1
  br label %49

60:                                               ; preds = %49
  ret void
}

define i32 @mainBody() {
  %1 = call i32 @rand()
  %2 = srem i32 %1, 100
  %3 = call i32 @artsGetCurrentNode()
  %4 = alloca [10 x i64], i64 10, align 8
  br label %5

5:                                                ; preds = %8, %0
  %6 = phi i64 [ %22, %8 ], [ 0, %0 ]
  %7 = icmp slt i64 %6, 100
  br i1 %7, label %8, label %23

8:                                                ; preds = %5
  %9 = srem i64 %6, 10
  %10 = icmp slt i64 %9, 0
  %11 = add i64 %9, 10
  %12 = select i1 %10, i64 %11, i64 %9
  %13 = icmp slt i64 %6, 0
  %14 = sub i64 -1, %6
  %15 = select i1 %13, i64 %14, i64 %6
  %16 = sdiv i64 %15, 10
  %17 = sub i64 -1, %16
  %18 = select i1 %13, i64 %17, i64 %16
  %19 = call i64 @artsReserveGuidRoute(i32 9, i32 %3)
  %20 = call ptr @artsDbCreateWithGuid(i64 %19, i64 8)
  %21 = getelementptr [10 x i64], ptr %4, i64 %18, i64 %12
  store i64 %19, ptr %21, align 8
  %22 = add i64 %6, 1
  br label %5

23:                                               ; preds = %5
  %24 = call i32 @artsGetCurrentNode()
  %25 = alloca [10 x i64], i64 10, align 8
  br label %26

26:                                               ; preds = %29, %23
  %27 = phi i64 [ %43, %29 ], [ 0, %23 ]
  %28 = icmp slt i64 %27, 100
  br i1 %28, label %29, label %44

29:                                               ; preds = %26
  %30 = srem i64 %27, 10
  %31 = icmp slt i64 %30, 0
  %32 = add i64 %30, 10
  %33 = select i1 %31, i64 %32, i64 %30
  %34 = icmp slt i64 %27, 0
  %35 = sub i64 -1, %27
  %36 = select i1 %34, i64 %35, i64 %27
  %37 = sdiv i64 %36, 10
  %38 = sub i64 -1, %37
  %39 = select i1 %34, i64 %38, i64 %37
  %40 = call i64 @artsReserveGuidRoute(i32 9, i32 %24)
  %41 = call ptr @artsDbCreateWithGuid(i64 %40, i64 8)
  %42 = getelementptr [10 x i64], ptr %25, i64 %39, i64 %33
  store i64 %40, ptr %42, align 8
  %43 = add i64 %27, 1
  br label %26

44:                                               ; preds = %26
  %45 = call i32 @artsGetCurrentNode()
  %46 = alloca i64, i64 0, align 8
  %47 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %45, i32 0, ptr %46, i32 1)
  %48 = call i64 @artsInitializeAndStartEpoch(i64 %47, i32 0)
  %49 = call i32 @artsGetCurrentNode()
  %50 = alloca i64, i64 1, align 8
  %51 = sext i32 %2 to i64
  store i64 %51, ptr %50, align 8
  %52 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %49, i32 1, ptr %50, i32 200, i64 %48)
  br label %53

53:                                               ; preds = %56, %44
  %54 = phi i64 [ %69, %56 ], [ 0, %44 ]
  %55 = icmp slt i64 %54, 100
  br i1 %55, label %56, label %70

56:                                               ; preds = %53
  %57 = srem i64 %54, 10
  %58 = icmp slt i64 %57, 0
  %59 = add i64 %57, 10
  %60 = select i1 %58, i64 %59, i64 %57
  %61 = icmp slt i64 %54, 0
  %62 = sub i64 -1, %54
  %63 = select i1 %61, i64 %62, i64 %54
  %64 = sdiv i64 %63, 10
  %65 = sub i64 -1, %64
  %66 = select i1 %61, i64 %65, i64 %64
  %67 = getelementptr [10 x i64], ptr %4, i64 %66, i64 %60
  %68 = load i64, ptr %67, align 8
  call void @artsSignalEdt(i64 %52, i32 0, i64 %68)
  %69 = add i64 %54, 1
  br label %53

70:                                               ; preds = %53
  br label %71

71:                                               ; preds = %74, %70
  %72 = phi i64 [ %87, %74 ], [ 0, %70 ]
  %73 = icmp slt i64 %72, 100
  br i1 %73, label %74, label %88

74:                                               ; preds = %71
  %75 = srem i64 %72, 10
  %76 = icmp slt i64 %75, 0
  %77 = add i64 %75, 10
  %78 = select i1 %76, i64 %77, i64 %75
  %79 = icmp slt i64 %72, 0
  %80 = sub i64 -1, %72
  %81 = select i1 %79, i64 %80, i64 %72
  %82 = sdiv i64 %81, 10
  %83 = sub i64 -1, %82
  %84 = select i1 %79, i64 %83, i64 %82
  %85 = getelementptr [10 x i64], ptr %25, i64 %84, i64 %78
  %86 = load i64, ptr %85, align 8
  call void @artsSignalEdt(i64 %52, i32 100, i64 %86)
  %87 = add i64 %72, 1
  br label %71

88:                                               ; preds = %71
  %89 = call i1 @artsWaitOnHandle(i64 %48)
  br label %90

90:                                               ; preds = %93, %88
  %91 = phi i64 [ %95, %93 ], [ 0, %88 ]
  %92 = icmp slt i64 %91, 10
  br i1 %92, label %93, label %96

93:                                               ; preds = %90
  %94 = call i32 (ptr, ...) @printf(ptr @str1)
  %95 = add i64 %91, 1
  br label %90

96:                                               ; preds = %90
  br label %97

97:                                               ; preds = %100, %96
  %98 = phi i64 [ %102, %100 ], [ 0, %96 ]
  %99 = icmp slt i64 %98, 10
  br i1 %99, label %100, label %103

100:                                              ; preds = %97
  %101 = call i32 (ptr, ...) @printf(ptr @str1)
  %102 = add i64 %98, 1
  br label %97

103:                                              ; preds = %97
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
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
