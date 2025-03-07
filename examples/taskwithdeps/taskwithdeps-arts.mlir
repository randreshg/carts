module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuidAndData(i64, memref<?xi8>, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task 1: Initializing a\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %1 = call @artsEventCreate(%0, %c1_i32) : (i32, i32) -> i64
    %2 = call @artsGetCurrentEpochGuid() : () -> i64
    %3 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<1xi64>
    %cast = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    affine.store %1, %alloca[0] : memref<1xi64>
    %4 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = call @artsEdtCreateWithEpoch(%5, %3, %c1_i32, %cast, %c1_i32, %2) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %7 = call @artsGetCurrentNode() : () -> i32
    %8 = call @artsEventCreate(%7, %c1_i32) : (i32, i32) -> i64
    %9 = call @artsGetCurrentEpochGuid() : () -> i64
    %10 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<1xi64>
    %cast_1 = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    affine.store %8, %alloca_0[0] : memref<1xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %10, %c1_i32, %cast_1, %c2_i32, %9) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%1, %13, %c1_i32) : (i64, i64, i32) -> ()
    %14 = call @artsGetCurrentEpochGuid() : () -> i64
    %15 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca() : memref<0xi64>
    %cast_3 = memref.cast %alloca_2 : memref<0xi64> to memref<?xi64>
    %16 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %17 = "polygeist.pointer2memref"(%16) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %18 = call @artsEdtCreateWithEpoch(%17, %15, %c0_i32, %cast_3, %c1_i32, %14) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%8, %18, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c10_i32 = arith.constant 10 : i32
    %0 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %2 = llvm.load %1 : !llvm.ptr -> i64
    %3 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %6 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.store %c10_i32, %3 : i32, !llvm.ptr
    %7 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %8 = llvm.load %7 : !llvm.ptr -> i64
    call @artsEventSatisfySlot(%8, %2, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c5_i32 = arith.constant 5 : i32
    %0 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %2 = llvm.load %1 : !llvm.ptr -> i64
    %3 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %4 = llvm.getelementptr %0[24] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %6 = llvm.load %5 : !llvm.ptr -> i32
    %7 = llvm.mlir.addressof @str1 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %9 = llvm.call @printf(%8, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %10 = arith.addi %6, %c5_i32 : i32
    llvm.store %10, %3 : i32, !llvm.ptr
    %11 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %12 = llvm.load %11 : !llvm.ptr -> i64
    call @artsEventSatisfySlot(%12, %2, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %0 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %1 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %2 = llvm.load %1 : !llvm.ptr -> i32
    %3 = llvm.mlir.addressof @str2 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %5 = llvm.call @printf(%4, %2) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    return
  }
  func.func @mainBody() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2_i32 = arith.constant 2 : i32
    %c1_i32 = arith.constant 1 : i32
    %c9_i32 = arith.constant 9 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %1 = call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
    %2 = call @artsGetCurrentNode() : () -> i32
    %3 = call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<0xi64>
    %cast_1 = memref.cast %alloca_0 : memref<0xi64> to memref<?xi64>
    %10 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = call @artsEdtCreateWithEpoch(%11, %9, %c0_i32, %cast_1, %c2_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalEdt(%12, %c0_i32, %1) : (i64, i32, i64) -> ()
    call @artsSignalEdt(%12, %c1_i32, %3) : (i64, i32, i64) -> ()
    %13 = call @artsWaitOnHandle(%8) : (i64) -> i1
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
    %2 = call @mainBody() : () -> i32
    return
  ^bb2:  // pred: ^bb0
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
}

