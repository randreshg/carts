--- ConvertArtsToFuncsPass START ---
[convert-arts-to-funcs] Iterate over all the functions
[convert-arts-to-funcs] Lowering arts.datablock: %0 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
[convert-arts-to-funcs] Processing memref.load: %12 = memref.load %1[%arg0] : memref<100xf64>
Created array of guids: %alloca_1 = memref.alloca(%c100) : memref<?xi64>
Created array of datablocks
[convert-arts-to-funcs] Lowering arts.datablock: %4 = arts.datablock "inout", %alloca_5 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
[convert-arts-to-funcs] Processing memref.load: %13 = memref.load %5[%arg0] : memref<100xf64>
Created array of guids: %alloca_8 = memref.alloca(%c100) : memref<?xi64>
Created array of datablocks
[convert-arts-to-funcs] Lowering arts.datablock: %17 = arts.datablock "out", %6 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.store: memref.store %27, %18[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %23 = arts.datablock "in", %6 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.load: %30 = memref.load %24[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %30 = arts.datablock "in", %6 : memref<?xmemref<?xi8>>[%29] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.load: %36 = memref.load %31[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %35 = arts.datablock "out", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.store: memref.store %44, %36[%c0] : memref<1xf64>
=== ConvertArtsToFuncsPass COMPLETE ===
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
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
    %alloca = memref.alloca() : memref<100xf64>
    %c1_0 = arith.constant 1 : index
    %0 = call @artsGetCurrentNode() : () -> i32
    %1 = arith.muli %c1_0, %c100 : index
    %c9_i32 = arith.constant 9 : i32
    %alloca_1 = memref.alloca(%c100) : memref<?xi64>
    %alloca_2 = memref.alloca(%c100) : memref<?xmemref<?xi8>>
    %c0_3 = arith.constant 0 : index
    %c1_4 = arith.constant 1 : index
    scf.for %arg0 = %c0_3 to %c100 step %c1_4 {
      %12 = func.call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
      %13 = arith.index_cast %c8 : index to i64
      %14 = func.call @artsDbCreateWithGuid(%12, %13) : (i64, i64) -> memref<?xi8>
      memref.store %12, %alloca_1[%arg0] : memref<?xi64>
      memref.store %14, %alloca_2[%arg0] : memref<?xmemref<?xi8>>
    }
    %2 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %alloca_5 = memref.alloca() : memref<100xf64>
    %c1_6 = arith.constant 1 : index
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = arith.muli %c1_6, %c100 : index
    %c9_i32_7 = arith.constant 9 : i32
    %alloca_8 = memref.alloca(%c100) : memref<?xi64>
    %alloca_9 = memref.alloca(%c100) : memref<?xmemref<?xi8>>
    %c0_10 = arith.constant 0 : index
    %c1_11 = arith.constant 1 : index
    scf.for %arg0 = %c0_10 to %c100 step %c1_11 {
      %12 = func.call @artsReserveGuidRoute(%c9_i32_7, %3) : (i32, i32) -> i64
      %13 = arith.index_cast %c8 : index to i64
      %14 = func.call @artsDbCreateWithGuid(%12, %13) : (i64, i64) -> memref<?xi8>
      memref.store %12, %alloca_8[%arg0] : memref<?xi64>
      memref.store %14, %alloca_9[%arg0] : memref<?xmemref<?xi8>>
    }
    %5 = arts.datablock "inout", %alloca_5 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %6 = call @rand() : () -> i32
    %7 = arith.remsi %6, %c100_i32 : i32
    arts.edt dependencies(%2, %5) : (memref<?xmemref<?xi8>>, memref<?xmemref<?xi8>>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %12 = arts.event %4 {grouped} : memref<100xi64>
        %13 = arith.sitofp %7 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %14 = arith.index_cast %arg0 : index to i32
          %c1_12 = arith.constant 1 : index
          %15 = func.call @artsGetCurrentNode() : () -> i32
          %16 = memref.load %alloca_8[%arg0] : memref<?xi64>
          %17 = memref.load %alloca_5[%arg0] : memref<100xf64>
          %18 = arts.datablock "out", %5 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %19 = memref.load %12[%arg0] : memref<100xi64>
          arts.edt dependencies(%18) : (memref<?xi8>), events(%19) : (i64) {
            %35 = arith.sitofp %14 : i32 to f64
            %36 = arith.addf %35, %13 : f64
            %37 = "polygeist.memref2pointer"(%18) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %36, %37 : f64, !llvm.ptr
            arts.yield
          }
          %c1_13 = arith.constant 1 : index
          %20 = func.call @artsGetCurrentNode() : () -> i32
          %21 = memref.load %alloca_8[%arg0] : memref<?xi64>
          %22 = memref.load %alloca_5[%arg0] : memref<100xf64>
          %23 = arts.datablock "in", %5 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %24 = arith.addi %14, %c-1_i32 : i32
          %25 = arith.index_cast %24 : i32 to index
          %c1_14 = arith.constant 1 : index
          %26 = func.call @artsGetCurrentNode() : () -> i32
          %27 = memref.load %alloca_8[%25] : memref<?xi64>
          %28 = memref.load %alloca_5[%25] : memref<100xf64>
          %29 = arts.datablock "in", %5 : memref<?xmemref<?xi8>>[%25] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %c1_15 = arith.constant 1 : index
          %30 = func.call @artsGetCurrentNode() : () -> i32
          %31 = memref.load %alloca_1[%arg0] : memref<?xi64>
          %32 = memref.load %alloca[%arg0] : memref<100xf64>
          %33 = arts.datablock "out", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %34 = memref.load %12[%25] : memref<100xi64>
          arts.edt dependencies(%29, %33, %23) : (memref<?xi8>, memref<?xi8>, memref<?xi8>), events(%34, %c-1_i32, %19) : (i64, i32, i64) {
            %35 = "polygeist.memref2pointer"(%23) : (memref<?xi8>) -> !llvm.ptr
            %36 = llvm.load %35 : !llvm.ptr -> f64
            %37 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
            %38 = llvm.load %37 : !llvm.ptr -> f64
            %39 = arith.cmpi sgt, %14, %c0_i32 : i32
            %40 = arith.select %39, %38, %cst : f64
            %41 = arith.addf %36, %40 : f64
            %42 = "polygeist.memref2pointer"(%33) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %41, %42 : f64, !llvm.ptr
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %8 = llvm.mlir.addressof @str0 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %10 = llvm.mlir.addressof @str1 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %12 = arith.index_cast %arg0 : index to i32
      %13 = "polygeist.memref2pointer"(%5) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %14 = arith.index_cast %arg0 : index to i64
      %15 = llvm.getelementptr %13[%14] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %16 = llvm.load %15 : !llvm.ptr -> f64
      %17 = llvm.call @printf(%9, %12, %16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %18 = "polygeist.memref2pointer"(%2) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %19 = arith.index_cast %arg0 : index to i64
      %20 = llvm.getelementptr %18[%19] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %21 = llvm.load %20 : !llvm.ptr -> f64
      %22 = llvm.call @printf(%11, %12, %21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  llvm.mlir.global internal constant @str1("B[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100 = arith.constant 100 : index
    %c8_i64 = arith.constant 8 : i64
    %c9_i32 = arith.constant 9 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100xf64>
    %0 = call @artsGetCurrentNode() : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = func.call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
      %11 = func.call @artsDbCreateWithGuid(%10, %c8_i64) : (i64, i64) -> memref<?xi8>
    }
    %1 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %2 = call @artsGetCurrentNode() : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
      %11 = func.call @artsDbCreateWithGuid(%10, %c8_i64) : (i64, i64) -> memref<?xi8>
    }
    %3 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %4 = call @rand() : () -> i32
    %5 = arith.remsi %4, %c100_i32 : i32
    arts.edt dependencies(%1, %3) : (memref<?xmemref<?xi8>>, memref<?xmemref<?xi8>>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %10 = arts.event %c100 {grouped} : memref<100xi64>
        %11 = arith.sitofp %5 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %12 = arith.index_cast %arg0 : index to i32
          %13 = func.call @artsGetCurrentNode() : () -> i32
          %14 = arts.datablock "out", %3 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %15 = memref.load %10[%arg0] : memref<100xi64>
          arts.edt dependencies(%14) : (memref<?xi8>), events(%15) : (i64) {
            %25 = arith.sitofp %12 : i32 to f64
            %26 = arith.addf %25, %11 : f64
            %27 = "polygeist.memref2pointer"(%14) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %26, %27 : f64, !llvm.ptr
            arts.yield
          }
          %16 = func.call @artsGetCurrentNode() : () -> i32
          %17 = arts.datablock "in", %3 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %18 = arith.addi %12, %c-1_i32 : i32
          %19 = arith.index_cast %18 : i32 to index
          %20 = func.call @artsGetCurrentNode() : () -> i32
          %21 = arts.datablock "in", %3 : memref<?xmemref<?xi8>>[%19] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %22 = func.call @artsGetCurrentNode() : () -> i32
          %23 = arts.datablock "out", %1 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %24 = memref.load %10[%19] : memref<100xi64>
          arts.edt dependencies(%21, %23, %17) : (memref<?xi8>, memref<?xi8>, memref<?xi8>), events(%24, %c-1_i32, %15) : (i64, i32, i64) {
            %25 = "polygeist.memref2pointer"(%17) : (memref<?xi8>) -> !llvm.ptr
            %26 = llvm.load %25 : !llvm.ptr -> f64
            %27 = "polygeist.memref2pointer"(%21) : (memref<?xi8>) -> !llvm.ptr
            %28 = llvm.load %27 : !llvm.ptr -> f64
            %29 = arith.cmpi sgt, %12, %c0_i32 : i32
            %30 = arith.select %29, %28, %cst : f64
            %31 = arith.addf %26, %30 : f64
            %32 = "polygeist.memref2pointer"(%23) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %31, %32 : f64, !llvm.ptr
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %8 = llvm.mlir.addressof @str1 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      %11 = "polygeist.memref2pointer"(%3) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %12 = arith.index_cast %arg0 : index to i64
      %13 = llvm.getelementptr %11[%12] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %14 = llvm.load %13 : !llvm.ptr -> f64
      %15 = llvm.call @printf(%7, %10, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %16 = "polygeist.memref2pointer"(%1) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %17 = llvm.getelementptr %16[%12] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %18 = llvm.load %17 : !llvm.ptr -> f64
      %19 = llvm.call @printf(%9, %10, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

