module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = call @artsGetCurrentNode() : () -> i32
    %c9_i32 = arith.constant 9 : i32
    %alloca = memref.alloca(%c10, %c10) : memref<?x?xi64>
    %alloca_0 = memref.alloca(%c10, %c10) : memref<?x?xmemref<?xi8>>
    %c0_1 = arith.constant 0 : index
    %c1_2 = arith.constant 1 : index
    scf.for %arg0 = %c0_1 to %c10 step %c1_2 {
      %c0_30 = arith.constant 0 : index
      %c1_31 = arith.constant 1 : index
      scf.for %arg1 = %c0_30 to %c10 step %c1_31 {
        %47 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
        %48 = arith.index_cast %c8 : index to i64
        %49 = func.call @artsDbCreateWithGuid(%47, %48) : (i64, i64) -> memref<?xi8>
        memref.store %47, %alloca[%arg0, %arg1] : memref<?x?xi64>
        memref.store %49, %alloca_0[%arg0, %arg1] : memref<?x?xmemref<?xi8>>
      }
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    %c9_i32_3 = arith.constant 9 : i32
    %alloca_4 = memref.alloca(%c10, %c10) : memref<?x?xi64>
    %alloca_5 = memref.alloca(%c10, %c10) : memref<?x?xmemref<?xi8>>
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    scf.for %arg0 = %c0_6 to %c10 step %c1_7 {
      %c0_30 = arith.constant 0 : index
      %c1_31 = arith.constant 1 : index
      scf.for %arg1 = %c0_30 to %c10 step %c1_31 {
        %47 = func.call @artsReserveGuidRoute(%c9_i32_3, %3) : (i32, i32) -> i64
        %48 = arith.index_cast %c8 : index to i64
        %49 = func.call @artsDbCreateWithGuid(%47, %48) : (i64, i64) -> memref<?xi8>
        memref.store %47, %alloca_4[%arg0, %arg1] : memref<?x?xi64>
        memref.store %49, %alloca_5[%arg0, %arg1] : memref<?x?xmemref<?xi8>>
      }
    }
    %4 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_8 = arith.constant 0 : i32
    %alloca_9 = memref.alloca() : memref<i32>
    memref.store %c0_i32_8, %alloca_9[] : memref<i32>
    %alloca_10 = memref.alloca() : memref<i32>
    %c0_i32_11 = arith.constant 0 : i32
    memref.store %c0_i32_11, %alloca_10[] : memref<i32>
    %5 = memref.load %alloca_10[] : memref<i32>
    %6 = arith.index_cast %5 : i32 to index
    %alloca_12 = memref.alloca(%6) : memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %7 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %9 = call @artsEdtCreate(%8, %4, %5, %alloca_12, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_13 = arith.constant 0 : i32
    %10 = call @artsInitializeAndStartEpoch(%9, %c0_i32_13) : (i64, i32) -> i64
    %11 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_14 = arith.constant 0 : i32
    %alloca_15 = memref.alloca() : memref<i32>
    memref.store %c0_i32_14, %alloca_15[] : memref<i32>
    %alloca_16 = memref.alloca() : memref<i32>
    memref.store %c0_i32_14, %alloca_16[] : memref<i32>
    %12 = memref.load %alloca_16[] : memref<i32>
    %alloca_17 = memref.alloca() : memref<i32>
    %c1_i32_18 = arith.constant 1 : i32
    memref.store %c1_i32_18, %alloca_17[] : memref<i32>
    %13 = memref.load %alloca_17[] : memref<i32>
    %14 = arith.index_cast %c10 : index to i32
    %15 = arith.muli %13, %14 : i32
    memref.store %15, %alloca_17[] : memref<i32>
    %16 = memref.load %alloca_17[] : memref<i32>
    %17 = arith.index_cast %c10 : index to i32
    %18 = arith.muli %16, %17 : i32
    memref.store %18, %alloca_17[] : memref<i32>
    %19 = memref.load %alloca_17[] : memref<i32>
    %20 = memref.load %alloca_16[] : memref<i32>
    %21 = arith.addi %20, %19 : i32
    memref.store %21, %alloca_16[] : memref<i32>
    %22 = memref.load %alloca_16[] : memref<i32>
    %alloca_19 = memref.alloca() : memref<i32>
    %c1_i32_20 = arith.constant 1 : i32
    memref.store %c1_i32_20, %alloca_19[] : memref<i32>
    %23 = memref.load %alloca_19[] : memref<i32>
    %24 = arith.index_cast %c10 : index to i32
    %25 = arith.muli %23, %24 : i32
    memref.store %25, %alloca_19[] : memref<i32>
    %26 = memref.load %alloca_19[] : memref<i32>
    %27 = arith.index_cast %c10 : index to i32
    %28 = arith.muli %26, %27 : i32
    memref.store %28, %alloca_19[] : memref<i32>
    %29 = memref.load %alloca_19[] : memref<i32>
    %30 = memref.load %alloca_16[] : memref<i32>
    %31 = arith.addi %30, %29 : i32
    memref.store %31, %alloca_16[] : memref<i32>
    %32 = memref.load %alloca_16[] : memref<i32>
    %alloca_21 = memref.alloca() : memref<i32>
    %c1_i32_22 = arith.constant 1 : i32
    memref.store %c1_i32_22, %alloca_21[] : memref<i32>
    %33 = memref.load %alloca_21[] : memref<i32>
    %34 = arith.index_cast %33 : i32 to index
    %alloca_23 = memref.alloca(%34) : memref<?xi64>
    %35 = arith.extsi %1 : i32 to i64
    %c0_24 = arith.constant 0 : index
    affine.store %35, %alloca_23[%c0_24] : memref<?xi64>
    %36 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %37 = "polygeist.pointer2memref"(%36) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %38 = call @artsEdtCreateWithEpoch(%37, %11, %33, %alloca_23, %32, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloc = memref.alloc() : memref<i32>
    memref.store %12, %alloc[] : memref<i32>
    %c0_25 = arith.constant 0 : index
    %c1_26 = arith.constant 1 : index
    %39 = scf.for %arg0 = %c0_25 to %c10 step %c1_26 iter_args(%arg1 = %alloc) -> (memref<i32>) {
      %c0_30 = arith.constant 0 : index
      %c1_31 = arith.constant 1 : index
      %47 = scf.for %arg2 = %c0_30 to %c10 step %c1_31 iter_args(%arg3 = %arg1) -> (memref<i32>) {
        %48 = memref.load %alloca[%arg0, %arg2] : memref<?x?xi64>
        %49 = memref.load %arg3[] : memref<i32>
        func.call @artsSignalEdt(%38, %49, %48) : (i64, i32, i64) -> ()
        scf.yield %arg3 : memref<i32>
      }
      scf.yield %arg1 : memref<i32>
    }
    %alloc_27 = memref.alloc() : memref<i32>
    memref.store %22, %alloc_27[] : memref<i32>
    %c0_28 = arith.constant 0 : index
    %c1_29 = arith.constant 1 : index
    %40 = scf.for %arg0 = %c0_28 to %c10 step %c1_29 iter_args(%arg1 = %alloc_27) -> (memref<i32>) {
      %c0_30 = arith.constant 0 : index
      %c1_31 = arith.constant 1 : index
      %47 = scf.for %arg2 = %c0_30 to %c10 step %c1_31 iter_args(%arg3 = %arg1) -> (memref<i32>) {
        %48 = memref.load %alloca_4[%arg0, %arg2] : memref<?x?xi64>
        %49 = memref.load %arg3[] : memref<i32>
        func.call @artsSignalEdt(%38, %49, %48) : (i64, i32, i64) -> ()
        scf.yield %arg3 : memref<i32>
      }
      scf.yield %arg1 : memref<i32>
    }
    %41 = llvm.mlir.addressof @str0 : !llvm.ptr
    %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %43 = llvm.mlir.addressof @str1 : !llvm.ptr
    %44 = llvm.getelementptr %43[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %47 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %49 = arith.index_cast %arg1 : index to i32
        %50 = arith.index_cast %arg0 : index to i64
        %51 = arith.index_cast %arg1 : index to i64
      }
      %48 = llvm.call @printf(%44) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %45 = llvm.mlir.addressof @str2 : !llvm.ptr
    %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %47 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %49 = arith.index_cast %arg1 : index to i32
        %50 = arith.index_cast %arg0 : index to i64
        %51 = arith.index_cast %arg1 : index to i64
      }
      %48 = llvm.call @printf(%44) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c10 = arith.constant 10 : index
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    %c0_1 = arith.constant 0 : index
    memref.store %c0_1, %alloca[] : memref<index>
    %c1_2 = arith.constant 1 : index
    %c10_3 = arith.constant 10 : index
    %c10_4 = arith.constant 10 : index
    %alloca_5 = memref.alloca(%c10_3, %c10_4) : memref<?x?xi64>
    %c0_6 = arith.constant 0 : index
    scf.for %arg4 = %c0_6 to %c10_3 step %c1_2 {
      %c0_24 = arith.constant 0 : index
      scf.for %arg5 = %c0_24 to %c10_4 step %c1_2 {
        %26 = memref.load %alloca[] : memref<index>
        %27 = memref.load %arg3[%26] : memref<?x!llvm.struct<(i64, i32, ptr)>>
        %28 = llvm.extractvalue %27[0] : !llvm.struct<(i64, i32, ptr)> 
        %29 = llvm.extractvalue %27[2] : !llvm.struct<(i64, i32, ptr)> 
        %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr) -> memref<?xi8>
        memref.store %28, %alloca_5[%arg4, %arg5] : memref<?x?xi64>
        %31 = arith.addi %26, %c1_2 : index
        memref.store %31, %alloca[] : memref<index>
      }
    }
    %c10_7 = arith.constant 10 : index
    %c10_8 = arith.constant 10 : index
    %alloca_9 = memref.alloca(%c10_7, %c10_8) : memref<?x?xi64>
    %c0_10 = arith.constant 0 : index
    scf.for %arg4 = %c0_10 to %c10_7 step %c1_2 {
      %c0_24 = arith.constant 0 : index
      scf.for %arg5 = %c0_24 to %c10_8 step %c1_2 {
        %26 = memref.load %alloca[] : memref<index>
        %27 = memref.load %arg3[%26] : memref<?x!llvm.struct<(i64, i32, ptr)>>
        %28 = llvm.extractvalue %27[0] : !llvm.struct<(i64, i32, ptr)> 
        %29 = llvm.extractvalue %27[2] : !llvm.struct<(i64, i32, ptr)> 
        %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr) -> memref<?xi8>
        memref.store %28, %alloca_9[%arg4, %arg5] : memref<?x?xi64>
        %31 = arith.addi %26, %c1_2 : index
        memref.store %31, %alloca[] : memref<index>
      }
    }
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca_11 = memref.alloca(%c10, %c10) : memref<?x?xi64>
    %c0_12 = arith.constant 0 : index
    %c1_13 = arith.constant 1 : index
    scf.for %arg4 = %c0_12 to %c10 step %c1_13 {
      %c0_24 = arith.constant 0 : index
      %c1_25 = arith.constant 1 : index
      scf.for %arg5 = %c0_24 to %c10 step %c1_25 {
        %c1_i32_26 = arith.constant 1 : i32
        %26 = func.call @artsEventCreate(%2, %c1_i32_26) : (i32, i32) -> i64
        memref.store %26, %alloca_11[%arg4, %arg5] : memref<?x?xi64>
      }
    }
    %3 = arith.sitofp %1 : i32 to f64
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %26 = arith.index_cast %arg4 : index to i32
      %27 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %28 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_24 = arith.constant 0 : i32
      %alloca_25 = memref.alloca() : memref<i32>
      memref.store %c0_i32_24, %alloca_25[] : memref<i32>
      %alloca_26 = memref.alloca() : memref<i32>
      memref.store %c0_i32_24, %alloca_26[] : memref<i32>
      %29 = memref.load %alloca_26[] : memref<i32>
      %alloca_27 = memref.alloca() : memref<i32>
      %c1_i32_28 = arith.constant 1 : i32
      memref.store %c1_i32_28, %alloca_27[] : memref<i32>
      %30 = memref.load %alloca_27[] : memref<i32>
      %31 = arith.index_cast %c10 : index to i32
      %32 = arith.muli %30, %31 : i32
      memref.store %32, %alloca_27[] : memref<i32>
      %33 = memref.load %alloca_27[] : memref<i32>
      %34 = memref.load %alloca_26[] : memref<i32>
      %35 = arith.addi %34, %33 : i32
      memref.store %35, %alloca_26[] : memref<i32>
      %36 = memref.load %alloca_25[] : memref<i32>
      %37 = arith.addi %36, %33 : i32
      memref.store %37, %alloca_25[] : memref<i32>
      %38 = memref.load %alloca_26[] : memref<i32>
      %alloca_29 = memref.alloca() : memref<i32>
      %c2_i32 = arith.constant 2 : i32
      memref.store %c2_i32, %alloca_29[] : memref<i32>
      %39 = memref.load %alloca_29[] : memref<i32>
      %40 = memref.load %alloca_25[] : memref<i32>
      %41 = arith.addi %39, %40 : i32
      memref.store %41, %alloca_29[] : memref<i32>
      %42 = memref.load %alloca_29[] : memref<i32>
      %43 = arith.index_cast %42 : i32 to index
      %alloca_30 = memref.alloca(%43) : memref<?xi64>
      %44 = arith.extsi %26 : i32 to i64
      %c0_31 = arith.constant 0 : index
      affine.store %44, %alloca_30[%c0_31] : memref<?xi64>
      %45 = arith.fptosi %3 : f64 to i64
      %c1_32 = arith.constant 1 : index
      affine.store %45, %alloca_30[%c1_32] : memref<?xi64>
      %alloca_33 = memref.alloca() : memref<i32>
      %c2_i32_34 = arith.constant 2 : i32
      memref.store %c2_i32_34, %alloca_33[] : memref<i32>
      %c0_35 = arith.constant 0 : index
      %c1_36 = arith.constant 1 : index
      scf.for %arg5 = %c0_35 to %c10 step %c1_36 {
        %49 = memref.load %alloca_11[%arg4, %arg5] : memref<?x?xi64>
        %50 = memref.load %alloca_33[] : memref<i32>
        %51 = arith.index_cast %50 : i32 to index
        memref.store %49, %alloca_30[%51] : memref<?xi64>
        %c1_i32_37 = arith.constant 1 : i32
        %52 = arith.addi %50, %c1_i32_37 : i32
        memref.store %52, %alloca_33[] : memref<i32>
      }
      %46 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %47 = "polygeist.pointer2memref"(%46) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %48 = func.call @artsEdtCreateWithEpoch(%47, %28, %42, %alloca_30, %38, %27) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = call @artsGetCurrentNode() : () -> i32
    %c0_i32 = arith.constant 0 : i32
    %alloca_14 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_14[] : memref<i32>
    %alloca_15 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_15[] : memref<i32>
    %6 = memref.load %alloca_15[] : memref<i32>
    %alloca_16 = memref.alloca() : memref<i32>
    %c1_i32 = arith.constant 1 : i32
    memref.store %c1_i32, %alloca_16[] : memref<i32>
    %7 = memref.load %alloca_16[] : memref<i32>
    %8 = arith.index_cast %c10 : index to i32
    %9 = arith.muli %7, %8 : i32
    memref.store %9, %alloca_16[] : memref<i32>
    %10 = memref.load %alloca_16[] : memref<i32>
    %11 = memref.load %alloca_15[] : memref<i32>
    %12 = arith.addi %11, %10 : i32
    memref.store %12, %alloca_15[] : memref<i32>
    %13 = memref.load %alloca_15[] : memref<i32>
    %alloca_17 = memref.alloca() : memref<i32>
    %c1_i32_18 = arith.constant 1 : i32
    memref.store %c1_i32_18, %alloca_17[] : memref<i32>
    %14 = memref.load %alloca_17[] : memref<i32>
    %15 = arith.index_cast %c10 : index to i32
    %16 = arith.muli %14, %15 : i32
    memref.store %16, %alloca_17[] : memref<i32>
    %17 = memref.load %alloca_17[] : memref<i32>
    %18 = memref.load %alloca_15[] : memref<i32>
    %19 = arith.addi %18, %17 : i32
    memref.store %19, %alloca_15[] : memref<i32>
    %20 = memref.load %alloca_15[] : memref<i32>
    %alloca_19 = memref.alloca() : memref<i32>
    %c0_i32_20 = arith.constant 0 : i32
    memref.store %c0_i32_20, %alloca_19[] : memref<i32>
    %21 = memref.load %alloca_19[] : memref<i32>
    %22 = arith.index_cast %21 : i32 to index
    %alloca_21 = memref.alloca(%22) : memref<?xi64>
    %23 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %25 = call @artsEdtCreateWithEpoch(%24, %5, %21, %alloca_21, %20, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloc = memref.alloc() : memref<i32>
    memref.store %6, %alloc[] : memref<i32>
    %c0_22 = arith.constant 0 : index
    %c1_23 = arith.constant 1 : index
    scf.for %arg4 = %c0_22 to %c10 step %c1_23 {
      %26 = memref.load %alloca_11[%c0, %arg4] : memref<?x?xi64>
      %27 = memref.load %alloc[] : memref<i32>
      func.call @artsAddDependence(%26, %25, %27) : (i64, i64, i32) -> ()
      %c1_i32_24 = arith.constant 1 : i32
      %28 = arith.addi %27, %c1_i32_24 : i32
      memref.store %28, %alloc[] : memref<i32>
    }
    scf.for %arg4 = %c1 to %c10 step %c1 {
      %26 = arith.index_cast %arg4 : index to i32
      %27 = arith.addi %26, %c-1_i32 : i32
      %28 = arith.index_cast %27 : i32 to index
      %29 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %30 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_24 = arith.constant 0 : i32
      %alloca_25 = memref.alloca() : memref<i32>
      memref.store %c0_i32_24, %alloca_25[] : memref<i32>
      %alloca_26 = memref.alloca() : memref<i32>
      memref.store %c0_i32_24, %alloca_26[] : memref<i32>
      %31 = memref.load %alloca_26[] : memref<i32>
      %alloca_27 = memref.alloca() : memref<i32>
      %c1_i32_28 = arith.constant 1 : i32
      memref.store %c1_i32_28, %alloca_27[] : memref<i32>
      %32 = memref.load %alloca_27[] : memref<i32>
      %33 = arith.index_cast %c10 : index to i32
      %34 = arith.muli %32, %33 : i32
      memref.store %34, %alloca_27[] : memref<i32>
      %35 = memref.load %alloca_27[] : memref<i32>
      %36 = memref.load %alloca_26[] : memref<i32>
      %37 = arith.addi %36, %35 : i32
      memref.store %37, %alloca_26[] : memref<i32>
      %38 = memref.load %alloca_26[] : memref<i32>
      %alloca_29 = memref.alloca() : memref<i32>
      %c1_i32_30 = arith.constant 1 : i32
      memref.store %c1_i32_30, %alloca_29[] : memref<i32>
      %39 = memref.load %alloca_29[] : memref<i32>
      %40 = arith.index_cast %c10 : index to i32
      %41 = arith.muli %39, %40 : i32
      memref.store %41, %alloca_29[] : memref<i32>
      %42 = memref.load %alloca_29[] : memref<i32>
      %43 = memref.load %alloca_26[] : memref<i32>
      %44 = arith.addi %43, %42 : i32
      memref.store %44, %alloca_26[] : memref<i32>
      %45 = memref.load %alloca_26[] : memref<i32>
      %alloca_31 = memref.alloca() : memref<i32>
      %c1_i32_32 = arith.constant 1 : i32
      memref.store %c1_i32_32, %alloca_31[] : memref<i32>
      %46 = memref.load %alloca_31[] : memref<i32>
      %47 = arith.index_cast %c10 : index to i32
      %48 = arith.muli %46, %47 : i32
      memref.store %48, %alloca_31[] : memref<i32>
      %49 = memref.load %alloca_31[] : memref<i32>
      %50 = memref.load %alloca_26[] : memref<i32>
      %51 = arith.addi %50, %49 : i32
      memref.store %51, %alloca_26[] : memref<i32>
      %52 = memref.load %alloca_26[] : memref<i32>
      %alloca_33 = memref.alloca() : memref<i32>
      %c0_i32_34 = arith.constant 0 : i32
      memref.store %c0_i32_34, %alloca_33[] : memref<i32>
      %53 = memref.load %alloca_33[] : memref<i32>
      %54 = arith.index_cast %53 : i32 to index
      %alloca_35 = memref.alloca(%54) : memref<?xi64>
      %55 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %56 = "polygeist.pointer2memref"(%55) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %57 = func.call @artsEdtCreateWithEpoch(%56, %30, %53, %alloca_35, %52, %29) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc_36 = memref.alloc() : memref<i32>
      memref.store %31, %alloc_36[] : memref<i32>
      %c0_37 = arith.constant 0 : index
      %c1_38 = arith.constant 1 : index
      scf.for %arg5 = %c0_37 to %c10 step %c1_38 {
        %58 = memref.load %alloca_11[%arg4, %arg5] : memref<?x?xi64>
        %59 = memref.load %alloc_36[] : memref<i32>
        func.call @artsAddDependence(%58, %57, %59) : (i64, i64, i32) -> ()
        %c1_i32_42 = arith.constant 1 : i32
        %60 = arith.addi %59, %c1_i32_42 : i32
        memref.store %60, %alloc_36[] : memref<i32>
      }
      %alloc_39 = memref.alloc() : memref<i32>
      memref.store %38, %alloc_39[] : memref<i32>
      %c0_40 = arith.constant 0 : index
      %c1_41 = arith.constant 1 : index
      scf.for %arg5 = %c0_40 to %c10 step %c1_41 {
        %58 = memref.load %alloca_11[%28, %arg5] : memref<?x?xi64>
        %59 = memref.load %alloc_39[] : memref<i32>
        func.call @artsAddDependence(%58, %57, %59) : (i64, i64, i32) -> ()
        %c1_i32_42 = arith.constant 1 : i32
        %60 = arith.addi %59, %c1_i32_42 : i32
        memref.store %60, %alloc_39[] : memref<i32>
      }
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    %c0_2 = arith.constant 0 : index
    memref.store %c0_2, %alloca[] : memref<index>
    %c1_3 = arith.constant 1 : index
    %c10_4 = arith.constant 10 : index
    %alloca_5 = memref.alloca(%c10_4) : memref<?xi64>
    %alloca_6 = memref.alloca(%c10_4) : memref<?xmemref<?xi8>>
    %c0_7 = arith.constant 0 : index
    scf.for %arg4 = %c0_7 to %c10_4 step %c1_3 {
      %7 = memref.load %alloca[] : memref<index>
      %8 = memref.load %arg3[%7] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %9 = llvm.extractvalue %8[0] : !llvm.struct<(i64, i32, ptr)> 
      %10 = llvm.extractvalue %8[2] : !llvm.struct<(i64, i32, ptr)> 
      %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr) -> memref<?xi8>
      memref.store %9, %alloca_5[%arg4] : memref<?xi64>
      memref.store %11, %alloca_6[%arg4] : memref<?xmemref<?xi8>>
      %12 = arith.addi %7, %c1_3 : index
      memref.store %12, %alloca[] : memref<index>
    }
    %4 = arith.sitofp %1 : i32 to f64
    %5 = arith.addf %4, %3 : f64
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %7 = "polygeist.memref2pointer"(%alloca_6) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %8 = arith.index_cast %arg4 : index to i64
      %9 = llvm.getelementptr %7[%8] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %5, %9 : f64, !llvm.ptr
    }
    %alloca_8 = memref.alloca() : memref<i32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %alloca_8[] : memref<i32>
    %c0_9 = arith.constant 0 : index
    %c1_10 = arith.constant 1 : index
    %6 = scf.for %arg4 = %c0_9 to %c10_4 step %c1_10 iter_args(%arg5 = %alloca_8) -> (memref<i32>) {
      %7 = memref.load %arg5[] : memref<i32>
      %8 = arith.index_cast %7 : i32 to index
      %9 = memref.load %arg1[%8] : memref<?xi64>
      %10 = memref.load %alloca_5[%arg4] : memref<?xi64>
      %c0_i32_11 = arith.constant 0 : i32
      func.call @artsEventSatisfySlot(%9, %10, %c0_i32_11) : (i64, i64, i32) -> ()
      %c1_i32 = arith.constant 1 : i32
      %11 = arith.addi %7, %c1_i32 : i32
      memref.store %11, %arg5[] : memref<i32>
      scf.yield %arg5 : memref<i32>
    }
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1_1 = arith.constant 1 : index
    %c10_2 = arith.constant 10 : index
    %alloca_3 = memref.alloca(%c10_2) : memref<?xi64>
    %alloca_4 = memref.alloca(%c10_2) : memref<?xmemref<?xi8>>
    %c0_5 = arith.constant 0 : index
    scf.for %arg4 = %c0_5 to %c10_2 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_3[%arg4] : memref<?xi64>
      memref.store %4, %alloca_4[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c10_6 = arith.constant 10 : index
    %alloca_7 = memref.alloca(%c10_6) : memref<?xi64>
    %alloca_8 = memref.alloca(%c10_6) : memref<?xmemref<?xi8>>
    %c0_9 = arith.constant 0 : index
    scf.for %arg4 = %c0_9 to %c10_6 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_7[%arg4] : memref<?xi64>
      memref.store %4, %alloca_8[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_4) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_8) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %5 = arith.index_cast %arg4 : index to i64
      %6 = llvm.getelementptr %4[%5] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %3, %6 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1_1 = arith.constant 1 : index
    %c10_2 = arith.constant 10 : index
    %alloca_3 = memref.alloca(%c10_2) : memref<?xi64>
    %alloca_4 = memref.alloca(%c10_2) : memref<?xmemref<?xi8>>
    %c0_5 = arith.constant 0 : index
    scf.for %arg4 = %c0_5 to %c10_2 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_3[%arg4] : memref<?xi64>
      memref.store %4, %alloca_4[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c10_6 = arith.constant 10 : index
    %alloca_7 = memref.alloca(%c10_6) : memref<?xi64>
    %alloca_8 = memref.alloca(%c10_6) : memref<?xmemref<?xi8>>
    %c0_9 = arith.constant 0 : index
    scf.for %arg4 = %c0_9 to %c10_6 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_7[%arg4] : memref<?xi64>
      memref.store %4, %alloca_8[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c10_10 = arith.constant 10 : index
    %alloca_11 = memref.alloca(%c10_10) : memref<?xi64>
    %alloca_12 = memref.alloca(%c10_10) : memref<?xmemref<?xi8>>
    %c0_13 = arith.constant 0 : index
    scf.for %arg4 = %c0_13 to %c10_10 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_11[%arg4] : memref<?xi64>
      memref.store %4, %alloca_12[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_4) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_8) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %5 = arith.index_cast %arg4 : index to i64
      %6 = llvm.getelementptr %4[%5] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %7 = llvm.load %6 : !llvm.ptr -> f64
      %8 = arith.addf %3, %7 : f64
      %9 = "polygeist.memref2pointer"(%alloca_12) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %10 = arith.index_cast %arg4 : index to i64
      %11 = llvm.getelementptr %9[%10] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %8, %11 : f64, !llvm.ptr
    }
    return
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c200_i32 = arith.constant 200 : i32
    %c100_i32 = arith.constant 100 : i32
    %c8_i64 = arith.constant 8 : i64
    %c1_i32 = arith.constant 1 : i32
    %c9_i32 = arith.constant 9 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<10x10xi64>
    affine.for %arg0 = 0 to 100 {
      %16 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
      %17 = func.call @artsDbCreateWithGuid(%16, %c8_i64) : (i64, i64) -> memref<?xi8>
      affine.store %16, %alloca[%arg0 floordiv 10, %arg0 mod 10] : memref<10x10xi64>
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<10x10xi64>
    affine.for %arg0 = 0 to 100 {
      %16 = func.call @artsReserveGuidRoute(%c9_i32, %3) : (i32, i32) -> i64
      %17 = func.call @artsDbCreateWithGuid(%16, %c8_i64) : (i64, i64) -> memref<?xi8>
      affine.store %16, %alloca_0[%arg0 floordiv 10, %arg0 mod 10] : memref<10x10xi64>
    }
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca_1 = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca_1 : memref<0xi64> to memref<?xi64>
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca() : memref<1xi64>
    %cast_3 = memref.cast %alloca_2 : memref<1xi64> to memref<?xi64>
    %10 = arith.extsi %1 : i32 to i64
    affine.store %10, %alloca_2[0] : memref<1xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %9, %c1_i32, %cast_3, %c200_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    affine.for %arg0 = 0 to 100 {
      %16 = affine.load %alloca[%arg0 floordiv 10, %arg0 mod 10] : memref<10x10xi64>
      func.call @artsSignalEdt(%13, %c0_i32, %16) : (i64, i32, i64) -> ()
    }
    affine.for %arg0 = 0 to 100 {
      %16 = affine.load %alloca_0[%arg0 floordiv 10, %arg0 mod 10] : memref<10x10xi64>
      func.call @artsSignalEdt(%13, %c100_i32, %16) : (i64, i32, i64) -> ()
    }
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    affine.for %arg0 = 0 to 10 {
      %16 = llvm.call @printf(%15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    affine.for %arg0 = 0 to 10 {
      %16 = llvm.call @printf(%15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %c0_i32 : i32
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c30_i32 = arith.constant 30 : i32
    %c20_i32 = arith.constant 20 : i32
    %c10_i32 = arith.constant 10 : i32
    %c12_i32 = arith.constant 12 : i32
    %c2_i32 = arith.constant 2 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    affine.store %c0, %alloca[] : memref<index>
    affine.for %arg4 = 0 to 100 {
      %10 = affine.load %alloca[] : memref<index>
      %11 = arith.addi %10, %c1 : index
      affine.store %11, %alloca[] : memref<index>
    }
    affine.for %arg4 = 0 to 100 {
      %10 = affine.load %alloca[] : memref<index>
      %11 = arith.addi %10, %c1 : index
      affine.store %11, %alloca[] : memref<index>
    }
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<10x10xi64>
    affine.for %arg4 = 0 to 100 {
      %10 = func.call @artsEventCreate(%2, %c1_i32) : (i32, i32) -> i64
      affine.store %10, %alloca_0[%arg4 floordiv 10, %arg4 mod 10] : memref<10x10xi64>
    }
    %3 = arith.sitofp %1 : i32 to f64
    %alloca_1 = memref.alloca() : memref<i32>
    %alloca_2 = memref.alloca() : memref<12xi64>
    %cast = memref.cast %alloca_2 : memref<12xi64> to memref<?xi64>
    %4 = arith.fptosi %3 : f64 to i64
    affine.for %arg4 = 0 to 10 {
      %10 = arith.index_cast %arg4 : index to i32
      %11 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %12 = func.call @artsGetCurrentNode() : () -> i32
      %13 = arith.extsi %10 : i32 to i64
      affine.store %13, %alloca_2[0] : memref<12xi64>
      affine.store %4, %alloca_2[1] : memref<12xi64>
      affine.store %c2_i32, %alloca_1[] : memref<i32>
      affine.for %arg5 = 0 to 10 {
        %17 = affine.load %alloca_0[%arg4, %arg5] : memref<10x10xi64>
        %18 = affine.load %alloca_1[] : memref<i32>
        %19 = arith.index_cast %18 : i32 to index
        memref.store %17, %alloca_2[%19] : memref<12xi64>
        %20 = arith.addi %18, %c1_i32 : i32
        affine.store %20, %alloca_1[] : memref<i32>
      }
      %14 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %16 = func.call @artsEdtCreateWithEpoch(%15, %12, %c12_i32, %cast, %c10_i32, %11) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    %5 = call @artsGetCurrentEpochGuid() : () -> i64
    %6 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca() : memref<0xi64>
    %cast_4 = memref.cast %alloca_3 : memref<0xi64> to memref<?xi64>
    %7 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %9 = call @artsEdtCreateWithEpoch(%8, %6, %c0_i32, %cast_4, %c20_i32, %5) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloc = memref.alloc() : memref<i32>
    affine.store %c0_i32, %alloc[] : memref<i32>
    affine.for %arg4 = 0 to 10 {
      %10 = affine.load %alloca_0[0, %arg4] : memref<10x10xi64>
      %11 = affine.load %alloc[] : memref<i32>
      func.call @artsAddDependence(%10, %9, %11) : (i64, i64, i32) -> ()
      %12 = arith.addi %11, %c1_i32 : i32
      affine.store %12, %alloc[] : memref<i32>
    }
    %alloca_5 = memref.alloca() : memref<0xi64>
    %cast_6 = memref.cast %alloca_5 : memref<0xi64> to memref<?xi64>
    affine.for %arg4 = 1 to 10 {
      %10 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %11 = func.call @artsGetCurrentNode() : () -> i32
      %12 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %14 = func.call @artsEdtCreateWithEpoch(%13, %11, %c0_i32, %cast_6, %c30_i32, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc_7 = memref.alloc() : memref<i32>
      affine.store %c0_i32, %alloc_7[] : memref<i32>
      affine.for %arg5 = 0 to 10 {
        %15 = affine.load %alloca_0[%arg4, %arg5] : memref<10x10xi64>
        %16 = affine.load %alloc_7[] : memref<i32>
        func.call @artsAddDependence(%15, %14, %16) : (i64, i64, i32) -> ()
        %17 = arith.addi %16, %c1_i32 : i32
        affine.store %17, %alloc_7[] : memref<i32>
      }
      %alloc_8 = memref.alloc() : memref<i32>
      affine.store %c10_i32, %alloc_8[] : memref<i32>
      affine.for %arg5 = 0 to 10 {
        %15 = affine.load %alloca_0[%arg4 - 1, %arg5] : memref<10x10xi64>
        %16 = affine.load %alloc_8[] : memref<i32>
        func.call @artsAddDependence(%15, %14, %16) : (i64, i64, i32) -> ()
        %17 = arith.addi %16, %c1_i32 : i32
        affine.store %17, %alloc_8[] : memref<i32>
      }
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = affine.load %arg1[1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    affine.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<10xi64>
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %7 = affine.load %alloca[] : memref<index>
      %8 = memref.load %arg3[%7] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %9 = llvm.extractvalue %8[0] : !llvm.struct<(i64, i32, ptr)> 
      %10 = llvm.extractvalue %8[2] : !llvm.struct<(i64, i32, ptr)> 
      %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr) -> memref<?xi8>
      affine.store %9, %alloca_0[%arg4] : memref<10xi64>
      affine.store %11, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %12 = arith.addi %7, %c1 : index
      affine.store %12, %alloca[] : memref<index>
    }
    %4 = arith.sitofp %1 : i32 to f64
    %5 = arith.addf %4, %3 : f64
    %6 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    affine.for %arg4 = 0 to 10 {
      %7 = arith.index_cast %arg4 : index to i64
      %8 = llvm.getelementptr %6[%7] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %5, %8 : f64, !llvm.ptr
    }
    %alloca_2 = memref.alloca() : memref<i32>
    affine.store %c0_i32, %alloca_2[] : memref<i32>
    affine.for %arg4 = 0 to 10 {
      %7 = affine.load %alloca_2[] : memref<i32>
      %8 = arith.index_cast %7 : i32 to index
      %9 = memref.load %arg1[%8] : memref<?xi64>
      %10 = affine.load %alloca_0[%arg4] : memref<10xi64>
      func.call @artsEventSatisfySlot(%9, %10, %c0_i32) : (i64, i64, i32) -> ()
      %11 = arith.addi %7, %c1_i32 : i32
      affine.store %11, %alloca_2[] : memref<i32>
    }
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    affine.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %2 = affine.load %alloca[] : memref<index>
      %3 = memref.load %arg3[%2] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %4 = llvm.extractvalue %3[2] : !llvm.struct<(i64, i32, ptr)> 
      %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi8>
      affine.store %5, %alloca_0[%arg4] : memref<10xmemref<?xi8>>
      %6 = arith.addi %2, %c1 : index
      affine.store %6, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %2 = affine.load %alloca[] : memref<index>
      %3 = memref.load %arg3[%2] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %4 = llvm.extractvalue %3[2] : !llvm.struct<(i64, i32, ptr)> 
      %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi8>
      affine.store %5, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %6 = arith.addi %2, %c1 : index
      affine.store %6, %alloca[] : memref<index>
    }
    %0 = "polygeist.memref2pointer"(%alloca_0) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    %1 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    affine.for %arg4 = 0 to 10 {
      %2 = arith.index_cast %arg4 : index to i64
      %3 = llvm.getelementptr %0[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %4 = llvm.load %3 : !llvm.ptr -> f64
      %5 = llvm.getelementptr %1[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %4, %5 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    affine.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %3 = affine.load %alloca[] : memref<index>
      %4 = memref.load %arg3[%3] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %5 = llvm.extractvalue %4[2] : !llvm.struct<(i64, i32, ptr)> 
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      affine.store %6, %alloca_0[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %3, %c1 : index
      affine.store %7, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %3 = affine.load %alloca[] : memref<index>
      %4 = memref.load %arg3[%3] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %5 = llvm.extractvalue %4[2] : !llvm.struct<(i64, i32, ptr)> 
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      affine.store %6, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %3, %c1 : index
      affine.store %7, %alloca[] : memref<index>
    }
    %alloca_2 = memref.alloca() : memref<10xmemref<?xi8>>
    affine.for %arg4 = 0 to 10 {
      %3 = affine.load %alloca[] : memref<index>
      %4 = memref.load %arg3[%3] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %5 = llvm.extractvalue %4[2] : !llvm.struct<(i64, i32, ptr)> 
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      affine.store %6, %alloca_2[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %3, %c1 : index
      affine.store %7, %alloca[] : memref<index>
    }
    %0 = "polygeist.memref2pointer"(%alloca_0) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    %1 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    %2 = "polygeist.memref2pointer"(%alloca_2) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
    affine.for %arg4 = 0 to 10 {
      %3 = arith.index_cast %arg4 : index to i64
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %5 = llvm.load %4 : !llvm.ptr -> f64
      %6 = llvm.getelementptr %1[%3] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %7 = llvm.load %6 : !llvm.ptr -> f64
      %8 = arith.addf %5, %7 : f64
      %9 = llvm.getelementptr %2[%3] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %8, %9 : f64, !llvm.ptr
    }
    return
  }
}

