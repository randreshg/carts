module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c100 = arith.constant 100 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %0 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %1 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    arts.edt dependencies(%0, %1) : (memref<100x100xf64>, memref<100x100xf64>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %12 = arith.sitofp %3 : i32 to f64
        affine.for %arg0 = 0 to 100 {
          %15 = arith.index_cast %arg0 : index to i32
          %16 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
          arts.edt dependencies(%16) : (memref<1x1xf64>) {
            %17 = arith.sitofp %15 : i32 to f64
            %18 = arith.addf %17, %12 : f64
            affine.for %arg1 = 0 to 100 {
              affine.store %18, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
            }
            arts.yield
          }
        }
        %13 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        %14 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%c0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
        arts.edt dependencies(%14, %13) : (memref<1x1xf64>, memref<1x1xf64>) {
          affine.for %arg0 = 0 to 100 {
            %15 = affine.load %alloca_0[0, %arg0] : memref<100x100xf64>
            affine.store %15, %alloca[0, %arg0] : memref<100x100xf64>
          }
          arts.yield
        }
        affine.for %arg0 = 1 to 100 {
          %15 = arith.index_cast %arg0 : index to i32
          %16 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
          %17 = arith.addi %15, %c-1_i32 : i32
          %18 = arith.index_cast %17 : i32 to index
          %19 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%18, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
          %20 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%arg0, %c0], sizes[%c1, %c1], type[f64], typeSize[%c8] {isLoad} -> memref<1x1xf64>
          arts.edt dependencies(%20, %16, %19) : (memref<1x1xf64>, memref<1x1xf64>, memref<1x1xf64>) {
            affine.for %arg1 = 0 to 100 {
              %21 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              %22 = affine.load %alloca_0[%arg0 - 1, %arg1] : memref<100x100xf64>
              %23 = arith.addf %21, %22 : f64
              affine.store %23, %alloca[%arg0, %arg1] : memref<100x100xf64>
            }
            arts.yield
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
      %12 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 100 {
        %14 = arith.index_cast %arg1 : index to i32
        %15 = affine.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %16 = llvm.call @printf(%5, %12, %14, %15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %13 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %8 = llvm.mlir.addressof @str2 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %10 = llvm.mlir.addressof @str1 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    affine.for %arg0 = 0 to 100 {
      %12 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 100 {
        %14 = arith.index_cast %arg1 : index to i32
        %15 = affine.load %alloca[%arg0, %arg1] : memref<100x100xf64>
        %16 = llvm.call @printf(%9, %12, %14, %15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %13 = llvm.call @printf(%11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

