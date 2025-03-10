module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.func @malloc(i64) -> !llvm.ptr
  llvm.func @artsRT(i32, !llvm.ptr) -> i32 attributes {sym_visibility = "private"}
  llvm.func @artsShutdown() attributes {sym_visibility = "private"}
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
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %x, B[%d] = %x\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Task %d - 3: Final B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : !llvm.ptr
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  llvm.func @atoi(!llvm.ptr) -> i32 attributes {sym_visibility = "private"}
  llvm.func @srand(i32) attributes {sym_visibility = "private"}
  llvm.func @time(!llvm.ptr) -> i64 attributes {sym_visibility = "private"}
  llvm.func @rand() -> i32 attributes {sym_visibility = "private"}
  llvm.func @__arts_edt_1(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    llvm.return
  }
  llvm.func @__arts_edt_2(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(-1 : index) : i64
    %1 = llvm.mlir.constant(3 : i32) : i32
    %2 = llvm.mlir.constant(2 : i32) : i32
    %3 = llvm.mlir.constant(0 : i32) : i32
    %4 = llvm.mlir.constant(24 : index) : i64
    %5 = llvm.mlir.constant(1 : i32) : i32
    %6 = llvm.mlir.constant(0 : index) : i64
    %7 = llvm.mlir.constant(1 : index) : i64
    %8 = llvm.mlir.constant(2 : index) : i64
    %9 = llvm.getelementptr %arg1[2] : (!llvm.ptr) -> !llvm.ptr, i64
    %10 = llvm.load %9 : !llvm.ptr -> i64
    %11 = llvm.alloca %7 x i64 : (i64) -> !llvm.ptr
    llvm.store %6, %11 : i64, !llvm.ptr
    %12 = llvm.alloca %10 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb1(%6 : i64)
  ^bb1(%13: i64):  // 2 preds: ^bb0, ^bb2
    %14 = llvm.icmp "slt" %13, %10 : i64
    llvm.cond_br %14, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %15 = llvm.load %11 : !llvm.ptr -> i64
    %16 = llvm.mul %15, %4  : i64
    %17 = llvm.trunc %16 : i64 to i32
    %18 = llvm.getelementptr %arg3[%17] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %20 = llvm.load %19 : !llvm.ptr -> i64
    %21 = llvm.getelementptr %12[%13] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %20, %21 : i64, !llvm.ptr
    %22 = llvm.add %15, %7  : i64
    llvm.store %22, %11 : i64, !llvm.ptr
    %23 = llvm.add %13, %7  : i64
    llvm.br ^bb1(%23 : i64)
  ^bb3:  // pred: ^bb1
    %24 = llvm.alloca %10 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb4(%6 : i64)
  ^bb4(%25: i64):  // 2 preds: ^bb3, ^bb5
    %26 = llvm.icmp "slt" %25, %10 : i64
    llvm.cond_br %26, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %27 = llvm.load %11 : !llvm.ptr -> i64
    %28 = llvm.mul %27, %4  : i64
    %29 = llvm.trunc %28 : i64 to i32
    %30 = llvm.getelementptr %arg3[%29] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %32 = llvm.load %31 : !llvm.ptr -> i64
    %33 = llvm.getelementptr %24[%25] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %32, %33 : i64, !llvm.ptr
    %34 = llvm.add %27, %7  : i64
    llvm.store %34, %11 : i64, !llvm.ptr
    %35 = llvm.add %25, %7  : i64
    llvm.br ^bb4(%35 : i64)
  ^bb6:  // pred: ^bb4
    %36 = llvm.call @artsGetCurrentNode() : () -> i32
    %37 = llvm.alloca %10 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb7(%6 : i64)
  ^bb7(%38: i64):  // 2 preds: ^bb6, ^bb8
    %39 = llvm.icmp "slt" %38, %10 : i64
    llvm.cond_br %39, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %40 = llvm.call @artsEventCreate(%36, %5) : (i32, i32) -> i64
    %41 = llvm.getelementptr %37[%38] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %40, %41 : i64, !llvm.ptr
    %42 = llvm.add %38, %7  : i64
    llvm.br ^bb7(%42 : i64)
  ^bb9:  // pred: ^bb7
    %43 = llvm.mlir.addressof @__arts_edt_3 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %44 = llvm.bitcast %43 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %45 = llvm.mlir.addressof @__arts_edt_6 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %46 = llvm.bitcast %45 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %47 = llvm.alloca %8 x i64 : (i64) -> !llvm.ptr
    %48 = llvm.alloca %7 x i64 : (i64) -> !llvm.ptr
    %49 = llvm.alloca %7 x i64 : (i64) -> !llvm.ptr
    %50 = llvm.alloca %7 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb10(%6 : i64)
  ^bb10(%51: i64):  // 2 preds: ^bb9, ^bb14
    %52 = llvm.icmp "slt" %51, %10 : i64
    llvm.cond_br %52, ^bb11, ^bb15
  ^bb11:  // pred: ^bb10
    %53 = llvm.trunc %51 : i64 to i32
    %54 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %55 = llvm.call @artsGetCurrentNode() : () -> i32
    %56 = llvm.sext %53 : i32 to i64
    llvm.store %56, %47 : i64, !llvm.ptr
    %57 = llvm.getelementptr %37[%51] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %58 = llvm.load %57 : !llvm.ptr -> i64
    %59 = llvm.getelementptr %47[1] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %58, %59 : i64, !llvm.ptr
    %60 = llvm.call @artsEdtCreateWithEpoch(%44, %55, %2, %47, %5, %54) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %61 = llvm.getelementptr %24[%51] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %62 = llvm.load %61 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%60, %3, %62) : (i64, i32, i64) -> ()
    %63 = llvm.icmp "eq" %51, %6 : i64
    llvm.cond_br %63, ^bb12, ^bb13
  ^bb12:  // pred: ^bb11
    %64 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %65 = llvm.call @artsGetCurrentNode() : () -> i32
    llvm.store %56, %48 : i64, !llvm.ptr
    %66 = llvm.mlir.addressof @__arts_edt_4 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %67 = llvm.bitcast %66 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %68 = llvm.call @artsEdtCreateWithEpoch(%67, %65, %5, %48, %2, %64) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %69 = llvm.load %57 : !llvm.ptr -> i64
    llvm.call @artsAddDependence(%69, %68, %5) : (i64, i64, i32) -> ()
    %70 = llvm.getelementptr %12[%51] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %71 = llvm.load %70 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%68, %3, %71) : (i64, i32, i64) -> ()
    llvm.br ^bb14
  ^bb13:  // pred: ^bb11
    %72 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %73 = llvm.call @artsGetCurrentNode() : () -> i32
    llvm.store %56, %49 : i64, !llvm.ptr
    %74 = llvm.mlir.addressof @__arts_edt_5 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %75 = llvm.bitcast %74 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %76 = llvm.call @artsEdtCreateWithEpoch(%75, %73, %5, %49, %1, %72) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %77 = llvm.load %57 : !llvm.ptr -> i64
    llvm.call @artsAddDependence(%77, %76, %2) : (i64, i64, i32) -> ()
    %78 = llvm.add %51, %0  : i64
    %79 = llvm.getelementptr %12[%78] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %80 = llvm.load %79 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%76, %3, %80) : (i64, i32, i64) -> ()
    %81 = llvm.getelementptr %12[%51] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %82 = llvm.load %81 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%76, %5, %82) : (i64, i32, i64) -> ()
    llvm.br ^bb14
  ^bb14:  // 2 preds: ^bb12, ^bb13
    %83 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %84 = llvm.call @artsGetCurrentNode() : () -> i32
    llvm.store %56, %50 : i64, !llvm.ptr
    %85 = llvm.call @artsEdtCreateWithEpoch(%46, %84, %5, %50, %5, %83) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %86 = llvm.getelementptr %12[%51] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %87 = llvm.load %86 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%85, %3, %87) : (i64, i32, i64) -> ()
    %88 = llvm.add %51, %7  : i64
    llvm.br ^bb10(%88 : i64)
  ^bb15:  // pred: ^bb10
    llvm.return
  }
  llvm.func @__arts_edt_3(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.mlir.constant(100 : i32) : i32
    %2 = llvm.load %arg1 : !llvm.ptr -> i64
    %3 = llvm.trunc %2 : i64 to i32
    %4 = llvm.getelementptr %arg3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %5 = llvm.load %4 : !llvm.ptr -> i64
    %6 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> !llvm.ptr
    %8 = llvm.call @rand() : () -> i32
    %9 = llvm.srem %8, %1  : i32
    llvm.store %9, %7 : i32, !llvm.ptr
    %10 = llvm.mlir.addressof @str3 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %12 = llvm.call @printf(%11, %3, %3, %9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    %13 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %14 = llvm.load %13 : !llvm.ptr -> i64
    llvm.call @artsEventSatisfySlot(%14, %5, %0) : (i64, i64, i32) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_4(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(5 : i32) : i32
    %1 = llvm.load %arg1 : !llvm.ptr -> i64
    %2 = llvm.trunc %1 : i64 to i32
    %3 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> !llvm.ptr
    %5 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> !llvm.ptr
    %8 = llvm.load %7 : !llvm.ptr -> i32
    %9 = llvm.add %8, %0  : i32
    llvm.store %9, %4 : i32, !llvm.ptr
    %10 = llvm.mlir.addressof @str4 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %12 = llvm.call @printf(%11, %2, %2, %9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    llvm.return
  }
  llvm.func @__arts_edt_5(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(5 : i32) : i32
    %1 = llvm.load %arg1 : !llvm.ptr -> i64
    %2 = llvm.trunc %1 : i64 to i32
    %3 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> !llvm.ptr
    %5 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> !llvm.ptr
    %8 = llvm.getelementptr %arg3[48] : (!llvm.ptr) -> !llvm.ptr, i8
    %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %10 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %11 = llvm.load %10 : !llvm.ptr -> i32
    %12 = llvm.load %4 : !llvm.ptr -> i32
    %13 = llvm.add %11, %12  : i32
    %14 = llvm.add %13, %0  : i32
    llvm.store %14, %7 : i32, !llvm.ptr
    %15 = llvm.mlir.addressof @str5 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %17 = llvm.call @printf(%16, %2, %2, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    llvm.return
  }
  llvm.func @__arts_edt_6(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.load %arg1 : !llvm.ptr -> i64
    %1 = llvm.trunc %0 : i64 to i32
    %2 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %3 = llvm.load %2 : !llvm.ptr -> !llvm.ptr
    %4 = llvm.load %3 : !llvm.ptr -> i32
    %5 = llvm.mlir.addressof @str6 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<31 x i8>
    %7 = llvm.call @printf(%6, %1, %1, %4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    llvm.return
  }
  llvm.func @mainBody(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(8 : i64) : i64
    %1 = llvm.mlir.constant(3 : index) : i64
    %2 = llvm.mlir.constant(3 : i32) : i32
    %3 = llvm.mlir.constant(4 : i64) : i64
    %4 = llvm.mlir.constant(9 : i32) : i32
    %5 = llvm.mlir.constant(0 : index) : i64
    %6 = llvm.mlir.constant(1 : index) : i64
    %7 = llvm.mlir.constant(0 : i32) : i32
    %8 = llvm.mlir.constant(1 : i32) : i32
    %9 = llvm.mlir.constant(2 : i32) : i32
    %10 = llvm.mlir.undef : i32
    %11 = llvm.icmp "slt" %arg0, %9 : i32
    %12 = llvm.icmp "sge" %arg0, %9 : i32
    %13 = llvm.select %11, %8, %10 : i1, i32
    llvm.cond_br %11, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    %14 = llvm.mlir.addressof @stderr : !llvm.ptr
    %15 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    %16 = llvm.mlir.addressof @str0 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
    %18 = llvm.load %arg1 : !llvm.ptr -> !llvm.ptr
    %19 = llvm.call @fprintf(%15, %17, %18) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    llvm.br ^bb2
  ^bb2:  // 2 preds: ^bb0, ^bb1
    %20 = llvm.select %12, %7, %13 : i1, i32
    llvm.cond_br %12, ^bb3, ^bb19
  ^bb3:  // pred: ^bb2
    %21 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %22 = llvm.load %21 : !llvm.ptr -> !llvm.ptr
    %23 = llvm.call @atoi(%22) : (!llvm.ptr) -> i32
    %24 = llvm.sext %23 : i32 to i64
    %25 = llvm.mlir.zero : !llvm.ptr
    %26 = llvm.call @time(%25) : (!llvm.ptr) -> i64
    %27 = llvm.trunc %26 : i64 to i32
    llvm.call @srand(%27) : (i32) -> ()
    %28 = llvm.mlir.addressof @str1 : !llvm.ptr
    %29 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %30 = llvm.call @printf(%29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %31 = llvm.mlir.addressof @str2 : !llvm.ptr
    %32 = llvm.getelementptr %31[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
    %33 = llvm.call @printf(%32, %23) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %34 = llvm.call @artsGetCurrentNode() : () -> i32
    %35 = llvm.alloca %24 x i64 : (i64) -> !llvm.ptr
    %36 = llvm.alloca %24 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb4(%5 : i64)
  ^bb4(%37: i64):  // 2 preds: ^bb3, ^bb5
    %38 = llvm.icmp "slt" %37, %24 : i64
    llvm.cond_br %38, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %39 = llvm.call @artsReserveGuidRoute(%4, %34) : (i32, i32) -> i64
    %40 = llvm.call @artsDbCreateWithGuid(%39, %3) : (i64, i64) -> !llvm.ptr
    %41 = llvm.getelementptr %35[%37] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %39, %41 : i64, !llvm.ptr
    %42 = llvm.getelementptr %36[%37] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %40, %42 : !llvm.ptr, !llvm.ptr
    %43 = llvm.add %37, %6  : i64
    llvm.br ^bb4(%43 : i64)
  ^bb6:  // pred: ^bb4
    %44 = llvm.call @artsGetCurrentNode() : () -> i32
    %45 = llvm.alloca %24 x i64 : (i64) -> !llvm.ptr
    %46 = llvm.alloca %24 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb7(%5 : i64)
  ^bb7(%47: i64):  // 2 preds: ^bb6, ^bb8
    %48 = llvm.icmp "slt" %47, %24 : i64
    llvm.cond_br %48, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %49 = llvm.call @artsReserveGuidRoute(%4, %44) : (i32, i32) -> i64
    %50 = llvm.call @artsDbCreateWithGuid(%49, %3) : (i64, i64) -> !llvm.ptr
    %51 = llvm.getelementptr %45[%47] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %49, %51 : i64, !llvm.ptr
    %52 = llvm.getelementptr %46[%47] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %50, %52 : !llvm.ptr, !llvm.ptr
    %53 = llvm.add %47, %6  : i64
    llvm.br ^bb7(%53 : i64)
  ^bb9:  // pred: ^bb7
    %54 = llvm.call @artsGetCurrentNode() : () -> i32
    %55 = llvm.alloca %5 x i64 : (i64) -> !llvm.ptr
    %56 = llvm.mlir.addressof @__arts_edt_1 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %57 = llvm.bitcast %56 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %58 = llvm.call @artsEdtCreate(%57, %54, %7, %55, %8) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %59 = llvm.call @artsInitializeAndStartEpoch(%58, %7) : (i64, i32) -> i64
    %60 = llvm.call @artsGetCurrentNode() : () -> i32
    %61 = llvm.add %24, %24  : i64
    %62 = llvm.trunc %61 : i64 to i32
    %63 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    llvm.store %24, %63 : i64, !llvm.ptr
    %64 = llvm.getelementptr %63[1] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %24, %64 : i64, !llvm.ptr
    %65 = llvm.getelementptr %63[2] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %24, %65 : i64, !llvm.ptr
    %66 = llvm.mlir.addressof @__arts_edt_2 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %67 = llvm.bitcast %66 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %68 = llvm.call @artsEdtCreateWithEpoch(%67, %60, %2, %63, %62, %59) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %69 = llvm.mul %0, %6  : i64
    %70 = llvm.call @malloc(%69) : (i64) -> !llvm.ptr
    llvm.store %5, %70 : i64, !llvm.ptr
    llvm.br ^bb10(%5 : i64)
  ^bb10(%71: i64):  // 2 preds: ^bb9, ^bb11
    %72 = llvm.icmp "slt" %71, %24 : i64
    llvm.cond_br %72, ^bb11, ^bb12
  ^bb11:  // pred: ^bb10
    %73 = llvm.getelementptr %35[%71] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %74 = llvm.load %73 : !llvm.ptr -> i64
    %75 = llvm.load %70 : !llvm.ptr -> i64
    %76 = llvm.trunc %75 : i64 to i32
    llvm.call @artsSignalEdt(%68, %76, %74) : (i64, i32, i64) -> ()
    %77 = llvm.add %75, %6  : i64
    llvm.store %77, %70 : i64, !llvm.ptr
    %78 = llvm.add %71, %6  : i64
    llvm.br ^bb10(%78 : i64)
  ^bb12:  // pred: ^bb10
    %79 = llvm.call @malloc(%69) : (i64) -> !llvm.ptr
    llvm.store %24, %79 : i64, !llvm.ptr
    llvm.br ^bb13(%5 : i64)
  ^bb13(%80: i64):  // 2 preds: ^bb12, ^bb14
    %81 = llvm.icmp "slt" %80, %24 : i64
    llvm.cond_br %81, ^bb14, ^bb15
  ^bb14:  // pred: ^bb13
    %82 = llvm.getelementptr %45[%80] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %83 = llvm.load %82 : !llvm.ptr -> i64
    %84 = llvm.load %79 : !llvm.ptr -> i64
    %85 = llvm.trunc %84 : i64 to i32
    llvm.call @artsSignalEdt(%68, %85, %83) : (i64, i32, i64) -> ()
    %86 = llvm.add %84, %6  : i64
    llvm.store %86, %79 : i64, !llvm.ptr
    %87 = llvm.add %80, %6  : i64
    llvm.br ^bb13(%87 : i64)
  ^bb15:  // pred: ^bb13
    %88 = llvm.call @artsWaitOnHandle(%59) : (i64) -> i1
    %89 = llvm.mlir.addressof @str7 : !llvm.ptr
    %90 = llvm.getelementptr %89[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
    %91 = llvm.call @printf(%90) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %92 = llvm.mlir.addressof @str8 : !llvm.ptr
    %93 = llvm.getelementptr %92[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    llvm.br ^bb16(%5 : i64)
  ^bb16(%94: i64):  // 2 preds: ^bb15, ^bb17
    %95 = llvm.icmp "slt" %94, %24 : i64
    llvm.cond_br %95, ^bb17, ^bb18
  ^bb17:  // pred: ^bb16
    %96 = llvm.trunc %94 : i64 to i32
    %97 = llvm.getelementptr %46[%94] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %98 = llvm.load %97 : !llvm.ptr -> i32
    %99 = llvm.getelementptr %36[%94] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %100 = llvm.load %99 : !llvm.ptr -> i32
    %101 = llvm.call @printf(%93, %96, %98, %96, %100) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
    %102 = llvm.add %94, %6  : i64
    llvm.br ^bb16(%102 : i64)
  ^bb18:  // pred: ^bb16
    %103 = llvm.mlir.addressof @str9 : !llvm.ptr
    %104 = llvm.getelementptr %103[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %105 = llvm.call @printf(%104) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.br ^bb19
  ^bb19:  // 2 preds: ^bb2, ^bb18
    llvm.return %20 : i32
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
    %3 = llvm.call @mainBody(%arg1, %arg2) : (i32, !llvm.ptr) -> i32
    llvm.call @artsShutdown() : () -> ()
    llvm.return
  }
  llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.call @artsRT(%arg0, %arg1) : (i32, !llvm.ptr) -> i32
    llvm.return %0 : i32
  }
}

