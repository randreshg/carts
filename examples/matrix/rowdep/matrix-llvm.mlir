module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.func @malloc(i64) -> !llvm.ptr
  llvm.func @artsRT(i32, !llvm.ptr) -> i32 attributes {sym_visibility = "private"}
  llvm.func @artsAddDependence(i64, i64, i32) attributes {sym_visibility = "private"}
  llvm.func @artsEventSatisfySlot(i64, i64, i32) attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentEpochGuid() -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsEventCreate(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsWaitOnHandle(i64) -> i1 attributes {sym_visibility = "private"}
  llvm.func @artsSignalEdt(i64, i32, i64) attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreateWithEpoch(!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreate(!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsDbCreateWithGuid(i64, i64) -> !llvm.ptr attributes {sym_visibility = "private"}
  llvm.func @artsReserveGuidRoute(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentNode() -> i32 attributes {llvm.nounwind, llvm.readnone, sym_visibility = "private"}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.func @rand() -> i32 attributes {sym_visibility = "private"}
  llvm.func @__arts_edt_1(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    llvm.return
  }
  llvm.func @__arts_edt_2(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(4 : i64) : i64
    %1 = llvm.mlir.constant(12 : index) : i64
    %2 = llvm.mlir.constant(30 : i32) : i32
    %3 = llvm.mlir.constant(0 : index) : i64
    %4 = llvm.mlir.constant(-1 : index) : i64
    %5 = llvm.mlir.constant(100 : index) : i64
    %6 = llvm.mlir.constant(10 : index) : i64
    %7 = llvm.mlir.constant(20 : i32) : i32
    %8 = llvm.mlir.constant(10 : i32) : i32
    %9 = llvm.mlir.constant(12 : i32) : i32
    %10 = llvm.mlir.constant(2 : i32) : i32
    %11 = llvm.mlir.constant(1 : i32) : i32
    %12 = llvm.mlir.constant(0 : i32) : i32
    %13 = llvm.mlir.constant(1 : index) : i64
    %14 = llvm.load %arg1 : !llvm.ptr -> i64
    %15 = llvm.trunc %14 : i64 to i32
    %16 = llvm.alloca %13 x i64 : (i64) -> !llvm.ptr
    %17 = llvm.getelementptr %16[0] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %3, %17 : i64, !llvm.ptr
    llvm.br ^bb1(%3 : i64)
  ^bb1(%18: i64):  // 2 preds: ^bb0, ^bb2
    %19 = llvm.icmp "slt" %18, %5 : i64
    llvm.cond_br %19, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %20 = llvm.load %17 : !llvm.ptr -> i64
    %21 = llvm.add %20, %13  : i64
    llvm.store %21, %17 : i64, !llvm.ptr
    %22 = llvm.add %18, %13  : i64
    llvm.br ^bb1(%22 : i64)
  ^bb3:  // pred: ^bb1
    llvm.br ^bb4(%3 : i64)
  ^bb4(%23: i64):  // 2 preds: ^bb3, ^bb5
    %24 = llvm.icmp "slt" %23, %5 : i64
    llvm.cond_br %24, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %25 = llvm.load %17 : !llvm.ptr -> i64
    %26 = llvm.add %25, %13  : i64
    llvm.store %26, %17 : i64, !llvm.ptr
    %27 = llvm.add %23, %13  : i64
    llvm.br ^bb4(%27 : i64)
  ^bb6:  // pred: ^bb4
    %28 = llvm.call @artsGetCurrentNode() : () -> i32
    %29 = llvm.alloca %6 x !llvm.array<10 x i64> : (i64) -> !llvm.ptr
    llvm.br ^bb7(%3 : i64)
  ^bb7(%30: i64):  // 2 preds: ^bb6, ^bb8
    %31 = llvm.icmp "slt" %30, %5 : i64
    llvm.cond_br %31, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %32 = llvm.srem %30, %6  : i64
    %33 = llvm.icmp "slt" %32, %3 : i64
    %34 = llvm.add %32, %6  : i64
    %35 = llvm.select %33, %34, %32 : i1, i64
    %36 = llvm.icmp "slt" %30, %3 : i64
    %37 = llvm.sub %4, %30  : i64
    %38 = llvm.select %36, %37, %30 : i1, i64
    %39 = llvm.sdiv %38, %6  : i64
    %40 = llvm.sub %4, %39  : i64
    %41 = llvm.select %36, %40, %39 : i1, i64
    %42 = llvm.call @artsEventCreate(%28, %11) : (i32, i32) -> i64
    %43 = llvm.getelementptr %29[%41, %35] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    llvm.store %42, %43 : i64, !llvm.ptr
    %44 = llvm.add %30, %13  : i64
    llvm.br ^bb7(%44 : i64)
  ^bb9:  // pred: ^bb7
    %45 = llvm.sitofp %15 : i32 to f64
    %46 = llvm.alloca %13 x i32 : (i64) -> !llvm.ptr
    %47 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb10(%3 : i64)
  ^bb10(%48: i64):  // 2 preds: ^bb9, ^bb14
    %49 = llvm.icmp "slt" %48, %6 : i64
    llvm.cond_br %49, ^bb11, ^bb15
  ^bb11:  // pred: ^bb10
    %50 = llvm.trunc %48 : i64 to i32
    %51 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %52 = llvm.call @artsGetCurrentNode() : () -> i32
    %53 = llvm.sext %50 : i32 to i64
    llvm.store %53, %47 : i64, !llvm.ptr
    %54 = llvm.fptosi %45 : f64 to i64
    %55 = llvm.getelementptr %47[1] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %54, %55 : i64, !llvm.ptr
    %56 = llvm.getelementptr %46[0] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %10, %56 : i32, !llvm.ptr
    llvm.br ^bb12(%3 : i64)
  ^bb12(%57: i64):  // 2 preds: ^bb11, ^bb13
    %58 = llvm.icmp "slt" %57, %6 : i64
    llvm.cond_br %58, ^bb13, ^bb14
  ^bb13:  // pred: ^bb12
    %59 = llvm.getelementptr %29[%48, %57] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %60 = llvm.load %59 : !llvm.ptr -> i64
    %61 = llvm.load %56 : !llvm.ptr -> i32
    %62 = llvm.sext %61 : i32 to i64
    %63 = llvm.getelementptr %47[%62] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %60, %63 : i64, !llvm.ptr
    %64 = llvm.add %61, %11  : i32
    llvm.store %64, %56 : i32, !llvm.ptr
    %65 = llvm.add %57, %13  : i64
    llvm.br ^bb12(%65 : i64)
  ^bb14:  // pred: ^bb12
    %66 = llvm.mlir.addressof @__arts_edt_3 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %67 = llvm.bitcast %66 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %68 = llvm.call @artsEdtCreateWithEpoch(%67, %52, %9, %47, %8, %51) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %69 = llvm.add %48, %13  : i64
    llvm.br ^bb10(%69 : i64)
  ^bb15:  // pred: ^bb10
    %70 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %71 = llvm.call @artsGetCurrentNode() : () -> i32
    %72 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %73 = llvm.mlir.addressof @__arts_edt_4 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %74 = llvm.bitcast %73 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %75 = llvm.call @artsEdtCreateWithEpoch(%74, %71, %12, %72, %7, %70) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %76 = llvm.mul %0, %13  : i64
    %77 = llvm.call @malloc(%76) : (i64) -> !llvm.ptr
    %78 = llvm.getelementptr %77[0] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %8, %78 : i32, !llvm.ptr
    llvm.br ^bb16(%3 : i64)
  ^bb16(%79: i64):  // 2 preds: ^bb15, ^bb17
    %80 = llvm.icmp "slt" %79, %6 : i64
    llvm.cond_br %80, ^bb17, ^bb18
  ^bb17:  // pred: ^bb16
    %81 = llvm.getelementptr %29[0, %79] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %82 = llvm.load %81 : !llvm.ptr -> i64
    %83 = llvm.load %78 : !llvm.ptr -> i32
    llvm.call @artsAddDependence(%82, %75, %83) : (i64, i64, i32) -> ()
    %84 = llvm.add %83, %11  : i32
    llvm.store %84, %78 : i32, !llvm.ptr
    %85 = llvm.add %79, %13  : i64
    llvm.br ^bb16(%85 : i64)
  ^bb18:  // pred: ^bb16
    %86 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb19(%13 : i64)
  ^bb19(%87: i64):  // 2 preds: ^bb18, ^bb26
    %88 = llvm.icmp "slt" %87, %6 : i64
    llvm.cond_br %88, ^bb20, ^bb27
  ^bb20:  // pred: ^bb19
    %89 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %90 = llvm.call @artsGetCurrentNode() : () -> i32
    %91 = llvm.mlir.addressof @__arts_edt_5 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %92 = llvm.bitcast %91 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %93 = llvm.call @artsEdtCreateWithEpoch(%92, %90, %12, %86, %2, %89) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %94 = llvm.call @malloc(%76) : (i64) -> !llvm.ptr
    %95 = llvm.getelementptr %94[0] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %12, %95 : i32, !llvm.ptr
    llvm.br ^bb21(%3 : i64)
  ^bb21(%96: i64):  // 2 preds: ^bb20, ^bb22
    %97 = llvm.icmp "slt" %96, %6 : i64
    llvm.cond_br %97, ^bb22, ^bb23
  ^bb22:  // pred: ^bb21
    %98 = llvm.getelementptr %29[%87, %96] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %99 = llvm.load %98 : !llvm.ptr -> i64
    %100 = llvm.load %95 : !llvm.ptr -> i32
    llvm.call @artsAddDependence(%99, %93, %100) : (i64, i64, i32) -> ()
    %101 = llvm.add %100, %11  : i32
    llvm.store %101, %95 : i32, !llvm.ptr
    %102 = llvm.add %96, %13  : i64
    llvm.br ^bb21(%102 : i64)
  ^bb23:  // pred: ^bb21
    %103 = llvm.call @malloc(%76) : (i64) -> !llvm.ptr
    %104 = llvm.getelementptr %103[0] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %7, %104 : i32, !llvm.ptr
    llvm.br ^bb24(%3 : i64)
  ^bb24(%105: i64):  // 2 preds: ^bb23, ^bb25
    %106 = llvm.icmp "slt" %105, %6 : i64
    llvm.cond_br %106, ^bb25, ^bb26
  ^bb25:  // pred: ^bb24
    %107 = llvm.add %87, %4  : i64
    %108 = llvm.getelementptr %29[%107, %105] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %109 = llvm.load %108 : !llvm.ptr -> i64
    %110 = llvm.load %104 : !llvm.ptr -> i32
    llvm.call @artsAddDependence(%109, %93, %110) : (i64, i64, i32) -> ()
    %111 = llvm.add %110, %11  : i32
    llvm.store %111, %104 : i32, !llvm.ptr
    %112 = llvm.add %105, %13  : i64
    llvm.br ^bb24(%112 : i64)
  ^bb26:  // pred: ^bb24
    %113 = llvm.add %87, %13  : i64
    llvm.br ^bb19(%113 : i64)
  ^bb27:  // pred: ^bb19
    llvm.return
  }
  llvm.func @__arts_edt_3(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(10 : index) : i64
    %1 = llvm.mlir.constant(24 : index) : i64
    %2 = llvm.mlir.constant(1 : i32) : i32
    %3 = llvm.mlir.constant(0 : i32) : i32
    %4 = llvm.mlir.constant(0 : index) : i64
    %5 = llvm.mlir.constant(1 : index) : i64
    %6 = llvm.load %arg1 : !llvm.ptr -> i64
    %7 = llvm.trunc %6 : i64 to i32
    %8 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %9 = llvm.load %8 : !llvm.ptr -> i64
    %10 = llvm.sitofp %9 : i64 to f64
    %11 = llvm.alloca %5 x i64 : (i64) -> !llvm.ptr
    %12 = llvm.getelementptr %11[0] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %4, %12 : i64, !llvm.ptr
    %13 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %14 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb1(%4 : i64)
  ^bb1(%15: i64):  // 2 preds: ^bb0, ^bb2
    %16 = llvm.icmp "slt" %15, %0 : i64
    llvm.cond_br %16, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %17 = llvm.load %12 : !llvm.ptr -> i64
    %18 = llvm.mul %17, %1  : i64
    %19 = llvm.trunc %18 : i64 to i32
    %20 = llvm.getelementptr %arg3[%19] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %22 = llvm.load %21 : !llvm.ptr -> i64
    %23 = llvm.getelementptr %20[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %24 = llvm.getelementptr %13[%15] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %22, %24 : i64, !llvm.ptr
    %25 = llvm.getelementptr %14[%15] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %23, %25 : !llvm.ptr, !llvm.ptr
    %26 = llvm.add %17, %5  : i64
    llvm.store %26, %12 : i64, !llvm.ptr
    %27 = llvm.add %15, %5  : i64
    llvm.br ^bb1(%27 : i64)
  ^bb3:  // pred: ^bb1
    %28 = llvm.sitofp %7 : i32 to f64
    %29 = llvm.fadd %28, %10  : f64
    llvm.br ^bb4(%4 : i64)
  ^bb4(%30: i64):  // 2 preds: ^bb3, ^bb5
    %31 = llvm.icmp "slt" %30, %0 : i64
    llvm.cond_br %31, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %32 = llvm.getelementptr %14[%30] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    llvm.store %29, %32 : f64, !llvm.ptr
    %33 = llvm.add %30, %5  : i64
    llvm.br ^bb4(%33 : i64)
  ^bb6:  // pred: ^bb4
    %34 = llvm.alloca %5 x i32 : (i64) -> !llvm.ptr
    %35 = llvm.getelementptr %34[0] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %3, %35 : i32, !llvm.ptr
    llvm.br ^bb7(%4 : i64)
  ^bb7(%36: i64):  // 2 preds: ^bb6, ^bb8
    %37 = llvm.icmp "slt" %36, %0 : i64
    llvm.cond_br %37, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %38 = llvm.load %35 : !llvm.ptr -> i32
    %39 = llvm.getelementptr %arg1[%38] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %40 = llvm.load %39 : !llvm.ptr -> i64
    %41 = llvm.trunc %36 : i64 to i32
    %42 = llvm.getelementptr %13[%41] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %43 = llvm.load %42 : !llvm.ptr -> i64
    llvm.call @artsEventSatisfySlot(%40, %43, %3) : (i64, i64, i32) -> ()
    %44 = llvm.add %38, %2  : i32
    llvm.store %44, %35 : i32, !llvm.ptr
    %45 = llvm.add %36, %5  : i64
    llvm.br ^bb7(%45 : i64)
  ^bb9:  // pred: ^bb7
    llvm.return
  }
  llvm.func @__arts_edt_4(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(10 : index) : i64
    %1 = llvm.mlir.constant(24 : index) : i64
    %2 = llvm.mlir.constant(0 : index) : i64
    %3 = llvm.mlir.constant(1 : index) : i64
    %4 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %5 = llvm.getelementptr %4[0] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %2, %5 : i64, !llvm.ptr
    %6 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb1(%2 : i64)
  ^bb1(%7: i64):  // 2 preds: ^bb0, ^bb2
    %8 = llvm.icmp "slt" %7, %0 : i64
    llvm.cond_br %8, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %9 = llvm.load %5 : !llvm.ptr -> i64
    %10 = llvm.mul %9, %1  : i64
    %11 = llvm.trunc %10 : i64 to i32
    %12 = llvm.getelementptr %arg3[%11] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %13 = llvm.getelementptr %12[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %14 = llvm.getelementptr %6[%7] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %13, %14 : !llvm.ptr, !llvm.ptr
    %15 = llvm.add %9, %3  : i64
    llvm.store %15, %5 : i64, !llvm.ptr
    %16 = llvm.add %7, %3  : i64
    llvm.br ^bb1(%16 : i64)
  ^bb3:  // pred: ^bb1
    %17 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb4(%2 : i64)
  ^bb4(%18: i64):  // 2 preds: ^bb3, ^bb5
    %19 = llvm.icmp "slt" %18, %0 : i64
    llvm.cond_br %19, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %20 = llvm.load %5 : !llvm.ptr -> i64
    %21 = llvm.mul %20, %1  : i64
    %22 = llvm.trunc %21 : i64 to i32
    %23 = llvm.getelementptr %arg3[%22] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %24 = llvm.getelementptr %23[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %25 = llvm.getelementptr %17[%18] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %24, %25 : !llvm.ptr, !llvm.ptr
    %26 = llvm.add %20, %3  : i64
    llvm.store %26, %5 : i64, !llvm.ptr
    %27 = llvm.add %18, %3  : i64
    llvm.br ^bb4(%27 : i64)
  ^bb6:  // pred: ^bb4
    llvm.br ^bb7(%2 : i64)
  ^bb7(%28: i64):  // 2 preds: ^bb6, ^bb8
    %29 = llvm.icmp "slt" %28, %0 : i64
    llvm.cond_br %29, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %30 = llvm.getelementptr %17[%28] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    %31 = llvm.load %30 : !llvm.ptr -> f64
    %32 = llvm.getelementptr %6[%28] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    llvm.store %31, %32 : f64, !llvm.ptr
    %33 = llvm.add %28, %3  : i64
    llvm.br ^bb7(%33 : i64)
  ^bb9:  // pred: ^bb7
    llvm.return
  }
  llvm.func @__arts_edt_5(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(10 : index) : i64
    %1 = llvm.mlir.constant(24 : index) : i64
    %2 = llvm.mlir.constant(0 : index) : i64
    %3 = llvm.mlir.constant(1 : index) : i64
    %4 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %5 = llvm.getelementptr %4[0] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %2, %5 : i64, !llvm.ptr
    %6 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb1(%2 : i64)
  ^bb1(%7: i64):  // 2 preds: ^bb0, ^bb2
    %8 = llvm.icmp "slt" %7, %0 : i64
    llvm.cond_br %8, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %9 = llvm.load %5 : !llvm.ptr -> i64
    %10 = llvm.mul %9, %1  : i64
    %11 = llvm.trunc %10 : i64 to i32
    %12 = llvm.getelementptr %arg3[%11] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %13 = llvm.getelementptr %12[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %14 = llvm.getelementptr %6[%7] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %13, %14 : !llvm.ptr, !llvm.ptr
    %15 = llvm.add %9, %3  : i64
    llvm.store %15, %5 : i64, !llvm.ptr
    %16 = llvm.add %7, %3  : i64
    llvm.br ^bb1(%16 : i64)
  ^bb3:  // pred: ^bb1
    %17 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb4(%2 : i64)
  ^bb4(%18: i64):  // 2 preds: ^bb3, ^bb5
    %19 = llvm.icmp "slt" %18, %0 : i64
    llvm.cond_br %19, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %20 = llvm.load %5 : !llvm.ptr -> i64
    %21 = llvm.mul %20, %1  : i64
    %22 = llvm.trunc %21 : i64 to i32
    %23 = llvm.getelementptr %arg3[%22] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %24 = llvm.getelementptr %23[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %25 = llvm.getelementptr %17[%18] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %24, %25 : !llvm.ptr, !llvm.ptr
    %26 = llvm.add %20, %3  : i64
    llvm.store %26, %5 : i64, !llvm.ptr
    %27 = llvm.add %18, %3  : i64
    llvm.br ^bb4(%27 : i64)
  ^bb6:  // pred: ^bb4
    %28 = llvm.alloca %0 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb7(%2 : i64)
  ^bb7(%29: i64):  // 2 preds: ^bb6, ^bb8
    %30 = llvm.icmp "slt" %29, %0 : i64
    llvm.cond_br %30, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %31 = llvm.load %5 : !llvm.ptr -> i64
    %32 = llvm.mul %31, %1  : i64
    %33 = llvm.trunc %32 : i64 to i32
    %34 = llvm.getelementptr %arg3[%33] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %35 = llvm.getelementptr %34[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %36 = llvm.getelementptr %28[%29] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %35, %36 : !llvm.ptr, !llvm.ptr
    %37 = llvm.add %31, %3  : i64
    llvm.store %37, %5 : i64, !llvm.ptr
    %38 = llvm.add %29, %3  : i64
    llvm.br ^bb7(%38 : i64)
  ^bb9:  // pred: ^bb7
    llvm.br ^bb10(%2 : i64)
  ^bb10(%39: i64):  // 2 preds: ^bb9, ^bb11
    %40 = llvm.icmp "slt" %39, %0 : i64
    llvm.cond_br %40, ^bb11, ^bb12
  ^bb11:  // pred: ^bb10
    %41 = llvm.getelementptr %6[%39] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    %42 = llvm.load %41 : !llvm.ptr -> f64
    %43 = llvm.getelementptr %28[%39] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    %44 = llvm.load %43 : !llvm.ptr -> f64
    %45 = llvm.fadd %42, %44  : f64
    %46 = llvm.getelementptr %17[%39] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    llvm.store %45, %46 : f64, !llvm.ptr
    %47 = llvm.add %39, %3  : i64
    llvm.br ^bb10(%47 : i64)
  ^bb12:  // pred: ^bb10
    llvm.return
  }
  llvm.func @mainBody() -> i32 {
    %0 = llvm.mlir.constant(-1 : index) : i64
    %1 = llvm.mlir.constant(1 : index) : i64
    %2 = llvm.mlir.constant(100 : index) : i64
    %3 = llvm.mlir.constant(0 : index) : i64
    %4 = llvm.mlir.constant(10 : index) : i64
    %5 = llvm.mlir.constant(200 : i32) : i32
    %6 = llvm.mlir.constant(100 : i32) : i32
    %7 = llvm.mlir.constant(8 : i64) : i64
    %8 = llvm.mlir.constant(1 : i32) : i32
    %9 = llvm.mlir.constant(9 : i32) : i32
    %10 = llvm.mlir.constant(0 : i32) : i32
    %11 = llvm.call @rand() : () -> i32
    %12 = llvm.srem %11, %6  : i32
    %13 = llvm.call @artsGetCurrentNode() : () -> i32
    %14 = llvm.alloca %4 x !llvm.array<10 x i64> : (i64) -> !llvm.ptr
    llvm.br ^bb1(%3 : i64)
  ^bb1(%15: i64):  // 2 preds: ^bb0, ^bb2
    %16 = llvm.icmp "slt" %15, %2 : i64
    llvm.cond_br %16, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %17 = llvm.srem %15, %4  : i64
    %18 = llvm.icmp "slt" %17, %3 : i64
    %19 = llvm.add %17, %4  : i64
    %20 = llvm.select %18, %19, %17 : i1, i64
    %21 = llvm.icmp "slt" %15, %3 : i64
    %22 = llvm.sub %0, %15  : i64
    %23 = llvm.select %21, %22, %15 : i1, i64
    %24 = llvm.sdiv %23, %4  : i64
    %25 = llvm.sub %0, %24  : i64
    %26 = llvm.select %21, %25, %24 : i1, i64
    %27 = llvm.call @artsReserveGuidRoute(%9, %13) : (i32, i32) -> i64
    %28 = llvm.call @artsDbCreateWithGuid(%27, %7) : (i64, i64) -> !llvm.ptr
    %29 = llvm.getelementptr %14[%26, %20] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    llvm.store %27, %29 : i64, !llvm.ptr
    %30 = llvm.add %15, %1  : i64
    llvm.br ^bb1(%30 : i64)
  ^bb3:  // pred: ^bb1
    %31 = llvm.call @artsGetCurrentNode() : () -> i32
    %32 = llvm.alloca %4 x !llvm.array<10 x i64> : (i64) -> !llvm.ptr
    llvm.br ^bb4(%3 : i64)
  ^bb4(%33: i64):  // 2 preds: ^bb3, ^bb5
    %34 = llvm.icmp "slt" %33, %2 : i64
    llvm.cond_br %34, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %35 = llvm.srem %33, %4  : i64
    %36 = llvm.icmp "slt" %35, %3 : i64
    %37 = llvm.add %35, %4  : i64
    %38 = llvm.select %36, %37, %35 : i1, i64
    %39 = llvm.icmp "slt" %33, %3 : i64
    %40 = llvm.sub %0, %33  : i64
    %41 = llvm.select %39, %40, %33 : i1, i64
    %42 = llvm.sdiv %41, %4  : i64
    %43 = llvm.sub %0, %42  : i64
    %44 = llvm.select %39, %43, %42 : i1, i64
    %45 = llvm.call @artsReserveGuidRoute(%9, %31) : (i32, i32) -> i64
    %46 = llvm.call @artsDbCreateWithGuid(%45, %7) : (i64, i64) -> !llvm.ptr
    %47 = llvm.getelementptr %32[%44, %38] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    llvm.store %45, %47 : i64, !llvm.ptr
    %48 = llvm.add %33, %1  : i64
    llvm.br ^bb4(%48 : i64)
  ^bb6:  // pred: ^bb4
    %49 = llvm.call @artsGetCurrentNode() : () -> i32
    %50 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %51 = llvm.mlir.addressof @__arts_edt_1 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %52 = llvm.bitcast %51 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %53 = llvm.call @artsEdtCreate(%52, %49, %10, %50, %8) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %54 = llvm.call @artsInitializeAndStartEpoch(%53, %10) : (i64, i32) -> i64
    %55 = llvm.call @artsGetCurrentNode() : () -> i32
    %56 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    %57 = llvm.sext %12 : i32 to i64
    llvm.store %57, %56 : i64, !llvm.ptr
    %58 = llvm.mlir.addressof @__arts_edt_2 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %59 = llvm.bitcast %58 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %60 = llvm.call @artsEdtCreateWithEpoch(%59, %55, %8, %56, %5, %54) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.br ^bb7(%3 : i64)
  ^bb7(%61: i64):  // 2 preds: ^bb6, ^bb8
    %62 = llvm.icmp "slt" %61, %2 : i64
    llvm.cond_br %62, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %63 = llvm.srem %61, %4  : i64
    %64 = llvm.icmp "slt" %63, %3 : i64
    %65 = llvm.add %63, %4  : i64
    %66 = llvm.select %64, %65, %63 : i1, i64
    %67 = llvm.icmp "slt" %61, %3 : i64
    %68 = llvm.sub %0, %61  : i64
    %69 = llvm.select %67, %68, %61 : i1, i64
    %70 = llvm.sdiv %69, %4  : i64
    %71 = llvm.sub %0, %70  : i64
    %72 = llvm.select %67, %71, %70 : i1, i64
    %73 = llvm.getelementptr %14[%72, %66] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %74 = llvm.load %73 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%60, %10, %74) : (i64, i32, i64) -> ()
    %75 = llvm.add %61, %1  : i64
    llvm.br ^bb7(%75 : i64)
  ^bb9:  // pred: ^bb7
    llvm.br ^bb10(%3 : i64)
  ^bb10(%76: i64):  // 2 preds: ^bb9, ^bb11
    %77 = llvm.icmp "slt" %76, %2 : i64
    llvm.cond_br %77, ^bb11, ^bb12
  ^bb11:  // pred: ^bb10
    %78 = llvm.srem %76, %4  : i64
    %79 = llvm.icmp "slt" %78, %3 : i64
    %80 = llvm.add %78, %4  : i64
    %81 = llvm.select %79, %80, %78 : i1, i64
    %82 = llvm.icmp "slt" %76, %3 : i64
    %83 = llvm.sub %0, %76  : i64
    %84 = llvm.select %82, %83, %76 : i1, i64
    %85 = llvm.sdiv %84, %4  : i64
    %86 = llvm.sub %0, %85  : i64
    %87 = llvm.select %82, %86, %85 : i1, i64
    %88 = llvm.getelementptr %32[%87, %81] : (!llvm.ptr, i64, i64) -> !llvm.ptr, !llvm.array<10 x i64>
    %89 = llvm.load %88 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%60, %6, %89) : (i64, i32, i64) -> ()
    %90 = llvm.add %76, %1  : i64
    llvm.br ^bb10(%90 : i64)
  ^bb12:  // pred: ^bb10
    %91 = llvm.call @artsWaitOnHandle(%54) : (i64) -> i1
    %92 = llvm.mlir.addressof @str1 : !llvm.ptr
    %93 = llvm.getelementptr %92[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    llvm.br ^bb13(%3 : i64)
  ^bb13(%94: i64):  // 2 preds: ^bb12, ^bb14
    %95 = llvm.icmp "slt" %94, %4 : i64
    llvm.cond_br %95, ^bb14, ^bb15
  ^bb14:  // pred: ^bb13
    %96 = llvm.call @printf(%93) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %97 = llvm.add %94, %1  : i64
    llvm.br ^bb13(%97 : i64)
  ^bb15:  // pred: ^bb13
    llvm.br ^bb16(%3 : i64)
  ^bb16(%98: i64):  // 2 preds: ^bb15, ^bb17
    %99 = llvm.icmp "slt" %98, %4 : i64
    llvm.cond_br %99, ^bb17, ^bb18
  ^bb17:  // pred: ^bb16
    %100 = llvm.call @printf(%93) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %101 = llvm.add %98, %1  : i64
    llvm.br ^bb16(%101 : i64)
  ^bb18:  // pred: ^bb16
    llvm.return %10 : i32
  }
  llvm.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: !llvm.ptr) {
    llvm.return
  }
  llvm.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: !llvm.ptr) {
    %0 = llvm.mlir.constant(1 : index) : i64
    %1 = llvm.sext %arg0 : i32 to i64
    %2 = llvm.icmp "uge" %1, %0 : i64
    llvm.cond_br %2, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    llvm.return
  ^bb2:  // pred: ^bb0
    %3 = llvm.call @mainBody() : () -> i32
    llvm.return
  }
  llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.call @artsRT(%arg0, %arg1) : (i32, !llvm.ptr) -> i32
    llvm.return %0 : i32
  }
}

