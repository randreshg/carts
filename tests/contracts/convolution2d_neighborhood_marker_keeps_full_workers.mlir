// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/convolution-2d/convolution-2d.c --pipeline concurrency --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/convolution-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DNI=1024 -DNJ=1024 -DNREPS=1 -lm >/dev/null && cat %t.compile/convolution-2d.concurrency.mlir' | %FileCheck %s

// Neighborhood-shaped kernels like convolution-2d may be classified into the
// stencil family by pattern discovery, but they do not carry an explicit
// halo/ownership contract. The stencil strip coarsening policy must therefore
// leave full worker parallelism intact and avoid forcing a block-size hint.
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-NOT: arts.partition_hint = {blockSize =

module {
}
