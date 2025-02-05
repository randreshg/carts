module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
    %3 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
    arts.edt parameters(%1) : (i32), constants(%c1, %c0, %c-1_i32, %c0_i32, %cst, %c100) : (index, index, i32, i32, f64, index), dependencies(%2, %3) : (memref<100xf64>, memref<100xf64>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %9 = arith.index_cast %arg0 : index to i32
          %10 = arts.datablock "out", %2 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          arts.edt parameters(%9, %8, %arg0) : (i32, f64, index), dependencies(%10) : (memref<1xf64>) {
            %16 = arith.sitofp %9 : i32 to f64
            %17 = arith.addf %16, %8 : f64
            memref.store %17, %10[%c0] : memref<1xf64>
            arts.yield
          }
          %11 = arith.addi %9, %c-1_i32 : i32
          %12 = arith.index_cast %11 : i32 to index
          %13 = arts.datablock "in", %2 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          %14 = arts.datablock "in", %2 : memref<100xf64>[%12] [%c1] [%c1] {isLoad} : memref<1xf64>
          %15 = arts.datablock "out", %3 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          arts.edt parameters(%9, %arg0, %12) : (i32, index, index), constants(%c0_i32, %cst) : (i32, f64), dependencies(%13, %14, %15) : (memref<1xf64>, memref<1xf64>, memref<1xf64>) {
            %16 = memref.load %13[%c0] : memref<1xf64>
            %17 = memref.load %14[%c0] : memref<1xf64>
            %18 = arith.cmpi sgt, %9, %c0_i32 : i32
            %19 = arith.select %18, %17, %cst : f64
            %20 = arith.addf %16, %19 : f64
            memref.store %20, %15[%c0] : memref<1xf64>
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %6 = affine.load %alloca_0[0] : memref<100xf64>
    %7 = llvm.call @printf(%5, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

