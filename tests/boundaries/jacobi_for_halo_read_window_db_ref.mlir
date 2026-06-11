// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement
// RUN: %FileCheck %s --implicit-check-not='element_sizes[%c1, %c1, %c32, %c32]{{.*}}haloViewDependency' \
// RUN:   < %t.dir/jacobi-for.post-db-refinement.mlir

// ARTS materializes 2D unit halos as explicit face dependencies. Row faces are
// direct contiguous halo byte windows; column faces are compact payload DBs
// packed before compute. This must not regress to one full-block halo acquire.

// CHECK-DAG: arts.db_alloc{{.*}}elementSizes[%c1, %c1, %c32, %c1]{{.*}}compact_halo_payload
// CHECK-DAG: arts.db_alloc{{.*}}elementSizes[%c1, %c1, %c32, %c1]{{.*}}compact_halo_payload
// CHECK: arts.edt <task> <internode> route{{.*}} attributes {compactHaloPack}
// CHECK: arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}

// CHECK: arts.db_acquire[<in>] {{.*}} element_offsets[%c0, %c0, %c31, %c0] element_sizes[%c1, %c1, %c1, %c32]
// CHECK-SAME: haloViewDependency
// CHECK-SAME: stencil_max_offsets = [0, 0]
// CHECK-SAME: stencil_min_offsets = [-1, 0]
// CHECK: arts.db_acquire[<in>] {{.*}} element_offsets[%c0, %c0, %c0, %c0] element_sizes[%c1, %c1, %c1, %c32]
// CHECK-SAME: haloViewDependency
// CHECK-SAME: stencil_max_offsets = [1, 0]
// CHECK-SAME: stencil_min_offsets = [0, 0]

// CHECK: arts.edt <task> <internode> route{{.*}}(%{{[^,]+}}, %{{[^,]+}}, %{{[^,]+}}, %{{[^,]+}}, %{{[^,]+}}, %{{[^,]+}}, %{{[^)]+}})
// CHECK-SAME: perBlockHaloExchange
// CHECK: scf.if {{.*}} -> (f64)
