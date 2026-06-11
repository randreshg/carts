// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering
// RUN: %FileCheck %s --implicit-check-not='host_whole' \
// RUN:   --implicit-check-not='local_only' \
// RUN:   --implicit-check-not='distributed_reject_reason' \
// RUN:   --implicit-check-not='arts.db_alloc{{.*}}<coarse>{{.*}}elementType(f64)' \
// RUN:   < %t.dir/jacobi-for.pre-lowering.mlir

// Rank-expanded stencil storage has leading grid dims and trailing tile dims:
// SIZE=256 uses 32x32 block DBs on an 8x8 block grid. Halo movement remains a
// block-grain dependency with explicit stencil halo facts; it must not fall
// back to host-whole storage. Scalar verification results may still use a
// single-block DB; they are not Jacobi state storage.

// CHECK-DAG: arts.db_alloc{{.*}}<block>{{.*}}sizes[%c8, %c8]{{.*}}elementSizes[%c1, %c1, %c32, %c32]
// CHECK-DAG: arts.db_alloc{{.*}}<block>{{.*}}planHaloShape = [1, 1, 0, 0]

// The halo frontier is a phase-level graph, not an epoch nested inside each
// compute-block launch.
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}stencil_supported_block_halo
// CHECK: arts_rt.edt_param_pack
// CHECK: %[[HALO_EDT:[A-Za-z0-9_]+]] = arts_rt.edt_create(%{{[A-Za-z0-9_]+}} : memref<?xi64>){{.*}}epoch(%[[HALO_EPOCH:[A-Za-z0-9_]+]] : i64)
// CHECK: arts_rt.rec_dep %[[HALO_EDT]]
// CHECK-SAME: byte_offsets(
// CHECK-SAME: byte_sizes(
// CHECK-SAME: acquire_modes = array<i32: 1, 1, 2
// CHECK-SAME: dep_flags = array<i32: 0, 4, 0
// CHECK-NOT: arts_rt.create_epoch
// CHECK: arts_rt.wait_on_epoch %[[HALO_EPOCH]]
// CHECK: %[[VERIFY_PACK:[A-Za-z0-9_]+]] = arts_rt.edt_param_pack
// CHECK: %[[VERIFY_EDT:[A-Za-z0-9_]+]] = arts_rt.edt_create(%[[VERIFY_PACK]] : memref<0xi64>){{.*}}epoch(%[[VERIFY_EPOCH:[A-Za-z0-9_]+]] : i64)
// CHECK: arts_rt.rec_dep %[[VERIFY_EDT]]
// CHECK: arts_rt.wait_on_epoch %[[VERIFY_EPOCH]]
