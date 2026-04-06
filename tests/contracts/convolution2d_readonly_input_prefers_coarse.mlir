// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/convolution-2d/convolution-2d.c --pipeline db-partitioning --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/convolution-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DNI=1024 -DNJ=1024 -DNREPS=1 -lm >/dev/null && cat %t.compile/convolution-2d.db-partitioning.mlir' | %FileCheck %s

// For single-node convolution-style neighborhood kernels, the read-only input
// should stay coarse while the output remains block-partitioned. This keeps
// row-halo reads local without forcing cross-chunk DB expansion on the input.
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<coarse>)
// CHECK-NOT: arts.db_acquire[<in>] {{.*}} arts.partition_hint = {blockSize =
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>

module {
}
