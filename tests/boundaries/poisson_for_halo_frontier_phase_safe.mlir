// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts cgeist external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c \
// RUN:   -O3 -DSIZE=256 -DNREPS=10 -Iexternal/carts-benchmarks/kastors-jacobi/include \
// RUN:   -S -fopenmp -o %t.dir/poisson-for.mlir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts compile %t.dir/poisson-for.mlir -O3 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering \
// RUN:   > %t.dir/poisson-for.pre-lowering.mlir
// RUN: %FileCheck %s --check-prefix=RT --implicit-check-not='host_whole' \
// RUN:   --implicit-check-not='local_only' \
// RUN:   --implicit-check-not='arts.db_alloc{{.*}}<coarse>{{.*}}elementType(f64)' \
// RUN:   --implicit-check-not='byte_sizes({{.*}}%c8192' \
// RUN:   --implicit-check-not='element_sizes[%c1, %c1, %c32, %c32]{{.*}}haloViewDependency' \
// RUN:   < %t.dir/poisson-for.pre-lowering.mlir

// RT keeps Poisson state in block DBs and lowers the stencil frontier with
// explicit row-face halo byte windows plus compact column payload deps.
// RT-DAG: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%c1, %c1, %c32, %c32]
// RT: arts.db_acquire[<in>]{{.*}}element_offsets[%c0, %c0, %c31, %c0] element_sizes[%c1, %c1, %c1, %c32]{{.*}}haloViewDependency
// RT: arts.db_acquire[<in>]{{.*}}element_offsets[%c0, %c0, %c0, %c0] element_sizes[%c1, %c1, %c1, %c32]{{.*}}haloViewDependency
// RT: arts_rt.edt_param_pack
// RT: %[[HALO_EDT:[A-Za-z0-9_]+]] = arts_rt.edt_create(%{{[A-Za-z0-9_]+}} : memref<?xi64>){{.*}}epoch(%[[HALO_EPOCH:[A-Za-z0-9_]+]] : i64)
// RT: arts_rt.rec_dep %[[HALO_EDT]]
// RT-SAME: byte_offsets(%c0, %c0, %c7936, %c0, %c0, %c0, %c0)
// RT-SAME: byte_sizes(%c0, %c0, %c256, %c256, %c0, %c0, %c0)
// RT-SAME: acquire_modes = array<i32: 1, 1, 1, 1, 1, 1, 2>
// RT-SAME: dep_flags = array<i32: 0, 0, 4, 4, 0, 0, 0>
// RT-NOT: arts_rt.create_epoch
// RT: arts_rt.wait_on_epoch %[[HALO_EPOCH]] : i64
// RT: %[[VERIFY_PACK:[A-Za-z0-9_]+]] = arts_rt.edt_param_pack
// RT: %[[VERIFY_EDT:[A-Za-z0-9_]+]] = arts_rt.edt_create(%[[VERIFY_PACK]] : memref<?xi64>){{.*}}epoch(%[[VERIFY_EPOCH:[A-Za-z0-9_]+]] : i64)
// RT: arts_rt.rec_dep %[[VERIFY_EDT]]
// RT: arts_rt.wait_on_epoch %[[VERIFY_EPOCH]] : i64
