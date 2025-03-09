module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
  llvm.mlir.global internal constant @str5("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("A: %d, B: %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 1: Initializing a with value %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Main thread: Creating tasks\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.func @__arts_edt_1(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    llvm.return
  }
  llvm.func @__arts_edt_2(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(1 : index) : i64
    %1 = llvm.mlir.constant(0 : index) : i64
    %2 = llvm.mlir.constant(2 : i32) : i32
    %3 = llvm.mlir.constant(1 : i32) : i32
    %4 = llvm.mlir.constant(0 : i32) : i32
    %5 = llvm.getelementptr %arg3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %6 = llvm.load %5 : !llvm.ptr -> i64
    %7 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %9 = llvm.load %8 : !llvm.ptr -> i64
    %10 = llvm.mlir.addressof @str1 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %12 = llvm.call @printf(%11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %13 = llvm.call @artsGetCurrentNode() : () -> i32
    %14 = llvm.call @artsEventCreate(%13, %3) : (i32, i32) -> i64
    %15 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %16 = llvm.call @artsGetCurrentNode() : () -> i32
    %17 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    llvm.store %14, %17 : i64, !llvm.ptr
    %18 = llvm.mlir.addressof @__arts_edt_3 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %19 = llvm.bitcast %18 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %20 = llvm.call @artsEdtCreateWithEpoch(%19, %16, %3, %17, %3, %15) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsSignalEdt(%20, %4, %9) : (i64, i32, i64) -> ()
    %21 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %22 = llvm.call @artsGetCurrentNode() : () -> i32
    %23 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    %24 = llvm.mlir.addressof @__arts_edt_4 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %25 = llvm.bitcast %24 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %26 = llvm.call @artsEdtCreateWithEpoch(%25, %22, %4, %23, %2, %21) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsAddDependence(%14, %26, %3) : (i64, i64, i32) -> ()
    llvm.call @artsSignalEdt(%26, %4, %6) : (i64, i32, i64) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_3(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.mlir.constant(10 : i32) : i32
    %2 = llvm.getelementptr %arg3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %3 = llvm.load %2 : !llvm.ptr -> i64
    %4 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    llvm.store %1, %4 : i32, !llvm.ptr
    %5 = llvm.mlir.addressof @str2 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %7 = llvm.call @printf(%6, %1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %8 = llvm.load %arg1 : !llvm.ptr -> i64
    llvm.call @artsEventSatisfySlot(%8, %3, %0) : (i64, i64, i32) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_4(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(5 : i32) : i32
    %1 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %2 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %3 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> i32
    %5 = llvm.mlir.addressof @str3 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %7 = llvm.call @printf(%6, %4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %8 = llvm.add %4, %0  : i32
    llvm.store %8, %1 : i32, !llvm.ptr
    llvm.return
  }
  llvm.func @mainBody() -> i32 {
    %0 = llvm.mlir.constant(0 : index) : i64
    %1 = llvm.mlir.constant(2 : i32) : i32
    %2 = llvm.mlir.constant(1 : i32) : i32
    %3 = llvm.mlir.constant(4 : i64) : i64
    %4 = llvm.mlir.constant(9 : i32) : i32
    %5 = llvm.mlir.constant(0 : i32) : i32
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = llvm.call @artsGetCurrentNode() : () -> i32
    %10 = llvm.call @artsReserveGuidRoute(%4, %9) : (i32, i32) -> i64
    %11 = llvm.call @artsDbCreateWithGuid(%10, %3) : (i64, i64) -> !llvm.ptr
    %12 = llvm.call @artsGetCurrentNode() : () -> i32
    %13 = llvm.call @artsReserveGuidRoute(%4, %12) : (i32, i32) -> i64
    %14 = llvm.call @artsDbCreateWithGuid(%13, %3) : (i64, i64) -> !llvm.ptr
    %15 = llvm.call @artsGetCurrentNode() : () -> i32
    %16 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %17 = llvm.mlir.addressof @__arts_edt_1 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %18 = llvm.bitcast %17 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %19 = llvm.call @artsEdtCreate(%18, %15, %5, %16, %2) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %20 = llvm.call @artsInitializeAndStartEpoch(%19, %5) : (i64, i32) -> i64
    %21 = llvm.call @artsGetCurrentNode() : () -> i32
    %22 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %23 = llvm.mlir.addressof @__arts_edt_2 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %24 = llvm.bitcast %23 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %25 = llvm.call @artsEdtCreateWithEpoch(%24, %21, %5, %22, %1, %20) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsSignalEdt(%25, %5, %10) : (i64, i32, i64) -> ()
    llvm.call @artsSignalEdt(%25, %2, %13) : (i64, i32, i64) -> ()
    %26 = llvm.call @artsWaitOnHandle(%20) : (i64) -> i1
    %27 = llvm.mlir.addressof @str4 : !llvm.ptr
    %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<14 x i8>
    %29 = llvm.load %14 : !llvm.ptr -> i32
    %30 = llvm.load %11 : !llvm.ptr -> i32
    %31 = llvm.call @printf(%28, %29, %30) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %32 = llvm.mlir.addressof @str5 : !llvm.ptr
    %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %34 = llvm.call @printf(%33) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.return %5 : i32
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
    llvm.call @artsShutdown() : () -> ()
    llvm.return
  }
  llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.call @artsRT(%arg0, %arg1) : (i32, !llvm.ptr) -> i32
    llvm.return %0 : i32
  }
}

