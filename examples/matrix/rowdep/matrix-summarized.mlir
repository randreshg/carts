module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c20_i32 = arith.constant 20 : i32
    %c10_i32 = arith.constant 10 : i32
    %c12_i32 = arith.constant 12 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %c0_i32 = arith.constant 0 : i32
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %9 = memref.load %alloca[] : memref<index>
        %10 = arith.addi %9, %c1 : index
        memref.store %10, %alloca[] : memref<index>
      }
    }
    scf.for %arg4 = %c0 to %c10 step %c1 {
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %9 = memref.load %alloca[] : memref<index>
        %10 = arith.addi %9, %c1 : index
        memref.store %10, %alloca[] : memref<index>
      }
    }
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<10x10xi64>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %9 = func.call @artsEventCreate(%2, %c1_i32) : (i32, i32) -> i64
        memref.store %9, %alloca_0[%arg4, %arg5] : memref<10x10xi64>
      }
    }
    %3 = arith.sitofp %1 : i32 to f64
    %alloca_1 = memref.alloca() : memref<i32>
    %alloca_2 = memref.alloca() : memref<12xi64>
    %cast = memref.cast %alloca_2 : memref<12xi64> to memref<?xi64>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %9 = arith.index_cast %arg4 : index to i32
      %10 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %11 = func.call @artsGetCurrentNode() : () -> i32
      %12 = arith.extsi %9 : i32 to i64
      %c0_10 = arith.constant 0 : index
      memref.store %12, %alloca_2[%c0_10] : memref<12xi64>
      %13 = arith.fptosi %3 : f64 to i64
      %c1_11 = arith.constant 1 : index
      memref.store %13, %alloca_2[%c1_11] : memref<12xi64>
      memref.store %c2_i32, %alloca_1[] : memref<i32>
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %17 = memref.load %alloca_0[%arg4, %arg5] : memref<10x10xi64>
        %18 = memref.load %alloca_1[] : memref<i32>
        %19 = arith.index_cast %18 : i32 to index
        memref.store %17, %alloca_2[%19] : memref<12xi64>
        %20 = arith.addi %18, %c1_i32 : i32
        memref.store %20, %alloca_1[] : memref<i32>
      }
      %14 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %16 = func.call @artsEdtCreateWithEpoch(%15, %11, %c12_i32, %cast, %c10_i32, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca() : memref<0xi64>
    %cast_4 = memref.cast %alloca_3 : memref<0xi64> to memref<?xi64>
    %6 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = call @artsEdtCreateWithEpoch(%7, %5, %c0_i32, %cast_4, %c20_i32, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloc = memref.alloc() : memref<i32>
    memref.store %c0_i32, %alloc[] : memref<i32>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %9 = memref.load %alloca_0[%c0, %arg4] : memref<10x10xi64>
      %10 = memref.load %alloc[] : memref<i32>
      func.call @artsAddDependence(%9, %8, %10) : (i64, i64, i32) -> ()
      %11 = arith.addi %10, %c1_i32 : i32
      memref.store %11, %alloc[] : memref<i32>
    }
    %alloca_5 = memref.alloca() : memref<i32>
    %alloca_6 = memref.alloca() : memref<i32>
    %alloca_7 = memref.alloca() : memref<i32>
    %alloca_8 = memref.alloca() : memref<i32>
    %alloca_9 = memref.alloca() : memref<i32>
    scf.for %arg4 = %c1 to %c10 step %c1 {
      %9 = arith.index_cast %arg4 : index to i32
      %10 = arith.addi %9, %c-1_i32 : i32
      %11 = arith.index_cast %10 : i32 to index
      %12 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %13 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0_i32, %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_6[] : memref<i32>
      %14 = memref.load %alloca_6[] : memref<i32>
      %15 = arith.muli %14, %c10_i32 : i32
      memref.store %15, %alloca_6[] : memref<i32>
      %16 = memref.load %alloca_6[] : memref<i32>
      %17 = memref.load %alloca_5[] : memref<i32>
      %18 = arith.addi %17, %16 : i32
      memref.store %18, %alloca_5[] : memref<i32>
      %19 = memref.load %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_7[] : memref<i32>
      %20 = memref.load %alloca_7[] : memref<i32>
      %21 = arith.muli %20, %c10_i32 : i32
      memref.store %21, %alloca_7[] : memref<i32>
      %22 = memref.load %alloca_7[] : memref<i32>
      %23 = memref.load %alloca_5[] : memref<i32>
      %24 = arith.addi %23, %22 : i32
      memref.store %24, %alloca_5[] : memref<i32>
      %25 = memref.load %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_8[] : memref<i32>
      %26 = memref.load %alloca_8[] : memref<i32>
      %27 = arith.muli %26, %c10_i32 : i32
      memref.store %27, %alloca_8[] : memref<i32>
      %28 = memref.load %alloca_8[] : memref<i32>
      %29 = memref.load %alloca_5[] : memref<i32>
      %30 = arith.addi %29, %28 : i32
      memref.store %30, %alloca_5[] : memref<i32>
      %31 = memref.load %alloca_5[] : memref<i32>
      memref.store %c0_i32, %alloca_9[] : memref<i32>
      %32 = memref.load %alloca_9[] : memref<i32>
      %33 = arith.index_cast %32 : i32 to index
      %alloca_10 = memref.alloca(%33) : memref<?xi64>
      %34 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %35 = "polygeist.pointer2memref"(%34) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %36 = func.call @artsEdtCreateWithEpoch(%35, %13, %32, %alloca_10, %31, %12) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc_11 = memref.alloc() : memref<i32>
      memref.store %19, %alloc_11[] : memref<i32>
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %37 = memref.load %alloca_0[%arg4, %arg5] : memref<10x10xi64>
        %38 = memref.load %alloc_11[] : memref<i32>
        func.call @artsAddDependence(%37, %36, %38) : (i64, i64, i32) -> ()
        %39 = arith.addi %38, %c1_i32 : i32
        memref.store %39, %alloc_11[] : memref<i32>
      }
      %alloc_12 = memref.alloc() : memref<i32>
      memref.store %25, %alloc_12[] : memref<i32>
      scf.for %arg5 = %c0 to %c10 step %c1 {
        %37 = memref.load %alloca_0[%11, %arg5] : memref<10x10xi64>
        %38 = memref.load %alloc_12[] : memref<i32>
        func.call @artsAddDependence(%37, %36, %38) : (i64, i64, i32) -> ()
        %39 = arith.addi %38, %c1_i32 : i32
        memref.store %39, %alloc_12[] : memref<i32>
      }
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %4 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca() : memref<10xi64>
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %8 = memref.load %alloca[] : memref<index>
      %9 = arith.muli %8, %c24 : index
      %10 = arith.index_cast %9 : index to i32
      %11 = llvm.getelementptr %4[%10] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %13 = llvm.load %12 : !llvm.ptr -> i64
      %14 = llvm.getelementptr %11[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr) -> memref<?xi8>
      memref.store %13, %alloca_0[%arg4] : memref<10xi64>
      memref.store %15, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %16 = arith.addi %8, %c1 : index
      memref.store %16, %alloca[] : memref<index>
    }
    %5 = arith.sitofp %1 : i32 to f64
    %6 = arith.addf %5, %3 : f64
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %8 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %9 = arith.index_cast %arg4 : index to i64
      %10 = llvm.getelementptr %8[%9] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %6, %10 : f64, !llvm.ptr
    }
    %alloca_2 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_2[] : memref<i32>
    %7 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %8 = memref.load %alloca_2[] : memref<i32>
      %9 = llvm.getelementptr %7[%8] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      %10 = llvm.load %9 : !llvm.ptr -> i64
      %11 = "polygeist.memref2pointer"(%alloca_0) : (memref<10xi64>) -> !llvm.ptr
      %12 = arith.index_cast %arg4 : index to i32
      %13 = llvm.getelementptr %11[%12] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      %14 = llvm.load %13 : !llvm.ptr -> i64
      func.call @artsEventSatisfySlot(%10, %14, %c0_i32) : (i64, i64, i32) -> ()
      %15 = arith.addi %8, %c1_i32 : i32
      memref.store %15, %alloca_2[] : memref<i32>
    }
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %0 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = memref.load %alloca[] : memref<index>
      %2 = arith.muli %1, %c24 : index
      %3 = arith.index_cast %2 : index to i32
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      memref.store %6, %alloca_0[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %1, %c1 : index
      memref.store %7, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = memref.load %alloca[] : memref<index>
      %2 = arith.muli %1, %c24 : index
      %3 = arith.index_cast %2 : index to i32
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      memref.store %6, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %1, %c1 : index
      memref.store %7, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = "polygeist.memref2pointer"(%alloca_0) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %2 = arith.index_cast %arg4 : index to i64
      %3 = llvm.getelementptr %1[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %4 = llvm.load %3 : !llvm.ptr -> f64
      %5 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %6 = llvm.getelementptr %5[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %4, %6 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %0 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = memref.load %alloca[] : memref<index>
      %2 = arith.muli %1, %c24 : index
      %3 = arith.index_cast %2 : index to i32
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      memref.store %6, %alloca_0[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %1, %c1 : index
      memref.store %7, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = memref.load %alloca[] : memref<index>
      %2 = arith.muli %1, %c24 : index
      %3 = arith.index_cast %2 : index to i32
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      memref.store %6, %alloca_1[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %1, %c1 : index
      memref.store %7, %alloca[] : memref<index>
    }
    %alloca_2 = memref.alloca() : memref<10xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = memref.load %alloca[] : memref<index>
      %2 = arith.muli %1, %c24 : index
      %3 = arith.index_cast %2 : index to i32
      %4 = llvm.getelementptr %0[%3] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %5 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi8>
      memref.store %6, %alloca_2[%arg4] : memref<10xmemref<?xi8>>
      %7 = arith.addi %1, %c1 : index
      memref.store %7, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c10 step %c1 {
      %1 = "polygeist.memref2pointer"(%alloca_1) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %2 = arith.index_cast %arg4 : index to i64
      %3 = llvm.getelementptr %1[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %4 = llvm.load %3 : !llvm.ptr -> f64
      %5 = "polygeist.memref2pointer"(%alloca_2) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %6 = llvm.getelementptr %5[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %7 = llvm.load %6 : !llvm.ptr -> f64
      %8 = arith.addf %4, %7 : f64
      %9 = "polygeist.memref2pointer"(%alloca_0) : (memref<10xmemref<?xi8>>) -> !llvm.ptr
      %10 = llvm.getelementptr %9[%2] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %8, %10 : f64, !llvm.ptr
    }
    return
  }
  func.func @mainBody() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1 = arith.constant 1 : index
    %c200_i32 = arith.constant 200 : i32
    %c100_i32 = arith.constant 100 : i32
    %c0 = arith.constant 0 : index
    %c8_i64 = arith.constant 8 : i64
    %c1_i32 = arith.constant 1 : i32
    %c9_i32 = arith.constant 9 : i32
    %c10 = arith.constant 10 : index
    %c0_i32 = arith.constant 0 : i32
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<10x10xi64>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %17 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
        %18 = func.call @artsDbCreateWithGuid(%17, %c8_i64) : (i64, i64) -> memref<?xi8>
        memref.store %17, %alloca[%arg0, %arg1] : memref<10x10xi64>
      }
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<10x10xi64>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %17 = func.call @artsReserveGuidRoute(%c9_i32, %3) : (i32, i32) -> i64
        %18 = func.call @artsDbCreateWithGuid(%17, %c8_i64) : (i64, i64) -> memref<?xi8>
        memref.store %17, %alloca_0[%arg0, %arg1] : memref<10x10xi64>
      }
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
    %c0_4 = arith.constant 0 : index
    memref.store %10, %alloca_2[%c0_4] : memref<1xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %9, %c1_i32, %cast_3, %c200_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    scf.for %arg0 = %c0 to %c10 step %c1 {
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %17 = memref.load %alloca[%arg0, %arg1] : memref<10x10xi64>
        func.call @artsSignalEdt(%13, %c0_i32, %17) : (i64, i32, i64) -> ()
      }
    }
    scf.for %arg0 = %c0 to %c10 step %c1 {
      scf.for %arg1 = %c0 to %c10 step %c1 {
        %17 = memref.load %alloca_0[%arg0, %arg1] : memref<10x10xi64>
        func.call @artsSignalEdt(%13, %c100_i32, %17) : (i64, i32, i64) -> ()
      }
    }
    %14 = call @artsWaitOnHandle(%8) : (i64) -> i1
    %15 = llvm.mlir.addressof @str1 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %17 = llvm.call @printf(%16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %17 = llvm.call @printf(%16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
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

