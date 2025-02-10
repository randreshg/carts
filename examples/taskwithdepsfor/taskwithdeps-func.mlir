--- ConvertArtsToFuncsPass START ---
[convert-arts-to-funcs] Iterate over all the functions
[convert-arts-to-funcs] Lowering arts.datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
[convert-arts-to-funcs] Processing memref.load: %10 = memref.load %3[%arg0] : memref<100xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %4 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
[convert-arts-to-funcs] Processing memref.load: %16 = memref.load %5[%arg0] : memref<100xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %14 = arts.datablock "out", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.store: memref.store %24, %15[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %19 = arts.datablock "out", %4 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.store: memref.store %28, %20[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %21 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.load: %25 = memref.load %22[%c0] : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %23 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%18] [%c1] [f64, %c8] -> memref<1xf64> {baseIsDb, isLoad}
[convert-arts-to-funcs] Processing memref.load: %28 = memref.load %24[%c0] : memref<1xf64>
=== ConvertArtsToFuncsPass COMPLETE ===
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %3 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    arts.edt dependencies(%2, %3) : (memref<?xmemref<?xi8>>, memref<?xmemref<?xi8>>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arts.datablock_size %2 : memref<?xmemref<?xi8>> -> index
        %9 = arts.event %8 {grouped} : memref<100xi64>
        %10 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %11 = arith.index_cast %arg0 : index to i32
          %12 = arts.datablock "out", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %13 = memref.load %9[%arg0] : memref<100xi64>
          arts.edt dependencies(%12) : (memref<?xi8>), events(%13) : (i64) {
            %20 = arith.sitofp %11 : i32 to f64
            %21 = arith.addf %20, %10 : f64
            %22 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %21, %22 : f64, !llvm.ptr
            arts.yield
          }
          %14 = arith.addi %11, %c-1_i32 : i32
          %15 = arith.index_cast %14 : i32 to index
          %16 = arts.datablock "out", %3 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %17 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %18 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%15] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %19 = memref.load %9[%15] : memref<100xi64>
          arts.edt dependencies(%16, %17, %18) : (memref<?xi8>, memref<?xi8>, memref<?xi8>), events(%c-1_i32, %13, %19) : (i32, i64, i64) {
            %20 = "polygeist.memref2pointer"(%17) : (memref<?xi8>) -> !llvm.ptr
            %21 = llvm.load %20 : !llvm.ptr -> f64
            %22 = "polygeist.memref2pointer"(%18) : (memref<?xi8>) -> !llvm.ptr
            %23 = llvm.load %22 : !llvm.ptr -> f64
            %24 = arith.cmpi sgt, %11, %c0_i32 : i32
            %25 = arith.select %24, %23, %cst : f64
            %26 = arith.addf %21, %25 : f64
            %27 = "polygeist.memref2pointer"(%16) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %26, %27 : f64, !llvm.ptr
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %8 = arith.index_cast %arg0 : index to i32
      %9 = "polygeist.memref2pointer"(%2) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %10 = arith.index_cast %arg0 : index to i64
      %11 = llvm.getelementptr %9[%10] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%5, %8, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = "polygeist.memref2pointer"(%3) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %15 = arith.index_cast %arg0 : index to i64
      %16 = llvm.getelementptr %14[%15] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %17 = llvm.load %16 : !llvm.ptr -> f64
      %18 = llvm.call @printf(%7, %8, %17) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    %3 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<?xmemref<?xi8>>
    arts.edt dependencies(%2, %3) : (memref<?xmemref<?xi8>>, memref<?xmemref<?xi8>>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arts.datablock_size %2 : memref<?xmemref<?xi8>> -> index
        %9 = arts.event %8 {grouped} : memref<100xi64>
        %10 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %11 = arith.index_cast %arg0 : index to i32
          %12 = arts.datablock "out", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %13 = memref.load %9[%arg0] : memref<100xi64>
          arts.edt dependencies(%12) : (memref<?xi8>), events(%13) : (i64) {
            %20 = arith.sitofp %11 : i32 to f64
            %21 = arith.addf %20, %10 : f64
            %22 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %21, %22 : f64, !llvm.ptr
            arts.yield
          }
          %14 = arith.addi %11, %c-1_i32 : i32
          %15 = arith.index_cast %14 : i32 to index
          %16 = arts.datablock "out", %3 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %17 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%arg0] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %18 = arts.datablock "in", %2 : memref<?xmemref<?xi8>>[%15] [%c1] [f64, %c8] -> memref<?xi8> {baseIsDb, isLoad}
          %19 = memref.load %9[%15] : memref<100xi64>
          arts.edt dependencies(%16, %17, %18) : (memref<?xi8>, memref<?xi8>, memref<?xi8>), events(%c-1_i32, %13, %19) : (i32, i64, i64) {
            %20 = "polygeist.memref2pointer"(%17) : (memref<?xi8>) -> !llvm.ptr
            %21 = llvm.load %20 : !llvm.ptr -> f64
            %22 = "polygeist.memref2pointer"(%18) : (memref<?xi8>) -> !llvm.ptr
            %23 = llvm.load %22 : !llvm.ptr -> f64
            %24 = arith.cmpi sgt, %11, %c0_i32 : i32
            %25 = arith.select %24, %23, %cst : f64
            %26 = arith.addf %21, %25 : f64
            %27 = "polygeist.memref2pointer"(%16) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %26, %27 : f64, !llvm.ptr
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %8 = arith.index_cast %arg0 : index to i32
      %9 = "polygeist.memref2pointer"(%2) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %10 = arith.index_cast %arg0 : index to i64
      %11 = llvm.getelementptr %9[%10] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%5, %8, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = "polygeist.memref2pointer"(%3) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %15 = llvm.getelementptr %14[%10] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %16 = llvm.load %15 : !llvm.ptr -> f64
      %17 = llvm.call @printf(%7, %8, %16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

