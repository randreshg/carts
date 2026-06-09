// RUN: cd %S/../.. && not dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 --arts-config %arts_config -o %t_arts 2>&1 | %FileCheck %s

// The current 2D Jacobi parallel-for shape requests a non-contiguous halo face
// from a host-whole DB view. CODIR must fail closed instead of widening that
// halo to a whole DB and relying on ARTS/runtime to repair the mismatch.
// CHECK: cannot materialize non-contiguous
// CHECK: halo destination slice
// CHECK: requests
// CHECK: compute-block
// CHECK: storage
// CHECK: host-whole DB view
