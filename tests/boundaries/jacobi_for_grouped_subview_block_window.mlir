// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=10240 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement
// RUN: %FileCheck %s < %t.dir/jacobi-for.post-db-refinement.mlir

// CHECK: %[[C3584:[A-Za-z0-9_]+]] = arith.constant 3584 : index
// CHECK: %[[C512:[A-Za-z0-9_]+]] = arith.constant 512 : index
// CHECK: scf.for %[[ROW_BASE:arg[0-9]+]] = {{.*}} step %[[C3584]]
// CHECK: scf.for %[[COL_BASE:arg[0-9]+]] = {{.*}} step %[[C512]]
// CHECK: %[[ROW_BLOCK:[A-Za-z0-9_]+]] = arith.divui %[[ROW_BASE]], %[[C512]] : index
// CHECK: %[[COL_BLOCK:[A-Za-z0-9_]+]] = arith.divui %[[COL_BASE]], %[[C512]] : index
// CHECK: %[[ROW_REMAIN:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %[[ROW_BLOCK]] : index
// CHECK: %[[ROW_GROUP:[A-Za-z0-9_]+]] = arith.minui %[[ROW_REMAIN]], %{{[A-Za-z0-9_]+}} : index
// CHECK: %[[COL_REMAIN:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %[[COL_BLOCK]] : index
// CHECK: %[[COL_GROUP:[A-Za-z0-9_]+]] = arith.minui %[[COL_REMAIN]], %{{[A-Za-z0-9_]+}} : index
// CHECK: arts.db_acquire[<{{inout|out}}>] {{.*}} offsets[%[[ROW_BLOCK]], %[[COL_BLOCK]]], sizes[%[[ROW_GROUP]], %[[COL_GROUP]]]
// CHECK: arts.edt <task> <intranode> route
// CHECK: %[[INNER_BLOCK:[A-Za-z0-9_]+]] = arith.divui %{{[A-Za-z0-9_]+}}, %[[C512]] : index
// CHECK: %[[LOCAL_BLOCK:[A-Za-z0-9_]+]] = arith.subi %[[INNER_BLOCK]], %arg{{[0-9]+}} : index
// CHECK: arts.db_ref %arg{{[0-9]+}}[%[[LOCAL_BLOCK]], %{{[A-Za-z0-9_]+}}]
