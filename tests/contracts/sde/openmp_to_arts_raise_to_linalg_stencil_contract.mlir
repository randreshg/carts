// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts | %FileCheck %s --check-prefix=OPENMP
// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline create-dbs | %FileCheck %s --check-prefix=DB

// Verify that a raiseable stencil loop lowered through linalg.generic still
// re-emits loop-based ARTS IR with the downstream-visible stencil contract.

// OPENMP-LABEL: func.func @main
// OPENMP: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_pattern = #arts.distribution_pattern<stencil>
// OPENMP: arts.for(%c1) to(%c63) step(%c1)
// OPENMP: scf.for %[[J:.*]] = %c1 to %c63 step %c1
// OPENMP: memref.load %arg0[%0, %[[J]]] : memref<64x64xf64>
// OPENMP: memref.load %arg0[%arg2, %3] : memref<64x64xf64>
// OPENMP: memref.store %{{.*}}, %arg1[%arg2, %[[J]]] : memref<64x64xf64>
// OPENMP: } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_pattern = #arts.distribution_pattern<stencil>}
// OPENMP-NOT: linalg.generic
// OPENMP-NOT: arts_sde.

// DB-LABEL: func.func @main
// DB: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]
// DB: } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
          scf.for %j = %c1 to %c63 step %c1 {
            %im1 = arith.subi %i, %c1 : index
            %ip1 = arith.addi %i, %c1 : index
            %jm1 = arith.subi %j, %c1 : index
            %jp1 = arith.addi %j, %c1 : index
            %n = memref.load %A[%im1, %j] : memref<64x64xf64>
            %s = memref.load %A[%ip1, %j] : memref<64x64xf64>
            %w = memref.load %A[%i, %jm1] : memref<64x64xf64>
            %e = memref.load %A[%i, %jp1] : memref<64x64xf64>
            %c = memref.load %A[%i, %j] : memref<64x64xf64>
            %s0 = arith.addf %n, %s : f64
            %s1 = arith.addf %w, %e : f64
            %s2 = arith.addf %s0, %s1 : f64
            %sum = arith.addf %s2, %c : f64
            memref.store %sum, %B[%i, %j] : memref<64x64xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
