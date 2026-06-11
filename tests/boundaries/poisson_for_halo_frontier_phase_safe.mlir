// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts cgeist external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c \
// RUN:   -O3 -DSIZE=256 -DNREPS=10 -Iexternal/carts-benchmarks/kastors-jacobi/include \
// RUN:   -S -fopenmp -o %t.dir/poisson-for.mlir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts compile %t.dir/poisson-for.mlir -O3 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=pre-lowering \
// RUN:   > %t.dir/poisson-for.pre-lowering.mlir
// RUN: %FileCheck %s --check-prefix=RT < %t.dir/poisson-for.pre-lowering.mlir

// RT keeps Jacobi state in block DBs and marks the stencil dependency with
// explicit block-halo facts. The compute EDT indexes dependency payloads through
// block-local coordinates and launches the halo/stencil frontier once before
// waiting on it.
// RT-DAG: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%c1, %c1, %c32, %c32]
// RT-DAG: arts.db_alloc{{.*}}planHaloShape = [1, 1, 0, 0]
// RT: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}stencil_supported_block_halo
// RT: arts_rt.rec_dep
// RT-SAME: acquire_modes = array<i32: 1, 1, 2
// RT: %[[HALO_EPOCH:.*]] = arts_rt.create_epoch : i64
// RT-NEXT: scf.for
// RT-NOT: arts_rt.create_epoch
// RT: arts_rt.wait_on_epoch %[[HALO_EPOCH]] : i64
// RT: %[[COMPUTE_EPOCH:.*]] = arts_rt.create_epoch : i64
// RT-NEXT: scf.for
