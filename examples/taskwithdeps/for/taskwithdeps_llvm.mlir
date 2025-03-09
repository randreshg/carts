module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.func @artsRT(i32, !llvm.ptr) -> i32 attributes {sym_visibility = "private"}
  llvm.func @artsShutdown() attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentEpochGuid() -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsWaitOnHandle(i64) -> i1 attributes {sym_visibility = "private"}
  llvm.func @artsSignalEdt(i64, i32, i64) attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreateWithEpoch(!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreate(!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsDbCreateWithGuid(i64, i64) -> !llvm.ptr attributes {sym_visibility = "private"}
  llvm.func @artsReserveGuidRoute(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentNode() -> i32 attributes {llvm.nounwind, llvm.readnone, sym_visibility = "private"}
  llvm.mlir.global internal constant @str6("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
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
    %0 = llvm.mlir.constant(1 : index) : i64
    %1 = llvm.mlir.constant(24 : index) : i64
    %2 = llvm.mlir.constant(1 : i32) : i32
    %3 = llvm.mlir.constant(0 : i32) : i32
    %4 = llvm.mlir.constant(0 : index) : i64
    %5 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %6 = llvm.load %5 : !llvm.ptr -> i64
    %7 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    llvm.store %4, %7 : i64, !llvm.ptr
    %8 = llvm.alloca %6 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb1(%4 : i64)
  ^bb1(%9: i64):  // 2 preds: ^bb0, ^bb2
    %10 = llvm.icmp "slt" %9, %6 : i64
    llvm.cond_br %10, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %11 = llvm.load %7 : !llvm.ptr -> i64
    %12 = llvm.mul %11, %1  : i64
    %13 = llvm.trunc %12 : i64 to i32
    %14 = llvm.getelementptr %arg3[%13] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %16 = llvm.load %15 : !llvm.ptr -> i64
    %17 = llvm.getelementptr %8[%9] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %16, %17 : i64, !llvm.ptr
    %18 = llvm.add %11, %0  : i64
    llvm.store %18, %7 : i64, !llvm.ptr
    %19 = llvm.add %9, %0  : i64
    llvm.br ^bb1(%19 : i64)
  ^bb3:  // pred: ^bb1
    %20 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    llvm.br ^bb4(%4 : i64)
  ^bb4(%21: i64):  // 2 preds: ^bb3, ^bb5
    %22 = llvm.icmp "slt" %21, %6 : i64
    llvm.cond_br %22, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %23 = llvm.trunc %21 : i64 to i32
    %24 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %25 = llvm.call @artsGetCurrentNode() : () -> i32
    %26 = llvm.sext %23 : i32 to i64
    llvm.store %26, %20 : i64, !llvm.ptr
    %27 = llvm.mlir.addressof @__arts_edt_3 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %28 = llvm.bitcast %27 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %29 = llvm.call @artsEdtCreateWithEpoch(%28, %25, %2, %20, %2, %24) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %30 = llvm.getelementptr %8[%21] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %31 = llvm.load %30 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%29, %3, %31) : (i64, i32, i64) -> ()
    %32 = llvm.add %21, %0  : i64
    llvm.br ^bb4(%32 : i64)
  ^bb6:  // pred: ^bb4
    llvm.return
  }
  llvm.func @__arts_edt_3(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(100 : i32) : i32
    %1 = llvm.load %arg1 : !llvm.ptr -> i64
    %2 = llvm.trunc %1 : i64 to i32
    %3 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> !llvm.ptr
    %5 = llvm.call @rand() : () -> i32
    %6 = llvm.srem %5, %0  : i32
    llvm.store %6, %4 : i32, !llvm.ptr
    %7 = llvm.mlir.addressof @str3 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
    %9 = llvm.call @printf(%8, %2, %2, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    llvm.return
  }
  llvm.func @mainBody(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(2 : index) : i64
    %1 = llvm.mlir.constant(4 : i64) : i64
    %2 = llvm.mlir.constant(9 : i32) : i32
    %3 = llvm.mlir.constant(0 : index) : i64
    %4 = llvm.mlir.constant(1 : index) : i64
    %5 = llvm.mlir.constant(0 : i32) : i32
    %6 = llvm.mlir.constant(1 : i32) : i32
    %7 = llvm.mlir.constant(2 : i32) : i32
    %8 = llvm.mlir.undef : i32
    %9 = llvm.icmp "slt" %arg0, %7 : i32
    %10 = llvm.icmp "sge" %arg0, %7 : i32
    %11 = llvm.select %9, %6, %8 : i1, i32
    llvm.cond_br %9, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    %12 = llvm.mlir.addressof @stderr : !llvm.ptr
    %13 = llvm.load %12 : !llvm.ptr -> !llvm.ptr
    %14 = llvm.mlir.addressof @str0 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
    %16 = llvm.load %arg1 : !llvm.ptr -> !llvm.ptr
    %17 = llvm.call @fprintf(%13, %15, %16) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    llvm.br ^bb2
  ^bb2:  // 2 preds: ^bb0, ^bb1
    %18 = llvm.select %10, %5, %11 : i1, i32
    llvm.cond_br %10, ^bb3, ^bb13
  ^bb3:  // pred: ^bb2
    %19 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %20 = llvm.load %19 : !llvm.ptr -> !llvm.ptr
    %21 = llvm.call @atoi(%20) : (!llvm.ptr) -> i32
    %22 = llvm.sext %21 : i32 to i64
    %23 = llvm.alloca %22 x i32 : (i64) -> !llvm.ptr
    %24 = llvm.mlir.zero : !llvm.ptr
    %25 = llvm.call @time(%24) : (!llvm.ptr) -> i64
    %26 = llvm.trunc %25 : i64 to i32
    llvm.call @srand(%26) : (i32) -> ()
    %27 = llvm.mlir.addressof @str1 : !llvm.ptr
    %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %29 = llvm.call @printf(%28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %30 = llvm.mlir.addressof @str2 : !llvm.ptr
    %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
    %32 = llvm.call @printf(%31, %21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %33 = llvm.call @artsGetCurrentNode() : () -> i32
    %34 = llvm.alloca %22 x i64 : (i64) -> !llvm.ptr
    %35 = llvm.alloca %22 x !llvm.ptr : (i64) -> !llvm.ptr
    llvm.br ^bb4(%3 : i64)
  ^bb4(%36: i64):  // 2 preds: ^bb3, ^bb5
    %37 = llvm.icmp "slt" %36, %22 : i64
    llvm.cond_br %37, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %38 = llvm.call @artsReserveGuidRoute(%2, %33) : (i32, i32) -> i64
    %39 = llvm.call @artsDbCreateWithGuid(%38, %1) : (i64, i64) -> !llvm.ptr
    %40 = llvm.getelementptr %34[%36] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    llvm.store %38, %40 : i64, !llvm.ptr
    %41 = llvm.getelementptr %35[%36] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    llvm.store %39, %41 : !llvm.ptr, !llvm.ptr
    %42 = llvm.add %36, %4  : i64
    llvm.br ^bb4(%42 : i64)
  ^bb6:  // pred: ^bb4
    %43 = llvm.call @artsGetCurrentNode() : () -> i32
    %44 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %45 = llvm.mlir.addressof @__arts_edt_1 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %46 = llvm.bitcast %45 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %47 = llvm.call @artsEdtCreate(%46, %43, %5, %44, %6) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %48 = llvm.call @artsInitializeAndStartEpoch(%47, %5) : (i64, i32) -> i64
    %49 = llvm.call @artsGetCurrentNode() : () -> i32
    %50 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    llvm.store %22, %50 : i64, !llvm.ptr
    %51 = llvm.getelementptr %50[1] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %22, %51 : i64, !llvm.ptr
    %52 = llvm.mlir.addressof @__arts_edt_2 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %53 = llvm.bitcast %52 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %54 = llvm.call @artsEdtCreateWithEpoch(%53, %49, %7, %50, %21, %48) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.br ^bb7(%3 : i64)
  ^bb7(%55: i64):  // 2 preds: ^bb6, ^bb8
    %56 = llvm.icmp "slt" %55, %22 : i64
    llvm.cond_br %56, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %57 = llvm.getelementptr %34[%55] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %58 = llvm.load %57 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%54, %5, %58) : (i64, i32, i64) -> ()
    %59 = llvm.add %55, %4  : i64
    llvm.br ^bb7(%59 : i64)
  ^bb9:  // pred: ^bb7
    %60 = llvm.call @artsWaitOnHandle(%48) : (i64) -> i1
    %61 = llvm.mlir.addressof @str4 : !llvm.ptr
    %62 = llvm.getelementptr %61[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
    %63 = llvm.call @printf(%62) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.br ^bb10(%3 : i64)
  ^bb10(%64: i64):  // 2 preds: ^bb9, ^bb11
    %65 = llvm.icmp "slt" %64, %22 : i64
    llvm.cond_br %65, ^bb11, ^bb12
  ^bb11:  // pred: ^bb10
    %66 = llvm.trunc %64 : i64 to i32
    %67 = llvm.mlir.addressof @str5 : !llvm.ptr
    %68 = llvm.getelementptr %67[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %69 = llvm.getelementptr %35[%64] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %70 = llvm.load %69 : !llvm.ptr -> i32
    %71 = llvm.getelementptr %23[%64] : (!llvm.ptr, i64) -> !llvm.ptr, i32
    %72 = llvm.load %71 : !llvm.ptr -> i32
    %73 = llvm.call @printf(%68, %66, %70, %66, %72) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
    %74 = llvm.add %64, %4  : i64
    llvm.br ^bb10(%74 : i64)
  ^bb12:  // pred: ^bb10
    %75 = llvm.mlir.addressof @str6 : !llvm.ptr
    %76 = llvm.getelementptr %75[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %77 = llvm.call @printf(%76) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.br ^bb13
  ^bb13:  // 2 preds: ^bb2, ^bb12
    llvm.return %18 : i32
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

