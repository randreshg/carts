// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile external/carts-benchmarks/ml-kernels/layernorm/layernorm.c -O3 -DBATCH=4 -DHIDDEN=16 -DNREPS=2 \
// RUN:   --arts-config %inputs_dir/arts_2t.cfg --pipeline=sde-to-arts
// RUN: %FileCheck %s < %t.dir/layernorm.sde-to-arts.mlir

// Layernorm has residual init/timer/checksum regions around the distributed
// kernel. Every CU that touches the DB-backed x/gamma/beta MUs needs an
// SDE-authored access window before ARTS dependency conversion.

// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc{{.*}}<block>
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<inout>]{{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<coarse>)
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>)
