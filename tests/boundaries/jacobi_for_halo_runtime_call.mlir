// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=arts-rt-to-llvm
// RUN: %FileCheck %s --implicit-check-not='{{func[.]call|llvm[.]call|call}} @arts_add_halo_dependence({{.*}}%c8192' \
// RUN:   < %t.dir/jacobi-for.arts-rt-to-llvm.mlir

// Jacobi's committed ARTS halo-view acquire must survive through final
// runtime-call lowering as the halo dependence API, not as a whole-DB or
// legacy pointer-slice dependence. The halo byte length is one 32-element
// f64 row, not the 32x32 full block.

// CHECK-DAG: %[[FACE_BYTES:[A-Za-z0-9_]+]] = arith.constant 256 : i64
// CHECK: {{func[.]call|llvm[.]call|call}} @arts_add_halo_dependence({{.*}}, %[[FACE_BYTES]])
// CHECK: {{func[.]call|llvm[.]call|call}} @arts_add_halo_dependence({{.*}}, %[[FACE_BYTES]])
// CHECK-NOT: {{func[.]call|llvm[.]call|call}} @arts_add_dependence_at
