// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering
// RUN: %FileCheck %s --implicit-check-not='<coarse>' < %t.dir/jacobi-for.pre-lowering.mlir

// Rank-expanded stencil storage has leading grid dims and trailing tile dims:
// SIZE=256 uses 32x32 block DBs on an 8x8 block grid. Halo movement remains a
// block-grain dependency with explicit stencil halo facts; it must not fall
// back to coarse/host-whole storage.

// CHECK-DAG: arts.db_alloc{{.*}}<block>{{.*}}sizes[%c8, %c8]{{.*}}elementSizes[%c1, %c1, %c32, %c32]
// CHECK-DAG: arts.db_alloc{{.*}}<block>{{.*}}planHaloShape = [1, 1, 0, 0]
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}stencil_supported_block_halo
// CHECK: arts_rt.rec_dep
// CHECK-SAME: byte_offsets(
// CHECK-SAME: byte_sizes(
// CHECK-SAME: acquire_modes = array<i32: 1, 1, 2
// CHECK-SAME: dep_flags = array<i32: 0, 4, 0

// The halo frontier is a phase-level graph, not an epoch nested inside each
// compute-block launch.
// CHECK: [[HALO_EPOCH:%[0-9]+]] = arts_rt.create_epoch
// CHECK-NEXT: scf.for
// CHECK-NOT: arts_rt.create_epoch
// CHECK: arts_rt.wait_on_epoch [[HALO_EPOCH]]
// CHECK: [[COMPUTE_EPOCH:%[0-9]+]] = arts_rt.create_epoch
// CHECK-NEXT: scf.for
