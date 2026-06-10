// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering
// RUN: %FileCheck %s < %t.dir/jacobi-for.pre-lowering.mlir

// A host-whole to compute-block bridge for an sde.mu_alloc root can drain
// compatible residual writers in sibling dispatch anchors. Those writers dirty
// the canonical block DB after the original bridge participant set was chosen,
// so CODIR-to-ARTS must copy the block DB back to the coarse host DB before the
// next loop-carried host/coarse read and before final host verification.

// CHECK: %[[UNEW_GUID:[A-Za-z0-9_]+]], %[[UNEW_PTR:[A-Za-z0-9_]+]] = arts.db_alloc{{.*}}<coarse>{{.*}}{arts.create_id = 3000 : i64}
// CHECK: %[[UNEW_HOST_PTR:[A-Za-z0-9_]+]] = arts_rt.db_gep(%[[UNEW_PTR]] : memref<?x!llvm.ptr>)

// The timestep stencil writes unew through the drained block DB.
// CHECK: arts_rt.edt_create{{.*}}{arts.create_id = 6000 : i64, arts.outlined_func = "__arts_edt_6"}
// CHECK: arts_rt.wait_on_epoch
// The drained writer must be followed by a block-to-host copy-out of the same
// coarse unew DB before the next timestep reloads unew.
// CHECK: arts.db_acquire[<inout>] (%[[UNEW_GUID]] : memref<?xi64>, %[[UNEW_PTR]] : memref<?x!llvm.ptr>) partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>] (
// CHECK: arts_rt.rec_dep {{.*}}{acquire_modes = array<i32: 2, 1{{[^>]*}}>}

// The final stencil also writes a drained block DB.
// CHECK: arts_rt.edt_create{{.*}}{arts.create_id = 12000 : i64, arts.outlined_func = "__arts_edt_12"}
// CHECK: arts_rt.wait_on_epoch
// Final host verification reads the coarse unew DB, so this copy-out must be
// present after the final stencil.
// CHECK: arts.db_acquire[<inout>] (%[[UNEW_GUID]] : memref<?xi64>, %[[UNEW_PTR]] : memref<?x!llvm.ptr>) partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>] (
// CHECK: arts_rt.rec_dep {{.*}}{acquire_modes = array<i32: 2, 1{{[^>]*}}>}
// CHECK: llvm.load %[[UNEW_HOST_PTR]]
