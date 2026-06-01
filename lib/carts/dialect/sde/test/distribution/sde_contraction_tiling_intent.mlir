// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-planning \
// RUN:   --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Contraction tiling as SDE intent.
//
// This mirrors a chained matmul G = E * F, where F = C * D is a sibling-
// computed distributed intermediate that G reads on its CONTRACTION axis (the
// reduction loop dim k). SDE decides — pattern-free, from the canonical matmul
// access shapes — to tile k: it stamps the element-space `contractionTileShape`
// (aligned to the producer owner block) plus the inert declarative facts
// `partialReductionDims` / `partialReductionOwnerDims`. It must NOT set the
// `partialReduction` UNIT attr (the CODIR ReductionPlanning trigger) — there is
// no materializer for the cross-owner matmul k-tile case yet, so the intent
// stays inert and the consumer's lowering is unchanged.
//
// Phase 1: F = C * D            (sibling matmul, writes the intermediate F)
// Phase 2: Out = E * F          (reads F on the contraction window {k, j})
//
// E and Out are host arguments; F is a sibling-computed intermediate. Only the
// second matmul (whose contraction-dim input F is a sibling intermediate) gets
// the contraction-tiling intent. The first matmul (F = C * D) reads only host
// inputs on its contraction window and must NOT get the intent.

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%C: memref<256x256xf32>, %D: memref<256x256xf32>,
                  %E: memref<256x256xf32>, %Out: memref<256x256xf32>) {
    %c0 = arith.constant 0 : index
    %c256 = arith.constant 256 : index
    %c1 = arith.constant 1 : index
    %F = memref.alloc() : memref<256x256xf32>
    omp.parallel {
      // Phase 1: F = C * D.
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c256) step (%c1) {
          scf.for %j = %c0 to %c256 step %c1 {
            scf.for %k = %c0 to %c256 step %c1 {
              %c = memref.load %C[%i, %k] : memref<256x256xf32>
              %d = memref.load %D[%k, %j] : memref<256x256xf32>
              %old = memref.load %F[%i, %j] : memref<256x256xf32>
              %prod = arith.mulf %c, %d : f32
              %sum = arith.addf %old, %prod : f32
              memref.store %sum, %F[%i, %j] : memref<256x256xf32>
            }
          }
          omp.yield
        }
      }
      // Phase 2: Out = E * F. F is read on the contraction window {k, j}.
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c256) step (%c1) {
          scf.for %j = %c0 to %c256 step %c1 {
            scf.for %k = %c0 to %c256 step %c1 {
              %e = memref.load %E[%i, %k] : memref<256x256xf32>
              %f = memref.load %F[%k, %j] : memref<256x256xf32>
              %old = memref.load %Out[%i, %j] : memref<256x256xf32>
              %prod = arith.mulf %e, %f : f32
              %sum = arith.addf %old, %prod : f32
              memref.store %sum, %Out[%i, %j] : memref<256x256xf32>
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    memref.dealloc %F : memref<256x256xf32>
    return
  }
}

// After DistributionPlanning, exactly one matmul scheduling unit carries the
// contraction-tiling intent: the second one (Out = E * F), whose contraction
// input F is the sibling-computed intermediate. The tile shape is element-space
// and aligned to the producer owner block; under the realized finer-grain
// planning the refinement leaves the full contraction extent (k-tile = 256, a
// single inert tile), so the intent stays maximally inert; the
// partial-reduction dims name the reduction axis and the parallel owner axes.
// The first matmul (F = C * D, host inputs only) carries no such intent.
// CHECK-LABEL: // -----// IR Dump After DistributionPlanning
// The matmul attr-dict (printed on the scheduling unit's closing brace line)
// carries the contraction-tiling intent, the block-aligned element-space tile,
// and the inert partial-reduction dims.
// CHECK: classification(<matmul>)
// CHECK: classification(<matmul>)
// CHECK: contractionTileShape = [256]
// CHECK-SAME: partialReductionDims = [2]
// CHECK-SAME: partialReductionOwnerDims = [0, 1]
// CHECK-SAME: physicalBlockShape = [8, 256]

// The intent must NOT carry the `partialReduction` UNIT attr (the CODIR
// ReductionPlanning trigger): no cross-owner matmul k-tile materializer exists
// yet, so the facts stay inert and the consumer lowers unchanged. The only
// partial-reduction attrs present are the inert *Dims carriers.
// CHECK-NOT: partialReduction,
// CHECK-NOT: {{partialReduction[}]}}
