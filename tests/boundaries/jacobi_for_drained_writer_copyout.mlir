// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering
// RUN: %FileCheck %s --implicit-check-not='host_whole' \
// RUN:   --implicit-check-not='local_only' \
// RUN:   --implicit-check-not='distributed_reject_reason' \
// RUN:   --implicit-check-not='arts.db_alloc{{.*}}<coarse>{{.*}}elementType(f64)' \
// RUN:   < %t.dir/jacobi-for.pre-lowering.mlir

// The direct SDE-to-ARTS path keeps Jacobi state in block DBs through the
// timestep and final host verification. Residual writer phases must be observed
// through block acquires and direct block DB reads, not through a coarse
// host-whole fallback. Scalar verification results may still use a single-block
// DB; they are not Jacobi state storage.

// CHECK: %[[UNEW_GUID:[A-Za-z0-9_]+]], %[[UNEW_PTR:[A-Za-z0-9_]+]] = arts.db_alloc{{.*}}<block>{{.*}}{arts.create_id = 3000 : i64

// The timestep stencil writes unew through block DB dependencies.
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}stencil_supported_block_halo
// CHECK-NOT: (%[[UNEW_GUID]]{{.*}}partitioning(<coarse>)
// CHECK: arts.db_acquire[<out>] (%[[UNEW_GUID]] : memref<?x?xi64>, %[[UNEW_PTR]] : memref<?x?x!llvm.ptr>) partitioning(<block>)
// CHECK: arts_rt.edt_create{{.*}}{arts.create_id = 5000 : i64, arts.outlined_func = "__arts_edt_5"}
// CHECK: arts_rt.rec_dep
// CHECK-SAME: byte_offsets(
// CHECK-SAME: byte_sizes(
// CHECK-SAME: acquire_modes = array<i32: 1, 1, 2>
// CHECK-SAME: dep_flags = array<i32: 0, 4, 0>
// CHECK: arts_rt.wait_on_epoch

// Final verification consumes block DBs in an EDT; the host only observes the
// scalar verification result DB.
// CHECK: arts.db_acquire[<in>] (%[[UNEW_GUID]] : memref<?x?xi64>, %[[UNEW_PTR]] : memref<?x?x!llvm.ptr>) partitioning(<block>)
// CHECK: arts_rt.edt_create{{.*}}{arts.create_id = 6000 : i64, arts.outlined_func = "__arts_edt_6"}
// CHECK: arts_rt.rec_dep
// CHECK-SAME: acquire_modes = array<i32: 1, 1, 2>
// CHECK: arts_rt.db_gep
// CHECK: llvm.load
