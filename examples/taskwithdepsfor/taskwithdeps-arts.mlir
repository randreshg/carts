-----------------------------------------
ConvertOpenMPToARTSPass STARTED
-----------------------------------------

[convert-openmp-to-arts] Converting omp.task to arts.edt
Naive collection of parameters and dependencies: 
  Adding parameter: %20 = "arith.index_cast"(%arg0) : (index) -> i32
  Adding parameter: %25 = "memref.load"(%8, %24) : (memref<100xf64>, index) -> f64
  Adding parameter: %22 = "memref.load"(%8, %arg0) : (memref<100xf64>, index) -> f64
  Adding parameter: <block argument> of type 'index' at index: 0
Adding dependency: %22 = "memref.load"(%8, %arg0) : (memref<100xf64>, index) -> f64 with mode: in
Adding dependency: %26 = "memref.load"(%8, %25) : (memref<100xf64>, index) -> f64 with mode: in
Adding dependency: %28 = "memref.load"(%7, %arg0) : (memref<100xf64>, index) -> f64 with mode: out
Naive collection of parameters and dependencies: 
  Adding dependency: %34 = "arts.datablock"(%8, %arg0, %33, %31) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  Adding parameter: <block argument> of type 'index' at index: 0
  Adding dependency: %39 = "arts.datablock"(%8, %27, %38, %36) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  Adding parameter: %27 = "arith.index_cast"(%26) : (i32) -> index
  Adding parameter: %23 = "arith.index_cast"(%arg0) : (index) -> i32
  Adding dependency: %44 = "arts.datablock"(%7, %arg0, %43, %41) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>

[convert-openmp-to-arts] Converting omp.task to arts.edt
Naive collection of parameters and dependencies: 
  Adding parameter: %23 = "arith.index_cast"(%arg0) : (index) -> i32
  Adding parameter: %22 = "arith.sitofp"(%10) : (i32) -> f64
  Adding parameter: <block argument> of type 'index' at index: 0
Adding dependency: %24 = "memref.load"(%8, %arg0) : (memref<100xf64>, index) -> f64 with mode: out
Naive collection of parameters and dependencies: 
  Adding parameter: %24 = "arith.index_cast"(%arg0) : (index) -> i32
  Adding parameter: %23 = "arith.sitofp"(%10) : (i32) -> f64
  Adding dependency: %30 = "arts.datablock"(%8, %arg0, %29, %27) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>

[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
Naive collection of parameters and dependencies: 
  Adding parameter: %10 = "arith.remsi"(%9, %6) : (i32, i32) -> i32
Naive collection of parameters and dependencies: 
  Adding parameter: %10 = "arith.remsi"(%9, %6) : (i32, i32) -> i32
  Adding dependency: %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
  Adding dependency: %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
-----------------------------------------
ConvertOpenMPToARTSPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100 = arith.constant 100 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    arts.parallel parameters(%1) : (i32), constants(%c1, %c0, %c-1_i32, %c0_i32, %cst, %c100) : (index, index, i32, i32, f64, index), dependencies(%alloca_0, %alloca) : (memref<100xf64>, memref<100xf64>) {
      arts.barrier
      arts.single {
        %6 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %7 = arith.index_cast %arg0 : index to i32
          %8 = arts.datablock "out", %alloca_0 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          arts.edt parameters(%7, %6, %arg0) : (i32, f64, index), constants(%c0) : (index), dependencies(%8) : (memref<1xf64>) {
            %14 = arith.sitofp %7 : i32 to f64
            %15 = arith.addf %14, %6 : f64
            memref.store %15, %8[%c0] : memref<1xf64>
          }
          %9 = arith.addi %7, %c-1_i32 : i32
          %10 = arith.index_cast %9 : i32 to index
          %11 = arts.datablock "in", %alloca_0 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          %12 = arts.datablock "in", %alloca_0 : memref<100xf64>[%10] [%c1] [%c1] {isLoad} : memref<1xf64>
          %13 = arts.datablock "out", %alloca : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
          arts.edt parameters(%7, %arg0, %10) : (i32, index, index), constants(%c0_i32, %cst, %c0) : (i32, f64, index), dependencies(%11, %12, %13) : (memref<1xf64>, memref<1xf64>, memref<1xf64>) {
            %14 = memref.load %11[%arg0] : memref<1xf64>
            %15 = memref.load %12[%10] : memref<1xf64>
            %16 = arith.cmpi sgt, %7, %c0_i32 : i32
            %17 = arith.select %16, %15, %cst : f64
            %18 = arith.addf %14, %17 : f64
            memref.store %18, %13[%c0] : memref<1xf64>
          }
        }
      }
      arts.barrier
    }
    %2 = llvm.mlir.addressof @str0 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %4 = affine.load %alloca_0[0] : memref<100xf64>
    %5 = llvm.call @printf(%3, %4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

