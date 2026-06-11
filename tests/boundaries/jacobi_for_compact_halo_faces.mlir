// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering
// RUN: %FileCheck %s --implicit-check-not='host_whole' \
// RUN:   --implicit-check-not='local_only' \
// RUN:   --implicit-check-not='distributed_reject_reason' \
// RUN:   --implicit-check-not='arts.db_alloc{{.*}}<coarse>{{.*}}elementType(f64)' \
// RUN:   --implicit-check-not='byte_sizes({{.*}}%c8192' \
// RUN:   --implicit-check-not='element_sizes[%c1, %c1, %c32, %c32]{{.*}}haloViewDependency' \
// RUN:   < %t.dir/jacobi-for.pre-lowering.mlir

// SIZE=256 uses 32x32 block DBs on an 8x8 owner grid. The stencil frontier
// must lower with compact row byte windows and compact column payloads, not
// full-block halo byte windows.

// CHECK-DAG: arts.db_acquire[<in>] {{.*}} element_offsets[%c0, %c0, %c31, %c0] element_sizes[%c1, %c1, %c1, %c32]{{.*}}haloViewDependency
// CHECK-DAG: arts.db_acquire[<in>] {{.*}} element_offsets[%c0, %c0, %c0, %c0] element_sizes[%c1, %c1, %c1, %c32]{{.*}}haloViewDependency

// CHECK: %[[HALO_EDT:[A-Za-z0-9_]+]] = arts_rt.edt_create
// CHECK: arts_rt.rec_dep %[[HALO_EDT]]
// CHECK-SAME: byte_offsets(%c0, %c0, %c7936, %c0, %c0, %c0, %c0)
// CHECK-SAME: byte_sizes(%c0, %c0, %c256, %c256, %c0, %c0, %c0)
// CHECK-SAME: acquire_modes = array<i32: 1, 1, 1, 1, 1, 1, 2>
// CHECK-SAME: dep_flags = array<i32: 0, 0, 4, 4, 0, 0, 0>
// CHECK: arts_rt.wait_on_epoch
