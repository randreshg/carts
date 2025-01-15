module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c8_i64 = arith.constant 8 : i64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = arith.extsi %arg0 : i32 to i64
    %1 = arith.muli %0, %c8_i64 : i64
    %2 = arith.index_cast %1 : i64 to index
    %3 = arith.divui %2, %c8 : index
    %alloc = memref.alloc(%3) : memref<?xf64>
    %alloc_0 = memref.alloc(%3) : memref<?xf64>
    %4 = call @rand() : () -> i32
    %5 = arith.remsi %4, %c100_i32 : i32
    %6 = arts.make_dep "inout", %alloc : memref<?xf64> : !arts.dep
    %7 = arts.make_dep "inout", %alloc_0 : memref<?xf64> : !arts.dep
    arts.parallel parameters(%arg0, %5, %c-1_i32, %c0_i32, %c0, %c1) : (i32, i32, i32, i32, index, index), dependencies(%6, %7) : (!arts.dep, !arts.dep) {
      arts.barrier
      arts.single {
        %8 = arith.index_cast %arg0 : i32 to index
        %9 = arith.sitofp %5 : i32 to f64
        scf.for %arg3 = %c0 to %8 step %c1 {
          %10 = arith.index_cast %arg3 : index to i32
          %11 = memref.load %alloc[%arg3] : memref<?xf64>
          %12 = arts.make_dep "out", %11 : f64 : !arts.dep
          arts.edt parameters(%10, %9, %arg3) : (i32, f64, index), dependencies(%12) : (!arts.dep) {
            %21 = arith.sitofp %10 : i32 to f64
            %22 = arith.addf %21, %9 : f64
            memref.store %22, %alloc[%arg3] : memref<?xf64>
          }
          %13 = memref.load %alloc[%arg3] : memref<?xf64>
          %14 = arith.addi %10, %c-1_i32 : i32
          %15 = arith.index_cast %14 : i32 to index
          %16 = memref.load %alloc[%15] : memref<?xf64>
          %17 = memref.load %alloc_0[%arg3] : memref<?xf64>
          %18 = arts.make_dep "in", %13 : f64 : !arts.dep
          %19 = arts.make_dep "in", %16 : f64 : !arts.dep
          %20 = arts.make_dep "out", %17 : f64 : !arts.dep
          arts.edt parameters(%10, %c0_i32, %arg3) : (i32, i32, index), dependencies(%18, %19, %20) : (!arts.dep, !arts.dep, !arts.dep) {
            %21 = arith.cmpi sgt, %10, %c0_i32 : i32
            scf.if %21 {
              %22 = arith.addf %13, %16 : f64
              memref.store %22, %alloc_0[%arg3] : memref<?xf64>
            } else {
              memref.store %13, %alloc_0[%arg3] : memref<?xf64>
            }
          }
        }
      }
      arts.barrier
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

