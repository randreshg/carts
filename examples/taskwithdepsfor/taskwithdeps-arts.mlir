#map = affine_map<(d0, d1) -> (d0 + 1, d1 - 1)>
#map1 = affine_map<(d0) -> (d0, 0)>
#map2 = affine_map<(d0) -> (d0 - 1, 0)>
#set = affine_set<(d0) : (d0 - 1 >= 0)>
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str1("B[%d][%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f64
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %0 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], offsets[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %1 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], offsets[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    arts.edt dependencies(%0, %1) : (memref<100x100xf64>, memref<100x100xf64>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arith.sitofp %3 : i32 to f64
        affine.for %arg0 = 0 to 100 {
          %9 = arith.index_cast %arg0 : index to i32
          %10 = arith.sitofp %9 : i32 to f64
          %11 = arith.addf %10, %8 : f64
          affine.for %arg1 = 0 to 100 {
            %15 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0, %arg1], sizes[%c1, %c1], type[f64], typeSize[%c8], affineMap=#map {isLoad} -> memref<1x1xf64>
            arts.edt dependencies(%15) : (memref<1x1xf64>) {
              affine.store %11, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              arts.yield
            }
          }
          %12 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=#map1 {isLoad} -> memref<1x100xf64>
          %13 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], offsets[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=#map2 {isLoad} -> memref<1x100xf64>
          %14 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], offsets[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=#map1 {isLoad} -> memref<1x100xf64>
          arts.edt dependencies(%14, %13, %12) : (memref<1x100xf64>, memref<1x100xf64>, memref<1x100xf64>) {
            affine.for %arg1 = 0 to 100 {
              %15 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              %16 = affine.if #set(%arg0) -> f64 {
                %18 = affine.load %alloca_0[%arg0 - 1, %arg1] : memref<100x100xf64>
                affine.yield %18 : f64
              } else {
                affine.yield %cst : f64
              }
              %17 = arith.addf %15, %16 : f64
              affine.store %17, %alloca[%arg0, %arg1] : memref<100x100xf64>
            }
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    affine.for %arg0 = 0 to 100 {
      affine.for %arg1 = 0 to 100 {
        %8 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %9 = affine.if #set(%arg0) -> f64 {
          %11 = affine.load %alloca_0[%arg0 - 1, %arg1] : memref<100x100xf64>
          affine.yield %11 : f64
        } else {
          affine.yield %cst : f64
        }
        %10 = arith.addf %8, %9 : f64
        affine.store %10, %alloca[%arg0, %arg1] : memref<100x100xf64>
      }
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
    affine.for %arg0 = 0 to 100 {
      %8 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 100 {
        %9 = arith.index_cast %arg1 : index to i32
        %10 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %11 = llvm.call @printf(%5, %8, %9, %10) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
        %12 = affine.if #set(%arg0) -> f64 {
          %15 = affine.load %alloca_0[%arg0 - 1, %arg1] : memref<100x100xf64>
          affine.yield %15 : f64
        } else {
          affine.yield %cst : f64
        }
        %13 = arith.addf %10, %12 : f64
        %14 = llvm.call @printf(%7, %8, %9, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

