module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str12("Node %u initialized.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str11("---------- Main EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str10("---------- Main EDT ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str9("The program has finished its execution\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("--------------------------------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("---------- Compute EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("---------- Compute EDT ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("---------- Parallel EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("---------- Parallel EDT started ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("B[%d]: %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("A[%d]: %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("------------------------\0A--- Compute B[%u] = %f\0A------------------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("------------------------\0A--- Compute A[%u] = %f - Guid: %lu\0A------------------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @computeA(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 2.000000e+00 : f64
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = affine.load %arg1[1] : memref<?xi64>
    %3 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %4 = llvm.load %3 : !llvm.ptr -> i64
    %5 = arith.uitofp %1 : i32 to f64
    %6 = arith.mulf %5, %cst : f64
    %7 = llvm.mlir.addressof @str0 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<86 x i8>
    %9 = llvm.call @printf(%8, %1, %6, %4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64, i64) -> i32
    call @artsEventSatisfySlot(%2, %4, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @computeB(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c2 = arith.constant 2 : index
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %3 = arith.index_cast %2 : index to i64
    %4 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %5 = llvm.getelementptr %4[%3] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
    %7 = llvm.load %6 : !llvm.ptr -> memref<?xi8>
    %8 = "polygeist.memref2pointer"(%7) : (memref<?xi8>) -> !llvm.ptr
    %9 = arith.cmpi ugt, %1, %c0_i32 : i32
    %10 = scf.if %9 -> (f64) {
      %20 = arith.muli %2, %c2 : index
      %21 = arith.index_cast %20 : index to i64
      %22 = llvm.getelementptr %4[%21] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %23 = llvm.getelementptr %22[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %24 = llvm.load %23 : !llvm.ptr -> memref<?xi8>
      %25 = "polygeist.memref2pointer"(%24) : (memref<?xi8>) -> !llvm.ptr
      %26 = llvm.load %25 : !llvm.ptr -> f64
      scf.yield %26 : f64
    } else {
      scf.yield %cst : f64
    }
    %11 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
    %12 = llvm.load %11 : !llvm.ptr -> memref<?xi8>
    %13 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
    %14 = llvm.load %8 : !llvm.ptr -> f64
    %15 = arith.addf %14, %10 : f64
    llvm.store %15, %13 : f64, !llvm.ptr
    %16 = llvm.mlir.addressof @str1 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<74 x i8>
    %18 = llvm.load %13 : !llvm.ptr -> f64
    %19 = llvm.call @printf(%17, %1, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    return
  }
  func.func @printDataBlockA(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %1 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %4 = scf.while (%arg4 = %c0_i32) : (i32) -> i32 {
      %5 = arith.extsi %arg4 : i32 to i64
      %6 = affine.load %arg1[0] : memref<?xi64>
      %7 = arith.cmpi slt, %5, %6 : i64
      scf.condition(%7) %arg4 : i32
    } do {
    ^bb0(%arg4: i32):
      %5 = arith.index_cast %arg4 : i32 to index
      %6 = arith.muli %5, %0 : index
      %7 = arith.index_cast %6 : index to i64
      %8 = llvm.getelementptr %1[%7] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%3, %arg4, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = arith.addi %arg4, %c1_i32 : i32
      scf.yield %14 : i32
    }
    return
  }
  func.func @printDataBlockB(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %1 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %4 = scf.while (%arg4 = %c0_i32) : (i32) -> i32 {
      %5 = arith.extsi %arg4 : i32 to i64
      %6 = affine.load %arg1[0] : memref<?xi64>
      %7 = arith.cmpi slt, %5, %6 : i64
      scf.condition(%7) %arg4 : i32
    } do {
    ^bb0(%arg4: i32):
      %5 = arith.index_cast %arg4 : i32 to index
      %6 = arith.muli %5, %0 : index
      %7 = arith.index_cast %6 : index to i64
      %8 = llvm.getelementptr %1[%7] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%3, %arg4, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = arith.addi %arg4, %c1_i32 : i32
      scf.yield %14 : i32
    }
    return
  }
  func.func @parallelEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8_i64 = arith.constant 8 : i64
    %c3_i32 = arith.constant 3 : i32
    %c2_i32 = arith.constant 2 : i32
    %cst = arith.constant 2.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<1xi64>
    %alloca_0 = memref.alloca() : memref<2xi64>
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = affine.load %arg1[0] : memref<?xi64>
    %6 = arith.trunci %5 : i64 to i32
    %7 = arith.index_cast %6 : i32 to index
    %alloca_1 = memref.alloca(%7) : memref<?xmemref<?xf32>>
    %alloca_2 = memref.alloca(%7) : memref<?xmemref<?xf32>>
    %alloca_3 = memref.alloca(%7) : memref<?xi64>
    %alloca_4 = memref.alloca(%7) : memref<?xi64>
    %8 = "polygeist.memref2pointer"(%alloca_1) : (memref<?xmemref<?xf32>>) -> !llvm.ptr
    %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xmemref<?xi8>>
    call @artsDbCreatePtrAndGuidArrayFromDeps(%9, %alloca_3, %6, %arg3, %c0_i32) : (memref<?xmemref<?xi8>>, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %10 = "polygeist.memref2pointer"(%alloca_2) : (memref<?xmemref<?xf32>>) -> !llvm.ptr
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr) -> memref<?xmemref<?xi8>>
    call @artsDbCreatePtrAndGuidArrayFromDeps(%11, %alloca_4, %6, %arg3, %6) : (memref<?xmemref<?xi8>>, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %12 = llvm.mlir.addressof @str2 : !llvm.ptr
    %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %14 = llvm.mlir.addressof @str3 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    scf.for %arg4 = %c0 to %7 step %c1 {
      %24 = arith.index_cast %arg4 : index to i32
      %25 = memref.load %alloca_1[%arg4] : memref<?xmemref<?xf32>>
      %26 = affine.load %25[0] : memref<?xf32>
      %27 = arith.extf %26 : f32 to f64
      %28 = llvm.call @printf(%13, %24, %27) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %29 = memref.load %alloca_2[%arg4] : memref<?xmemref<?xf32>>
      %30 = affine.load %29[0] : memref<?xf32>
      %31 = arith.extf %30 : f32 to f64
      %32 = llvm.call @printf(%15, %24, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    scf.for %arg4 = %c0 to %7 step %c1 {
      %24 = arith.index_cast %arg4 : index to i32
      %25 = memref.load %alloca_1[%arg4] : memref<?xmemref<?xf32>>
      %26 = arith.sitofp %24 : i32 to f64
      %27 = arith.mulf %26, %cst : f64
      %28 = arith.truncf %27 : f64 to f32
      affine.store %28, %25[0] : memref<?xf32>
      %29 = memref.load %alloca_2[%arg4] : memref<?xmemref<?xf32>>
      affine.store %28, %29[0] : memref<?xf32>
    }
    %16 = arith.extsi %6 : i32 to i64
    %17 = arith.muli %16, %c8_i64 : i64
    %18 = call @artsMalloc(%17) : (i64) -> memref<?xi8>
    %19 = "polygeist.memref2pointer"(%18) : (memref<?xi8>) -> !llvm.ptr
    scf.for %arg4 = %c0 to %7 step %c1 {
      %24 = arith.index_cast %arg4 : index to i32
      %25 = func.call @artsEventCreate(%3, %c1_i32) : (i32, i32) -> i64
      %26 = llvm.getelementptr %19[%24] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      llvm.store %25, %26 : i64, !llvm.ptr
    }
    %20 = "polygeist.get_func"() <{name = @computeA}> : () -> !llvm.ptr
    %cast = memref.cast %alloca_0 : memref<2xi64> to memref<?xi64>
    %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %22 = "polygeist.get_func"() <{name = @computeB}> : () -> !llvm.ptr
    %cast_5 = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    %23 = "polygeist.pointer2memref"(%22) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    scf.for %arg4 = %c0 to %7 step %c1 {
      %24 = arith.index_cast %arg4 : index to i32
      %25 = arith.extsi %24 : i32 to i64
      affine.store %25, %alloca_0[0] : memref<2xi64>
      %26 = llvm.getelementptr %19[%24] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      %27 = llvm.load %26 : !llvm.ptr -> i64
      affine.store %27, %alloca_0[1] : memref<2xi64>
      %28 = func.call @artsEdtCreateWithEpoch(%21, %3, %c2_i32, %cast, %c1_i32, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      affine.store %25, %alloca[0] : memref<1xi64>
      %29 = arith.cmpi sgt, %24, %c0_i32 : i32
      %30 = arith.select %29, %c3_i32, %c2_i32 : i32
      %31 = func.call @artsEdtCreateWithEpoch(%23, %3, %c2_i32, %cast_5, %30, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %32 = llvm.load %26 : !llvm.ptr -> i64
      func.call @artsAddDependence(%32, %31, %c1_i32) : (i64, i64, i32) -> ()
      scf.if %29 {
        %35 = arith.addi %24, %c-1_i32 : i32
        %36 = llvm.getelementptr %19[%35] : (!llvm.ptr, i32) -> !llvm.ptr, i64
        %37 = llvm.load %36 : !llvm.ptr -> i64
        func.call @artsAddDependence(%37, %31, %c2_i32) : (i64, i64, i32) -> ()
      }
      %33 = memref.load %alloca_3[%arg4] : memref<?xi64>
      func.call @artsSignalEdt(%28, %c0_i32, %33) : (i64, i32, i64) -> ()
      %34 = memref.load %alloca_3[%arg4] : memref<?xi64>
      func.call @artsSignalEdt(%31, %c0_i32, %34) : (i64, i32, i64) -> ()
    }
    return
  }
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreatePtrAndGuidArrayFromDeps(memref<?xmemref<?xi8>>, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsMalloc(i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @parallelDoneEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str5 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<45 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func @computeEDT(%arg0: i32, %arg1: memref<?x!llvm.struct<(i64, memref<?xi8>)>>, %arg2: memref<?x!llvm.struct<(i64, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2_i32 = arith.constant 2 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<1xi64>
    %alloca_0 = memref.alloca() : memref<1xi64>
    %0 = llvm.mlir.addressof @str6 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = "polygeist.get_func"() <{name = @parallelDoneEdt}> : () -> !llvm.ptr
    %6 = llvm.mlir.zero : !llvm.ptr
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr) -> memref<?xi64>
    %8 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %9 = call @artsEdtCreate(%8, %3, %c0_i32, %7, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %10 = call @artsInitializeAndStartEpoch(%9, %c0_i32) : (i64, i32) -> i64
    %11 = arith.extsi %arg0 : i32 to i64
    affine.store %11, %alloca_0[0] : memref<1xi64>
    %12 = arith.muli %arg0, %c2_i32 : i32
    %13 = "polygeist.get_func"() <{name = @parallelEdt}> : () -> !llvm.ptr
    %cast = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    %14 = "polygeist.pointer2memref"(%13) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %15 = call @artsEdtCreateWithEpoch(%14, %3, %c1_i32, %cast, %12, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg1, %15, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    call @artsSignalDbs(%arg2, %15, %arg0, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %16 = call @artsWaitOnHandle(%10) : (i64) -> i8
    affine.store %11, %alloca[0] : memref<1xi64>
    %17 = "polygeist.get_func"() <{name = @printDataBlockA}> : () -> !llvm.ptr
    %cast_1 = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %19 = call @artsEdtCreateWithEpoch(%18, %3, %c1_i32, %cast_1, %arg0, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg1, %19, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %20 = "polygeist.get_func"() <{name = @printDataBlockB}> : () -> !llvm.ptr
    %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %22 = call @artsEdtCreateWithEpoch(%21, %3, %c1_i32, %cast_1, %arg0, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg2, %22, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %23 = llvm.mlir.addressof @str7 : !llvm.ptr
    %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %25 = llvm.call @printf(%24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalDbs(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i8 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @finishMainEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str8 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = llvm.mlir.addressof @str9 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %5 = llvm.call @printf(%4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %6 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @mainEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5 = arith.constant 5 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8_i64 = arith.constant 8 : i64
    %c9_i32 = arith.constant 9 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c5_i32 = arith.constant 5 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = llvm.mlir.addressof @str10 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<32 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = "polygeist.get_func"() <{name = @finishMainEdt}> : () -> !llvm.ptr
    %5 = llvm.mlir.zero : !llvm.ptr
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi64>
    %7 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %8 = call @artsEdtCreate(%7, %3, %c0_i32, %6, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %9 = call @artsInitializeAndStartEpoch(%8, %c0_i32) : (i64, i32) -> i64
    %alloca = memref.alloca() : memref<5xf64>
    %alloca_0 = memref.alloca() : memref<5xf64>
    scf.for %arg4 = %c0 to %c5 step %c1 {
      %18 = arith.index_cast %arg4 : index to i32
      %19 = arith.sitofp %18 : i32 to f64
      memref.store %19, %alloca[%arg4] : memref<5xf64>
      memref.store %cst, %alloca_0[%arg4] : memref<5xf64>
    }
    %alloca_1 = memref.alloca() : memref<5x!llvm.struct<(i64, memref<?xi8>)>>
    %cast = memref.cast %alloca_1 : memref<5x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %alloca_2 = memref.alloca() : memref<5x!llvm.struct<(i64, memref<?xi8>)>>
    %cast_3 = memref.cast %alloca_2 : memref<5x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %10 = "polygeist.memref2pointer"(%alloca) : (memref<5xf64>) -> !llvm.ptr
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr) -> memref<?xi8>
    call @artsDbCreateArray(%cast, %c8_i64, %c9_i32, %c5_i32, %11) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
    %12 = "polygeist.memref2pointer"(%alloca_0) : (memref<5xf64>) -> !llvm.ptr
    %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr) -> memref<?xi8>
    call @artsDbCreateArray(%cast_3, %c8_i64, %c9_i32, %c5_i32, %13) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
    call @computeEDT(%c5_i32, %cast, %cast_3) : (i32, memref<?x!llvm.struct<(i64, memref<?xi8>)>>, memref<?x!llvm.struct<(i64, memref<?xi8>)>>) -> ()
    %14 = call @artsWaitOnHandle(%9) : (i64) -> i8
    %15 = llvm.mlir.addressof @str11 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<41 x i8>
    %17 = llvm.call @printf(%16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func private @artsDbCreateArray(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<0xi64>
    %0 = arith.cmpi eq, %arg0, %c0_i32 : i32
    scf.if %0 {
      %4 = func.call @artsGetCurrentNode() : () -> i32
      %5 = "polygeist.get_func"() <{name = @mainEdt}> : () -> !llvm.ptr
      %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %7 = func.call @artsEdtCreate(%6, %4, %c1_i32, %cast, %c0_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    }
    %1 = llvm.mlir.addressof @str12 : !llvm.ptr
    %2 = llvm.getelementptr %1[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %3 = llvm.call @printf(%2, %arg0) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    return
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: memref<?xmemref<?xi8>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
