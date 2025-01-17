-----------------------------------------
ConvertOpenMPToARTSPass STARTED
-----------------------------------------

[convert-openmp-to-arts] Converting omp.task to arts.edt
Naive collection of parameters and dependencies: 
  Adding parameter: %21 = "arith.index_cast"(%arg3) : (index) -> i32
  Adding parameter: %26 = "memref.load"(%12, %25) : (memref<?xf64>, index) -> f64
  Adding parameter: %23 = "memref.load"(%12, %arg3) : (memref<?xf64>, index) -> f64
  Adding dependency: %13 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
  Adding parameter: <block argument> of type 'index' at index: 0
Adding dependency: %23 = "memref.load"(%12, %arg3) : (memref<?xf64>, index) -> f64 with mode: in
Adding dependency: %27 = "memref.load"(%12, %26) : (memref<?xf64>, index) -> f64 with mode: in
Adding dependency: %29 = "memref.load"(%13, %arg3) : (memref<?xf64>, index) -> f64 with mode: out
   Dependency already added as memref - Removing it and creating makedep

[convert-openmp-to-arts] Converting omp.task to arts.edt
Naive collection of parameters and dependencies: 
  Adding parameter: %24 = "arith.index_cast"(%arg3) : (index) -> i32
  Adding parameter: %15 = "arith.remsi"(%14, %7) : (i32, i32) -> i32
  Adding dependency: %12 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
  Adding parameter: <block argument> of type 'index' at index: 0
Adding dependency: %25 = "memref.load"(%12, %arg3) : (memref<?xf64>, index) -> f64 with mode: out
   Dependency already added as memref - Removing it and creating makedep

[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
Naive collection of parameters and dependencies: 
  Adding parameter: <block argument> of type 'i32' at index: 0
  Adding dependency: %12 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
  Adding parameter: %15 = "arith.remsi"(%14, %7) : (i32, i32) -> i32
  Adding dependency: %13 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
Adding dependency: %12 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64> with mode: inout
   Dependency already added as memref - Removing it and creating makedep
Adding dependency: %13 = "memref.alloc"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64> with mode: inout
   Dependency already added as memref - Removing it and creating makedep
-----------------------------------------
ConvertOpenMPToARTSPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c8_i64 = arith.constant 8 : i64
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = arith.extsi %arg0 : i32 to i64
    %1 = arith.muli %0, %c8_i64 : i64
    %2 = arith.index_cast %1 : i64 to index
    %3 = arith.divui %2, %c8 : index
    %4 = arts.alloc(%3) : memref<?xf64>
    %5 = arts.alloc(%3) : memref<?xf64>
    %6 = call @rand() : () -> i32
    %7 = arith.remsi %6, %c100_i32 : i32
    %8 = arts.make_dep "inout", %4 : memref<?xf64> : !arts.dep
    %9 = arts.make_dep "inout", %5 : memref<?xf64> : !arts.dep
    arts.parallel parameters(%arg0, %7) : (i32, i32), dependencies(%8, %9) : (!arts.dep, !arts.dep) {
      arts.barrier
      arts.single {
        %10 = arith.index_cast %arg0 : i32 to index
        scf.for %arg3 = %c0 to %10 step %c1 {
          %11 = arith.index_cast %arg3 : index to i32
          %12 = memref.load %4[%arg3] : memref<?xf64>
          %13 = arts.make_dep "out", %12 : f64 : !arts.dep
          arts.edt parameters(%11, %7, %arg3) : (i32, i32, index), dependencies(%13) : (!arts.dep) {
            %22 = arith.sitofp %11 : i32 to f64
            %23 = arith.sitofp %7 : i32 to f64
            %24 = arith.addf %22, %23 : f64
            memref.store %24, %4[%arg3] : memref<?xf64>
          }
          %14 = memref.load %4[%arg3] : memref<?xf64>
          %15 = arith.addi %11, %c-1_i32 : i32
          %16 = arith.index_cast %15 : i32 to index
          %17 = memref.load %4[%16] : memref<?xf64>
          %18 = memref.load %5[%arg3] : memref<?xf64>
          %19 = arts.make_dep "in", %14 : f64 : !arts.dep
          %20 = arts.make_dep "in", %17 : f64 : !arts.dep
          %21 = arts.make_dep "out", %18 : f64 : !arts.dep
          arts.edt parameters(%11, %arg3) : (i32, index), dependencies(%19, %20, %21) : (!arts.dep, !arts.dep, !arts.dep) {
            %22 = arith.cmpi sgt, %11, %c0_i32 : i32
            %23 = arith.select %22, %17, %cst : f64
            %24 = arith.addf %14, %23 : f64
            memref.store %24, %5[%arg3] : memref<?xf64>
          }
        }
      }
      arts.barrier
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

