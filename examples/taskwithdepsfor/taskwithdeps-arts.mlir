-----------------------------------------
ConvertOpenMPToARTSPass STARTED
-----------------------------------------

[convert-openmp-to-arts] Converting omp.task to arts.edt
Naive collection of parameters and dependencies: 
  Adding parameter: %19 = "arith.index_cast"(%arg3) : (index) -> i32
  Adding parameter: %18 = "arith.sitofp"(%11) : (i32) -> f64
  Adding dependency: %9 = "memref.alloc"(%8) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
  Adding parameter: <block argument> of type 'index' at index: 0
Adding dependency: %20 = "memref.load"(%9, %arg3) : (memref<?xf64>, index) -> f64 with mode: out
   Dependency already added as memref - Removing it and creating makedep
Naive collection of parameters and dependencies: 

[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
Naive collection of parameters and dependencies: 
  Adding parameter: <block argument> of type 'i32' at index: 0
  Adding parameter: %11 = "arith.remsi"(%10, %4) : (i32, i32) -> i32
  Adding dependency: %9 = "memref.alloc"(%8) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
Naive collection of parameters and dependencies: 
Adding dependency: %9 = "memref.alloc"(%8) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64> with mode: inout
   Dependency already added as memref - Removing it and creating makedep
-----------------------------------------
ConvertOpenMPToARTSPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c8 = arith.constant 8 : index
    %c8_i64 = arith.constant 8 : i64
    %c100_i32 = arith.constant 100 : i32
    %0 = arith.extsi %arg0 : i32 to i64
    %1 = arith.muli %0, %c8_i64 : i64
    %2 = arith.index_cast %1 : i64 to index
    %3 = arith.divui %2, %c8 : index
    %4 = arts.alloc(%3) : memref<?xf64>
    %5 = call @rand() : () -> i32
    %6 = arith.remsi %5, %c100_i32 : i32
    %7 = arts.datablock "inout", %4 : memref<?xf64>[%c0] [%3] [%c1] : memref<?xf64>
    arts.parallel parameters(%arg0, %6) : (i32, i32), constants(%c1, %c0) : (index, index), dependencies(%7) : (memref<?xf64>) {
      arts.barrier
      arts.single {
        %12 = arith.index_cast %arg0 : i32 to index
        %13 = arith.sitofp %6 : i32 to f64
        scf.for %arg3 = %c0 to %12 step %c1 {
          %14 = arith.index_cast %arg3 : index to i32
          %15 = arts.datablock "out", %7 : memref<?xf64>[%arg3] [%c1] [%c1] : memref<1xf64>
          arts.edt parameters(%14, %13, %arg3) : (i32, f64, index), constants(%c0) : (index), dependencies(%15) : (memref<1xf64>) {
            %16 = arith.sitofp %14 : i32 to f64
            %17 = arith.addf %16, %13 : f64
            memref.store %17, %15[%c0] : memref<1xf64>
          }
        }
      }
      arts.barrier
    }
    %8 = llvm.mlir.addressof @str0 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %10 = affine.load %4[0] : memref<?xf64>
    %11 = llvm.call @printf(%9, %10) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

