// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=arts-rt-to-llvm
// RUN: %FileCheck %s < %t.dir/jacobi-for.arts-rt-to-llvm.mlir

// Jacobi's committed ARTS halo-view acquire must survive through final
// runtime-call lowering as the halo dependence API, not as a whole-DB or
// legacy pointer-slice dependence.

// CHECK: {{func[.]call|llvm[.]call|call}} @arts_add_halo_dependence(
// CHECK-NOT: {{func[.]call|llvm[.]call|call}} @arts_add_dependence_at
