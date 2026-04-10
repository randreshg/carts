// RUN: %carts-compile %S/../inputs/snapshots/conv2d_openmp_to_arts.mlir --pipeline concurrency --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Neighborhood-shaped kernels like convolution-2d may be classified into the
// stencil family by pattern discovery, but they do not carry an explicit
// halo/ownership contract. The stencil strip coarsening policy must therefore
// leave full worker parallelism intact and avoid forcing a block-size hint.
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-NOT: arts.partition_hint = {blockSize =
