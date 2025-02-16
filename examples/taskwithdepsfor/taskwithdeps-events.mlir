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
    %0 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %1 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [f64, %c8] -> memref<100xf64>
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    arts.edt dependencies(%0, %1) : (memref<100xf64>, memref<100xf64>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arts.event[%c100] -> : memref<100xi64>
        %9 = arith.sitofp %3 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %10 = arith.index_cast %arg0 : index to i32
          %11 = arts.datablock "out", %1 : memref<100xf64>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {ptrIsDb, isLoad}
          %12 = memref.load %8[%arg0] : memref<100xi64>
          arts.edt dependencies(%11) : (memref<1xf64>), events(%12) : (i64) {
            %19 = arith.sitofp %10 : i32 to f64
            %20 = arith.addf %19, %9 : f64
            memref.store %20, %11[%c0] : memref<1xf64>
            arts.yield
          }
          %13 = arts.datablock "in", %1 : memref<100xf64>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {ptrIsDb, isLoad}
          %14 = arith.addi %10, %c-1_i32 : i32
          %15 = arith.index_cast %14 : i32 to index
          %16 = arts.datablock "in", %1 : memref<100xf64>[%15] [%c1] [f64, %c8] -> memref<1xf64> {ptrIsDb, isLoad}
          %17 = arts.datablock "out", %0 : memref<100xf64>[%arg0] [%c1] [f64, %c8] -> memref<1xf64> {ptrIsDb, isLoad}
          %18 = memref.load %8[%15] : memref<100xi64>
          arts.edt dependencies(%16, %17, %13) : (memref<1xf64>, memref<1xf64>, memref<1xf64>), events(%18, %c-1_i32, %12) : (i64, i32, i64) {
            %19 = memref.load %13[%c0] : memref<1xf64>
            %20 = memref.load %16[%c0] : memref<1xf64>
            %21 = arith.cmpi sgt, %10, %c0_i32 : i32
            %22 = arith.select %21, %20, %cst : f64
            %23 = arith.addf %19, %22 : f64
            memref.store %23, %17[%c0] : memref<1xf64>
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
      %9 = memref.load %1[%arg0] : memref<100xf64>
      %10 = llvm.call @printf(%5, %8, %9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %11 = memref.load %0[%arg0] : memref<100xf64>
      %12 = llvm.call @printf(%7, %8, %11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

