// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=10240 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement
// RUN: %FileCheck %s < %t.dir/jacobi-for.post-db-refinement.mlir

// CHECK: %[[C7:[A-Za-z0-9_]+]] = arith.constant 7 : index
// CHECK: %[[C3584:[A-Za-z0-9_]+]] = arith.constant 3584 : index
// CHECK: scf.for {{.*}} step %[[C3584]]
// CHECK: %[[REMAIN:[A-Za-z0-9_]+]] = arith.subi
// CHECK: %[[GROUP:[A-Za-z0-9_]+]] = arith.minui %[[REMAIN]], %[[C7]] : index
// CHECK: arts.db_acquire[<{{inout|out}}>] {{.*}} sizes[%[[GROUP]], %{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <intranode> route{{.*}} attributes {{.*}}planLogicalWorkerSlice = [3584, 512]
// CHECK: %[[LOCAL_BLOCK:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %arg{{[0-9]+}} : index
// CHECK: arts.db_ref %arg{{[0-9]+}}[%[[LOCAL_BLOCK]], %{{[A-Za-z0-9_]+}}]
