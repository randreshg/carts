module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %3 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    arts.edt dependencies(%2, %3) : (memref<100x100xf64>, memref<100x100xf64>) attributes {task} {
      %10 = arith.sitofp %1 : i32 to f64
      scf.for %arg0 = %c0 to %c100 step %c1 {
        %13 = arith.index_cast %arg0 : index to i32
        %14 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
        arts.edt dependencies(%14) : (memref<100xf64>) attributes {task} {
          %15 = arith.sitofp %13 : i32 to f64
          %16 = arith.addf %15, %10 : f64
          scf.for %arg1 = %c0 to %c100 step %c1 {
            memref.store %16, %14[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      %11 = arts.datablock "in" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
      %12 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
      arts.edt dependencies(%11, %12) : (memref<100xf64>, memref<100xf64>) attributes {task} {
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %13 = memref.load %11[%arg0] : memref<100xf64>
          memref.store %13, %12[%arg0] : memref<100xf64>
        }
        arts.yield
      }
      scf.for %arg0 = %c1 to %c100 step %c1 {
        %13 = arith.index_cast %arg0 : index to i32
        %14 = arith.addi %13, %c-1_i32 : i32
        %15 = arith.index_cast %14 : i32 to index
        %16 = arts.datablock "in" ptr[%2 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
        %17 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
        %18 = arts.datablock "in" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] -> memref<100xf64>
        arts.edt dependencies(%16, %17, %18) : (memref<100xf64>, memref<100xf64>, memref<100xf64>) attributes {task} {
          scf.for %arg1 = %c0 to %c100 step %c1 {
            %19 = memref.load %18[%arg1] : memref<100xf64>
            %20 = memref.load %16[%arg1] : memref<100xf64>
            %21 = arith.addf %19, %20 : f64
            memref.store %21, %17[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %2[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%5, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %8 = llvm.mlir.addressof @str2 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %3[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%9, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

