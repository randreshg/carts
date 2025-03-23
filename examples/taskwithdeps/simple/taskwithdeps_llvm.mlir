module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.func @artsRT(i32, !llvm.ptr) -> i32 attributes {sym_visibility = "private"}
  llvm.func @artsShutdown() attributes {sym_visibility = "private"}
  llvm.func @artsPersistentEventIncrementLatch(i64, i64) attributes {sym_visibility = "private"}
  llvm.func @artsPersistentEventDecrementLatch(i64, i64) attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentEpochGuid() -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsWaitOnHandle(i64) -> i1 attributes {sym_visibility = "private"}
  llvm.func @artsAddDependenceToPersistentEvent(i64, i64, i32) attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreateWithEpoch(!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreate(!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsPersistentEventCreate(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsDbCreateWithGuid(i64, i64) -> !llvm.ptr attributes {sym_visibility = "private"}
  llvm.func @artsReserveGuidRoute(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentNode() -> i32 attributes {llvm.nounwind, llvm.readnone, sym_visibility = "private"}
  llvm.mlir.global internal constant @str7("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("A: %d, B: %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Parallel region DONE\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 1: Initializing a with value %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Main thread: Creating tasks\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.func @srand(i32) attributes {sym_visibility = "private"}
  llvm.func @time(!llvm.ptr) -> i64 attributes {sym_visibility = "private"}
  llvm.func @rand() -> i32 attributes {sym_visibility = "private"}
  llvm.func @__arts_edt_1(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    llvm.return
  }
  llvm.func @__arts_edt_2(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(1 : index) : i64
    %1 = llvm.mlir.constant(0 : index) : i64
    %2 = llvm.mlir.constant(0 : i32) : i32
    %3 = llvm.mlir.constant(1 : i32) : i32
    %4 = llvm.mlir.constant(2 : i32) : i32
    %5 = llvm.call @artsGetCurrentNode() : () -> i32
    %6 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %7 = llvm.call @artsPersistentEventCreate(%5, %2) : (i32, i32) -> i64
    llvm.store %7, %6 : i64, !llvm.ptr
    %8 = llvm.call @artsGetCurrentNode() : () -> i32
    %9 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %10 = llvm.call @artsPersistentEventCreate(%8, %2) : (i32, i32) -> i64
    llvm.store %10, %9 : i64, !llvm.ptr
    %11 = llvm.mlir.addressof @str1 : !llvm.ptr
    %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %13 = llvm.call @printf(%12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %14 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %15 = llvm.call @artsGetCurrentNode() : () -> i32
    %16 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %17 = llvm.load %9 : !llvm.ptr -> i64
    llvm.store %17, %16 : i64, !llvm.ptr
    %18 = llvm.mlir.addressof @__arts_edt_3 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %19 = llvm.bitcast %18 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %20 = llvm.call @artsEdtCreateWithEpoch(%19, %15, %3, %16, %3, %14) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %21 = llvm.load %9 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%21, %20, %2) : (i64, i64, i32) -> ()
    %22 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %23 = llvm.call @artsGetCurrentNode() : () -> i32
    %24 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %25 = llvm.load %6 : !llvm.ptr -> i64
    llvm.store %25, %24 : i64, !llvm.ptr
    %26 = llvm.mlir.addressof @__arts_edt_4 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %27 = llvm.bitcast %26 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %28 = llvm.call @artsEdtCreateWithEpoch(%27, %23, %3, %24, %4, %22) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %29 = llvm.load %6 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%29, %28, %2) : (i64, i64, i32) -> ()
    %30 = llvm.load %9 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%30, %28, %3) : (i64, i64, i32) -> ()
    %31 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %32 = llvm.call @artsGetCurrentNode() : () -> i32
    %33 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    %34 = llvm.mlir.addressof @__arts_edt_5 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %35 = llvm.bitcast %34 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %36 = llvm.call @artsEdtCreateWithEpoch(%35, %32, %2, %33, %3, %31) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %37 = llvm.load %6 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%37, %36, %2) : (i64, i64, i32) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_3(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(100 : i32) : i32
    %1 = llvm.getelementptr %arg3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %2 = llvm.load %1 : !llvm.ptr -> i64
    %3 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> !llvm.ptr
    %5 = llvm.call @rand() : () -> i32
    %6 = llvm.srem %5, %0  : i32
    llvm.store %6, %4 : i32, !llvm.ptr
    %7 = llvm.mlir.addressof @str2 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %9 = llvm.call @printf(%8, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %10 = llvm.load %arg1 : !llvm.ptr -> i64
    llvm.call @artsPersistentEventDecrementLatch(%10, %2) : (i64, i64) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_4(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.mlir.constant(5 : i32) : i32
    %1 = llvm.getelementptr %arg3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %2 = llvm.load %1 : !llvm.ptr -> i64
    %3 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> !llvm.ptr
    %5 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> !llvm.ptr
    %8 = llvm.load %7 : !llvm.ptr -> i32
    %9 = llvm.mlir.addressof @str3 : !llvm.ptr
    %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %11 = llvm.call @printf(%10, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %12 = llvm.add %8, %0  : i32
    llvm.store %12, %4 : i32, !llvm.ptr
    %13 = llvm.load %arg1 : !llvm.ptr -> i64
    llvm.call @artsPersistentEventDecrementLatch(%13, %2) : (i64, i64) -> ()
    llvm.return
  }
  llvm.func @__arts_edt_5(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) attributes {sym_visibility = "private"} {
    %0 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %1 = llvm.load %0 : !llvm.ptr -> !llvm.ptr
    %2 = llvm.load %1 : !llvm.ptr -> i32
    %3 = llvm.mlir.addressof @str4 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %5 = llvm.call @printf(%4, %2) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    llvm.return
  }
  llvm.func @mainBody() -> i32 {
    %0 = llvm.mlir.constant(0 : index) : i64
    %1 = llvm.mlir.constant(1 : index) : i64
    %2 = llvm.mlir.constant(1 : i32) : i32
    %3 = llvm.mlir.constant(0 : i32) : i32
    %4 = llvm.mlir.constant(2 : i32) : i32
    %5 = llvm.mlir.constant(4 : i64) : i64
    %6 = llvm.mlir.constant(9 : i32) : i32
    %7 = llvm.mlir.addressof @str0 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %9 = llvm.call @printf(%8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %10 = llvm.mlir.zero : !llvm.ptr
    %11 = llvm.call @time(%10) : (!llvm.ptr) -> i64
    %12 = llvm.trunc %11 : i64 to i32
    llvm.call @srand(%12) : (i32) -> ()
    %13 = llvm.call @artsGetCurrentNode() : () -> i32
    %14 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    %15 = llvm.call @artsPersistentEventCreate(%13, %3) : (i32, i32) -> i64
    llvm.store %15, %14 : i64, !llvm.ptr
    %16 = llvm.call @artsGetCurrentNode() : () -> i32
    %17 = llvm.call @artsReserveGuidRoute(%6, %16) : (i32, i32) -> i64
    %18 = llvm.call @artsDbCreateWithGuid(%17, %5) : (i64, i64) -> !llvm.ptr
    %19 = llvm.call @artsGetCurrentNode() : () -> i32
    %20 = llvm.alloca %1 x i64 : (i64) -> !llvm.ptr
    %21 = llvm.call @artsPersistentEventCreate(%19, %3) : (i32, i32) -> i64
    llvm.store %21, %20 : i64, !llvm.ptr
    %22 = llvm.call @artsGetCurrentNode() : () -> i32
    %23 = llvm.call @artsReserveGuidRoute(%6, %22) : (i32, i32) -> i64
    %24 = llvm.call @artsDbCreateWithGuid(%23, %5) : (i64, i64) -> !llvm.ptr
    %25 = llvm.call @artsGetCurrentNode() : () -> i32
    %26 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %27 = llvm.mlir.addressof @__arts_edt_1 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %28 = llvm.bitcast %27 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %29 = llvm.call @artsEdtCreate(%28, %25, %3, %26, %2) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %30 = llvm.call @artsInitializeAndStartEpoch(%29, %3) : (i64, i32) -> i64
    %31 = llvm.call @artsGetCurrentNode() : () -> i32
    %32 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %33 = llvm.mlir.addressof @__arts_edt_2 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>>
    %34 = llvm.bitcast %33 : !llvm.ptr<func<void (i32, ptr, i32, ptr)>> to !llvm.ptr
    %35 = llvm.call @artsEdtCreateWithEpoch(%34, %31, %3, %32, %4, %30) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %36 = llvm.load %14 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%36, %35, %3) : (i64, i64, i32) -> ()
    %37 = llvm.load %20 : !llvm.ptr -> i64
    llvm.call @artsAddDependenceToPersistentEvent(%37, %35, %2) : (i64, i64, i32) -> ()
    %38 = llvm.call @artsWaitOnHandle(%30) : (i64) -> i1
    %39 = llvm.mlir.addressof @str5 : !llvm.ptr
    %40 = llvm.getelementptr %39[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %41 = llvm.call @printf(%40) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %42 = llvm.mlir.addressof @str6 : !llvm.ptr
    %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<14 x i8>
    %44 = llvm.load %24 : !llvm.ptr -> i32
    %45 = llvm.load %18 : !llvm.ptr -> i32
    %46 = llvm.call @printf(%43, %44, %45) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %47 = llvm.mlir.addressof @str7 : !llvm.ptr
    %48 = llvm.getelementptr %47[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %49 = llvm.call @printf(%48) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.return %3 : i32
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

