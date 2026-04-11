// RUN: %carts-compile %S/../inputs/snapshots/conv2d_openmp_to_arts.mlir --pipeline concurrency --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Neighborhood-shaped kernels like convolution-2d are classified into the
// stencil_tiling_nd family with an explicit halo contract (owner dims, offsets).
// The concurrency stage must preserve full worker parallelism (64 workers) on
// the EDT while computing a block-size hint for halo-aware DB partitioning.
// CHECK: arts.edt <parallel>
// CHECK-SAME: workers = #arts.workers<64>
// CHECK: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-SAME: stencil_supported_block_halo
