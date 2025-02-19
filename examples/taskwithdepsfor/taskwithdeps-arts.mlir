#map = affine_map<(d0, d1) -> (d0, d1)>
#map1 = affine_map<(d0) -> (0, d0)>
#map2 = affine_map<(d0, d1) -> (d0 - 1, d1)>
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %0 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], offsets[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %1 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], offsets[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    arts.edt dependencies(%1, %0) : (memref<100x100xf64>, memref<100x100xf64>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %10 = arith.sitofp %3 : i32 to f64
        affine.for %arg0 = 0 to 100 {
          %11 = arith.index_cast %arg0 : index to i32
          %12 = arith.sitofp %11 : i32 to f64
          %13 = arith.addf %12, %10 : f64
          affine.for %arg1 = 0 to 100 {
            %14 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0, %arg1], sizes[%c1, %c1], type[f64], typeSize[%c8], affineMap=#map {isLoad} -> memref<1x1xf64>
            arts.edt dependencies(%14) : (memref<1x1xf64>) {
              affine.store %13, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              arts.yield
            }
          }
        }
        affine.for %arg0 = 0 to 100 {
          %11 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=#map1 {isLoad} -> memref<1x100xf64>
          %12 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], offsets[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=#map1 {isLoad} -> memref<1x100xf64>
          arts.edt dependencies(%12, %11) : (memref<1x100xf64>, memref<1x100xf64>) {
            %13 = affine.load %alloca_0[0, %arg0] : memref<100x100xf64>
            affine.store %13, %alloca[0, %arg0] : memref<100x100xf64>
            arts.yield
          }
        }
        affine.for %arg0 = 1 to 100 {
          affine.for %arg1 = 0 to 100 {
            %11 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0, %arg1], sizes[%c1, %c1], type[f64], typeSize[%c8], affineMap=#map {isLoad} -> memref<1x1xf64>
            %12 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0, %arg1], sizes[%c1, %c1], type[f64], typeSize[%c8], affineMap=#map2 {isLoad} -> memref<1x1xf64>
            %13 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], offsets[%arg0, %arg1], sizes[%c1, %c1], type[f64], typeSize[%c8], affineMap=#map {isLoad} -> memref<1x1xf64>
            arts.edt dependencies(%12, %13, %11) : (memref<1x1xf64>, memref<1x1xf64>, memref<1x1xf64>) {
              %14 = affine.load %alloca_0[%arg0 - 1, %arg1] : memref<100x100xf64>
              %15 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              %16 = arith.addf %15, %14 : f64
              affine.store %16, %alloca[%arg0, %arg1] : memref<100x100xf64>
              arts.yield
            }
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    affine.for %arg0 = 0 to 100 {
      %10 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 100 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%5, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %8 = llvm.mlir.addressof @str2 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    affine.for %arg0 = 0 to 100 {
      %10 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 100 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = affine.load %alloca[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%9, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

