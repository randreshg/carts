// RUN: cd %S/../.. && not dekk carts compile %samples_dir/stencil/jacobi2d_two_sweep.c -O3 --arts-config %arts_config -o %t_arts 2>&1 | %FileCheck %s

// This source currently lowers to a 2D host-whole-to-compute-block halo whose
// face is not contiguous in row-major storage. The legal behavior is to reject
// it at CODIR-to-ARTS, not to send a whole DB or stamp a downstream contract.
// CHECK: cannot materialize non-contiguous
// CHECK: halo destination slice
// CHECK: requests
// CHECK: compute-block
// CHECK: storage
// CHECK: host-whole DB view
