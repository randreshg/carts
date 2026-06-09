// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir)' 2>&1 \
// RUN:   | %FileCheck %s

// A committed block/window finer than the previously tiled scheduling step is a
// stale SDE grain: the SU body still carries the coarse tile bounds while the
// physical plan claims a finer block. SDE-to-CODIR must NOT retile the cloned
// body to repair it -- the real retile is an SDE transform (Tiling). CODIR
// consumes the committed grain or fails closed.

// CHECK: committed owner-strip block/window finer than the scheduling step
// CHECK-SAME: CODIR does not retile a stale SU body
func.func @rejects_coarse_su_with_finer_committed_block(%input: memref<64xf32>,
                                                        %output: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c64 = arith.constant 64 : index
  sde.su_iterate (%c0) to (%c64) step (%c16) schedule(<static>)
      classification(<elementwise>) {
  ^bb0(%i: index):
    %tile_limit = arith.addi %i, %c16 : index
    %tile_upper = arith.minui %tile_limit, %c64 : index
    sde.cu_region <single> {
      scf.for %j = %i to %tile_upper step %c1 {
        %v = memref.load %input[%j] : memref<64xf32>
        memref.store %v, %output[%j] : memref<64xf32>
      }
    }
    sde.yield
  } {physicalOwnerDims = [0], physicalBlockShape = [4],
     logicalWorkerSlice = [4],
     iterationTopology = #sde.iteration_topology<owner_strip>,
     pattern = #sde.pattern<uniform>}
  return
}
