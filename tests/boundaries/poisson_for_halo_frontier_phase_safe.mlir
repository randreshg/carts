// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts cgeist external/carts-benchmarks/kastors-jacobi/poisson-for/poisson-for.c \
// RUN:   -O3 -DSIZE=256 -DNREPS=10 -Iexternal/carts-benchmarks/kastors-jacobi/include \
// RUN:   -S -fopenmp -o %t.dir/poisson-for.mlir
// RUN: cd %S/../.. && .dekk/env/bin/dekk carts compile %t.dir/poisson-for.mlir -O3 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement \
// RUN:   > %t.dir/poisson-for.post-db-refinement.mlir
// RUN: %FileCheck %s --check-prefix=ARTS --implicit-check-not='host_whole' \
// RUN:   --implicit-check-not='byte_sizes({{.*}}%c8192' \
// RUN:   --implicit-check-not='element_sizes[%c1, %c1, %c32, %c32]{{.*}}haloViewDependency' \
// RUN:   < %t.dir/poisson-for.post-db-refinement.mlir

// ARTS keeps Poisson state in block DBs and represents the stencil frontier with
// explicit row-face halo windows plus compact column payload deps.
// ARTS-DAG: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%c32, %c1]{{.*}}compact_halo_payload
// ARTS-DAG: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%c32, %c1]{{.*}}compact_halo_payload
// ARTS: arts.edt <task> <internode> route{{.*}} attributes {compactHaloPack}
// ARTS: arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
// ARTS: arts.db_acquire[<in>]{{.*}}element_offsets[%c31, %c0] element_sizes[%c1, %c32]{{.*}}haloViewDependency
// ARTS-SAME: stencil_max_offsets = [0, 0]
// ARTS-SAME: stencil_min_offsets = [-1, 0]
// ARTS: arts.db_acquire[<in>]{{.*}}element_offsets[%c0, %c0] element_sizes[%c1, %c32]{{.*}}haloViewDependency
// ARTS-SAME: stencil_max_offsets = [1, 0]
// ARTS-SAME: stencil_min_offsets = [0, 0]
// ARTS: arts.db_acquire[<in>]{{.*}}bounds_valid
// ARTS-SAME: runtime_db_mode = #arts.runtime_db_mode<ro>
// ARTS: scf.if {{.*}} -> (f64)
