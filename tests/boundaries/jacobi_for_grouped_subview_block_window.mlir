// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=10240 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement
// RUN: %FileCheck %s < %t.dir/jacobi-for.post-db-refinement.mlir

// CHECK-DAG: arith.constant 1280 : index
// CHECK-DAG: arith.constant 512 : index
// CHECK-DAG: arith.constant 256 : index
// CHECK-DAG: arith.constant 40 : index
// CHECK-DAG: arith.constant 8 : index
// CHECK-DAG: arith.constant 5 : index
// CHECK-DAG: arith.constant 1 : index
// CHECK: arts.db_alloc{{.*}}sizes[%c8, %c8]{{.*}}elementSizes[%c1280, %c1280]
// CHECK: arts.db_alloc{{.*}}sizes[%c40, %c40]{{.*}}elementSizes[%c256, %c256]{{.*}}perBlockSingleWriterStencil
// CHECK: arith.minui %{{[A-Za-z0-9_]+}}, %c1 : index
// CHECK: arts.db_acquire[<out>] {{.*}}partitioning(<block>){{.*}}runtime_db_mode<ew>
// CHECK: arts.edt <task> <internode> route{{.*}}ownerLocalWriterSplit
// CHECK: arith.minui %{{[A-Za-z0-9_]+}}, %c1 : index
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>){{.*}}runtime_db_mode<ro>
// CHECK: arts.db_acquire[<in>] {{.*}}sizes[%c1, %{{[A-Za-z0-9_]+}}] bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}}sizes[%c1, %{{[A-Za-z0-9_]+}}] bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}}sizes[%{{[A-Za-z0-9_]+}}, %c1] bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}}sizes[%{{[A-Za-z0-9_]+}}, %c1] bounds_valid
// CHECK: arts.runtime_query <total_nodes>
// CHECK: arts.edt <task> <internode> route
// CHECK: arts.db_ref %arg{{[0-9]+}}[%c0, %c0]
// CHECK: arts.db_ref %arg{{[0-9]+}}[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
