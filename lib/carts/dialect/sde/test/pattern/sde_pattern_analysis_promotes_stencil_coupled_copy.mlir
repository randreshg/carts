// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg --start-from sde-planning --pipeline sde-planning --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// SDE-5 reconciliation. In an iterative double-buffer timestep the elementwise
// copy `u = unew` writes `u`, and a sibling stencil reads `u` with a halo and so
// fixes `u` as a 2-D owner-tile array. A rank-2 elementwise copy is normally NOT
// promoted (the owner-tile of an unrelated/large elementwise space is not worth
// the inner-loop split), but when a sibling stencil consumes the written array
// the copy MUST adopt the same 2-D owner loop so `u` carries a single layout and
// the per-timestep host bridge can hoist. PatternAnalysis must therefore promote
// the coupled copy's inner loop into a second SDE owner dimension.

// CHECK-LABEL: // -----// IR Dump After PatternAnalysis (sde-pattern-analysis) //----- //
// CHECK: func.func @main
// The stencil-coupled copy is promoted from a 1-D owner loop to a 2-D owner loop.
// CHECK: sde.su_iterate (%c0, %c0) to
// CHECK-SAME: classification(<elementwise>)

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%u: memref<64x64xf64>, %unew: memref<64x64xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    %c64 = arith.constant 64 : index
    // Phase 1: elementwise copy u = unew (writes u, reads unew). 1-D owner loop
    // with an inner column scf.for, exactly as C/OpenMP exposes it.
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c0 to %c64 step %c1 {
          %v = memref.load %unew[%i, %j] : memref<64x64xf64>
          memref.store %v, %u[%i, %j] : memref<64x64xf64>
        }
        sde.yield
      }
      sde.yield
    }
    // Phase 2: 5-point stencil unew = f(u) (writes unew, reads u with a halo).
    // This sibling consumer fixes u as a 2-D owner-tile array.
    sde.cu_region <parallel> {
      sde.su_iterate (%c1) to (%c63) step (%c1) {
      ^bb0(%i: index):
        scf.for %j = %c1 to %c63 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %jm1 = arith.subi %j, %c1 : index
          %jp1 = arith.addi %j, %c1 : index
          %x0 = memref.load %u[%im1, %j] : memref<64x64xf64>
          %x1 = memref.load %u[%ip1, %j] : memref<64x64xf64>
          %y0 = memref.load %u[%i, %jm1] : memref<64x64xf64>
          %y1 = memref.load %u[%i, %jp1] : memref<64x64xf64>
          %s0 = arith.addf %x0, %x1 : f64
          %s1 = arith.addf %y0, %y1 : f64
          %sum = arith.addf %s0, %s1 : f64
          memref.store %sum, %unew[%i, %j] : memref<64x64xf64>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}
