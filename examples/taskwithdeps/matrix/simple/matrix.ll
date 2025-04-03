module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventIncrementLatch(i64, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventDecrementLatch(i64, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependenceToPersistentEvent(i64, i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsPersistentEventCreate(i32, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str10("\0AVerification: FAILED\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str9("\0AVerification: PASSED\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("\0AMatrix B:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("%6.2f \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Matrix A:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task 2: Computing B[%d][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 1: Computing B[0][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 0: Initializing A[%d][%d] = %.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Random number: %d\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s <N>\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func private @exit(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c0_i64 = arith.constant 0 : i64
    %c0_i32 = arith.constant 0 : i32
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %0 = memref.load %arg1[%c1] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c5] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %4 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg4 = %c0 to %3 step %c1 {
      scf.for %arg5 = %c0 to %3 step %c1 {
        %7 = memref.load %alloca[] : memref<index>
        %8 = arith.muli %7, %c24 : index
        %9 = arith.index_cast %8 : index to i32
        %10 = llvm.getelementptr %4[%9] : (!llvm.ptr, i32) -> !llvm.ptr, i8
        %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
        %12 = llvm.load %11 : !llvm.ptr -> i64
        memref.store %12, %alloca_0[%arg4, %arg5] : memref<?x?xi64>
        %13 = arith.addi %7, %c1 : index
        memref.store %13, %alloca[] : memref<index>
      }
    }
    %alloca_1 = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg4 = %c0 to %3 step %c1 {
      scf.for %arg5 = %c0 to %3 step %c1 {
        %7 = memref.load %alloca[] : memref<index>
        %8 = arith.muli %7, %c24 : index
        %9 = arith.index_cast %8 : index to i32
        %10 = llvm.getelementptr %4[%9] : (!llvm.ptr, i32) -> !llvm.ptr, i8
        %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
        %12 = llvm.load %11 : !llvm.ptr -> i64
        memref.store %12, %alloca_1[%arg4, %arg5] : memref<?x?xi64>
        %13 = arith.addi %7, %c1 : index
        memref.store %13, %alloca[] : memref<index>
      }
    }
    %5 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg4 = %c0 to %3 step %c1 {
      scf.for %arg5 = %c0 to %3 step %c1 {
        %7 = func.call @artsPersistentEventCreate(%5, %c0_i32, %c0_i64) : (i32, i32, i64) -> i64
        memref.store %7, %alloca_2[%arg4, %arg5] : memref<?x?xi64>
      }
    }
    %6 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg4 = %c0 to %3 step %c1 {
      scf.for %arg5 = %c0 to %3 step %c1 {
        %7 = func.call @artsPersistentEventCreate(%6, %c0_i32, %c0_i64) : (i32, i32, i64) -> i64
        memref.store %7, %alloca_3[%arg4, %arg5] : memref<?x?xi64>
      }
    }
    %alloca_4 = memref.alloca() : memref<index>
    %alloca_5 = memref.alloca() : memref<index>
    %alloca_6 = memref.alloca() : memref<index>
    %alloca_7 = memref.alloca() : memref<index>
    scf.for %arg4 = %c0 to %3 step %c1 {
      %7 = arith.index_cast %arg4 : index to i32
      scf.for %arg5 = %c0 to %3 step %c1 {
        %8 = arith.index_cast %arg5 : index to i32
        %9 = func.call @artsGetCurrentEpochGuid() : () -> i64
        %10 = func.call @artsGetCurrentNode() : () -> i32
        memref.store %c0, %alloca_4[] : memref<index>
        memref.store %c0, %alloca_5[] : memref<index>
        %11 = memref.load %alloca_5[] : memref<index>
        %12 = arith.addi %11, %c1 : index
        memref.store %12, %alloca_5[] : memref<index>
        %13 = memref.load %alloca_4[] : memref<index>
        %14 = arith.addi %13, %c1 : index
        memref.store %14, %alloca_4[] : memref<index>
        %15 = memref.load %alloca_5[] : memref<index>
        %16 = arith.index_cast %15 : index to i32
        memref.store %c3, %alloca_6[] : memref<index>
        %17 = memref.load %alloca_6[] : memref<index>
        %18 = memref.load %alloca_4[] : memref<index>
        %19 = arith.addi %17, %18 : index
        memref.store %19, %alloca_6[] : memref<index>
        %20 = memref.load %alloca_6[] : memref<index>
        %21 = arith.index_cast %20 : index to i32
        %alloca_14 = memref.alloca(%20) : memref<?xi64>
        %22 = arith.extsi %7 : i32 to i64
        memref.store %22, %alloca_14[%c0] : memref<?xi64>
        %23 = arith.extsi %1 : i32 to i64
        memref.store %23, %alloca_14[%c1] : memref<?xi64>
        %24 = arith.extsi %8 : i32 to i64
        memref.store %24, %alloca_14[%c2] : memref<?xi64>
        memref.store %c3, %alloca_7[] : memref<index>
        %25 = memref.load %alloca_3[%arg4, %arg5] : memref<?x?xi64>
        %26 = memref.load %alloca_7[] : memref<index>
        memref.store %25, %alloca_14[%26] : memref<?xi64>
        %27 = arith.addi %26, %c1 : index
        memref.store %27, %alloca_7[] : memref<index>
        %28 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %30 = func.call @artsEdtCreateWithEpoch(%29, %10, %21, %alloca_14, %16, %9) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
        %31 = memref.load %alloca_3[%arg4, %arg5] : memref<?x?xi64>
        %32 = memref.load %alloca_0[%arg4, %arg5] : memref<?x?xi64>
        %33 = arith.index_cast %11 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%31, %30, %33, %32) : (i64, i64, i32, i64) -> ()
        %34 = memref.load %alloca_3[%arg4, %arg5] : memref<?x?xi64>
        %35 = memref.load %alloca_0[%arg4, %arg5] : memref<?x?xi64>
        func.call @artsPersistentEventIncrementLatch(%34, %35) : (i64, i64) -> ()
      }
    }
    %alloca_8 = memref.alloca() : memref<index>
    %alloca_9 = memref.alloca() : memref<index>
    %alloca_10 = memref.alloca() : memref<index>
    %alloca_11 = memref.alloca() : memref<index>
    scf.for %arg4 = %c0 to %3 step %c1 {
      %7 = arith.index_cast %arg4 : index to i32
      %8 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %9 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0, %alloca_8[] : memref<index>
      memref.store %c0, %alloca_9[] : memref<index>
      %10 = memref.load %alloca_9[] : memref<index>
      %11 = arith.addi %10, %c1 : index
      memref.store %11, %alloca_9[] : memref<index>
      %12 = memref.load %alloca_9[] : memref<index>
      %13 = arith.addi %12, %c1 : index
      memref.store %13, %alloca_9[] : memref<index>
      %14 = memref.load %alloca_8[] : memref<index>
      %15 = arith.addi %14, %c1 : index
      memref.store %15, %alloca_8[] : memref<index>
      %16 = memref.load %alloca_9[] : memref<index>
      %17 = arith.index_cast %16 : index to i32
      memref.store %c1, %alloca_10[] : memref<index>
      %18 = memref.load %alloca_10[] : memref<index>
      %19 = memref.load %alloca_8[] : memref<index>
      %20 = arith.addi %18, %19 : index
      memref.store %20, %alloca_10[] : memref<index>
      %21 = memref.load %alloca_10[] : memref<index>
      %22 = arith.index_cast %21 : index to i32
      %alloca_14 = memref.alloca(%21) : memref<?xi64>
      %23 = arith.extsi %7 : i32 to i64
      memref.store %23, %alloca_14[%c0] : memref<?xi64>
      memref.store %c1, %alloca_11[] : memref<index>
      %24 = memref.load %alloca_2[%c0, %arg4] : memref<?x?xi64>
      %25 = memref.load %alloca_11[] : memref<index>
      memref.store %24, %alloca_14[%25] : memref<?xi64>
      %26 = arith.addi %25, %c1 : index
      memref.store %26, %alloca_11[] : memref<index>
      %27 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %28 = "polygeist.pointer2memref"(%27) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %29 = func.call @artsEdtCreateWithEpoch(%28, %9, %22, %alloca_14, %17, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %30 = memref.load %alloca_3[%c0, %arg4] : memref<?x?xi64>
      %31 = memref.load %alloca_0[%c0, %arg4] : memref<?x?xi64>
      %32 = arith.index_cast %10 : index to i32
      func.call @artsAddDependenceToPersistentEvent(%30, %29, %32, %31) : (i64, i64, i32, i64) -> ()
      %33 = memref.load %alloca_2[%c0, %arg4] : memref<?x?xi64>
      %34 = memref.load %alloca_1[%c0, %arg4] : memref<?x?xi64>
      %35 = arith.index_cast %12 : index to i32
      func.call @artsAddDependenceToPersistentEvent(%33, %29, %35, %34) : (i64, i64, i32, i64) -> ()
      %36 = memref.load %alloca_2[%c0, %arg4] : memref<?x?xi64>
      %37 = memref.load %alloca_1[%c0, %arg4] : memref<?x?xi64>
      func.call @artsPersistentEventIncrementLatch(%36, %37) : (i64, i64) -> ()
    }
    %alloca_12 = memref.alloca() : memref<index>
    %alloca_13 = memref.alloca() : memref<index>
    scf.for %arg4 = %c1 to %3 step %c1 {
      %7 = arith.index_cast %arg4 : index to i32
      scf.for %arg5 = %c0 to %3 step %c1 {
        %8 = arith.index_cast %arg5 : index to i32
        %9 = arith.addi %7, %c-1_i32 : i32
        %10 = arith.index_cast %9 : i32 to index
        %11 = func.call @artsGetCurrentEpochGuid() : () -> i64
        %12 = func.call @artsGetCurrentNode() : () -> i32
        memref.store %c0, %alloca_12[] : memref<index>
        %13 = memref.load %alloca_12[] : memref<index>
        %14 = arith.addi %13, %c1 : index
        memref.store %14, %alloca_12[] : memref<index>
        %15 = memref.load %alloca_12[] : memref<index>
        %16 = arith.addi %15, %c1 : index
        memref.store %16, %alloca_12[] : memref<index>
        %17 = memref.load %alloca_12[] : memref<index>
        %18 = arith.addi %17, %c1 : index
        memref.store %18, %alloca_12[] : memref<index>
        %19 = memref.load %alloca_12[] : memref<index>
        %20 = arith.index_cast %19 : index to i32
        memref.store %c2, %alloca_13[] : memref<index>
        %21 = memref.load %alloca_13[] : memref<index>
        %22 = arith.index_cast %21 : index to i32
        %alloca_14 = memref.alloca(%21) : memref<?xi64>
        %23 = arith.extsi %7 : i32 to i64
        memref.store %23, %alloca_14[%c0] : memref<?xi64>
        %24 = arith.extsi %8 : i32 to i64
        memref.store %24, %alloca_14[%c1] : memref<?xi64>
        %25 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %26 = "polygeist.pointer2memref"(%25) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
        %27 = func.call @artsEdtCreateWithEpoch(%26, %12, %22, %alloca_14, %20, %11) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
        %28 = memref.load %alloca_3[%10, %arg5] : memref<?x?xi64>
        %29 = memref.load %alloca_0[%10, %arg5] : memref<?x?xi64>
        %30 = arith.index_cast %13 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%28, %27, %30, %29) : (i64, i64, i32, i64) -> ()
        %31 = memref.load %alloca_3[%arg4, %arg5] : memref<?x?xi64>
        %32 = memref.load %alloca_0[%arg4, %arg5] : memref<?x?xi64>
        %33 = arith.index_cast %15 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%31, %27, %33, %32) : (i64, i64, i32, i64) -> ()
        %34 = memref.load %alloca_2[%arg4, %arg5] : memref<?x?xi64>
        %35 = memref.load %alloca_1[%arg4, %arg5] : memref<?x?xi64>
        %36 = arith.index_cast %17 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%34, %27, %36, %35) : (i64, i64, i32, i64) -> ()
      }
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.trunci %2 : i64 to i32
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.trunci %4 : i64 to i32
    %6 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %8 = llvm.load %7 : !llvm.ptr -> i64
    %9 = llvm.getelementptr %6[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
    %11 = arith.sitofp %1 : i32 to f64
    %12 = arith.sitofp %3 : i32 to f64
    %13 = arith.addf %11, %12 : f64
    %14 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %13, %14 : f64, !llvm.ptr
    %15 = llvm.mlir.addressof @str2 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<39 x i8>
    %17 = llvm.call @printf(%16, %1, %5, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
    %18 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %19 = llvm.getelementptr %18[3] : (!llvm.ptr) -> !llvm.ptr, i64
    %20 = llvm.load %19 : !llvm.ptr -> i64
    call @artsPersistentEventDecrementLatch(%20, %8) : (i64, i64) -> ()
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %3 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> memref<?xi8>
    %5 = llvm.getelementptr %2[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> i64
    %8 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %9 = llvm.load %8 : !llvm.ptr -> memref<?xi8>
    %10 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %11 = llvm.load %10 : !llvm.ptr -> f64
    %12 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %11, %12 : f64, !llvm.ptr
    %13 = llvm.mlir.addressof @str3 : !llvm.ptr
    %14 = llvm.getelementptr %13[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %15 = llvm.call @printf(%14, %1, %11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    %16 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %17 = llvm.getelementptr %16[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %18 = llvm.load %17 : !llvm.ptr -> i64
    call @artsPersistentEventDecrementLatch(%18, %7) : (i64, i64) -> ()
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.trunci %2 : i64 to i32
    %4 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %6 = llvm.load %5 : !llvm.ptr -> memref<?xi8>
    %7 = llvm.getelementptr %4[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %8 = llvm.getelementptr %7[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %9 = llvm.load %8 : !llvm.ptr -> memref<?xi8>
    %10 = llvm.getelementptr %4[48] : (!llvm.ptr) -> !llvm.ptr, i8
    %11 = llvm.getelementptr %10[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %12 = llvm.load %11 : !llvm.ptr -> memref<?xi8>
    %13 = "polygeist.memref2pointer"(%6) : (memref<?xi8>) -> !llvm.ptr
    %14 = llvm.load %13 : !llvm.ptr -> f64
    %15 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %16 = llvm.load %15 : !llvm.ptr -> f64
    %17 = arith.addf %16, %14 : f64
    %18 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %17, %18 : f64, !llvm.ptr
    %19 = llvm.mlir.addressof @str4 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %21 = llvm.call @printf(%20, %1, %3, %17) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
    return
  }
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c6_i32 = arith.constant 6 : i32
    %c0_i32 = arith.constant 0 : i32
    %c8_i64 = arith.constant 8 : i64
    %c5 = arith.constant 5 : index
    %c4 = arith.constant 4 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c9_i32 = arith.constant 9 : i32
    %c0_i64 = arith.constant 0 : i64
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c11_i32 = arith.constant 11 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = arith.cmpi slt, %arg0, %c2_i32 : i32
    scf.if %0 {
      %41 = llvm.mlir.addressof @stderr : !llvm.ptr
      %42 = llvm.load %41 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %43 = "polygeist.memref2pointer"(%42) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %44 = llvm.mlir.addressof @str0 : !llvm.ptr
      %45 = llvm.getelementptr %44[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %46 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
      %47 = "polygeist.memref2pointer"(%46) : (memref<?xi8>) -> !llvm.ptr
      %48 = llvm.call @fprintf(%43, %45, %47) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
      func.call @exit(%c1_i32) : (i32) -> ()
    }
    %1 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
    %2 = call @atoi(%1) : (memref<?xi8>) -> i32
    %3 = arith.index_cast %2 : i32 to index
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = func.call @artsPersistentEventCreate(%4, %c0_i32, %c0_i64) : (i32, i32, i64) -> i64
        memref.store %41, %alloca[%arg2, %arg3] : memref<?x?xi64>
      }
    }
    %5 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca(%3, %3) : memref<?x?xi64>
    %alloca_1 = memref.alloca(%3, %3) : memref<?x?xmemref<?xi8>>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = func.call @artsReserveGuidRoute(%c9_i32, %5) : (i32, i32) -> i64
        %42 = func.call @artsDbCreateWithGuid(%41, %c8_i64) : (i64, i64) -> memref<?xi8>
        memref.store %41, %alloca_0[%arg2, %arg3] : memref<?x?xi64>
        memref.store %42, %alloca_1[%arg2, %arg3] : memref<?x?xmemref<?xi8>>
      }
    }
    %6 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca(%3, %3) : memref<?x?xi64>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = func.call @artsPersistentEventCreate(%6, %c0_i32, %c0_i64) : (i32, i32, i64) -> i64
        memref.store %41, %alloca_2[%arg2, %arg3] : memref<?x?xi64>
      }
    }
    %7 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca(%3, %3) : memref<?x?xi64>
    %alloca_4 = memref.alloca(%3, %3) : memref<?x?xmemref<?xi8>>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = func.call @artsReserveGuidRoute(%c9_i32, %7) : (i32, i32) -> i64
        %42 = func.call @artsDbCreateWithGuid(%41, %c8_i64) : (i64, i64) -> memref<?xi8>
        memref.store %41, %alloca_3[%arg2, %arg3] : memref<?x?xi64>
        memref.store %42, %alloca_4[%arg2, %arg3] : memref<?x?xmemref<?xi8>>
      }
    }
    %8 = llvm.mlir.zero : !llvm.ptr
    %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
    %10 = call @time(%9) : (memref<?xi64>) -> i64
    %11 = arith.trunci %10 : i64 to i32
    call @srand(%11) : (i32) -> ()
    %12 = call @rand() : () -> i32
    %13 = arith.remsi %12, %c11_i32 : i32
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<19 x i8>
    %16 = llvm.call @printf(%15, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %17 = call @artsGetCurrentNode() : () -> i32
    %alloca_5 = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca_5 : memref<0xi64> to memref<?xi64>
    %18 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %19 = "polygeist.pointer2memref"(%18) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %20 = call @artsEdtCreate(%19, %17, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %21 = call @artsInitializeAndStartEpoch(%20, %c0_i32) : (i64, i32) -> i64
    %22 = call @artsGetCurrentNode() : () -> i32
    %23 = arith.muli %3, %3 : index
    %24 = arith.addi %23, %23 : index
    %25 = arith.index_cast %24 : index to i32
    %alloca_6 = memref.alloca() : memref<6xi64>
    %cast_7 = memref.cast %alloca_6 : memref<6xi64> to memref<?xi64>
    %26 = arith.index_cast %3 : index to i64
    memref.store %26, %alloca_6[%c0] : memref<6xi64>
    %27 = arith.extsi %13 : i32 to i64
    memref.store %27, %alloca_6[%c1] : memref<6xi64>
    memref.store %26, %alloca_6[%c2] : memref<6xi64>
    memref.store %26, %alloca_6[%c3] : memref<6xi64>
    memref.store %26, %alloca_6[%c4] : memref<6xi64>
    memref.store %26, %alloca_6[%c5] : memref<6xi64>
    %28 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %30 = call @artsEdtCreateWithEpoch(%29, %22, %c6_i32, %cast_7, %25, %21) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloc = memref.alloc() : memref<index>
    memref.store %c0, %alloc[] : memref<index>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = memref.load %alloca[%arg2, %arg3] : memref<?x?xi64>
        %42 = memref.load %alloc[] : memref<index>
        %43 = memref.load %alloca_0[%arg2, %arg3] : memref<?x?xi64>
        %44 = arith.index_cast %42 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%41, %30, %44, %43) : (i64, i64, i32, i64) -> ()
        %45 = arith.addi %42, %c1 : index
        memref.store %45, %alloc[] : memref<index>
      }
    }
    %alloc_8 = memref.alloc() : memref<index>
    memref.store %23, %alloc_8[] : memref<index>
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %41 = memref.load %alloca_2[%arg2, %arg3] : memref<?x?xi64>
        %42 = memref.load %alloc_8[] : memref<index>
        %43 = memref.load %alloca_3[%arg2, %arg3] : memref<?x?xi64>
        %44 = arith.index_cast %42 : index to i32
        func.call @artsAddDependenceToPersistentEvent(%41, %30, %44, %43) : (i64, i64, i32, i64) -> ()
        %45 = arith.addi %42, %c1 : index
        memref.store %45, %alloc_8[] : memref<index>
      }
    }
    %31 = call @artsWaitOnHandle(%21) : (i64) -> i1
    %32 = llvm.mlir.addressof @str5 : !llvm.ptr
    %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %34 = llvm.call @printf(%33) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %44 = llvm.mlir.addressof @str6 : !llvm.ptr
        %45 = llvm.getelementptr %44[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %46 = polygeist.subindex %alloca_1[%arg2] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %47 = "polygeist.typeSize"() <{source = memref<?xi8>}> : () -> index
        %48 = arith.muli %arg3, %47 : index
        %49 = arith.index_cast %48 : index to i64
        %50 = "polygeist.memref2pointer"(%46) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %51 = llvm.getelementptr %50[%49] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %52 = llvm.load %51 : !llvm.ptr -> !llvm.ptr
        %53 = llvm.load %52 : !llvm.ptr -> f64
        %54 = llvm.call @printf(%45, %53) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %41 = llvm.mlir.addressof @str7 : !llvm.ptr
      %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %43 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %35 = llvm.mlir.addressof @str8 : !llvm.ptr
    %36 = llvm.getelementptr %35[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %37 = llvm.call @printf(%36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %3 step %c1 {
      scf.for %arg3 = %c0 to %3 step %c1 {
        %44 = llvm.mlir.addressof @str6 : !llvm.ptr
        %45 = llvm.getelementptr %44[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %46 = polygeist.subindex %alloca_4[%arg2] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %47 = "polygeist.typeSize"() <{source = memref<?xi8>}> : () -> index
        %48 = arith.muli %arg3, %47 : index
        %49 = arith.index_cast %48 : index to i64
        %50 = "polygeist.memref2pointer"(%46) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %51 = llvm.getelementptr %50[%49] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %52 = llvm.load %51 : !llvm.ptr -> !llvm.ptr
        %53 = llvm.load %52 : !llvm.ptr -> f64
        %54 = llvm.call @printf(%45, %53) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %41 = llvm.mlir.addressof @str7 : !llvm.ptr
      %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %43 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %38:3 = scf.while (%arg2 = %c0_i32, %arg3 = %c1_i32) : (i32, i32) -> (i32, i32, i32) {
      %41 = arith.cmpi slt, %arg2, %2 : i32
      %42:3 = scf.if %41 -> (i32, i32, i32) {
        %43 = arith.index_cast %arg2 : i32 to index
        %44 = polygeist.subindex %alloca_4[%c0] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %45 = "polygeist.typeSize"() <{source = memref<?xi8>}> : () -> index
        %46 = arith.muli %43, %45 : index
        %47 = arith.index_cast %46 : index to i64
        %48 = "polygeist.memref2pointer"(%44) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %49 = llvm.getelementptr %48[%47] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %50 = llvm.load %49 : !llvm.ptr -> !llvm.ptr
        %51 = llvm.load %50 : !llvm.ptr -> f64
        %52 = polygeist.subindex %alloca_1[%c0] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %53 = "polygeist.memref2pointer"(%52) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %54 = llvm.getelementptr %53[%47] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %55 = llvm.load %54 : !llvm.ptr -> !llvm.ptr
        %56 = llvm.load %55 : !llvm.ptr -> f64
        %57 = arith.cmpf une, %51, %56 : f64
        %58 = arith.select %57, %c0_i32, %arg3 : i32
        %59 = arith.addi %arg2, %c1_i32 : i32
        %60 = llvm.mlir.undef : i32
        scf.yield %59, %58, %60 : i32, i32, i32
      } else {
        scf.yield %arg2, %arg3, %arg3 : i32, i32, i32
      }
      scf.condition(%41) %42#0, %42#1, %42#2 : i32, i32, i32
    } do {
    ^bb0(%arg2: i32, %arg3: i32, %arg4: i32):
      scf.yield %arg2, %arg3 : i32, i32
    }
    %39 = scf.for %arg2 = %c1 to %3 step %c1 iter_args(%arg3 = %38#2) -> (i32) {
      %41 = arith.index_cast %arg2 : index to i32
      %42 = scf.for %arg4 = %c0 to %3 step %c1 iter_args(%arg5 = %arg3) -> (i32) {
        %43 = polygeist.subindex %alloca_4[%arg2] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %44 = "polygeist.typeSize"() <{source = memref<?xi8>}> : () -> index
        %45 = arith.muli %arg4, %44 : index
        %46 = arith.index_cast %45 : index to i64
        %47 = "polygeist.memref2pointer"(%43) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %48 = llvm.getelementptr %47[%46] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %49 = llvm.load %48 : !llvm.ptr -> !llvm.ptr
        %50 = llvm.load %49 : !llvm.ptr -> f64
        %51 = polygeist.subindex %alloca_1[%arg2] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %52 = "polygeist.memref2pointer"(%51) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %53 = llvm.getelementptr %52[%46] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %54 = llvm.load %53 : !llvm.ptr -> !llvm.ptr
        %55 = llvm.load %54 : !llvm.ptr -> f64
        %56 = arith.addi %41, %c-1_i32 : i32
        %57 = arith.index_cast %56 : i32 to index
        %58 = polygeist.subindex %alloca_1[%57] () : memref<?x?xmemref<?xi8>> -> memref<?xmemref<?xi8>>
        %59 = "polygeist.memref2pointer"(%58) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %60 = llvm.getelementptr %59[%46] : (!llvm.ptr, i64) -> !llvm.ptr, i8
        %61 = llvm.load %60 : !llvm.ptr -> !llvm.ptr
        %62 = llvm.load %61 : !llvm.ptr -> f64
        %63 = arith.addf %55, %62 : f64
        %64 = arith.cmpf une, %50, %63 : f64
        %65 = arith.select %64, %c0_i32, %arg5 : i32
        scf.yield %65 : i32
      }
      scf.yield %42 : i32
    }
    %40 = arith.cmpi ne, %39, %c0_i32 : i32
    scf.if %40 {
      %41 = llvm.mlir.addressof @str9 : !llvm.ptr
      %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %43 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    } else {
      %41 = llvm.mlir.addressof @str10 : !llvm.ptr
      %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %43 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    return
  }
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    %c1 = arith.constant 1 : index
    %0 = arith.index_cast %arg0 : i32 to index
    %1 = arith.cmpi uge, %0, %c1 : index
    cf.cond_br %1, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    return
  ^bb2:  // pred: ^bb0
    %2 = call @mainBody(%arg1, %arg2) : (i32, memref<?xmemref<?xi8>>) -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
}
