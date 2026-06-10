// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=10240 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=codir-to-arts
// RUN: %FileCheck %s < %t.dir/jacobi-for.codir-to-arts.mlir

// CHECK: %[[C7:[A-Za-z0-9_]+]] = arith.constant 7 : index
// CHECK: %[[REMAIN:[A-Za-z0-9_]+]] = arith.subi
// CHECK: %[[GROUP:[A-Za-z0-9_]+]] = arith.minui %[[REMAIN]], %[[C7]] : index
// CHECK: arts.db_acquire[<out>] {{.*}} sizes[%[[GROUP]], %{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <intranode> route{{.*}} attributes {{.*}}planLogicalWorkerSlice = [3584, 512]
// CHECK: arts.db_ref %arg{{[0-9]+}}[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
