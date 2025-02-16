--- ConvertArtsToFuncsPass START ---
[convert-arts-to-funcs] Iterate over all the functions
[convert-arts-to-funcs] Lowering arts.datablock
[convert-arts-to-funcs] Lowering arts.datablock
[convert-arts-to-funcs] Lowering arts.edt parallel
-----------------------------------------
-----------------------------------------
[arts-codegen] Rewiring array datablock: %15 = "arts.datablock"(%8, %1, %3, %2) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<?xmemref<?xi8>>
[arts-codegen] Rewiring array datablock: %24 = "arts.datablock"(%17, %1, %3, %2) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<?xmemref<?xi8>>
[convert-arts-to-funcs] Parallel op lowered

[convert-arts-to-funcs] Skipping arts.event: %2 = arts.event[%c100] -> : memref<100xi64>
[convert-arts-to-funcs] Lowering arts.datablock
[arts-codegen] Base is a datablock: %5 = arts.datablock "out", %alloca_10 : memref<?xmemref<?xi8>>[%arg4] [%c1] [f64, %c8] -> memref<?xi8> {ptrIsDb, isLoad}
[convert-arts-to-funcs] Lowering arts.edt
-----------------------------------------
[arts-codegen] Rewiring single datablock: %26 = "arts.datablock"(%20, %1, %arg4, %2) <{elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[convert-arts-to-funcs] Lowering arts.datablock
[arts-codegen] Base is a datablock: %45 = "arts.datablock"(%20, %1, %arg4, %2) <{elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[convert-arts-to-funcs] Lowering arts.datablock
[arts-codegen] Base is a datablock: %49 = "arts.datablock"(%20, %1, %48, %2) <{elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[convert-arts-to-funcs] Lowering arts.datablock
[arts-codegen] Base is a datablock: %51 = "arts.datablock"(%15, %1, %arg4, %2) <{elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[convert-arts-to-funcs] Lowering arts.edt
-----------------------------------------
[arts-codegen] Rewiring single datablock: %49 = "arts.datablock"(%20, %1, %48, %2) <{elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[arts-codegen] Rewiring single datablock: %51 = "arts.datablock"(%15, %1, %arg4, %2) <{elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[arts-codegen] Rewiring single datablock: %45 = "arts.datablock"(%20, %1, %arg4, %2) <{elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {ptrIsDb, isLoad} : (memref<?xmemref<?xi8>>, index, index, index) -> memref<?xi8>
[convert-arts-to-funcs] Skipping arts.event: %23 = "arts.event"(%0) : (index) -> memref<100xi64>
=== ConvertArtsToFuncsPass COMPLETE ===
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetPtrFromEdtDep(!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetGuidFromEdtDep(!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str1("B[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c100 = arith.constant 100 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %c9_i32 = arith.constant 9 : i32
    %alloca = memref.alloca(%c100) : memref<?xi64>
    %alloca_0 = memref.alloca(%c100) : memref<?xmemref<?xi8>>
    %c0_1 = arith.constant 0 : index
    %c1_2 = arith.constant 1 : index
    scf.for %arg0 = %c0_1 to %c100 step %c1_2 {
      %21 = func.call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
      %22 = arith.index_cast %c8 : index to i64
      %23 = func.call @artsDbCreateWithGuid(%21, %22) : (i64, i64) -> memref<?xi8>
      memref.store %21, %alloca[%arg0] : memref<?xi64>
      memref.store %23, %alloca_0[%arg0] : memref<?xmemref<?xi8>>
    }
    %1 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %c9_i32_3 = arith.constant 9 : i32
    %alloca_4 = memref.alloca(%c100) : memref<?xi64>
    %alloca_5 = memref.alloca(%c100) : memref<?xmemref<?xi8>>
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    scf.for %arg0 = %c0_6 to %c100 step %c1_7 {
      %21 = func.call @artsReserveGuidRoute(%c9_i32_3, %1) : (i32, i32) -> i64
      %22 = arith.index_cast %c8 : index to i64
      %23 = func.call @artsDbCreateWithGuid(%21, %22) : (i64, i64) -> memref<?xi8>
      memref.store %21, %alloca_4[%arg0] : memref<?xi64>
      memref.store %23, %alloca_5[%arg0] : memref<?xmemref<?xi8>>
    }
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    %4 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %c0_i32_8 = arith.constant 0 : i32
    %alloca_9 = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca_9 : memref<0xi64> to memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32_8, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_10 = arith.constant 0 : i32
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32_10) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %c0_11 = arith.constant 0 : index
    %c0_12 = arith.constant 0 : index
    %10 = arith.addi %c0_11, %c0_12 : index
    %c0_13 = arith.constant 0 : index
    %11 = arith.addi %10, %c0_13 : index
    %12 = arith.index_cast %11 : index to i32
    %c1_i32_14 = arith.constant 1 : i32
    %alloca_15 = memref.alloca() : memref<1xi64>
    %13 = arith.extsi %3 : i32 to i64
    %c0_16 = arith.constant 0 : index
    affine.store %13, %alloca_15[%c0_16] : memref<1xi64>
    %cast_17 = memref.cast %alloca_15 : memref<1xi64> to memref<?xi64>
    %14 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %16 = call @artsEdtCreateWithEpoch(%15, %9, %c1_i32_14, %cast_17, %12, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %17 = llvm.mlir.addressof @str0 : !llvm.ptr
    %18 = llvm.getelementptr %17[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %19 = llvm.mlir.addressof @str1 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %21 = arith.index_cast %arg0 : index to i32
      %22 = arith.index_cast %arg0 : index to i64
      %23 = arith.index_cast %arg0 : index to i64
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c100 = arith.constant 100 : index
    %c8 = arith.constant 8 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    %c0_1 = arith.constant 0 : index
    memref.store %c0_1, %alloca[] : memref<index>
    %c1_2 = arith.constant 1 : index
    %c100_3 = arith.constant 100 : index
    %alloca_4 = memref.alloca(%c100_3) : memref<?xi64>
    %c0_5 = arith.constant 0 : index
    %c1_6 = arith.constant 1 : index
    scf.for %arg4 = %c0_5 to %c100_3 step %c1_6 {
      %4 = memref.load %alloca[] : memref<index>
      %5 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
      %6 = func.call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
      %7 = func.call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
      memref.store %6, %alloca_4[%arg4] : memref<?xi64>
      %8 = arith.addi %4, %c1_2 : index
      memref.store %8, %alloca[] : memref<index>
    }
    %c100_7 = arith.constant 100 : index
    %alloca_8 = memref.alloca(%c100_7) : memref<?xi64>
    %c0_9 = arith.constant 0 : index
    %c1_10 = arith.constant 1 : index
    scf.for %arg4 = %c0_9 to %c100_7 step %c1_10 {
      %4 = memref.load %alloca[] : memref<index>
      %5 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
      %6 = func.call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
      %7 = func.call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
      memref.store %6, %alloca_8[%arg4] : memref<?xi64>
      %8 = arith.addi %4, %c1_2 : index
      memref.store %8, %alloca[] : memref<index>
    }
    %2 = arts.event[%c100] -> : memref<100xi64>
    %3 = arith.sitofp %1 : i32 to f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %4 = arith.index_cast %arg4 : index to i32
      %5 = memref.load %2[%arg4] : memref<100xi64>
      %6 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %7 = func.call @artsGetCurrentNode() {llvm.readnone} : () -> i32
      %c0_11 = arith.constant 0 : index
      %c0_12 = arith.constant 0 : index
      %8 = arith.addi %c0_11, %c0_12 : index
      %9 = arith.index_cast %8 : index to i32
      %c2_i32 = arith.constant 2 : i32
      %alloca_13 = memref.alloca() : memref<2xi64>
      %10 = arith.extsi %4 : i32 to i64
      %c0_14 = arith.constant 0 : index
      affine.store %10, %alloca_13[%c0_14] : memref<2xi64>
      %11 = arith.fptosi %3 : f64 to i64
      %c1_15 = arith.constant 1 : index
      affine.store %11, %alloca_13[%c1_15] : memref<2xi64>
      %cast = memref.cast %alloca_13 : memref<2xi64> to memref<?xi64>
      %12 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %14 = func.call @artsEdtCreateWithEpoch(%13, %7, %c2_i32, %cast, %9, %6) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %15 = arith.addi %4, %c-1_i32 : i32
      %16 = arith.index_cast %15 : i32 to index
      %17 = memref.load %2[%16] : memref<100xi64>
      %18 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %19 = func.call @artsGetCurrentNode() {llvm.readnone} : () -> i32
      %c0_16 = arith.constant 0 : index
      %c0_17 = arith.constant 0 : index
      %20 = arith.addi %c0_16, %c0_17 : index
      %c0_18 = arith.constant 0 : index
      %21 = arith.addi %20, %c0_18 : index
      %c0_19 = arith.constant 0 : index
      %22 = arith.addi %21, %c0_19 : index
      %23 = arith.index_cast %22 : index to i32
      %c1_i32 = arith.constant 1 : i32
      %alloca_20 = memref.alloca() : memref<1xi64>
      %24 = arith.extsi %4 : i32 to i64
      %c0_21 = arith.constant 0 : index
      affine.store %24, %alloca_20[%c0_21] : memref<1xi64>
      %cast_22 = memref.cast %alloca_20 : memref<1xi64> to memref<?xi64>
      %25 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %26 = "polygeist.pointer2memref"(%25) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %27 = func.call @artsEdtCreateWithEpoch(%26, %19, %c1_i32, %cast_22, %23, %18) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1_1 = arith.constant 1 : index
    %4 = memref.load %alloca[] : memref<index>
    %5 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %6 = call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %7 = call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %8 = arith.addi %4, %c1_1 : index
    memref.store %8, %alloca[] : memref<index>
    %9 = arith.sitofp %1 : i32 to f64
    %10 = arith.addf %9, %3 : f64
    %11 = "polygeist.memref2pointer"(%7) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %10, %11 : f64, !llvm.ptr
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1 = arith.constant 1 : index
    %2 = memref.load %alloca[] : memref<index>
    %3 = memref.load %arg3[%2] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %4 = call @artsGetGuidFromEdtDep(%3) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %5 = call @artsGetPtrFromEdtDep(%3) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %6 = arith.addi %2, %c1 : index
    memref.store %6, %alloca[] : memref<index>
    %7 = memref.load %alloca[] : memref<index>
    %8 = memref.load %arg3[%7] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %9 = call @artsGetGuidFromEdtDep(%8) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %10 = call @artsGetPtrFromEdtDep(%8) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %11 = arith.addi %7, %c1 : index
    memref.store %11, %alloca[] : memref<index>
    %12 = memref.load %alloca[] : memref<index>
    %13 = memref.load %arg3[%12] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %14 = call @artsGetGuidFromEdtDep(%13) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %15 = call @artsGetPtrFromEdtDep(%13) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %16 = arith.addi %12, %c1 : index
    memref.store %16, %alloca[] : memref<index>
    %17 = "polygeist.memref2pointer"(%15) : (memref<?xi8>) -> !llvm.ptr
    %18 = llvm.load %17 : !llvm.ptr -> f64
    %19 = "polygeist.memref2pointer"(%5) : (memref<?xi8>) -> !llvm.ptr
    %20 = llvm.load %19 : !llvm.ptr -> f64
    %21 = arith.cmpi sgt, %1, %c0_i32 : i32
    %22 = arith.select %21, %20, %cst : f64
    %23 = arith.addf %18, %22 : f64
    %24 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %23, %24 : f64, !llvm.ptr
    return
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetPtrFromEdtDep(!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetGuidFromEdtDep(!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str1("B[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c8_i64 = arith.constant 8 : i64
    %c1_i32 = arith.constant 1 : i32
    %c9_i32 = arith.constant 9 : i32
    %c100 = arith.constant 100 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %0 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %14 = func.call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
      %15 = func.call @artsDbCreateWithGuid(%14, %c8_i64) : (i64, i64) -> memref<?xi8>
    }
    %1 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %14 = func.call @artsReserveGuidRoute(%c9_i32, %1) : (i32, i32) -> i64
      %15 = func.call @artsDbCreateWithGuid(%14, %c8_i64) : (i64, i64) -> memref<?xi8>
    }
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    %4 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %alloca = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() {llvm.readnone} : () -> i32
    %alloca_0 = memref.alloca() : memref<1xi64>
    %10 = arith.extsi %3 : i32 to i64
    affine.store %10, %alloca_0[0] : memref<1xi64>
    %cast_1 = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %9, %c1_i32, %cast_1, %c0_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %4 = memref.load %alloca[] : memref<index>
      %5 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
      %6 = func.call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
      %7 = func.call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
      %8 = arith.addi %4, %c1 : index
      memref.store %8, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %4 = memref.load %alloca[] : memref<index>
      %5 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
      %6 = func.call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
      %7 = func.call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
      %8 = arith.addi %4, %c1 : index
      memref.store %8, %alloca[] : memref<index>
    }
    %2 = arts.event[%c100] -> : memref<100xi64>
    %3 = arith.sitofp %1 : i32 to f64
    %alloca_0 = memref.alloca() : memref<2xi64>
    %alloca_1 = memref.alloca() : memref<1xi64>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %4 = arith.index_cast %arg4 : index to i32
      %5 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %6 = func.call @artsGetCurrentNode() {llvm.readnone} : () -> i32
      %7 = arith.extsi %4 : i32 to i64
      affine.store %7, %alloca_0[0] : memref<2xi64>
      %8 = arith.fptosi %3 : f64 to i64
      affine.store %8, %alloca_0[1] : memref<2xi64>
      %cast = memref.cast %alloca_0 : memref<2xi64> to memref<?xi64>
      %9 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %11 = func.call @artsEdtCreateWithEpoch(%10, %6, %c2_i32, %cast, %c0_i32, %5) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %12 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %13 = func.call @artsGetCurrentNode() {llvm.readnone} : () -> i32
      affine.store %7, %alloca_1[0] : memref<1xi64>
      %cast_2 = memref.cast %alloca_1 : memref<1xi64> to memref<?xi64>
      %14 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %16 = func.call @artsEdtCreateWithEpoch(%15, %13, %c1_i32, %cast_2, %c0_i32, %12) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %4 = memref.load %arg3[%c0] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %5 = call @artsGetGuidFromEdtDep(%4) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %6 = call @artsGetPtrFromEdtDep(%4) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %7 = arith.sitofp %1 : i32 to f64
    %8 = arith.addf %7, %3 : f64
    %9 = "polygeist.memref2pointer"(%6) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %8, %9 : f64, !llvm.ptr
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg3[%c0] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %3 = call @artsGetGuidFromEdtDep(%2) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %4 = call @artsGetPtrFromEdtDep(%2) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %5 = memref.load %arg3[%c1] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %6 = call @artsGetGuidFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %7 = call @artsGetPtrFromEdtDep(%5) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %8 = memref.load %arg3[%c2] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %9 = call @artsGetGuidFromEdtDep(%8) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> i64
    %10 = call @artsGetPtrFromEdtDep(%8) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    %12 = llvm.load %11 : !llvm.ptr -> f64
    %13 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %14 = llvm.load %13 : !llvm.ptr -> f64
    %15 = arith.cmpi sgt, %1, %c0_i32 : i32
    %16 = arith.select %15, %14, %cst : f64
    %17 = arith.addf %12, %16 : f64
    %18 = "polygeist.memref2pointer"(%7) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %17, %18 : f64, !llvm.ptr
    return
  }
}

